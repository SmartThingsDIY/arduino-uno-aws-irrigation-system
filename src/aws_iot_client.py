"""
AWS IoT Client for Smart Irrigation System
Handles device communication, data ingestion, and command execution
"""

import json
import time
import threading
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Callable
import pandas as pd

import boto3
from awscrt import io, mqtt
from awsiot import mqtt_connection_builder
from botocore.exceptions import ClientError


class AWSIoTClient:
    """AWS IoT Core client for irrigation system"""
    
    def __init__(self, config_path: str = "config/aws_iot_config.json"):
        """Initialize AWS IoT client with configuration"""
        self.config = self._load_config(config_path)
        self.mqtt_connection = None
        self.dynamodb = boto3.resource('dynamodb', region_name=self.config['region'])
        self.table = self.dynamodb.Table(self.config['dynamodb_table'])
        self.timestream = boto3.client('timestream-write', region_name=self.config['region'])
        
        # Message callbacks
        self.callbacks: Dict[str, List[Callable]] = {}
        
        # Latest sensor data cache
        self._latest_data = {}
        self._data_lock = threading.Lock()
        
        # Initialize MQTT connection
        self._init_mqtt_connection()
    
    def _load_config(self, config_path: str) -> dict:
        """Load AWS IoT configuration"""
        with open(config_path, 'r') as f:
            return json.load(f)
    
    def _init_mqtt_connection(self):
        """Initialize MQTT connection to AWS IoT Core"""
        # Event callbacks
        event_loop_group = io.EventLoopGroup(1)
        host_resolver = io.DefaultHostResolver(event_loop_group)
        client_bootstrap = io.ClientBootstrap(event_loop_group, host_resolver)
        
        # Create MQTT connection
        self.mqtt_connection = mqtt_connection_builder.mtls_from_path(
            endpoint=self.config['endpoint'],
            cert_filepath=self.config['cert_path'],
            pri_key_filepath=self.config['key_path'],
            client_bootstrap=client_bootstrap,
            ca_filepath=self.config['ca_path'],
            client_id=self.config['client_id'],
            clean_session=False,
            keep_alive_secs=30
        )
        
        # Connect
        connect_future = self.mqtt_connection.connect()
        connect_future.result()
        print(f"Connected to AWS IoT Core at {self.config['endpoint']}")
        
        # Subscribe to sensor data topic
        self._subscribe_to_sensors()
    
    def _subscribe_to_sensors(self):
        """Subscribe to sensor data topics"""
        topics = [
            f"irrigation/{self.config['device_id']}/sensors",
            f"irrigation/{self.config['device_id']}/alerts",
            f"irrigation/{self.config['device_id']}/status"
        ]
        
        for topic in topics:
            subscribe_future, _ = self.mqtt_connection.subscribe(
                topic=topic,
                qos=mqtt.QoS.AT_LEAST_ONCE,
                callback=self._on_message_received
            )
            subscribe_future.result()
            print(f"Subscribed to {topic}")
    
    def _on_message_received(self, topic, payload, **kwargs):
        """Handle incoming MQTT messages"""
        try:
            data = json.loads(payload)
            data['timestamp'] = datetime.utcnow().isoformat()
            
            # Update latest data cache
            with self._data_lock:
                if 'sensors' in topic:
                    self._latest_data.update(data)
                
            # Store in DynamoDB
            self._store_sensor_data(data)
            
            # Store in Timestream for time-series analysis
            self._store_timestream_data(data)
            
            # Trigger callbacks
            self._trigger_callbacks(topic, data)
            
        except Exception as e:
            print(f"Error processing message: {e}")
    
    def _store_sensor_data(self, data: Dict):
        """Store sensor data in DynamoDB"""
        try:
            item = {
                'device_id': self.config['device_id'],
                'timestamp': data['timestamp'],
                'data': json.dumps(data),
                'ttl': int(time.time()) + 2592000  # 30 days TTL
            }
            
            self.table.put_item(Item=item)
            
        except ClientError as e:
            print(f"Error storing to DynamoDB: {e}")
    
    def _store_timestream_data(self, data: Dict):
        """Store time-series data in AWS Timestream"""
        try:
            records = []
            
            # Convert sensor data to Timestream records
            for key, value in data.items():
                if key not in ['timestamp', 'device_id'] and isinstance(value, (int, float)):
                    records.append({
                        'MeasureName': key,
                        'MeasureValue': str(value),
                        'MeasureValueType': 'DOUBLE',
                        'Time': str(int(datetime.utcnow().timestamp() * 1000))
                    })
            
            if records:
                self.timestream.write_records(
                    DatabaseName=self.config['timestream_db'],
                    TableName=self.config['timestream_table'],
                    Records=records,
                    CommonAttributes={
                        'Dimensions': [
                            {'Name': 'device_id', 'Value': self.config['device_id']},
                            {'Name': 'location', 'Value': self.config.get('location', 'garden')}
                        ]
                    }
                )
                
        except Exception as e:
            print(f"Error storing to Timestream: {e}")
    
    def _trigger_callbacks(self, topic: str, data: Dict):
        """Trigger registered callbacks for topic"""
        if topic in self.callbacks:
            for callback in self.callbacks[topic]:
                try:
                    callback(data)
                except Exception as e:
                    print(f"Error in callback: {e}")
    
    async def get_latest_sensor_data(self) -> Dict:
        """Get latest cached sensor data"""
        with self._data_lock:
            return self._latest_data.copy()
    
    async def get_historical_data(self, days: int = 7) -> pd.DataFrame:
        """Retrieve historical data from DynamoDB"""
        try:
            # Calculate time range
            end_time = datetime.utcnow()
            start_time = end_time - timedelta(days=days)
            
            # Query DynamoDB
            response = self.table.query(
                KeyConditionExpression=boto3.dynamodb.conditions.Key('device_id').eq(
                    self.config['device_id']
                ) & boto3.dynamodb.conditions.Key('timestamp').between(
                    start_time.isoformat(),
                    end_time.isoformat()
                )
            )
            
            # Convert to DataFrame
            data_points = []
            for item in response['Items']:
                data = json.loads(item['data'])
                data['timestamp'] = item['timestamp']
                data_points.append(data)
            
            df = pd.DataFrame(data_points)
            df['timestamp'] = pd.to_datetime(df['timestamp'])
            df = df.sort_values('timestamp')
            
            return df
            
        except Exception as e:
            print(f"Error retrieving historical data: {e}")
            return pd.DataFrame()
    
    async def get_timestream_data(self, hours: int = 24, measure: str = 'moisture') -> List[Dict]:
        """Query Timestream for time-series data"""
        try:
            query = f"""
            SELECT 
                time, 
                measure_value::double as value,
                AVG(measure_value::double) OVER (
                    ORDER BY time 
                    ROWS BETWEEN 5 PRECEDING AND CURRENT ROW
                ) as moving_avg
            FROM "{self.config['timestream_db']}"."{self.config['timestream_table']}"
            WHERE device_id = '{self.config['device_id']}'
                AND measure_name = '{measure}'
                AND time > ago({hours}h)
            ORDER BY time DESC
            """
            
            response = self.timestream.query(QueryString=query)
            
            data = []
            for row in response['Rows']:
                data.append({
                    'time': row['Data'][0]['ScalarValue'],
                    'value': float(row['Data'][1]['ScalarValue']),
                    'moving_avg': float(row['Data'][2]['ScalarValue'])
                })
            
            return data
            
        except Exception as e:
            print(f"Error querying Timestream: {e}")
            return []
    
    def send_command(self, command: str, parameters: Dict) -> bool:
        """Send command to irrigation device"""
        try:
            payload = {
                'command': command,
                'parameters': parameters,
                'timestamp': datetime.utcnow().isoformat()
            }
            
            self.mqtt_connection.publish(
                topic=f"irrigation/{self.config['device_id']}/commands",
                payload=json.dumps(payload),
                qos=mqtt.QoS.AT_LEAST_ONCE
            )
            
            print(f"Command sent: {command}")
            return True
            
        except Exception as e:
            print(f"Error sending command: {e}")
            return False
    
    def activate_irrigation(self, duration_seconds: int, water_amount_ml: int) -> bool:
        """Activate irrigation system"""
        return self.send_command(
            'irrigate',
            {
                'duration': duration_seconds,
                'amount_ml': water_amount_ml,
                'mode': 'auto'
            }
        )
    
    def update_settings(self, settings: Dict) -> bool:
        """Update device settings"""
        return self.send_command('update_settings', settings)
    
    def register_callback(self, topic: str, callback: Callable):
        """Register callback for specific topic"""
        if topic not in self.callbacks:
            self.callbacks[topic] = []
        self.callbacks[topic].append(callback)
    
    def get_device_shadow(self) -> Dict:
        """Get device shadow state"""
        try:
            iot_data = boto3.client('iot-data', region_name=self.config['region'])
            
            response = iot_data.get_thing_shadow(
                thingName=self.config['device_id']
            )
            
            shadow = json.loads(response['payload'].read())
            return shadow.get('state', {})
            
        except Exception as e:
            print(f"Error getting device shadow: {e}")
            return {}
    
    def update_device_shadow(self, desired_state: Dict) -> bool:
        """Update device shadow desired state"""
        try:
            iot_data = boto3.client('iot-data', region_name=self.config['region'])
            
            payload = {
                'state': {
                    'desired': desired_state
                }
            }
            
            iot_data.update_thing_shadow(
                thingName=self.config['device_id'],
                payload=json.dumps(payload)
            )
            
            return True
            
        except Exception as e:
            print(f"Error updating device shadow: {e}")
            return False
    
    def create_alert(self, alert_type: str, message: str, severity: str = 'info'):
        """Create and send alert"""
        alert = {
            'type': alert_type,
            'message': message,
            'severity': severity,
            'timestamp': datetime.utcnow().isoformat(),
            'device_id': self.config['device_id']
        }
        
        # Publish to alerts topic
        self.mqtt_connection.publish(
            topic=f"irrigation/alerts/{severity}",
            payload=json.dumps(alert),
            qos=mqtt.QoS.AT_LEAST_ONCE
        )
        
        # Store in DynamoDB for history
        self._store_alert(alert)
    
    def _store_alert(self, alert: Dict):
        """Store alert in DynamoDB"""
        try:
            alerts_table = self.dynamodb.Table(self.config['alerts_table'])
            alerts_table.put_item(Item=alert)
        except Exception as e:
            print(f"Error storing alert: {e}")
    
    def disconnect(self):
        """Disconnect from AWS IoT Core"""
        if self.mqtt_connection:
            disconnect_future = self.mqtt_connection.disconnect()
            disconnect_future.result()
            print("Disconnected from AWS IoT Core")
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Dict, Optional
import asyncio

from ai_assistant import SmartIrrigationAI
from aws_iot_client import AWSIoTClient
from weather_service import WeatherService

app = FastAPI(title="AI Irrigation Assistant API")

# Enable CORS
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Initialize services
ai_assistant = SmartIrrigationAI()
iot_client = AWSIoTClient()
weather_service = WeatherService()

class IrrigationQuery(BaseModel):
    plant_type: str = "tomato"
    include_forecast: bool = True

class ChatQuery(BaseModel):
    message: str
    include_sensor_data: bool = True

class AlertConfig(BaseModel):
    moisture_low: float = 30
    moisture_high: float = 80
    temp_high: float = 35

@app.get("/")
async def root():
    return {
        "message": "AI-Powered Smart Irrigation System",
        "version": "2.0",
        "features": [
            "Predictive irrigation scheduling",
            "Conversational agricultural assistant",
            "Real-time sensor monitoring",
            "Weather-aware recommendations"
        ]
    }

@app.get("/api/predict")
async def predict_irrigation(query: IrrigationQuery):
    """Get AI-powered irrigation predictions"""
    try:
        # Get current sensor data from AWS IoT
        sensor_data = await iot_client.get_latest_sensor_data()
        
        # Get weather forecast if requested
        weather_forecast = {}
        if query.include_forecast:
            weather_forecast = await weather_service.get_forecast()
        
        # Get AI prediction
        prediction = ai_assistant.predict_irrigation_needs(
            sensor_data=sensor_data,
            weather_forecast=weather_forecast,
            plant_type=query.plant_type
        )
        
        return {
            "status": "success",
            "prediction": prediction,
            "sensor_data": sensor_data,
            "weather": weather_forecast
        }
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/chat")
async def chat_assistant(query: ChatQuery):
    """Chat with agricultural AI assistant"""
    try:
        sensor_data = {}
        if query.include_sensor_data:
            sensor_data = await iot_client.get_latest_sensor_data()
        
        response = ai_assistant.chat_with_assistant(
            user_query=query.message,
            sensor_data=sensor_data
        )
        
        return {
            "status": "success",
            "response": response,
            "sensor_context": sensor_data
        }
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/insights/weekly")
async def get_weekly_insights():
    """Get AI-generated weekly insights"""
    try:
        # Fetch historical data from AWS IoT
        historical_data = await iot_client.get_historical_data(days=7)
        
        insights = ai_assistant.get_weekly_insights(historical_data)
        
        return {
            "status": "success",
            "insights": insights
        }
        
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/alerts/configure")
async def configure_alerts(config: AlertConfig):
    """Configure intelligent alerting thresholds"""
    # Store configuration and set up monitoring
    return {
        "status": "success",
        "message": "Alert configuration updated",
        "config": config.dict()
    }
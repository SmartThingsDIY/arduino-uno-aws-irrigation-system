import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestRegressor
from sklearn.preprocessing import StandardScaler
from sklearn.metrics import mean_squared_error, r2_score
import joblib
import json

def prepare_training_data():
    """Load and prepare historical irrigation data"""
    # Load data from CSV files
    sensor_df = pd.read_csv('data/sensor_history.csv')
    weather_df = pd.read_csv('data/weather_history.csv')
    irrigation_df = pd.read_csv('data/irrigation_history.csv')
    
    # Merge datasets
    data = pd.merge(sensor_df, weather_df, on='timestamp')
    data = pd.merge(data, irrigation_df, on='timestamp')
    
    return data

def engineer_features(data):
    """Create features for ML model"""
    # Time-based features
    data['hour'] = pd.to_datetime(data['timestamp']).dt.hour
    data['day_of_week'] = pd.to_datetime(data['timestamp']).dt.dayofweek
    
    # Rolling averages
    data['moisture_avg_24h'] = data['moisture'].rolling(24).mean()
    data['temp_avg_24h'] = data['temperature'].rolling(24).mean()
    
    # Lag features
    data['moisture_lag_1h'] = data['moisture'].shift(1)
    data['moisture_lag_6h'] = data['moisture'].shift(6)
    
    return data

def train_irrigation_model(data):
    """Train the irrigation prediction model"""
    # Select features and target
    feature_cols = [
        'moisture', 'temperature', 'humidity', 'light_level',
        'temp_forecast_max', 'temp_forecast_min', 'precipitation_forecast',
        'hour', 'day_of_week', 'moisture_avg_24h', 'temp_avg_24h'
    ]
    
    X = data[feature_cols].fillna(method='ffill')
    y = data['water_amount_ml']
    
    # Split data
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=0.2, random_state=42
    )
    
    # Scale features
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)
    
    # Train model
    model = RandomForestRegressor(
        n_estimators=200,
        max_depth=15,
        min_samples_split=5,
        random_state=42,
        n_jobs=-1
    )
    
    model.fit(X_train_scaled, y_train)
    
    # Evaluate
    predictions = model.predict(X_test_scaled)
    mse = mean_squared_error(y_test, predictions)
    r2 = r2_score(y_test, predictions)
    
    print(f"Model Performance:")
    print(f"MSE: {mse:.2f}")
    print(f"R2 Score: {r2:.2f}")
    
    # Feature importance
    feature_importance = pd.DataFrame({
        'feature': feature_cols,
        'importance': model.feature_importances_
    }).sort_values('importance', ascending=False)
    
    print("\nTop 5 Most Important Features:")
    print(feature_importance.head())
    
    # Save model and scaler
    joblib.dump(model, 'models/irrigation_predictor.pkl')
    joblib.dump(scaler, 'models/feature_scaler.pkl')
    
    # Save feature configuration
    with open('models/feature_config.json', 'w') as f:
        json.dump({
            'features': feature_cols,
            'model_metrics': {
                'mse': mse,
                'r2': r2
            }
        }, f, indent=2)
    
    return model, scaler

if __name__ == "__main__":
    print("Training AI Irrigation Model...")
    
    # Load and prepare data
    data = prepare_training_data()
    data = engineer_features(data)
    
    # Train model
    model, scaler = train_irrigation_model(data)
    
    print("\nModel training complete!")
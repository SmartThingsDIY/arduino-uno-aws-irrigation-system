"""
Weather Service Integration
Provides weather data and forecasts for intelligent irrigation decisions
"""

import os
import asyncio
import aiohttp
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
import json
from functools import lru_cache


@dataclass
class WeatherData:
    """Weather data structure"""
    temperature: float
    humidity: float
    pressure: float
    wind_speed: float
    wind_direction: int
    precipitation: float
    uv_index: float
    description: str
    icon: str
    timestamp: datetime


@dataclass
class WeatherForecast:
    """Weather forecast structure"""
    date: datetime
    temp_min: float
    temp_max: float
    humidity: float
    precipitation: float
    precipitation_probability: float
    wind_speed: float
    description: str
    icon: str


class WeatherService:
    """Multi-provider weather service with fallback support"""
    
    def __init__(self, config_path: str = "config/weather_config.json"):
        """Initialize weather service with API configurations"""
        self.config = self._load_config(config_path)
        self.session = None
        self._cache = {}
        self._cache_duration = timedelta(minutes=30)
        
        # Provider priority order
        self.providers = [
            self._get_openweather,
            self._get_weatherapi,
            self._get_meteomatics
        ]
    
    def _load_config(self, config_path: str) -> dict:
        """Load weather API configuration"""
        with open(config_path, 'r') as f:
            config = json.load(f)
        
        # Override with environment variables if available
        config['openweather_api_key'] = os.getenv('OPENWEATHER_API_KEY', config.get('openweather_api_key'))
        config['weatherapi_key'] = os.getenv('WEATHERAPI_KEY', config.get('weatherapi_key'))
        
        return config
    
    async def __aenter__(self):
        """Async context manager entry"""
        self.session = aiohttp.ClientSession()
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """Async context manager exit"""
        if self.session:
            await self.session.close()
    
    def _get_cache_key(self, method: str, params: dict) -> str:
        """Generate cache key"""
        return f"{method}:{json.dumps(params, sort_keys=True)}"
    
    def _is_cache_valid(self, cache_key: str) -> bool:
        """Check if cached data is still valid"""
        if cache_key not in self._cache:
            return False
        
        cached_time = self._cache[cache_key]['timestamp']
        return datetime.utcnow() - cached_time < self._cache_duration
    
    async def get_current_weather(self, lat: float = None, lon: float = None) -> Optional[WeatherData]:
        """Get current weather data with fallback providers"""
        if lat is None:
            lat = self.config['default_latitude']
        if lon is None:
            lon = self.config['default_longitude']
        
        cache_key = self._get_cache_key('current', {'lat': lat, 'lon': lon})
        
        # Check cache
        if self._is_cache_valid(cache_key):
            return self._cache[cache_key]['data']
        
        # Try each provider in order
        for provider in self.providers:
            try:
                weather = await provider(lat, lon, 'current')
                if weather:
                    # Cache successful result
                    self._cache[cache_key] = {
                        'data': weather,
                        'timestamp': datetime.utcnow()
                    }
                    return weather
            except Exception as e:
                print(f"Weather provider error: {e}")
                continue
        
        return None
    
    async def get_forecast(self, days: int = 5, lat: float = None, lon: float = None) -> List[WeatherForecast]:
        """Get weather forecast for specified days"""
        if lat is None:
            lat = self.config['default_latitude']
        if lon is None:
            lon = self.config['default_longitude']
        
        cache_key = self._get_cache_key('forecast', {'lat': lat, 'lon': lon, 'days': days})
        
        # Check cache
        if self._is_cache_valid(cache_key):
            return self._cache[cache_key]['data']
        
        # Try each provider
        for provider in self.providers:
            try:
                forecast = await provider(lat, lon, 'forecast', days=days)
                if forecast:
                    # Cache successful result
                    self._cache[cache_key] = {
                        'data': forecast,
                        'timestamp': datetime.utcnow()
                    }
                    return forecast
            except Exception as e:
                print(f"Forecast provider error: {e}")
                continue
        
        return []
    
    async def _get_openweather(self, lat: float, lon: float, data_type: str, **kwargs) -> Optional[any]:
        """Get data from OpenWeatherMap API"""
        if not self.config.get('openweather_api_key'):
            return None
        
        api_key = self.config['openweather_api_key']
        
        if data_type == 'current':
            url = f"https://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={api_key}&units=metric"
            
            async with self.session.get(url) as response:
                if response.status == 200:
                    data = await response.json()
                    
                    return WeatherData(
                        temperature=data['main']['temp'],
                        humidity=data['main']['humidity'],
                        pressure=data['main']['pressure'],
                        wind_speed=data['wind']['speed'],
                        wind_direction=data['wind'].get('deg', 0),
                        precipitation=data.get('rain', {}).get('1h', 0),
                        uv_index=0,  # Not available in basic API
                        description=data['weather'][0]['description'],
                        icon=data['weather'][0]['icon'],
                        timestamp=datetime.utcnow()
                    )
        
        elif data_type == 'forecast':
            days = kwargs.get('days', 5)
            url = f"https://api.openweathermap.org/data/2.5/forecast?lat={lat}&lon={lon}&appid={api_key}&units=metric&cnt={days * 8}"
            
            async with self.session.get(url) as response:
                if response.status == 200:
                    data = await response.json()
                    
                    # Group by day and aggregate
                    daily_forecasts = {}
                    
                    for item in data['list']:
                        date = datetime.fromtimestamp(item['dt']).date()
                        
                        if date not in daily_forecasts:
                            daily_forecasts[date] = {
                                'temps': [],
                                'humidity': [],
                                'precipitation': [],
                                'wind_speed': [],
                                'descriptions': []
                            }
                        
                        daily_forecasts[date]['temps'].append(item['main']['temp'])
                        daily_forecasts[date]['humidity'].append(item['main']['humidity'])
                        daily_forecasts[date]['precipitation'].append(item.get('rain', {}).get('3h', 0))
                        daily_forecasts[date]['wind_speed'].append(item['wind']['speed'])
                        daily_forecasts[date]['descriptions'].append(item['weather'][0]['description'])
                    
                    # Convert to WeatherForecast objects
                    forecasts = []
                    for date, values in sorted(daily_forecasts.items())[:days]:
                        forecasts.append(WeatherForecast(
                            date=datetime.combine(date, datetime.min.time()),
                            temp_min=min(values['temps']),
                            temp_max=max(values['temps']),
                            humidity=sum(values['humidity']) / len(values['humidity']),
                            precipitation=sum(values['precipitation']),
                            precipitation_probability=len([p for p in values['precipitation'] if p > 0]) / len(values['precipitation']) * 100,
                            wind_speed=sum(values['wind_speed']) / len(values['wind_speed']),
                            description=max(set(values['descriptions']), key=values['descriptions'].count),
                            icon='10d' if sum(values['precipitation']) > 0 else '01d'
                        ))
                    
                    return forecasts
        
        return None
    
    async def _get_weatherapi(self, lat: float, lon: float, data_type: str, **kwargs) -> Optional[any]:
        """Get data from WeatherAPI.com"""
        if not self.config.get('weatherapi_key'):
            return None
        
        api_key = self.config['weatherapi_key']
        
        if data_type == 'current':
            url = f"https://api.weatherapi.com/v1/current.json?key={api_key}&q={lat},{lon}"
            
            async with self.session.get(url) as response:
                if response.status == 200:
                    data = await response.json()
                    
                    return WeatherData(
                        temperature=data['current']['temp_c'],
                        humidity=data['current']['humidity'],
                        pressure=data['current']['pressure_mb'],
                        wind_speed=data['current']['wind_kph'] / 3.6,  # Convert to m/s
                        wind_direction=data['current']['wind_degree'],
                        precipitation=data['current']['precip_mm'],
                        uv_index=data['current']['uv'],
                        description=data['current']['condition']['text'],
                        icon=data['current']['condition']['icon'],
                        timestamp=datetime.utcnow()
                    )
        
        elif data_type == 'forecast':
            days = kwargs.get('days', 5)
            url = f"https://api.weatherapi.com/v1/forecast.json?key={api_key}&q={lat},{lon}&days={days}"
            
            async with self.session.get(url) as response:
                if response.status == 200:
                    data = await response.json()
                    
                    forecasts = []
                    for day in data['forecast']['forecastday']:
                        forecasts.append(WeatherForecast(
                            date=datetime.strptime(day['date'], '%Y-%m-%d'),
                            temp_min=day['day']['mintemp_c'],
                            temp_max=day['day']['maxtemp_c'],
                            humidity=day['day']['avghumidity'],
                            precipitation=day['day']['totalprecip_mm'],
                            precipitation_probability=day['day']['daily_chance_of_rain'],
                            wind_speed=day['day']['maxwind_kph'] / 3.6,
                            description=day['day']['condition']['text'],
                            icon=day['day']['condition']['icon']
                        ))
                    
                    return forecasts
        
        return None
    
    async def _get_meteomatics(self, lat: float, lon: float, data_type: str, **kwargs) -> Optional[any]:
        """Get data from Meteomatics API (if configured)"""
        # Placeholder for additional weather provider
        return None
    
    def analyze_irrigation_impact(self, forecast: List[WeatherForecast]) -> Dict:
        """Analyze weather forecast for irrigation planning"""
        if not forecast:
            return {
                'recommendation': 'Unable to analyze - no forecast data',
                'risk_level': 'unknown'
            }
        
        # Calculate metrics
        total_precipitation = sum(f.precipitation for f in forecast)
        avg_temp = sum(f.temp_max for f in forecast) / len(forecast)
        max_temp = max(f.temp_max for f in forecast)
        rain_days = sum(1 for f in forecast if f.precipitation > 1)
        
        # Determine irrigation recommendation
        recommendation = []
        risk_level = 'normal'
        
        if total_precipitation > 20:
            recommendation.append(f"Significant rain expected ({total_precipitation:.1f}mm total). Reduce irrigation by 50%.")
            risk_level = 'low'
        elif total_precipitation > 10:
            recommendation.append(f"Moderate rain expected ({total_precipitation:.1f}mm). Reduce irrigation by 25%.")
            risk_level = 'low'
        
        if max_temp > 35:
            recommendation.append(f"Extreme heat expected ({max_temp:.1f}°C). Increase watering frequency.")
            risk_level = 'high'
        elif avg_temp > 30:
            recommendation.append("High temperatures predicted. Monitor soil moisture closely.")
            risk_level = 'medium' if risk_level != 'high' else 'high'
        
        if rain_days == 0 and avg_temp > 25:
            recommendation.append("No rain expected with warm temperatures. Maintain regular irrigation schedule.")
            risk_level = 'medium' if risk_level == 'normal' else risk_level
        
        return {
            'recommendation': ' '.join(recommendation) if recommendation else 'Weather conditions normal. Follow standard irrigation schedule.',
            'risk_level': risk_level,
            'metrics': {
                'total_precipitation_mm': round(total_precipitation, 1),
                'average_temperature_c': round(avg_temp, 1),
                'max_temperature_c': round(max_temp, 1),
                'rain_days': rain_days
            }
        }
    
    @lru_cache(maxsize=128)
    def get_plant_weather_requirements(self, plant_type: str) -> Dict:
        """Get weather-related requirements for specific plants"""
        # Plant-specific weather preferences
        plant_requirements = {
            'tomato': {
                'optimal_temp_range': (18, 27),
                'max_safe_temp': 35,
                'min_safe_temp': 10,
                'water_loss_rate': 'high',
                'drought_tolerance': 'medium'
            },
            'lettuce': {
                'optimal_temp_range': (15, 22),
                'max_safe_temp': 28,
                'min_safe_temp': 5,
                'water_loss_rate': 'medium',
                'drought_tolerance': 'low'
            },
            'pepper': {
                'optimal_temp_range': (20, 30),
                'max_safe_temp': 38,
                'min_safe_temp': 12,
                'water_loss_rate': 'medium',
                'drought_tolerance': 'high'
            },
            'herbs': {
                'optimal_temp_range': (18, 25),
                'max_safe_temp': 32,
                'min_safe_temp': 8,
                'water_loss_rate': 'low',
                'drought_tolerance': 'high'
            }
        }
        
        return plant_requirements.get(plant_type, plant_requirements['tomato'])
"""
AI-Powered Irrigation Assistant
Integrates LLMs, RAG, and predictive ML for smart agriculture
"""

import os
import json
import numpy as np
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple

from langchain import LLMChain, PromptTemplate
from langchain.embeddings import OpenAIEmbeddings
from langchain.vectorstores import Chroma
from langchain.document_loaders import TextLoader, PDFLoader
from langchain.text_splitter import RecursiveCharacterTextSplitter
from langchain.memory import ConversationBufferWindowMemory
from langchain.chat_models import ChatOpenAI
from langchain.schema import Document

import pandas as pd
from sklearn.ensemble import RandomForestRegressor
import joblib
import requests


class SmartIrrigationAI:
    """Main AI system for intelligent irrigation management"""
    
    def __init__(self, config_path: str = "config.json"):
        """Initialize AI components"""
        self.config = self._load_config(config_path)
        self.setup_llm()
        self.setup_rag_system()
        self.setup_predictive_model()
        self.conversation_memory = ConversationBufferWindowMemory(k=5)
        
    def _load_config(self, config_path: str) -> dict:
        """Load configuration from JSON file"""
        with open(config_path, 'r') as f:
            return json.load(f)
    
    def setup_llm(self):
        """Initialize Language Model"""
        self.llm = ChatOpenAI(
            temperature=0.7,
            model_name="gpt-3.5-turbo",
            openai_api_key=os.getenv("OPENAI_API_KEY")
        )
        
        # Irrigation assistant prompt
        self.assistant_prompt = PromptTemplate(
            input_variables=["query", "context", "sensor_data", "knowledge"],
            template="""You are an expert agricultural AI assistant helping with smart irrigation.

Current Sensor Data:
{sensor_data}

Relevant Agricultural Knowledge:
{knowledge}

Context from our conversation:
{context}

User Question: {query}

Provide specific, actionable advice based on the sensor data and agricultural best practices. 
Include specific numbers and thresholds when relevant."""
        )
        
        self.llm_chain = LLMChain(
            llm=self.llm,
            prompt=self.assistant_prompt,
            memory=self.conversation_memory
        )
    
    def setup_rag_system(self):
        """Setup Retrieval Augmented Generation for agricultural knowledge"""
        # Initialize embeddings
        self.embeddings = OpenAIEmbeddings(
            openai_api_key=os.getenv("OPENAI_API_KEY")
        )
        
        # Load agricultural documents
        documents = self._load_agricultural_knowledge()
        
        # Create vector store
        self.vectorstore = Chroma.from_documents(
            documents=documents,
            embedding=self.embeddings,
            persist_directory="./chroma_db"
        )
        
    def predict_irrigation_needs(
        self, 
        sensor_data: Dict,
        weather_forecast: Dict,
        plant_type: str = "tomato"
    ) -> Dict:
        """Predict optimal irrigation based on current conditions"""
        
        # Extract features
        features = self._extract_features(sensor_data, weather_forecast, plant_type)
        
        # Make prediction
        try:
            features_scaled = self.scaler.transform([features])
            water_amount_ml = self.irrigation_model.predict(features_scaled)[0]
            
            # Get next watering time prediction
            next_water_hours = self._predict_next_watering(features_scaled)
            
            # Generate LLM-enhanced recommendation
            recommendation = self._generate_irrigation_recommendation(
                water_amount_ml, 
                next_water_hours,
                sensor_data,
                weather_forecast
            )
            
            return {
                "water_amount_ml": water_amount_ml,
                "next_watering_in_hours": next_water_hours,
                "recommendation": recommendation,
                "confidence": 0.85,
                "factors": {
                    "current_moisture": sensor_data.get("moisture", 0),
                    "temperature_impact": weather_forecast.get("temp_max", 0) > 30,
                    "rain_expected": weather_forecast.get("precipitation", 0) > 0
                }
            }
            
        except Exception as e:
            # Fallback to rule-based approach
            return self._rule_based_irrigation(sensor_data, weather_forecast, plant_type)
    
    def chat_with_assistant(
        self, 
        user_query: str,
        sensor_data: Dict
    ) -> str:
        """Interactive chat with agricultural assistant"""
        
        # Search relevant knowledge
        relevant_docs = self.vectorstore.similarity_search(user_query, k=3)
        knowledge = "\n".join([doc.page_content for doc in relevant_docs])
        
        # Format sensor data
        sensor_str = json.dumps(sensor_data, indent=2)
        
        # Get response from LLM
        response = self.llm_chain.run(
            query=user_query,
            context=self.conversation_memory.buffer,
            sensor_data=sensor_str,
            knowledge=knowledge
        )
        
        return response
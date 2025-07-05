"""
Plant Health Analyzer using Computer Vision
Detects diseases, nutrient deficiencies, and growth issues
"""

import cv2
import numpy as np
from PIL import Image
import tensorflow as tf
from typing import Dict, List, Tuple, Optional
import json
import base64
from io import BytesIO
from dataclasses import dataclass
from datetime import datetime
import os

# Optional: For advanced models
try:
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False


@dataclass
class PlantHealthResult:
    """Plant health analysis result"""
    overall_health: str  # healthy, warning, critical
    confidence: float
    issues: List[Dict]
    recommendations: List[str]
    growth_stage: str
    leaf_coverage: float
    color_analysis: Dict
    timestamp: datetime


class PlantHealthAnalyzer:
    """Analyze plant health using computer vision and ML"""
    
    def __init__(self, model_path: str = "models/plant_health"):
        """Initialize plant health analyzer"""
        self.model_path = model_path
        self.load_models()
        
        # Plant disease classes
        self.disease_classes = [
            'healthy',
            'powdery_mildew',
            'leaf_spot',
            'yellow_leaf_curl',
            'blight',
            'nutrient_deficiency',
            'pest_damage',
            'water_stress'
        ]
        
        # Color ranges for plant analysis (HSV)
        self.color_ranges = {
            'healthy_green': [(35, 40, 40), (85, 255, 255)],
            'yellow': [(20, 40, 40), (35, 255, 255)],
            'brown': [(10, 40, 40), (20, 255, 255)],
            'dark_spots': [(0, 0, 0), (180, 255, 30)]
        }
    
    def load_models(self):
        """Load pre-trained models"""
        try:
            # Load TensorFlow Lite model for edge deployment
            self.interpreter = tf.lite.Interpreter(
                model_path=f"{self.model_path}/plant_disease_model.tflite"
            )
            self.interpreter.allocate_tensors()
            
            # Get input and output details
            self.input_details = self.interpreter.get_input_details()
            self.output_details = self.interpreter.get_output_details()
            
            # Load YOLO model if available
            if YOLO_AVAILABLE and os.path.exists(f"{self.model_path}/plant_detection.pt"):
                self.yolo_model = YOLO(f"{self.model_path}/plant_detection.pt")
            else:
                self.yolo_model = None
                
            print("Plant health models loaded successfully")
            
        except Exception as e:
            print(f"Error loading models: {e}")
            self.interpreter = None
    
    async def analyze_image(self, image_data: str) -> PlantHealthResult:
        """Analyze plant health from base64 encoded image"""
        try:
            # Decode image
            image = self._decode_image(image_data)
            
            # Run multiple analyses
            disease_result = self._detect_diseases(image)
            color_analysis = self._analyze_colors(image)
            growth_analysis = self._analyze_growth(image)
            leaf_coverage = self._calculate_leaf_coverage(image)
            
            # Combine results
            overall_health, confidence = self._determine_overall_health(
                disease_result, color_analysis, growth_analysis
            )
            
            # Generate recommendations
            recommendations = self._generate_recommendations(
                overall_health, disease_result, color_analysis
            )
            
            return PlantHealthResult(
                overall_health=overall_health,
                confidence=confidence,
                issues=disease_result['issues'],
                recommendations=recommendations,
                growth_stage=growth_analysis['stage'],
                leaf_coverage=leaf_coverage,
                color_analysis=color_analysis,
                timestamp=datetime.utcnow()
            )
            
        except Exception as e:
            print(f"Error analyzing image: {e}")
            return self._get_error_result(str(e))
    
    def _decode_image(self, image_data: str) -> np.ndarray:
        """Decode base64 image to numpy array"""
        # Remove data URL prefix if present
        if ',' in image_data:
            image_data = image_data.split(',')[1]
        
        # Decode base64
        image_bytes = base64.b64decode(image_data)
        image = Image.open(BytesIO(image_bytes))
        
        # Convert to OpenCV format
        return cv2.cvtColor(np.array(image), cv2.COLOR_RGB2BGR)
    
    def _detect_diseases(self, image: np.ndarray) -> Dict:
        """Detect plant diseases using ML model"""
        if self.interpreter is None:
            return self._mock_disease_detection()
        
        try:
            # Preprocess image
            input_shape = self.input_details[0]['shape']
            processed_image = cv2.resize(image, (input_shape[1], input_shape[2]))
            processed_image = processed_image.astype(np.float32) / 255.0
            processed_image = np.expand_dims(processed_image, axis=0)
            
            # Run inference
            self.interpreter.set_tensor(self.input_details[0]['index'], processed_image)
            self.interpreter.invoke()
            
            # Get predictions
            predictions = self.interpreter.get_tensor(self.output_details[0]['index'])[0]
            
            # Parse results
            issues = []
            for i, prob in enumerate(predictions):
                if prob > 0.3 and self.disease_classes[i] != 'healthy':
                    issues.append({
                        'type': self.disease_classes[i],
                        'confidence': float(prob),
                        'severity': self._get_severity(prob)
                    })
            
            return {
                'healthy_confidence': float(predictions[0]),
                'issues': sorted(issues, key=lambda x: x['confidence'], reverse=True)
            }
            
        except Exception as e:
            print(f"Error in disease detection: {e}")
            return self._mock_disease_detection()
    
    def _analyze_colors(self, image: np.ndarray) -> Dict:
        """Analyze plant colors for health indicators"""
        # Convert to HSV for better color analysis
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        
        results = {}
        total_pixels = image.shape[0] * image.shape[1]
        
        for color_name, (lower, upper) in self.color_ranges.items():
            # Create mask for color range
            lower = np.array(lower)
            upper = np.array(upper)
            mask = cv2.inRange(hsv, lower, upper)
            
            # Calculate percentage
            pixel_count = cv2.countNonZero(mask)
            percentage = (pixel_count / total_pixels) * 100
            
            results[color_name] = round(percentage, 2)
        
        # Additional color metrics
        results['average_hue'] = float(np.mean(hsv[:, :, 0]))
        results['average_saturation'] = float(np.mean(hsv[:, :, 1]))
        results['average_brightness'] = float(np.mean(hsv[:, :, 2]))
        
        return results
    
    def _analyze_growth(self, image: np.ndarray) -> Dict:
        """Analyze plant growth stage and patterns"""
        if self.yolo_model:
            # Use YOLO for advanced detection
            results = self.yolo_model(image)
            
            # Extract plant parts detection
            plant_parts = {
                'leaves': 0,
                'flowers': 0,
                'fruits': 0,
                'stems': 0
            }
            
            for r in results:
                for box in r.boxes:
                    class_name = self.yolo_model.names[int(box.cls)]
                    if class_name in plant_parts:
                        plant_parts[class_name] += 1
            
            # Determine growth stage
            if plant_parts['fruits'] > 0:
                stage = 'fruiting'
            elif plant_parts['flowers'] > 0:
                stage = 'flowering'
            elif plant_parts['leaves'] > 5:
                stage = 'vegetative_mature'
            else:
                stage = 'seedling'
            
            return {
                'stage': stage,
                'plant_parts': plant_parts
            }
        else:
            # Simple analysis based on image characteristics
            return self._simple_growth_analysis(image)
    
    def _simple_growth_analysis(self, image: np.ndarray) -> Dict:
        """Simple growth analysis without YOLO"""
        # Analyze image size and green content
        height, width = image.shape[:2]
        
        # Calculate green pixels
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        green_mask = cv2.inRange(hsv, np.array([35, 40, 40]), np.array([85, 255, 255]))
        green_percentage = (cv2.countNonZero(green_mask) / (height * width)) * 100
        
        # Estimate growth stage
        if green_percentage < 10:
            stage = 'seedling'
        elif green_percentage < 30:
            stage = 'vegetative_early'
        elif green_percentage < 60:
            stage = 'vegetative_mature'
        else:
            stage = 'flowering'  # Assuming dense foliage
        
        return {
            'stage': stage,
            'green_coverage': green_percentage
        }
    
    def _calculate_leaf_coverage(self, image: np.ndarray) -> float:
        """Calculate percentage of image covered by leaves"""
        # Convert to HSV
        hsv = cv2.cvtColor(image, cv2.COLOR_BGR2HSV)
        
        # Create mask for green areas (leaves)
        lower_green = np.array([35, 40, 40])
        upper_green = np.array([85, 255, 255])
        mask = cv2.inRange(hsv, lower_green, upper_green)
        
        # Apply morphological operations to clean up
        kernel = np.ones((5, 5), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
        
        # Calculate percentage
        total_pixels = image.shape[0] * image.shape[1]
        leaf_pixels = cv2.countNonZero(mask)
        
        return round((leaf_pixels / total_pixels) * 100, 2)
    
    def _determine_overall_health(
        self, 
        disease_result: Dict,
        color_analysis: Dict,
        growth_analysis: Dict
    ) -> Tuple[str, float]:
        """Determine overall plant health status"""
        
        # Weight different factors
        health_score = 100.0
        confidence_scores = []
        
        # Disease impact
        if disease_result['issues']:
            for issue in disease_result['issues']:
                health_score -= issue['confidence'] * 30
                confidence_scores.append(issue['confidence'])
        else:
            confidence_scores.append(disease_result['healthy_confidence'])
        
        # Color analysis impact
        yellow_percentage = color_analysis.get('yellow', 0)
        brown_percentage = color_analysis.get('brown', 0)
        
        if yellow_percentage > 20:
            health_score -= 15
        if brown_percentage > 10:
            health_score -= 20
        
        # Growth stage appropriateness
        if growth_analysis['stage'] == 'seedling' and color_analysis.get('healthy_green', 0) < 20:
            health_score -= 10
        
        # Determine status
        if health_score >= 80:
            status = 'healthy'
        elif health_score >= 60:
            status = 'warning'
        else:
            status = 'critical'
        
        # Calculate confidence
        confidence = np.mean(confidence_scores) if confidence_scores else 0.5
        
        return status, float(confidence)
    
    def _generate_recommendations(
        self,
        overall_health: str,
        disease_result: Dict,
        color_analysis: Dict
    ) -> List[str]:
        """Generate actionable recommendations"""
        recommendations = []
        
        # Health-based recommendations
        if overall_health == 'critical':
            recommendations.append("Immediate attention required. Consider isolating affected plants.")
        elif overall_health == 'warning':
            recommendations.append("Monitor closely and adjust care routine.")
        
        # Disease-specific recommendations
        for issue in disease_result['issues']:
            if issue['type'] == 'powdery_mildew':
                recommendations.append("Improve air circulation and reduce humidity. Consider fungicide treatment.")
            elif issue['type'] == 'nutrient_deficiency':
                recommendations.append("Apply balanced fertilizer. Check soil pH levels.")
            elif issue['type'] == 'water_stress':
                recommendations.append("Adjust watering schedule. Check soil moisture regularly.")
            elif issue['type'] == 'pest_damage':
                recommendations.append("Inspect for pests. Consider organic pest control methods.")
        
        # Color-based recommendations
        if color_analysis.get('yellow', 0) > 20:
            recommendations.append("Yellowing detected - possible nitrogen deficiency or overwatering.")
        if color_analysis.get('brown', 0) > 10:
            recommendations.append("Brown spots detected - check for fungal infections or sun damage.")
        
        # Ensure we always have at least one recommendation
        if not recommendations:
            recommendations.append("Plant appears healthy. Continue current care routine.")
        
        return recommendations[:5]  # Limit to 5 recommendations
    
    def _get_severity(self, confidence: float) -> str:
        """Determine issue severity based on confidence"""
        if confidence > 0.8:
            return 'high'
        elif confidence > 0.5:
            return 'medium'
        else:
            return 'low'
    
    def _mock_disease_detection(self) -> Dict:
        """Mock disease detection for testing"""
        return {
            'healthy_confidence': 0.85,
            'issues': []
        }
    
    def _get_error_result(self, error_message: str) -> PlantHealthResult:
        """Return error result"""
        return PlantHealthResult(
            overall_health='unknown',
            confidence=0.0,
            issues=[{'type': 'error', 'message': error_message}],
            recommendations=['Unable to analyze image. Please try again with a clearer photo.'],
            growth_stage='unknown',
            leaf_coverage=0.0,
            color_analysis={},
            timestamp=datetime.utcnow()
        )
    
    def generate_health_report(self, results: List[PlantHealthResult]) -> Dict:
        """Generate comprehensive health report from multiple analyses"""
        if not results:
            return {'error': 'No analysis results available'}
        
        # Aggregate health scores
        health_scores = {
            'healthy': 0,
            'warning': 0,
            'critical': 0
        }
        
        all_issues = []
        all_recommendations = set()
        
        for result in results:
            health_scores[result.overall_health] += 1
            all_issues.extend(result.issues)
            all_recommendations.update(result.recommendations)
        
        # Calculate trends
        latest_result = results[-1]
        health_trend = 'stable'
        
        if len(results) > 1:
            if results[-1].overall_health == 'healthy' and results[-2].overall_health != 'healthy':
                health_trend = 'improving'
            elif results[-1].overall_health != 'healthy' and results[-2].overall_health == 'healthy':
                health_trend = 'declining'
        
        return {
            'summary': {
                'current_status': latest_result.overall_health,
                'health_trend': health_trend,
                'total_analyses': len(results),
                'health_distribution': health_scores
            },
            'common_issues': self._get_common_issues(all_issues),
            'priority_recommendations': list(all_recommendations)[:5],
            'latest_analysis': {
                'timestamp': latest_result.timestamp.isoformat(),
                'confidence': latest_result.confidence,
                'leaf_coverage': latest_result.leaf_coverage
            }
        }
    
    def _get_common_issues(self, issues: List[Dict]) -> List[Dict]:
        """Get most common issues from multiple analyses"""
        issue_counts = {}
        
        for issue in issues:
            issue_type = issue['type']
            if issue_type not in issue_counts:
                issue_counts[issue_type] = {
                    'count': 0,
                    'avg_confidence': 0,
                    'confidences': []
                }
            
            issue_counts[issue_type]['count'] += 1
            issue_counts[issue_type]['confidences'].append(issue.get('confidence', 0))
        
        # Calculate averages and sort
        common_issues = []
        for issue_type, data in issue_counts.items():
            avg_confidence = np.mean(data['confidences']) if data['confidences'] else 0
            common_issues.append({
                'type': issue_type,
                'frequency': data['count'],
                'average_confidence': round(avg_confidence, 3)
            })
        
        return sorted(common_issues, key=lambda x: x['frequency'], reverse=True)[:5]
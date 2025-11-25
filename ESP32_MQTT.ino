// ESP32 DHT22 MQTT Publisher to HiveMQ
// Libraries: DHT sensor library (Adafruit), PubSubClient, WiFi

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>  // For JSON serialization 

// Constants - Grouped for easy configuration
const char* WIFI_SSID = "myssid";        
const char* WIFI_PASSWORD = "password"; 
const char* MQTT_BROKER = "broker.hivemq.com";   // HiveMQ public broker
const int MQTT_PORT = 1883;                      // Standard MQTT port (no TLS)
const char* MQTT_TOPIC = "dht22/mydata";  
const int DHT_PIN = 22;                           // GPIO pin for DHT22 data
const int LED_PIN =4;
const int READ_INTERVAL_MS = 10000;              // Publish every 10 seconds


// DHT22 setup
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// MQTT and WiFi clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// Timers for non-blocking operations
unsigned long lastReadTime = 0;

// Function prototypes for modularity
void connectToWiFi();
void connectToMQTT();
void publishSensorData();

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println("Starting ESP32 DHT22 MQTT Project...");
  pinMode(LED_PIN,OUTPUT);
  // Initialize DHT sensor
  dht.begin();
  Serial.println("DHT22 sensor initialized.");
  // Initialize LED
  digitalWrite( LED_PIN, LOW);
  // Connect to WiFi
  connectToWiFi();

  // Set MQTT server and callback (none needed since we only publish)
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  connectToMQTT();
}

void loop() {
  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();  // Handle MQTT internals

  // Non-blocking read and publish
  unsigned long currentTime = millis();
  if (currentTime - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = currentTime;
    publishSensorData();
  }
}

// Connect to WiFi with retry logic
void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 10) {  // Max 10 retries
    delay(1000);
    Serial.print(".");
    retries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi connection failed. Restarting...");
    ESP.restart();  
  }
}

// Connect to MQTT with retry and unique client ID
void connectToMQTT() {
  String clientId = "ESP32Client-" + String(random(0xffff), HEX);  // Unique ID to avoid collisions
  
  Serial.print("Connecting to MQTT broker...");
  int retries = 0;
  while (!mqttClient.connected() && retries < 5) {  // Max 5 retries
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("\nMQTT connected.");
      digitalWrite( LED_PIN, HIGH);

    } else {
      Serial.print(".");
      delay(3000);  
      retries++;
    }
  }
  
}

// Read DHT22 and publish as JSON
void publishSensorData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();  // Celsius by default
  
  // Check for read errors (NAN = failed checksum or wiring issue)
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT22!");
    return;  
  }
  
  // Create JSON document (StaticJsonDocument for low memory use)
  StaticJsonDocument<64> doc;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  
  // Serialize to string
  String jsonData;
  serializeJson(doc, jsonData);
  
  // Publish
  if (mqttClient.publish(MQTT_TOPIC, jsonData.c_str())) {
    Serial.println("Published: " + jsonData + " to topic: " + MQTT_TOPIC);
  } else {
    Serial.println("Publish failed.");
  }
}

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

const char* ssid = "DNETWORK";
const char* password = "tr45yujk"; 

const char* mqtt_server = "10.223.58.140"; 
const int mqtt_port = 1883;            

const char* mqtt_user = "";
const char* mqtt_pass = "";

const char* publishTopic = "test/iot_water_level";
const char* clientID = "ESP32-Client-UniqueID-1234"; 

long lastMsg = 0;
char msg[50];
int value = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(clientID, mqtt_user, mqtt_pass)) { 
      Serial.println("connected");
      client.publish(publishTopic, "ESP32 is online!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port); 
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    ++value;
    
    StaticJsonDocument<200> doc;

    doc["water_level"] = random(100);
    
    char jsonBuffer[200];
    serializeJson(doc, jsonBuffer); 

    Serial.print("Publishing JSON: ");
    Serial.println(jsonBuffer);
    
    client.publish(publishTopic, jsonBuffer);
  }
}
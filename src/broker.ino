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

const char* publishTopic = "esp32/test/hello_world_001";
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

float level1 = 1.56;
float level2 = 2.56;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  // Publica a cada 5 segundos
  if (now - lastMsg > 5000) { 
    lastMsg = now;
    
    // 1. Alocação de Memória: 
    // StaticJsonDocument<256> deve ser suficiente, mas você pode usar 300
    // para segurança, dependendo de quantos itens no array você for ter.
    StaticJsonDocument<300> doc; 

    // 2. Criar o Array "buffer" no Objeto Raiz
    JsonArray buffer = doc.createNestedArray("buffer");

    // 3. Adicionar o Primeiro Objeto ao Array
    JsonObject item1 = buffer.createNestedObject();
    item1["water_level"] = level1; // 1.56

    // 4. Adicionar o Segundo Objeto ao Array
    JsonObject item2 = buffer.createNestedObject();
    item2["water_level"] = level2; // 2.56

    // (Opcional: Simular mudança de dados)
    level1 += 0.1;
    level2 += 0.1;

    // 5. Serializar o objeto (converter para string)
    char jsonBuffer[300]; 
    size_t n = serializeJson(doc, jsonBuffer); // 'n' é o tamanho da string JSON

    // 6. Publicar a string JSON via MQTT
    Serial.print("Publishing JSON (");
    Serial.print(n);
    Serial.print(" bytes): ");
    Serial.println(jsonBuffer);
    
    client.publish(publishTopic, jsonBuffer);
  }
}
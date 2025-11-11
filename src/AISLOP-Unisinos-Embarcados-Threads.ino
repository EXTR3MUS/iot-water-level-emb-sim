#include <Arduino.h>
#include "FS.h"
#include "SPIFFS.h"

// ========== CONFIGURATION ==========
#define BUFFER_FILE "/buffer.txt"

// Production timing (comment out for testing)
#define READ_INTERVAL_MS  (60000UL) // 1 min
#define SEND_INTERVAL_MS  (60000UL) // 1 min
#define RETRY_INTERVAL_MS (5000UL)   // 5 s
#define MAX_RETRIES       5
#define MAX_BUFFER_LINES  12

// For quick testing, uncomment:
// #undef READ_INTERVAL_MS
// #undef SEND_INTERVAL_MS
// #define READ_INTERVAL_MS  5000UL
// #define SEND_INTERVAL_MS  5000UL

// ===================================

SemaphoreHandle_t mutex1;

struct Measurement {
  time_t timestamp;
  int level;
  String status;
};

// Forward declarations
void readSensorTask(void *pv);
void saveDataTask(void *pv);
void sendDataTask(void *pv);
void appendToBuffer(const Measurement &m);
String classifyStatus(int level);
bool simulateSend(const String &payload);
void clearBuffer();

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    while (true);
  }

  mutex1 = xSemaphoreCreateMutex();
  srand((unsigned int)millis());

  Serial.println("\n=== Sistema de Monitoramento de Nível - ESP32 ===");

  xTaskCreatePinnedToCore(readSensorTask, "Leitura_Nivel", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(saveDataTask, "Save_Data", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(sendDataTask, "Transmissao_IOT", 8192, NULL, 1, NULL, 0);
}

void loop() {
  // Main task does nothing; tasks handle everything.
  vTaskDelay(portMAX_DELAY);
}

// ========== TASK 1: Read Sensor ==========
void readSensorTask(void *pv) {
  while (true) {
    int level = rand() % 101; // 0–100%
    Measurement m;
    m.timestamp = time(NULL);
    m.level = level;
    m.status = classifyStatus(level);

    if (xSemaphoreTake(mutex1, portMAX_DELAY)) {
      appendToBuffer(m);
      Serial.printf("[READ] %ld | Nivel: %d%% | Status: %s\n",
                    (long)m.timestamp, m.level, m.status.c_str());
      xSemaphoreGive(mutex1);
    }

    vTaskDelay(READ_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

// ========== TASK 2: Save Data ==========
void saveDataTask(void *pv) {
  while (true) {
    if (xSemaphoreTake(mutex1, portMAX_DELAY)) {
      File f = SPIFFS.open(BUFFER_FILE, FILE_READ);
      int count = 0;
      if (f) {
        while (f.available()) {
          f.readStringUntil('\n');
          count++;
        }
        f.close();
      }
      Serial.printf("[SAVE] Buffer contem %d leituras.\n", count);
      xSemaphoreGive(mutex1);
    }
    vTaskDelay(READ_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

// ========== TASK 3: Send Data ==========
void sendDataTask(void *pv) {
  while (true) {
    vTaskDelay(SEND_INTERVAL_MS / portTICK_PERIOD_MS);

    if (!xSemaphoreTake(mutex1, portMAX_DELAY)) continue;

    File f = SPIFFS.open(BUFFER_FILE, FILE_READ);
    if (!f || f.size() == 0) {
      Serial.println("[SEND] Nenhum dado para enviar.");
      if (f) f.close();
      xSemaphoreGive(mutex1);
      continue;
    }

    String payload;
    while (f.available()) {
      payload += f.readStringUntil('\n') + "\n";
    }
    f.close();

    bool sent = false;
    for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
      Serial.printf("[SEND] Tentativa %d/%d...\n", attempt, MAX_RETRIES);
      if (simulateSend(payload)) {
        sent = true;
        break;
      } else {
        Serial.printf("[SEND] Falha no envio. Tentando novamente em %lus...\n", RETRY_INTERVAL_MS / 1000);
        xSemaphoreGive(mutex1);
        vTaskDelay(RETRY_INTERVAL_MS / portTICK_PERIOD_MS);
        if (!xSemaphoreTake(mutex1, portMAX_DELAY)) break;
      }
    }

    if (sent) {
      clearBuffer();
      Serial.println("[SEND] Envio bem-sucedido. Buffer limpo.");
    } else {
      Serial.println("[SEND] Todas tentativas falharam. Mantendo buffer.");
    }

    xSemaphoreGive(mutex1);
  }
}

// ========== HELPERS ==========

String classifyStatus(int level) {
  if (level >= 35) return "Normal";
  if (level >= 20) return "Alerta";
  return "Critico";
}

void appendToBuffer(const Measurement &m) {
  // Append line
  File f = SPIFFS.open(BUFFER_FILE, FILE_APPEND);
  if (!f) {
    Serial.println("[ERR] Falha ao abrir buffer.txt");
    return;
  }
  f.printf("%ld,%d,%s\n", (long)m.timestamp, m.level, m.status.c_str());
  f.close();

  // Trim to last MAX_BUFFER_LINES
  f = SPIFFS.open(BUFFER_FILE, FILE_READ);
  if (!f) return;

  std::vector<String> lines;
  while (f.available()) {
    lines.push_back(f.readStringUntil('\n'));
  }
  f.close();

  if (lines.size() > MAX_BUFFER_LINES) {
    File fw = SPIFFS.open(BUFFER_FILE, FILE_WRITE);
    for (size_t i = lines.size() - MAX_BUFFER_LINES; i < lines.size(); i++) {
      fw.println(lines[i]);
    }
    fw.close();
  }
}

bool simulateSend(const String &payload) {
  int r = rand() % 100;
  bool success = (r < 70); // 70% success chance
  if (success) {
    Serial.println("=== ENVIANDO DADOS ===");
    Serial.print(payload);
    Serial.println("=== ENVIO OK ===");
  }
  return success;
}

void clearBuffer() {
  File f = SPIFFS.open(BUFFER_FILE, FILE_WRITE);
  if (f) f.close();
}

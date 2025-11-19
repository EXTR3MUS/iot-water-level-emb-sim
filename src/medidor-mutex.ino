// Standard Arduino Libraries
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Sensor Library
#include "Adafruit_VL53L0X.h"

// Communication/Data Structure Library
#include <ArduinoJson.h> // REQUIRED: Install the ArduinoJson library

// FreeRTOS and Task Management for ESP32

// --- CONFIGURATION ---
#define MAX_BUFFER_SIZE 12   // Max number of readings to store before sending fails
#define BAUD_RATE 115200
#define JSON_DOC_SIZE 1536   // Sufficient size for 12 float objects in the JSON payload

// --- DATA STRUCTURES ---

// Structure to hold a single reading for the buffer
struct WaterLevelReading {
    float water_level;
};

// --- GLOBAL VARIABLES (Hardware/General) ---
LiquidCrystal_I2C lcd(0X27, 20, 4); // 20x4 LCD
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// Shared variables for the current reading (protected by xMutex)
volatile float g_dist_corrected = 0.0; // The final corrected distance in cm
volatile int g_buffersize = 0;         // The size of the last measurement window
long int g_raw_media = 0;              // Raw sum of readings (only read by Task 1)
float g_last_raw_dist = 0.0;           // Last raw measurement

// Mutex Handle (used to protect g_dist_corrected and g_buffersize)
SemaphoreHandle_t xMutex;

// --- GLOBAL VARIABLES (Buffer and Communication) ---
WaterLevelReading g_data_buffer[MAX_BUFFER_SIZE];
volatile int g_buffer_head = 0;        // Index of the oldest (unsent) reading
volatile int g_buffer_tail = 0;        // Index where the next new reading will be written
volatile int g_current_buffer_count = 0; // Number of items currently in the buffer

// Mutex Handle (used to protect buffer variables: head, tail, count, and array content)
SemaphoreHandle_t xBufferMutex;

// --- FUNCTION PROTOTYPES ---
void TaskReadSensor(void *pvParameters);
void TaskDisplayLCD(void *pvParameters);
void TaskSendBroker(void *pvParameters);
bool simulateSend(const char* json_payload);


// ====================================================================
// PLACEHOLDER: Broker Communication Simulation
// ====================================================================
bool simulateSend(const char* json_payload) {
    
    Serial.printf("[Broker] Attempting to send: %s\n", json_payload); // Verbose logging disabled
    
    // Simulate connection failure sometimes to test the buffer
    if (rand() % 10 < 8) { // 40% chance of failure
        Serial.println("[Broker] FAILED to send data! Buffer will hold it.");
        return false;
    }
    
    Serial.println("[Broker] Successfully sent batch.");
    return true;
}


// ====================================================================
// TASK 1: Read Sensor and Calculate Distance (PRODUCER)
// ====================================================================
void TaskReadSensor(void *pvParameters) {
  const int NUM_SAMPLES = 30;
  
  while (1) {
    // Serial.printf("Task 1 start\n"); 
    g_raw_media = 0;
    
    // 1. COLLECT SAMPLES
    for (int i = 0; i < NUM_SAMPLES; i++) {
      VL53L0X_RangingMeasurementData_t measure;
      lox.rangingTest(&measure, false);

      if (measure.RangeStatus != 4) {
        g_last_raw_dist = (float)measure.RangeMilliMeter;
      }
      g_raw_media += g_last_raw_dist;
      // vTaskDelay(pdMS_TO_TICKS(1)); 
    }

    // 2. CALCULATE FINAL VALUE (in cm)
    float avg_mm = (float)g_raw_media / NUM_SAMPLES;
    float avg_cm = avg_mm / 10.0;
    float new_corrected_dist_cm = (avg_cm / 0.82) - 7.5; // Final value in cm

    // 3. UPDATE CURRENT READING (for LCD Task)
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      g_dist_corrected = new_corrected_dist_cm;
      g_buffersize = NUM_SAMPLES;
      xSemaphoreGive(xMutex);
    }

    // 4. ADD READING TO BUFFER (for Send Task)
    if (xSemaphoreTake(xBufferMutex, portMAX_DELAY) == pdTRUE) {
      if (g_current_buffer_count < MAX_BUFFER_SIZE) {
        // Store new data at the tail index
        g_data_buffer[g_buffer_tail].water_level = new_corrected_dist_cm; 
        
        // Move the tail forward (circularly)
        g_buffer_tail = (g_buffer_tail + 1) % MAX_BUFFER_SIZE;
        g_current_buffer_count++;
        Serial.printf("[Buffer] New reading added. Count: %d/%d\n", g_current_buffer_count, MAX_BUFFER_SIZE);
      } else {
        // Buffer is full, drop the reading (Data Loss)
        Serial.println("[Buffer] WARNING: Buffer full. Dropping latest reading!");
      }
      xSemaphoreGive(xBufferMutex);
    }
    
    // Wait for 0,5 seconds before the next reading cycle
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// ====================================================================
// TASK 2: Send Data to Broker (CONSUMER/BUFFER MANAGER)
// --- NOW SENDS ALL PENDING DATA IN ONE BATCH ---
// ====================================================================
void TaskSendBroker(void *pvParameters) {
    while(1) {
        // Serial.printf("Task 2 start\m"); 
        // Local copies of state to safely work outside the mutex lock
        int readings_to_send_count = 0;
        int current_head = 0;

        // 1. Check and safely copy state variables
        if (xSemaphoreTake(xBufferMutex, portMAX_DELAY) == pdTRUE) {
            readings_to_send_count = g_current_buffer_count;
            current_head = g_buffer_head;
            xSemaphoreGive(xBufferMutex);
        }

        if (readings_to_send_count > 0) {
            
            // 2. FORMAT ALL AVAILABLE DATA INTO JSON
            StaticJsonDocument<JSON_DOC_SIZE> doc; 
            JsonArray buffer_array = doc.createNestedArray("buffer");
            
            // Iterate over all available readings in the buffer starting from the head
            for (int i = 0; i < readings_to_send_count; i++) {
                // Calculate the index in the circular array
                int index = (current_head + i) % MAX_BUFFER_SIZE;
                
                // Read the data from the buffer
                JsonObject reading_obj = buffer_array.createNestedObject();
                reading_obj["water_level"] = g_data_buffer[index].water_level;
            }

            char json_output[JSON_DOC_SIZE]; // Match document size
            serializeJson(doc, json_output);

            // 3. ATTEMPT TO SEND BATCH
            if (simulateSend(json_output)) {
                // SUCCESS: Remove all sent items from buffer (PROTECT WRITE/HEAD/COUNT)
                if (xSemaphoreTake(xBufferMutex, portMAX_DELAY) == pdTRUE) {
                    // Advance the head by the total number of items sent
                    g_buffer_head = (g_buffer_head + readings_to_send_count) % MAX_BUFFER_SIZE;
                    g_current_buffer_count = 0; // Reset count
                    Serial.printf("[Buffer] Batch of %d items sent and removed. Remaining: 0\n", readings_to_send_count);
                    xSemaphoreGive(xBufferMutex);
                }
                // Short delay before checking for new data
                vTaskDelay(pdMS_TO_TICKS(100)); 
            } else {
                // FAILURE: Keep items in buffer and wait longer before retrying
                vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 5 seconds before next send attempt
            }
        } else {
            // Buffer is empty, wait for the sensor task to produce data
            vTaskDelay(pdMS_TO_TICKS(100)); 
        }
    }
}


// ====================================================================
// TASK 3: Display Data on LCD
// --- FIXED CURSOR POSITIONS TO PREVENT TRUNCATION ---
// ====================================================================
void TaskDisplayLCD(void *pvParameters) {
  // Local copies for display data, read under mutex lock
  float display_dist = 0.0;
  int display_buffer_count = 0;
  int display_buffersize = 0;
  
  // Print static labels ONCE to prevent flicker.
  // Using shorter labels to ensure everything fits (20 chars max width).
  // Line 0: Distance
  lcd.setCursor(0, 0); lcd.print("Dist(cm):"); // 10 characters (0-9)
  // Line 2: Buffer
  lcd.setCursor(0, 2); lcd.print("Buf: ");      // 5 characters (0-4)

  while (1) {
    // Serial.printf("Task 3 start\n"); 
    // 1. READ SHARED DATA
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      display_dist = g_dist_corrected;
      display_buffersize = g_buffersize; // Not currently displayed, but available
      xSemaphoreGive(xMutex);
    }
    if (xSemaphoreTake(xBufferMutex, portMAX_DELAY) == pdTRUE) {
      display_buffer_count = g_current_buffer_count;
      xSemaphoreGive(xBufferMutex);
    }

    // 2. DISPLAY DISTANCE (Line 0)
    // Start printing value at column 10 (after "Dist(cm): ")
    lcd.setCursor(10, 0); 
    char distStr[8];
    // Use %4.1f to ensure the number takes up a consistent 4 characters (e.g., 99.9)
    sprintf(distStr, "%4.1f", display_dist);
    lcd.print(distStr);
    lcd.print("cm");     // Add unit
    //lcd.print("    ");   // Clear remaining space to the end of the line (col 15-19)

    // 3. DISPLAY BUFFER STATUS (Line 2)
    // Start printing value at column 5 (after "Buf: ")
    lcd.setCursor(5, 2);
    char bufStr[10];
    // Format: "X/12"
    sprintf(bufStr, "%2d/%d ", display_buffer_count, MAX_BUFFER_SIZE);
    lcd.print(bufStr);
    //lcd.print("      "); // Clear remaining space

    // 4. DISPLAY WARNING (Line 3)
    lcd.setCursor(0, 3);
    if (display_buffer_count >= MAX_BUFFER_SIZE * 0.8) {
      lcd.print("! BUFFER CRITICAL !"); // 23 chars, needs to be <= 20
    } else if (display_buffer_count > 0) {
      lcd.print("- SENDING PENDING -");
    } else {
      lcd.print("                    "); // Clear warning line (20 spaces)
    }

    // Display update rate 
    vTaskDelay(pdMS_TO_TICKS(100)); 
  }
}


// ====================================================================
// ARDUINO SETUP FUNCTION
// ====================================================================
void setup() {
  Serial.begin(BAUD_RATE);
  srand(millis()); // Initialize random seed for simulation

  // 1. Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // 2. Initialize VL53L0X Sensor
  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X. Check wiring!"));
    lcd.setCursor(0,0); lcd.print("VL53L0X ERROR!");
    while (1) { vTaskDelay(pdMS_TO_TICKS(100)); }
  }
  Serial.println(F("VL53L0X Initialized."));

  // 3. Create Mutexes
  xMutex = xSemaphoreCreateMutex(); 
  xBufferMutex = xSemaphoreCreateMutex(); 
  if (xMutex == NULL || xBufferMutex == NULL) {
    Serial.println("Error: Failed to create mutexes!");
    lcd.setCursor(0,0); lcd.print("MUTEX ERROR!");
    while (1) { vTaskDelay(pdMS_TO_TICKS(100)); }
  }

  // 4. Create FreeRTOS Tasks
  
  // Task 1: Sensor Read (Producer)
  xTaskCreate(TaskReadSensor, "Sensor_Task", 4096, NULL, 2, NULL); 

  // Task 2: Broker Sender (Consumer/Manager)
  xTaskCreate(TaskSendBroker, "Send_Task", 6144, NULL, 1, NULL); // Larger stack for JSON

  // Task 3: LCD Display
  xTaskCreate(TaskDisplayLCD, "LCD_Task", 2048, NULL, 1, NULL); 

  Serial.println("All FreeRTOS Tasks started.");
}

void loop() {
  // FreeRTOS manages all continuous tasks; the main loop does nothing.
  // vTaskDelay(pdMS_TO_TICKS(10)); 
}

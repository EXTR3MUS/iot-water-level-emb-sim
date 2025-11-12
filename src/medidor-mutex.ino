// Standard Arduino Libraries
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Sensor Library
#include "Adafruit_VL53L0X.h"

// FreeRTOS and Task Management for ESP32
// We don't need to manually include FreeRTOS headers, as the ESP32 Arduino core handles them.
// We use xTaskCreate and vTaskDelay.

// --- GLOBAL VARIABLES ---

// Shared variables (MUST be protected by a mutex)
volatile float g_dist_corrected = 0.0; // The final corrected distance in cm
volatile int g_buffersize = 0;         // The size of the measurement buffer
long int g_raw_media = 0;              // Raw sum of readings (only read by Task 1)
float g_last_raw_dist = 0.0;           // Last raw measurement

// Mutex Handle (used to protect shared variables g_dist_corrected and g_buffersize)
SemaphoreHandle_t xMutex;

// Hardware Objects
LiquidCrystal_I2C lcd(0X27, 20, 4);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

// --- FUNCTION PROTOTYPES (for FreeRTOS Tasks) ---
void TaskReadSensor(void *pvParameters);
void TaskDisplayLCD(void *pvParameters);


// ====================================================================
// TASK 1: Read Sensor and Calculate Distance
// Runs on Core 0 (default)
// ====================================================================
void TaskReadSensor(void *pvParameters) {
  const int NUM_SAMPLES = 100;

  // The 'while(1)' loop is the heart of a FreeRTOS task
  while (1) {
    g_raw_media = 0; // Reset raw media accumulation

    // 1. COLLECT 100 SAMPLES
    for (int i = 0; i < NUM_SAMPLES; i++) {
      VL53L0X_RangingMeasurementData_t measure;
      lox.rangingTest(&measure, false); // No debug printout

      if (measure.RangeStatus != 4) { // Status 4 means 'Phase Fail' (invalid data)
        // RangeMilliMeter returns mm. Store the raw distance.
        g_last_raw_dist = (float)measure.RangeMilliMeter;
      }
      g_raw_media += g_last_raw_dist;
      vTaskDelay(pdMS_TO_TICKS(1)); // Wait 1ms between samples
    }

    // 2. CALCULATE FINAL VALUE (Raw reading is in mm)
    // The division by 1000 converts the averaged millimeters to meters
    // The original code uses: media = (media / 1000); // distancia em centimetros
    // This looks like it converts mm to meters, and then the float calculation converts to cm, which is messy.
    // Let's calculate the average in mm, then convert to cm.
    float avg_mm = (float)g_raw_media / NUM_SAMPLES;
    float avg_cm = avg_mm / 10.0;

    // Apply calibration/correction formula from original code
    // Final value is in cm
    float new_corrected_dist_cm = (avg_cm / 0.82) - 7.5;

    // 3. SYNCHRONIZE AND UPDATE SHARED DATA
    // Wait indefinitely for the mutex to be available
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      g_dist_corrected = new_corrected_dist_cm;
      g_buffersize = NUM_SAMPLES;
      xSemaphoreGive(xMutex); // Release the mutex
    }

    Serial.printf("Sensor Task: Raw Avg (cm): %.2f | Corrected (cm): %.2f\n", avg_cm, g_dist_corrected);

    // Wait for 5 seconds before the next reading cycle
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}


// ====================================================================
// TASK 2: Display Data on LCD
// Runs on Core 1 (default for tasks created without specified core)
// ====================================================================
void TaskDisplayLCD(void *pvParameters) {
  // Local copies for display data, read under mutex lock
  float display_dist = 0.0;
  int display_buffersize = 0;

  while (1) {
    // 1. SYNCHRONIZE AND READ SHARED DATA
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE) {
      display_dist = g_dist_corrected;
      display_buffersize = g_buffersize;
      xSemaphoreGive(xMutex); // Release the mutex
    }

    // 2. DISPLAY DATA
    lcd.clear();

    // Line 0: Distance
    lcd.setCursor(0, 0);
    lcd.print("Distance (cm):");
    lcd.setCursor(15, 0); // Start printing value
    
    // Print the float value formatted to 1 decimal place
    // ESP32 supports sprintf
    char distStr[8];
    sprintf(distStr, "%.1f", display_dist);
    lcd.print(distStr);

    // Line 2: Buffer Size (using a 4-line display)
    lcd.setCursor(0, 2);
    lcd.print("Buffer Size: ");
    lcd.setCursor(13, 2);
    lcd.print(display_buffersize);

    // Wait for 5 milliseconds (or whatever the original delay intended)
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}


// ====================================================================
// ARDUINO SETUP FUNCTION
// ====================================================================
void setup() {
  Serial.begin(115200);

  // 1. Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // 2. Initialize VL53L0X Sensor
  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X. Check wiring!"));
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("VL53L0X ERROR!");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  Serial.println(F("VL53L0X Initialized."));

  // 3. Create Mutex
  // This creates a recursive mutex, which is generally safer, though a standard mutex works too.
  xMutex = xSemaphoreCreateMutex(); 
  if (xMutex == NULL) {
    Serial.println("Error: Failed to create mutex!");
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("MUTEX ERROR!");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
  }

  // 4. Create FreeRTOS Tasks
  // xTaskCreate( TaskFunction_t pvTaskCode, const char * const pcName, uint32_t usStackDepth, void *pvParameters, UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask );
  
  // Create Sensor Read Task (Higher priority than display)
  xTaskCreate(
    TaskReadSensor,   // Task function
    "Sensor_Task",    // Name of task
    4096,             // Stack size (bytes)
    NULL,             // Parameter to pass to the task
    2,                // Task priority (0 is the lowest)
    NULL              // Task Handle (optional)
  );

  // Create LCD Display Task
  xTaskCreate(
    TaskDisplayLCD,   // Task function
    "LCD_Task",       // Name of task
    2048,             // Stack size (bytes)
    NULL,             // Parameter to pass to the task
    1,                // Task priority
    NULL              // Task Handle (optional)
  );

  Serial.println("Tasks started.");
  // NOTE: setup() exits cleanly. The main loop() function will run,
  // but it is often left empty in FreeRTOS applications like this.
}

// ====================================================================
// ARDUINO LOOP FUNCTION
// ====================================================================
void loop() {
  // Since the tasks handle the continuous work (reading and displaying)
  // this loop can be left empty. FreeRTOS will manage the two created tasks.
  vTaskDelay(pdMS_TO_TICKS(100)); // Small delay to yield to other tasks/Wi-Fi stack
}

#include "Adafruit_VL53L0X.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#define D  60

//RODAR EM BOARD "ESP32 Dev Module"

//setup 

float distancia = 0;
long int media = 0;

LiquidCrystal_I2C lcd(0X27, 20, 4);
Adafruit_VL53L0X lox = Adafruit_VL53L0X();

void setup() {
  Serial.begin(115200);
  lcd.init(); // Serve para iniciar a comunicação com o display já conectado
  lcd.backlight(); // Serve para ligar a luz do display
  lcd.clear(); // Serve para limpar a tela do display

  // wait until serial port opens for native USB devices
  while (! Serial) {
    delay(1);
  }

  Serial.println("Adafruit VL53L0X test");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while (1);
  }
  // power
  Serial.println(F("VL53L0X API Simple Ranging example\n\n"));
}

void loop() {
  //inicio medição
  VL53L0X_RangingMeasurementData_t measure;
  //Serial.print("Reading a measurement... ");
  lox.rangingTest(&measure, true); // pass in 'true' to get debug data printout!
  
  for (int i = 0; i < 100; i++ ) {
    if (measure.RangeStatus != 4) {  // phase failures have incorrect data
      distancia = (measure.RangeMilliMeter);
    }
    media = media + distancia;
    delay(1);
  }
  media = (media / 1000); //distancia em centimetros

  Serial.print("Distancia (cm):");
  Serial.println(media);
  float med = (media / 0.82) - 7.5; //medição final
  Serial.print("Distancia corrigida (cm):");
  Serial.println(med);

  int level = med;
  media = 0;

  // inicio display
  lcd.clear();
  //Posiciona o cursor na coluna 0, linha 0;
  lcd.setCursor(0, 0);
  
  //Envia o texto entre aspas para o LCD
  lcd.print("Distancia =");
  lcd.setCursor(12, 0);
  lcd.print(med);
  lcd.setCursor(18, 0);
  lcd.print("cm");
  lcd.setCursor(0, 1);
  
  //Envia o texto entre aspas para o LCD
  lcd.setCursor(0, 2);
  lcd.print("Buffer Size: ");
  lcd.setCursor(12, 2);
  lcd.print("100");
  delay(10);
}  //fim do loop

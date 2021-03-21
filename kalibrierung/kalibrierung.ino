#include "HX711.h"
#include <SPI.h>
#include <Wire.h>
#include <stdarg.h>
#include "soc/rtc.h"


HX711 loadcell;

// 1. HX711 circuit wiring
const int LOADCELL_DOUT_PIN = D7;
const int LOADCELL_SCK_PIN = D8;

const int MOSFET_PIN = D2;


const bool SERIAL_ = true;
const bool DISPLAY_ = false;


int CALIBRATING_WEIGHT = 5000;

float scale = 0;
float offset = 0;

float EPSILON = 0.1;

bool enterIsPressed() {
  while(Serial.available()>0)    //Checks is there any data in buffer 
  {
    if (char(Serial.read()) == '\n') {
      return true;
    }
  }
  return false;
}

bool userConfirmation() {
  return enterIsPressed();
}

bool is_accurate()
{
  Serial.println("3. Testen der Kalibrierung.");
  delay(500);
  Serial.print("Platziere ein ");
  Serial.print(CALIBRATING_WEIGHT);
  Serial.println("g Gewicht auf der Waage und druecke Knopf oder Enter");
  while (!userConfirmation())
    delay(10);
  Serial.println("Messe Gewicht. Bitte warten.");
  float measurement = loadcell.get_units(10);
  Serial.print("Messung: ");
  Serial.println(measurement, 8);
  float diff = measurement - CALIBRATING_WEIGHT;
  diff = diff > 0 ? diff : -diff;
  return diff / CALIBRATING_WEIGHT < EPSILON;
}
void setup() {
  Serial.begin(115200);
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
  delay(1000);
  Serial.println("Initialisiere Waage.");
  
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, HIGH);
  delay(500);
  
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.tare();
  do {
    Serial.println("Waage muss kalibriert werden.");
    delay(500);
    Serial.println("1. Testen der Nulllage, ohne Gewichte. Druecke Knopf oder Enter");
    while (!userConfirmation())
      delay(10);
    Serial.println("Messe Nullage. Bitte warten.");
    offset = loadcell.read_average(32);
    loadcell.set_offset(offset);
    Serial.print("Nulllage (unskaliert): ");
    Serial.println(offset, 8);  
    
    Serial.print("2. Platziere ein ");
    Serial.print(CALIBRATING_WEIGHT);
    Serial.println("g Gewicht auf der Waage und druecke Knopf oder Enter");
    while (!userConfirmation())
      delay(10);
    
    Serial.println("Messe Gewicht. Bitte warten.");
    float measurement = loadcell.get_value(32);
    scale = CALIBRATING_WEIGHT / measurement;
    if (scale == INFINITY || isnan(scale)) {
      Serial.println("Skalierung waere inf. Wiederhole");
      continue;
    }
    
    Serial.print("Rohes Messdatum: ");
    Serial.println(measurement, 8);  
    Serial.print("Skalierung: ");
    Serial.println(scale, 8);
    loadcell.set_scale(1/scale);
    
  } while (!is_accurate() && !is_accurate());
  Serial.println("Kalibrierung beendet.");
  Serial.print("Nullage (unskaliert): ");
  Serial.println(offset, 8);  
  Serial.print("Skalierung: ");
  Serial.println(scale, 8);
  Serial.println("Knopf oder Enter druecken um Messung zu beginnen.");  
  
}

void loop() {
  Serial.println("Messe Gewicht.");
  float measurement = loadcell.get_units(32);
  Serial.print("Gewicht (in g): ");
  Serial.println(measurement, 4);
  Serial.println("Druecke Knopf oder Enter um erneut zu messen"); 
  while (!userConfirmation())
      delay(10);
  delay(1000);
}

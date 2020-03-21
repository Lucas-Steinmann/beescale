#include "HX711.h"
#include <SenseBoxMCU.h>
#include <SPI.h>
#include <Wire.h>
#include "dualprint.h"
#include <stdarg.h>


HX711 loadcell;

// 1. HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 1;


Button menuButton(0);

const bool SERIAL_ = true;
const bool DISPLAY_ = true;

DualPrint printer(SERIAL_, DISPLAY_);

int CALIBRATING_WEIGHT = 2000;

float scale = 0;
float offset = 0;

float EPS = 0.1;

bool is_accurate()
{
  printer.clear_display();
  printer.println("3. Testen der Kalibrierung.");
  delay(500);
  printer.print("Platziere ein ");
  printer.print(CALIBRATING_WEIGHT);
  printer.println("g Gewicht auf die Waage und druecke switch");
  while (menuButton.isPressed() != HIGH)
    delay(10);
  printer.clear_display();
  printer.println("Messe Gewicht. Bitte warten.");
  float measurement = loadcell.get_units(10);
  printer.clear_display();
  printer.print("Messung: ");
  printer.println(measurement, 8);
  float diff = measurement - CALIBRATING_WEIGHT;
  diff = diff > 0 ? diff : -diff;
  return diff / CALIBRATING_WEIGHT < EPS;
}
void setup() {
  menuButton.begin();
  printer.init();
  printer.println("Initialisiere Waage.");
  
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.tare();
  do {
    printer.println("Waage muss kalibriert werden.");
    delay(500);
    printer.println("1. Testen der Nulllage, ohne Gewichte. Druecke switch");
    while (menuButton.isPressed() != HIGH)
      delay(10);
    printer.clear_display();
    printer.println("Messe Nullage. Bitte warten.");
    offset = loadcell.read_average(32);
    loadcell.set_offset(offset);
    printer.print("Nulllage (unskaliert): ");
    printer.println(offset, 8);  
    
    printer.clear_display();
    
    printer.print("2. Platziere ein ");
    printer.print(CALIBRATING_WEIGHT);
    printer.println("g Gewicht auf die Waage und druecke switch");
    while (menuButton.isPressed() != HIGH)
      delay(10);
    printer.clear_display();
    
    printer.println("Messe Gewicht. Bitte warten.");
    float measurement = loadcell.get_value(32);
    scale = CALIBRATING_WEIGHT / measurement;
    if (scale == INFINITY || isnan(scale)) {
      printer.println("Skalierung waere inf. Wiederhole");
      printer.clear_display();
      continue;
    }
    
    printer.clear_display();
    printer.print("Rohes Messdatum: ");
    printer.println(measurement, 8);  
    printer.print("Skalierung: ");
    printer.println(scale, 8);
    loadcell.set_scale(1/scale);
    
  } while (!is_accurate() && !is_accurate());
  printer.println("Kalibrierung beendet.");
  printer.print("Nullage (unskaliert): ");
  printer.println(offset, 8);  
  printer.print("Skalierung: ");
  printer.println(scale, 8);
  printer.println("Switch druecken um Messung zu beginnen.");  
  
}

void loop() {
  printer.clear_display();
  printer.println("Messe Gewicht.");
  float measurement = loadcell.get_units(32);
  printer.print("Gewicht (in g): ");
  printer.println(measurement, 4);
  printer.println("Druecke switch um erneut zu messen"); 
  while (menuButton.isPressed() != HIGH)
      delay(10);
  delay(1000);
}

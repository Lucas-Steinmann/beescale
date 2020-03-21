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

// 2. Adjustment settings
const long LOADCELL_OFFSET = 50682624;
const long LOADCELL_DIVIDER = 5895655;

Button menuButton(0);

const bool SERIAL_ = true;
const bool DISPLAY_ = true;

DualPrint printer(SERIAL_, DISPLAY_);

const float scale = 0.03643585;
const float offset = 179904.0;

const int samples = 32;


void setup() {
  menuButton.begin();
  printer.init();
  printer.println("Initialisiere Waage.");
  
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  //loadcell.tare();
  loadcell.set_offset(offset);
  loadcell.set_scale(1/scale);
}

void loop() {
  printer.clear_display();
  printer.println("Messe Gewicht.");
  float measurement = loadcell.get_units(samples);
  printer.print("Gewicht (in g): ");
  printer.println(measurement, 4);
  printer.println("Druecke switch um erneut zu messen"); 
  while (menuButton.isPressed() != HIGH)
      delay(10);
  delay(1000);
}

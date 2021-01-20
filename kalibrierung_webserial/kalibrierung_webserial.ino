#include "HX711.h"
#include <SPI.h>
#include <Wire.h>
#include <stdarg.h>
#include "soc/rtc.h"
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

#include "config.h"

AsyncWebServer server(80);


HX711 loadcell;

// 1. HX711 circuit wiring
const int LOADCELL_DOUT_PIN = D3;
const int LOADCELL_SCK_PIN = D4;

// MOSFET to turn on sensors
const int MOSFET_PIN = D2;


int CALIBRATING_WEIGHT = 5000;

float scale = 0;
float offset = 0;

const float EPSILON = 0.1;
const float AVG_WINDOW = 32;

// Since we receive signals asynchronously
// we have to keep the state in a global variable
enum state {
  ZERO = 0, // => 2
  CALIB = 1,
  CHECK = 2, 
  MEASURE = 3
};
state curr_state = ZERO; // the stage, which will be executed next

void initialize_loadcell() {
  WebSerial.println("Initialisiere Waage.");
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  loadcell.tare();
}
void next_zero() {
  // Set the state to ZERO and print user message
  WebSerial.println("Waage muss kalibriert werden.");
  delay(500);
  WebSerial.println("1. Testen der Nulllage, ohne Gewichte. Druecke Knopf oder Enter");
  curr_state = ZERO;
}

void next_calib() {
  // Set the state to CALIB and print user message
  WebSerial.print("2. Platziere ein ");
  WebSerial.print(String(CALIBRATING_WEIGHT));
  WebSerial.println("g Gewicht auf der Waage und druecke Knopf oder Enter");
  curr_state = CALIB;  
}

void next_check() {
  WebSerial.println("3. Testen der Kalibrierung.");
  delay(500);
  WebSerial.print("Platziere ein ");
  WebSerial.print(String(CALIBRATING_WEIGHT));
  WebSerial.println("g Gewicht auf der Waage und druecke Knopf oder Enter");
  curr_state = CHECK;
}

void next_measure() {
  WebSerial.println("Enter druecken um Testmessung mit anderen Gewichten zu beginnen.");  
  curr_state = MEASURE;
}

void zero() {
  // Measure zero point
  WebSerial.println("Messe Nullage. Bitte warten.");
  offset = loadcell.read_average(AVG_WINDOW);
  loadcell.set_offset(offset);
  WebSerial.print("Nulllage (unskaliert): ");
  WebSerial.println(String(offset, 8));  
  next_calib();
}

void calib() {
  WebSerial.println("Messe Gewicht. Bitte warten.");
  float measurement = loadcell.get_value(32);
  scale = CALIBRATING_WEIGHT / measurement;
  if (scale == INFINITY || isnan(scale)) {
    WebSerial.println("Skalierung waere inf. Wiederhole");
    next_calib();
    return;
  }
  WebSerial.print("Rohes Messdatum: ");
  WebSerial.println(String(measurement, 8));  
  WebSerial.print("Skalierung: ");
  WebSerial.println(String(scale, 8));
  loadcell.set_scale(1/scale);
  next_check();
}

void check() {
  WebSerial.println("Messe Gewicht. Bitte warten.");
  float measurement = loadcell.get_units(10);
  WebSerial.print("Messung: ");
  WebSerial.println(String(measurement, 8));
  float diff = measurement - CALIBRATING_WEIGHT;
  diff = diff > 0 ? diff : -diff;
  if (diff / CALIBRATING_WEIGHT < EPSILON) {
    WebSerial.println("Kalibrierung beendet.");
    WebSerial.print("Nullage (unskaliert): ");
    WebSerial.println(String(offset, 8));  
    WebSerial.print("Skalierung: ");
    WebSerial.println(String(scale, 8));
    next_measure();
  } else {
    WebSerial.println("Kontrollmessung fehlgeschlagen. Kalibrierung wird neugestartet.");
    next_zero();
  }
}

void measure() {
  WebSerial.println("Messe Gewicht.");
  float measurement = loadcell.get_units(32);
  WebSerial.print("Gewicht (in g): ");
  WebSerial.println(String(measurement, 4));
  next_measure();
}

// Mapping from state to function
void (*state_table[])() = {zero, calib, check, measure};
void execute_next_stage() {
  state_table[curr_state]();
}

void receive_meassage(uint8_t *data, size_t len) {
  execute_next_stage();
}

void setup_webserial() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.printf("WiFi Failed!\n");
    return;
  }
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  // WebSerial is accessible at "<IP Address>/webserial" in browser
  WebSerial.begin(&server);
  WebSerial.msgCallback(receive_meassage);
  server.begin();
}

void setup() {
  // Start Webserial
  rtc_clk_cpu_freq_set(RTC_CPU_FREQ_80M);
  delay(1000);
  Serial.begin(115200);
  // The order of the following operations
  // seem very important
  // First WiFi must be enabled, since
  // after setting MOSFET PIN to high,
  // enabling WiFI crashes with error: 
  setup_webserial();
  // And we have to open MOSFEt before initializing load cell. Otherwise the loadcell will not be tared.
  pinMode(MOSFET_PIN, OUTPUT);
  digitalWrite(MOSFET_PIN, HIGH);
  delay(500);
  
  initialize_loadcell();
  next_zero();
  Serial.println("Am here");
  WebSerial.println("I can print to Web");
}

void loop() {
}

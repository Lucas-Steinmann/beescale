#include "HX711.h"
#include <SPI.h>
#include <stdarg.h>
#include <WiFi.h>
#include <MQTT.h>

// Configuration

const char ssid[] = "";
const char pass[] = "";
const char broker_url[] = "";
const char* topic = "";


// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 33;
const int LOADCELL_SCK_PIN = 32;

// Scale and offset of the scale. Must be determined with the calibration script.
const float scale = 0.03623254;
const float offset = 178529.0;

// Number of samples to take and average over before sending the result
const int samples = 32;

// Time interval between to measurements in us
const uint64_t interval = 15 * 60 * 1000 * 1000;

// Program State

IPAddress ip;
WiFiClient wifi;
MQTTClient client;
HX711 loadcell;

bool printed_button_message = false;


void setup_scale() {
  Serial.println("Initialisiere Waage.");
  loadcell.power_up();
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  //loadcell.tare();
  loadcell.set_offset(offset);
  loadcell.set_scale(1/scale);  
}


bool connect(uint64_t timeout_millis) {  
  uint64_t startConnect = millis();
  // connect to WiFi network
  Serial.print("Verbinde mit WiFi. ");
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    if (millis() - startConnect > timeout_millis) {
      return false;
    }
    delay(500);
  }
  
  ip = WiFi.localIP();
  Serial.print(" -> IP: ");
  Serial.println(ip);
  
  client.begin(broker_url, wifi);
  
  Serial.print("Verbinde mit MQTT...");
  while (!client.connect("arduino", "try", "try")) {
    Serial.print(".");
    if (millis() - startConnect > timeout_millis) {
      return false;
    }
    delay(500);
  }
  Serial.println();
  return true;
}

void disconnect() {
  client.disconnect();
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
}

void deepsleep(uint64_t sleep_time) {
  disconnect();
  esp_sleep_enable_timer_wakeup(sleep_time);
  Serial.print("Gehe fuer ");
  Serial.print(sleep_time / 1000000.0);
  Serial.println("s schlafen.");
  //Serial.flush(); 
  loadcell.power_down();             // put the ADC in sleep mode
  esp_deep_sleep_start();
}

void setup() {
  // Measure time before measuring to get a more accurate sleep interval.
  uint64_t lastMillis = millis();
  Serial.begin(115200);
  delay(250);
  
  // Try connect to see if connection is possible
  if (not connect(60 * 1000)) {
    // If connection was not succesfull,
    // go to sleep again and try later instead of wasting energy
    deepsleep(interval - (millis() - lastMillis) * 1000);
  }
  setup_scale();
  
  Serial.println("Messe Gewicht.");
  float measurement = loadcell.get_units(samples);
  Serial.print("Gewicht (in g): ");
  Serial.println(measurement, 4);
  // Send to MQTT
  client.publish(topic, String(measurement));
  
  // Sleep for the rest of the interval
  uint64_t sleep_time = interval - (millis() - lastMillis) * 1000;
  deepsleep(sleep_time);
}


void loop() {
}

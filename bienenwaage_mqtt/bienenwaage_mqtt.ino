#include "HX711.h"
#include <SenseBoxMCU.h>
#include <SPI.h>
#include <Wire.h>
#include <stdarg.h>
#include <WiFi101.h>
#include <MQTT.h>
#include "dualprint.h"

// Configuration

const char ssid[] = "";
const char pass[] = "";
const char broker_url[] = "";
const char* topic = "";

// Toggle serial or OLED printing
const bool SERIAL_ = true;
const bool DISPLAY_ = false;

// HX711 circuit wiring
const int LOADCELL_DOUT_PIN = 2;
const int LOADCELL_SCK_PIN = 1;

// Scale and offset of the scale. Must be determined with the calibration script.
const float scale = 0.03643585;
const float offset = 179904.0;

// Number of samples to take and average over before sending the result
const int samples = 32;

// Time interval between to measurements in ms
const long interval = 9000;
// Whether to accept button presses, to immediatly run next measurements
const bool accept_button = false;


// Enable to sanity check if MQTT message can be subscribed and is correctly returned.
const bool loopback_mqtt = true;


// Program State

IPAddress ip;
WiFiClient wifi;
MQTTClient client;
HX711 loadcell;
// Helper class to print to serial and OLED at the same time.
DualPrint printer(SERIAL_, DISPLAY_);
Button menuButton(0);

unsigned long lastMillis = 0;
bool printed_button_message = false;




void setup_scale() {
  printer.println("Initialisiere Waage.");
  loadcell.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  //loadcell.tare();
  loadcell.set_offset(offset);
  loadcell.set_scale(1/scale);  
}


void messageReceived(String &topic, String &payload) {
  printer.println("Nachricht erhalten: " + topic + " - " + payload);

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}


void connect() {
  senseBoxIO.powerXB1(true);  // power on
  
  printer.println("Teste WiFi-Schild.");
  if(WiFi.status() == WL_NO_SHIELD)
  {
    printer.println("WiFi-Schild konnte nicht gefunden werden.");
    WiFi.end();
    return; // don't continue
  }
  printer.println("Schild gefunden.");
  
  // connect to WiFi network
  printer.print("Teste WiFi. ");
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED)
  {
    printer.print(".");
    delay(500);
  }
  
  ip = WiFi.localIP();
  printer.print(" -> IP: ");
  printer.println(ip);
  
  client.begin(broker_url, wifi);
  
  printer.print("Verbinde mit MQTT...");
  while (!client.connect("arduino", "try", "try")) {
    printer.print(".");
  }
  printer.println();
  if (loopback_mqtt) {
    client.subscribe(topic);
  }
}

void disconnect() {
  client.disconnect();
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
  }
  senseBoxIO.powerXB1(false);
}

void setup() {
  if (accept_button) {
    menuButton.begin();  
  }
  printer.init();
  // XBEE1 Socket
  senseBoxIO.powerXB1(false); // power off to reset
  delay(250);
  // Try connect to see if connection is possible
  connect();

  client.onMessage(messageReceived);


  setup_scale();
  printer.println("setup() abgeschlossen.");
  lastMillis = -interval;
}


void loop() {
  
  if (accept_button && !printed_button_message) {
    printed_button_message = true;
    printer.println("Druecke switch um erneut zu messen");
  }

  // Make a measurment if the time difference exceeds interval or button is active and pressed
  if (millis() - lastMillis > interval || (accept_button and menuButton.isPressed() == HIGH)) {
    lastMillis = millis();
    printed_button_message = false;
    
    printer.clear_display();
    printer.println("Messe Gewicht.");
    float measurement = loadcell.get_units(samples);
    printer.print("Gewicht (in g): ");
    printer.println(measurement, 4);
    // Connect if not already connected
    if (!client.connected()) {
      connect();
    }
    // Send to MQTT
    client.publish(topic, String(measurement));
    // allow client to receive messages
    client.loop();
  }
  // Sleep for the rest of the interval button is disabled
  if (!accept_button) {
    long sleep_time = interval - (millis() - lastMillis);
    printer.clear_display();
    if (sleep_time <= 0) {
      printer.print("Warnung: Messinterval ist kleiner als benoetigte Zeit.");
    }
    else {
      disconnect();
      printer.print("Gehe fuer ");
      printer.print(sleep_time / 1000.0);
      printer.println("s schlafen.");
      delay(sleep_time);
    }
  }
}

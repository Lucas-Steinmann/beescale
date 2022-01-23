#include <HX711.h>
#include <SPI.h>
#include <stdarg.h>
#include <WiFi.h>
#include <MQTT.h>
// Temperature
#include <OneWire.h>
#include <DallasTemperature.h>
// Time
#include <NTPClient.h>
#include <WiFiUdp.h>

// Libraries for SD card
#include "FS.h"
#include "SD.h"

#include "config.h"


#if TOWEBSERIAL
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>
#endif


// Configuration

// Network
const char ssid[] = WIFI_SSID;
const char pass[] = WIFI_PASS;
const char broker_url[] = MQTT_BROKER_URL;
const char* topic_weight = MQTT_TOPIC_WEIGHT;
const char* topic_raindrop = MQTT_TOPIC_RAINDROP;
const char* topic_battery = MQTT_TOPIC_BATTERY;
const char* topic_temp = MQTT_TOPIC_TEMP;


// Scale and offset of the scale. Must be determined with the calibration script.
const float scale_1 = -0.02251771; // -0.02276484;
const float offset_1 =  -38602.00; //12759.00000000;
const float scale_2 =  0.02439099; // 0.02199127;
const float offset_2 = -9011.00; // 48621.00000000;

// Raindrop threshold. If measurement falls below this value, rain is detected. Range: [0, 4095]
const int rain_threshold = 1000;


const float scale_bat_v = 0.001722332451;

// Program State

IPAddress ip;
WiFiClient wifi;
MQTTClient client;
// Time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);
// Sensors
HX711 loadcell_1;
HX711 loadcell_2;
OneWire oneWire;
DallasTemperature temperature_sensor;

void multi_print(String str) {
#if TOWEBSERIAL
  WebSerial.print(str);
#endif
#if TOSERIAL
  Serial.print(str);
#endif
#ifdef TOSD
  appendFile(SD, LOGFILE, str.c_str());
#endif
}

void multi_println(String str) {
#if TOWEBSERIAL
  WebSerial.println(str);
#endif
#if TOSERIAL
  Serial.println(str);
#endif
#ifdef LOGFILE
  str += "\r\n";
  appendFile(SD, LOGFILE, str.c_str());
#endif  
}

void setup_scale() {
  multi_println("Initialisiere Waage.");
  loadcell_1.power_up();
  loadcell_2.power_up();
  loadcell_1.begin(LOADCELL_DOUT_PIN_1, LOADCELL_SCK_PIN_1);
  loadcell_2.begin(LOADCELL_DOUT_PIN_2, LOADCELL_SCK_PIN_2);
  // Do not tare, since the weight of the beehive 
  // is already on the scale during start up.
  // loadcell.tare();
  loadcell_1.set_offset(offset_1);
  loadcell_1.set_scale(1/scale_1); 
  loadcell_2.set_offset(offset_2);
  loadcell_2.set_scale(1/scale_2);
}

void teardown_scale() {
  loadcell_1.power_down();
  loadcell_2.power_down();
}

void setup_raindrop() {
  if (RAINDROP_DOUT_PIN >= 0) {
    pinMode(RAINDROP_DOUT_PIN, INPUT);
  }
}

void setup_battery() {
  if (BATTERY_VOLTAGE_PIN >= 0) {
    pinMode(BATTERY_VOLTAGE_PIN, INPUT);
  }
}


void setup_temperature() {
  if (ONE_WIRE_BUS >= 0) {
    // Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
    oneWire = OneWire(ONE_WIRE_BUS);
    // Pass our oneWire reference to Dallas Temperature. 
    temperature_sensor = DallasTemperature(&oneWire);
    temperature_sensor.begin();
  }
}

void setup_sdcard() {
   // Initialize SD card
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }

  // If the LOGFILE file doesn't exist
  // Create a file on the SD card and write the data labels
  File file = SD.open(LOGFILE);
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD, LOGFILE, "Start of log\r\n");
  }
  else {
    Serial.println("File already exists. We will append.");  
  }
  file.close();
}

bool connect_wifi(uint64_t timeout_millis) { 
  uint64_t startConnect = millis();
  // connect to WiFi network
  multi_print("Verbinde mit WiFi. ");
  WiFi.begin(ssid, pass);
  while(WiFi.status() != WL_CONNECTED)
  {
    multi_print(".");
    if (millis() - startConnect > timeout_millis) {
      return false;
    }
    delay(500);
  }
  
  ip = WiFi.localIP();
  multi_print(" -> IP: ");
  multi_println(ip.toString());
  
  client.begin(broker_url, wifi);
  
  multi_print("Verbinde mit MQTT...");
  while (!client.connect("arduino", "try", "try")) {
    multi_print(".");
    if (millis() - startConnect > timeout_millis) {
      return false;
    }
    delay(500);
  }
  multi_println("");
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
  multi_print("Gehe fuer ");
  multi_print(String(sleep_time / 1000000.0));
  multi_println("s schlafen.");
  //Serial.flush(); 
  loadcell_1.power_down(); // put the ADC in sleep mode
  loadcell_2.power_down(); // put the ADC in sleep mode
  esp_deep_sleep_start();
}

void measure_weight() {
  multi_println("Messe Gewichte:");
  float measurement_1 = loadcell_1.get_units(samples);
  float measurement_2 = loadcell_2.get_units(samples);
  Serial.println("Test");
  // Log message
  float sum = measurement_1 + measurement_2;
  String measurement_str = String(sum, 4);
  String log_message = "Gewichte (in g): " + String(measurement_1, 4) + " + " + String(measurement_2, 4) + " = " + measurement_str;
  multi_println(log_message);
  // Send to MQTT
  client.publish(topic_weight, measurement_str);
  // and write to log
  add_record_to_current_csv_line(measurement_str.c_str());
}

void measure_raindrop() {
  if (RAINDROP_DOUT_PIN >= 0) {
    int measurement = analogRead(RAINDROP_DOUT_PIN);
    String measurement_str = String(measurement);
    multi_println("Regensensor (0-4095): " + measurement_str);
    client.publish(topic_raindrop, measurement_str);
    // and write to log
    add_record_to_current_csv_line(measurement_str.c_str());
  }
}

void measure_battery_voltage() {
  if (BATTERY_VOLTAGE_PIN >= 0) {
    int measurement = analogRead(BATTERY_VOLTAGE_PIN);
    String raw_measurement_str = String(measurement);
    multi_println("Batterie input (0-4096): " + raw_measurement_str);
    float v = measurement * scale_bat_v;
    String measurement_str = String(v);
    client.publish(topic_battery, measurement_str);
    // and write to log
    add_record_to_current_csv_line(measurement_str.c_str());
  }
}

void measure_temperature() {
  if (ONE_WIRE_BUS >= 0) {
    multi_print("Temperature Celsius: ");
    temperature_sensor.requestTemperatures();
    float temp = temperature_sensor.getTempCByIndex(0);
    String measurement_str = String(temp, 4);
    if(temp == -127.00) {
      multi_println("Failed to read from DS18B20 sensor");
    } else {
      multi_println("Measurement from DS18B20: " + measurement_str);
    }
    client.publish(topic_temp, measurement_str);
    // and write to log
    add_record_to_current_csv_line(measurement_str.c_str());
  }
}

void measure_time() {
  timeClient.begin();
  timeClient.update();
  String log_message = "UTC Time is: " + String(timeClient.getFormattedTime());
  multi_println(log_message);
  unsigned long epoch_time = timeClient.getEpochTime();
  String measurement_str = String(epoch_time);
  add_record_to_current_csv_line(measurement_str.c_str());
}

void setup() {
  // Measure time before measuring to get a more accurate sleep interval.
  uint64_t lastMillis = millis();
  
  
  Serial.begin(115200);
  delay(250);
  // Setup SD card early, since we need it for logging
  setup_sdcard();
  multi_println("\n\n\nRebooted.");
  
  // Try connect to see if connection is possible
  if (not connect_wifi(60 * 1000)) {
    // If connection was not succesfull,
    // go to sleep again and try later instead of wasting energy
    deepsleep(interval - (millis() - lastMillis) * 1000);
  }

  // Power on sensors and give them some time to stabilize
  if (SENSOR_SWITCH_PIN >= 0) {
    pinMode(SENSOR_SWITCH_PIN, OUTPUT);
    digitalWrite(SENSOR_SWITCH_PIN, HIGH);
    delay(500);
  }
  
  setup_scale();
  setup_raindrop();
  setup_temperature();
  setup_battery();
  
  measure_time();
  measure_weight();
  measure_raindrop();
  measure_temperature();
  measure_battery_voltage();
  #ifdef CSVFILE
    appendFile(SD, CSVFILE, "\n");
  #endif

  teardown_scale();
  
  if (SENSOR_SWITCH_PIN >= 0) {
    digitalWrite(SENSOR_SWITCH_PIN, LOW);
  }
  // Sleep for the rest of the interval
  uint64_t sleep_time = interval - (millis() - lastMillis) * 1000;
  deepsleep(sleep_time);
}


void loop() {
}


void add_record_to_current_csv_line(const char *record) {
#ifdef CSVFILE
  // TODO: rewrite as str concat
  Serial.println("Appending: " + String(record) + " to SD Card");
  appendFile(SD, CSVFILE, record);
  appendFile(SD, CSVFILE, ",");
#endif
}


// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char * path, const char * message) {
//  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)) {
    Serial.println("Message appended");
  } else {
    Serial.println("Append failed");
  }
  file.close();
}

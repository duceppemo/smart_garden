//I2C
#include <Arduino.h>

//#include <SPI.h>  // SSD1306 OLED Display (128x64)
#include <Wire.h>
#include <Adafruit_GFX.h>  
#include <Adafruit_SSD1306.h>
#include <hp_BH1750.h>  // BH1750 light senor (0x23)
#include <SensirionI2CScd4x.h>  // Adafruit SCD-40 CO2, temp, HR sensor
//#include "Adafruit_VEML7700.h"  // Adafruit VEML7700 Lux Sensor

// SDD1306 OLED Display (128x64)
// 21 char wide per line and 8 line high with text size 1 (128x64) 1char = 6x8
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// uint8_t: 0-255
// uint16_t: 0-65,535
// uint32_t: 0-4,294,967,295
// float: -3.4028235E+38 to 3.4028235E+38

// UV sensor pin
uint8_t uv_pin = A0;

// Low water level sensor pin
uint8_t bucket_water_level_pin = A1;

// Moisture sensor input pins
uint8_t moist_pins[] = {A2, A3, A6, A7};
uint8_t n_moist = 4;  // sizeof(moist_pins)

// Water pump output pins
uint8_t water_pins[] = {3, 4, 5, 6};
uint8_t n_water = 4; 

// I2C devices/sensors initiation
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);  // SSD1306 OLED Display (128x64)
hp_BH1750 bh1750; // BH1750 light senor (lux)
SensirionI2CScd4x scd4x;  // Adafruit SCD-40 CO2 (400-2000ppm), temp, HR sensor

// Capacitive Soil Moisture Sensor (analogue)
const uint16_t AirValue = 620;
const uint16_t WaterValue = 280;
const uint16_t WateringThreshold = 400;
const uint16_t WateringTime = 5000;
const uint16_t MonitorDelay = 1000;

// Frequency of sensor reading
uint16_t sensor_delay = 5000;

// How long we water
uint16_t watering_time = 60000;  // 1 min

void print_splash_screen(){
  // Clear the buffer (don't display the Adafruit logo)
  display.clearDisplay();

  // Welcome message
  // TODO: Add logo
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(27,15);
  display.println("Smart Garden");  // 12 char
  display.setCursor(51,23);
  display.println("v0.1");  // 4 char
  display.setCursor(39,31);
  display.println("by deMod");  // 8 char
  display.display();
  delay(3000);
}

uint8_t get_co2_temp_rh(){
  uint16_t error;
  char errorMessage[256];

  // Read Measurement
  uint16_t co2;
  float temp;
  float hr;
  
  error = scd4x.readMeasurement(co2, temperature, humidity);
  if (error) {
    Serial.print("Error trying to execute readMeasurement(): ");
    errorToString(error, errorMessage, 256);
    Serial.println(errorMessage);
  } else if (co2 == 0) {
    Serial.println("Invalid sample detected, skipping.");
  } else {
    Serial.print("Co2:");
    Serial.print(co2);
    Serial.print("\t");
    Serial.print("Temperature:");
    Serial.print(temperature);
    Serial.print("\t");
    Serial.print("Humidity:");
    Serial.println(humidity);
  }
}

float get_lux(){
  bh1750.start();   // Measure light intensity
  float lux = bh1750.getLux();  // Convert to lux
  return lux;
}

float get_uv(){
  float sensorValue = analogRead(A0);
  float sensorVoltage = sensorValue/1024*5.0;  // Convert to voltage
  float uv_index = sensorVoltage/0.1;  // Convert to UV index

  Serial.print("UV sensor reading:");
  Serial.println(sensorValue);
  Serial.print("UV sensor voltage:");
  Serial.println(sensorVoltage);
  Serial.print("UV index:");
  Serial.println(uv_index, 1);  // Keep 1 decimal

  return uv_index;
}

void update_display_sensor_values(int temp, int rh, int s1, int s2, int s3, int s4, int lux, int uv){
  // Erase any old text
  display.clearDisplay();

  // Temp
  display.setCursor(0,0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("T:");  // Temperature
  display.print(temp);
  display.print((char)247);
  display.print("C");

  // RH
  display.setCursor(63,0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("RH:");
  display.print(rh);
  display.print("%");

  // Soil moisture (4 sensors)
  display.setCursor(0,23);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Soil Moisture");
  display.setCursor(0,31);  // s1
  display.print("s1:");
  display.print(s1);
  display.print("%");
  display.setCursor(63,31);  // s2
  display.print("s2:");
  display.print(s2);
  display.print("%");
  display.setCursor(0,39);  // s3
  display.print("s3:");
  display.print(s3);
  display.print("%");
  display.setCursor(63,39);  // s4
  display.print("s4:");
  display.print(s4);
  display.print("%");

  // Light
  display.setCursor(0,55);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Visible:");  // lux
  display.print(lux);
  display.print("lx");
  display.setCursor(31,55);  // uv
  display.print("UV:");
  display.print(uv, 1);  // 1 decimal

  // Print on screen
  display.display();
}

uint8_t get_soil_moisture(uint8_t moist_pin){
  // Read moisture values
  uint8_t moisture = analogRead(moist_pin);  //sensor #1

  // Print raw sensor values to serial
  Serial.print(moist_pin);
  Serial.print(":");
  Serial.println(moisture);

  // Convert sensor values to %
  uint8_t moist_percentage = map(moist_pin, AirValue, WaterValue, 0, 100);

  // Adjust value if out of range
  if(moist_percentage >= 100) {
    moist_percentage = 100;
  } else if(moist_percentage <=0) {
    moist_percentage = 0;
  }
  // Return array
  return moist_percentage;
}

void water(uint8_t output_pin, uint16_t watering_time){
  // Activate relay for the pump
  digitalWrite(output_pin, HIGH);
  delay(watering_time);
  digitalWrite(output_pin, LOW);
}

void setup() {
  // Open serial port, set the baud rate to 9600 bps
  Serial.begin(9600);
  while (!Serial) {
    delay(100);
  }
  
  // Setup OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Setup SCD-40 CO2, temperature and humidity
  Wire.begin();
  uint16_t error;
  char errorMessage[256];
  scd4x.begin(Wire);
  // stop potentially previously started measurement
  error = scd4x.stopPeriodicMeasurement();
    if (error) {
        Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
        errorToString(error, errorMessage, 256);
        Serial.println(errorMessage);
    }
  // Start Measurement
  error = scd4x.startPeriodicMeasurement();
  if (error) {
      Serial.print("Error trying to execute startPeriodicMeasurement(): ");
      errorToString(error, errorMessage, 256);
      Serial.println(errorMessage);
  }

  // Splash screen
  print_splash_screen();

  // Setup digital output pins as output
  for(uint8_t i = 0; i < 4; i++){
    pinMode(water_pins[i], OUTPUT);
  }
}

void loop() {
  uint8_t moistArray[4] = {0,0,0,0};
  for(uint8_t i = 0; i < moistArray[4]; i++){
    moistArray[i] = get_soil_moisture(moist_pins[i]);  // Soil moisture %
  }

  //uint8_t temp = get_temp();
  //uint8_t rh = get_rh();
  uint8_t temp get_co2_temp_rh();
  uint8_t lux = get_lux();
  float uv = get_uv();

  // Update sensor values on display
  update_display_sensor_values(temp, rh, s1, s2, s3, s4, lux, uv);

  // Check if need watering
  for(uint8_t i = 0; i < moistArray[4]; i++){
    if(moistArray[i] < ){
      water(moist_pins[i], watering_time);
    }
  }

  // Read sensors every 5s
  delay(sensor_delay)
  }
}

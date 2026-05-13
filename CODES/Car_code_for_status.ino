#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

#define BATTERY_PIN 34   // ADC GPIO34 for battery voltage
#define CHARGER_PIN 32   // ADC GPIO32 for charging detection
#define LED_PIN 4        // GPIO4 for LED indicator

#define SDA_PIN 21       // I2C SDA
#define SCL_PIN 22       // I2C SCL

// Battery Voltage Levels
#define MAX_BATTERY_VOLTAGE 4.2  // Fully charged (1.4V per cell)
#define MIN_BATTERY_VOLTAGE 3.0  // Low battery (1.0V per cell)

// Voltage Divider Resistor Values
#define R1 100.0  // 100kΩ
#define R2 220.0  // 220kΩ

// Charging Detection Resistor Values
#define R3 15000.0  // 15kΩ
#define R4 3000.0   // 3kΩ

LiquidCrystal_PCF8574 lcd(0x27);  // I2C LCD address

void setup() {
    Serial.begin(115200);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // Turn off LED initially

    Wire.begin(SDA_PIN, SCL_PIN);
    lcd.begin(16, 2);
    lcd.setBacklight(HIGH);  // LCD backlight ON

    lcd.setCursor(0, 0);
    lcd.print("Battery Monitor");
    delay(2000);
}

void loop() {
    float batteryVoltage = readBatteryVoltage();
    int batteryPercentage = calculateBatteryPercentage(batteryVoltage);
    bool charging = isCharging();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Voltage: ");
    lcd.print(batteryVoltage, 2);  // 2 decimal places
    lcd.print("V");

    lcd.setCursor(0, 1);
    lcd.print("Batt: ");
    lcd.print(batteryPercentage);
    lcd.print("% ");

    // if (charging) {
    //     lcd.print("CHARGING");
    //     digitalWrite(LED_PIN, HIGH);
    // } else {
    //     lcd.print("NOT");
    //     digitalWrite(LED_PIN, LOW);
    // }

    Serial.print("Voltage: ");
    Serial.print(batteryVoltage);
    Serial.print("V, Battery: ");
    Serial.print(batteryPercentage);
    Serial.println("%");

    Serial.println(charging ? "Charging" : "Not Charging");
lcd.print(charging ? "Charging" : "Not Charging");
    delay(10000);
}

// Function to read battery voltage
float readBatteryVoltage() {
    int rawValue = analogRead(BATTERY_PIN);
    // Serial.println(rawValue);
    float voltage = (rawValue / 4095.0) * 3.3;  // Convert ADC reading to ESP32 voltage
    float batteryVoltage = voltage * ((R1 + R2) / R2);  // Apply voltage divider correction
    return batteryVoltage;
}

// Function to calculate battery percentage
int calculateBatteryPercentage(float voltage) {
    if (voltage >= MAX_BATTERY_VOLTAGE) return 100;
    if (voltage <= MIN_BATTERY_VOLTAGE) return 0;

    return (int)(((voltage - MIN_BATTERY_VOLTAGE) / 
                 (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE)) * 100);
}

// Function to check if charger is connected
bool isCharging() {
    int rawADC = analogRead(CHARGER_PIN);
    Serial.println(analogRead(CHARGER_PIN));
    float chargerVoltage = (rawADC / 4095.0) * 3.3;  // Convert ADC to voltage

    // Apply voltage divider correction
    float actualChargerVoltage = chargerVoltage * ((R3 + R4) / R4);

    return actualChargerVoltage > readBatteryVoltage();  // Charging if charger voltage > battery voltage
}

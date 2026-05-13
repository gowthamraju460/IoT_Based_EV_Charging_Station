#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "qrcodegen.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_PN532.h>

// WiFi Credentials
const char* ssid = "gowtham";
const char* password = "12345678";

// Razorpay API Credentials
const char* razorpay_key = "rzp_test_qCRBDwuXceIyC2";
const char* razorpay_secret = "bvb9kNiv5CBPuJPUpQUUy5qD";

// IR Sensor Pin
#define IR_SENSOR_PIN 34
#define RELAY_PIN 5
// NFC Module Pins
#define SDA_PIN 21
#define SCL_PIN 22
Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_ADDRESS   0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// NFC Allowed UIDs
const byte allowedUIDs[][4] = {
    {0x83, 0x45, 0xD0, 0xC},
    {0xCA, 0xFE, 0xBA, 0xBE},
};

// QR Code Parameters
#define QR_VERSION 3
uint8_t qrcode[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_VERSION)];
uint8_t tempBuffer[qrcodegen_BUFFER_LEN_FOR_VERSION(QR_VERSION)];

// Timer Variables
unsigned long startTime = 0;
bool timing = false;

// Payment Variables
String paymentId = "";
bool paymentReceived = false;

void connectWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\n✅ Connected to WiFi!");
}

bool checkNFC() {
    uint8_t uid[7];
    uint8_t uidLength;
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
        Serial.print("NFC UID: ");
        for (uint8_t i = 0; i < uidLength; i++) {
            Serial.print(" 0x");
            Serial.print(uid[i], HEX);
        }
        Serial.println("");
        for (int i = 0; i < sizeof(allowedUIDs) / sizeof(allowedUIDs[0]); i++) {
            if (memcmp(uid, allowedUIDs[i], 4) == 0) {
                Serial.println("✅ Authorized NFC Tag!");
                return true;
            }
        }
        Serial.println("❌ Unauthorized NFC Tag!");
        delay(5000);
    }
    return false;
}

// Function to generate and display the QR code
void drawQRCode(const char *text) {
    if (qrcodegen_encodeText(text, tempBuffer, qrcode, qrcodegen_Ecc_LOW, 
                             QR_VERSION, QR_VERSION, qrcodegen_Mask_AUTO, true)) {
        display.clearDisplay();
        int qrSize = qrcodegen_getSize(qrcode);
        
        // Dynamic Scaling: Fit QR code in screen without cutoff
        int scale = min(SCREEN_WIDTH / qrSize, SCREEN_HEIGHT / qrSize);

        // Centering
        int xOffset = (SCREEN_WIDTH - (qrSize * scale)) / 2;
        int yOffset = (SCREEN_HEIGHT - (qrSize * scale)) / 2;

        // Draw each module of the QR code
        for (int y = 0; y < qrSize; y++) {
            for (int x = 0; x < qrSize; x++) {
                if (qrcodegen_getModule(qrcode, x, y)) {
                    display.fillRect(xOffset + x * scale, yOffset + y * scale, scale, scale, SSD1306_WHITE);
                }
            }
        }
        display.display();  // Update OLED display
    } else {
        Serial.println("QR Code Generation Failed!");
    }
}

String createPaymentLink(int amount) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, "https://api.razorpay.com/v1/payment_links");
    http.addHeader("Content-Type", "application/json");
    http.setAuthorization(razorpay_key, razorpay_secret);

    StaticJsonDocument<512> doc;
    doc["amount"] = amount * 100;  // Convert to paise
    doc["currency"] = "INR";
    doc["description"] = "IR Sensor Payment";

    String requestBody;
    serializeJson(doc, requestBody);
    
    int httpResponseCode = http.POST(requestBody);
    String response = http.getString();
    http.end();

    if (httpResponseCode == 200) {
        DynamicJsonDocument resDoc(1024);
        deserializeJson(resDoc, response);

        // Extract Payment ID and Short URL
        String payId = resDoc["id"].as<String>();  
        String shortURL = resDoc["short_url"].as<String>();

        Serial.println("✅ Payment ID: " + payId);
        Serial.println("🔗 Short URL: " + shortURL);

        return shortURL;  // Return the Razorpay short URL
    } else {
        Serial.print("❌ Error Creating Payment Link: ");
        Serial.println(httpResponseCode);
    }
    return "";
}

void waitForPayment(String paymentId) {
    unsigned long startWait = millis();
    while (!paymentReceived) {
        Serial.println("⏳ Checking payment status...");

        if (checkPaymentStatus(paymentId)) {
            paymentReceived = true;
            Serial.println("✅ Payment Successful!");
            return;
        }

        if (millis() - startWait > 60000) {  // Timeout after 60 seconds
            Serial.println("❌ Payment Timed Out!");
            return;
        }

        delay(5000);  // Check every 5 seconds
    }
}

bool checkPaymentStatus(String paymentId) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;

    String url = "https://api.razorpay.com/v1/payment_links/" + paymentId;
    http.begin(client, url);
    http.setAuthorization(razorpay_key, razorpay_secret);
    
    int httpResponseCode = http.GET();
    String response = http.getString();
    http.end();

    if (httpResponseCode == 200) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);

        String status = doc["status"].as<String>();
        Serial.print("🔄 Payment Status: ");
        Serial.println(status);

        return (status == "paid");
    } else {
        Serial.print("❌ Error Checking Payment Status: ");
        Serial.println(httpResponseCode);
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    pinMode(IR_SENSOR_PIN, INPUT);
    connectWiFi();
    nfc.begin();
    if (display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS)) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(0, 0);
        display.println("WELCOME...");
        display.display();
        pinMode(RELAY_PIN, OUTPUT);
        digitalWrite(RELAY_PIN, LOW);
    }
}

void loop() {
    if (digitalRead(IR_SENSOR_PIN) == 0 && !timing) {
        Serial.println("⏳ Object Detected, Waiting for NFC...");
        if (checkNFC()) {
            digitalWrite(RELAY_PIN, HIGH);
            startTime = millis();
            timing = true;
            Serial.println("⏳ Timing Started...");
        }
    }

    if (digitalRead(IR_SENSOR_PIN) == 1 && timing) {
        digitalWrite(RELAY_PIN, LOW);
        unsigned long elapsedTime = (millis() - startTime) / 1000;
        int amount = elapsedTime * 5;
        Serial.print("⏲ Time: "); Serial.print(elapsedTime); Serial.println(" sec");
        Serial.print("💰 Amount: ₹"); Serial.println(amount);

        paymentId = createPaymentLink(amount);  // Get Payment Link
        if (paymentId != "") {
            Serial.println("⏳ Waiting for payment...");
            drawQRCode(paymentId.c_str());  // Display the QR code
            waitForPayment(paymentId);  // Pass Payment ID
        }
        timing = false;
    }
}

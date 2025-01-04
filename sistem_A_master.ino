#include <esp_now.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "ArduinoJson.h"
#include "Button2.h"

// Pin konfigurasi motor
#define MOTOR_A_IN1 19
#define MOTOR_A_IN2 18
#define MOTOR_B_IN1 4
#define MOTOR_B_IN2 2
#define MAX_PWM_SPEED 2047 // Kecepatan maksimum motor

// Konfigurasi pin tombol
#define BUTTON_NEXT_MODE_PIN 26 // Tombol untuk mengontrol mode
#define BUTTON_PREV_MODE_PIN 27 // Tombol untuk mengontrol mode
#define BUTTON_SPEAKER_PIN 14   // Tombol untuk mengontrol mode
#define BUTTON_MOTOR_PIN 12     // Tombol untuk mengontrol motor
#define BUTTON_TRANSMIT_PIN 13  // Tombol untuk mengontrol transmisi data

// Konfigurasi pin speaker
#define SPEAKER_PIN 33

// Konfigurasi pin indikator LED (Green)
#define LED_TRANSMIT_INDICATOR_PIN 15 

// Konfigurasi pin indikator mode (Red)
#define LED_WIFI_INDICATOR_PIN 17

#define LED_READY_INDICATOR_PIN 16

#define LED_A_PIN 5 // LSB
#define LED_B_PIN 22
#define LED_C_PIN 23 // MSB

// Status kontrol
uint8_t modeState = 1;           // Status mode
uint8_t speakerState = 0;        // Status frekuensi speaker
uint8_t motorState = 0;          // Status pergerakan motor
bool isTransmitReady = false;    // Flag untuk menentukan apakah data akan dikirim
bool isHeader = false;           // Falg untuk menentukan header atau bukan
uint16_t srcFrequency;

const uint16_t FREQUENCIES[5] = {0, 700, 800, 900, 1000};

// Konfigurasi WiFi dan server
const char *WIFI_SSID = "apaya";
const char *WIFI_PASSWORD = "241196221b";
const char *SERVER_URL = "https://script.google.com/macros/s/xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx/exec";

String jsonString;

// ESP-NOW
typedef struct {
    double obsvFrequency = 0;
    float temperature = 0;
} ESPNowMessage;

ESPNowMessage espNowData; // Struct untuk menyimpan data dari ESP-NOW

Button2 buttonNextMode(BUTTON_NEXT_MODE_PIN, INPUT_PULLUP);
Button2 buttonPrevMode(BUTTON_PREV_MODE_PIN, INPUT_PULLUP);
Button2 buttonSpeaker(BUTTON_SPEAKER_PIN, INPUT_PULLUP);
Button2 buttonMotor(BUTTON_MOTOR_PIN, INPUT_PULLUP);
Button2 buttonTransmit(BUTTON_TRANSMIT_PIN, INPUT_PULLUP);

void setup() {
    Serial.begin(115200);

    pinMode(LED_TRANSMIT_INDICATOR_PIN, OUTPUT);
    pinMode(LED_READY_INDICATOR_PIN, OUTPUT);
    pinMode(LED_WIFI_INDICATOR_PIN, OUTPUT);
    pinMode(LED_A_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT);
    pinMode(LED_C_PIN, OUTPUT);

    // Inisialisasi pin speaker
    ledcAttach(SPEAKER_PIN, 1000, 12);

    // Inisialisasi pin motor
    ledcAttach(MOTOR_A_IN1, 1000, 12);
    ledcAttach(MOTOR_A_IN2, 1000, 12);
    ledcAttach(MOTOR_B_IN1, 1000, 12);
    ledcAttach(MOTOR_B_IN2, 1000, 12);
    stopMotor();

    buttonNextMode.setDebounceTime(50);
    buttonPrevMode.setDebounceTime(50);
    buttonSpeaker.setDebounceTime(50);
    buttonMotor.setDebounceTime(50);
    buttonTransmit.setDebounceTime(50);

    buttonNextMode.setTapHandler([] (Button2 &btn) { handleMode(1); });
    buttonPrevMode.setTapHandler([] (Button2 &btn) { handleMode(-1); });
    buttonSpeaker.setTapHandler(handleSpeaker);
    buttonMotor.setTapHandler(handleMotor);
    buttonTransmit.setTapHandler(handleTransmit);

    // Koneksi WiFi
    connectToWiFi();

    // Inisialisasi ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Gagal menginisialisasi ESP-NOW.");
        return;
    }
    esp_now_register_recv_cb(onESPNowDataReceived);

    displayMode();

    xTaskCreate(postTask, "PostTask", 5120, NULL, 1, NULL);
}

void loop() {
    buttonNextMode.loop();
    buttonPrevMode.loop();
    buttonSpeaker.loop();
    buttonMotor.loop();
    buttonTransmit.loop();

    displayMode();
    displayisTransmitReady();
    displayWiFiStatus();
}

// Fungsi callback untuk ESP-NOW
void onESPNowDataReceived(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    memcpy(&espNowData, incomingData, sizeof(espNowData));
    Serial.printf("Frekuensi pendengar: %f Hz", espNowData.obsvFrequency);
    Serial.printf("Suhu: %f°C", espNowData.temperature);
    srcFrequency = FREQUENCIES[speakerState];

    // Buat payload JSON
    jsonString = createJsonPayload(
        modeState,                // Status mode saat ini
        espNowData.temperature,   // Suhu dari slave
        srcFrequency,             // Frekuensi speaker saat ini
        espNowData.obsvFrequency, // Frekuensi (terdengar) dari slave
        isHeader                  // Header atau bukan
    );
}

// Fungsi untuk menghubungkan ke WiFi
void connectToWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Menghubungkan ke WiFi...");
    }
    Serial.println("Terhubung ke WiFi.");
}

// Fungsi callback untuk buttonNextMode dan buttonPrevMode
void handleMode(uint8_t delta) {
    modeState = (modeState + delta + 8) % 8;
    isTransmitReady = false;
    Serial.printf("Mode transmisi saat ini: %d", modeState);
}

// Fungsi callback untuk buttonSpeaker
void handleSpeaker(Button2 &btn) {
    speakerState = (speakerState + 1) % 5;
    ledcWriteTone(SPEAKER_PIN, FREQUENCIES[speakerState]);
    Serial.printf("Frekuensi speaker saat ini: %d", FREQUENCIES[speakerState]);
}

// Fungsi callback untuk buttonMotor
void handleMotor(Button2 &btn) {
    motorState = (motorState + 1) % 4;
    Serial.printf("Mode motor saat ini: %d", motorState);

    switch (motorState) {
        case 0: stopMotor(); break;
        case 1: moveForward(); break;
        case 2: stopMotor(); break;
        case 3: moveBackward(); break;
    }
}

// Fungsi callback untuk buttonTrasmit
void handleTransmit(Button2 &btn) {
    isTransmitReady = !isTransmitReady;
    isHeader = true;
    Serial.print("Ready? ");
    Serial.println(isTransmitReady ? "Ya" : "Tidak");
}

// Fungsi untuk indikator mode
void displayMode() {
    digitalWrite(LED_A_PIN, (modeState & 0x01));
    digitalWrite(LED_B_PIN, (modeState >> 1) & 0x01);
    digitalWrite(LED_C_PIN, (modeState >> 2) & 0x01);
}

// Fungsi untuk indikator isTransmitReady
void displayisTransmitReady() {
    digitalWrite(LED_READY_INDICATOR_PIN, isTransmitReady ? HIGH : LOW);
}

// Fungsi untuk indikator isTransmitReady
void displayWiFiStatus() {
    digitalWrite(LED_WIFI_INDICATOR_PIN, (WiFi.status() == WL_CONNECTED) ? HIGH : LOW);
}

// Fungsi motor maju
void moveForward() {
    ledcWrite(MOTOR_A_IN1, MAX_PWM_SPEED);
    ledcWrite(MOTOR_A_IN2, 0);
    ledcWrite(MOTOR_B_IN1, MAX_PWM_SPEED);
    ledcWrite(MOTOR_B_IN2, 0);
    Serial.println("Motor maju");
}

// Fungsi motor mundur
void moveBackward() {
    ledcWrite(MOTOR_A_IN1, 0);
    ledcWrite(MOTOR_A_IN2, MAX_PWM_SPEED);
    ledcWrite(MOTOR_B_IN1, 0);
    ledcWrite(MOTOR_B_IN2, MAX_PWM_SPEED);
    Serial.println("Motor mundur");
}

// Fungsi motor berhenti
void stopMotor() {
    ledcWrite(MOTOR_A_IN1, 0);
    ledcWrite(MOTOR_A_IN2, 0);
    ledcWrite(MOTOR_B_IN1, 0);
    ledcWrite(MOTOR_B_IN2, 0);
    Serial.println("Motor berhenti");
}

// Fungsi untuk membuat payload JSON
String createJsonPayload(uint8_t mode, float temp, uint16_t srcFreq, double obsvFreq, bool header) {
    StaticJsonDocument<200> jsonPayload;
    jsonPayload["mode"] = String(mode);
    jsonPayload["temp"] = temp;
    jsonPayload["src"] = srcFreq;
    jsonPayload["obsv"] = obsvFreq;
    jsonPayload["reset"] = header;

    String jsonString;
    serializeJson(jsonPayload, jsonString);
    return jsonString;
}

// Fungsi POST untuk dijalankan di task RTOS
void postTask(void *parameter) {
    while (true) {
        if (isTransmitReady && WiFi.status() == WL_CONNECTED) {
            if (isHeader) {
                isHeader = false;
            }
            Serial.println("Mengirim POST...");

            digitalWrite(LED_TRANSMIT_INDICATOR_PIN, HIGH);
            HTTPClient http;
            http.begin(SERVER_URL);
            http.addHeader("Content-Type", "application/json; charset=UTF-8");

            int httpResponseCode = http.POST(jsonString);
            if (httpResponseCode > 0) {
                String response = http.getString();
                Serial.println("Kode respons HTTP: " + String(httpResponseCode));
                Serial.println("Respons: " + response);
            } else {
                Serial.println("Gagal mengirim data: " + String(httpResponseCode));
            }
            http.end();
            digitalWrite(LED_TRANSMIT_INDICATOR_PIN, LOW);
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS); // Jeda untuk efisiensi CPU
    }
}

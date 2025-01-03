#include <esp_now.h>
#include <WiFi.h>
#include "arduinoFFT.h"
#include "Button2.h"
#include "DallasTemperature.h"
#include "OneWire.h"

// Pin konfigurasi motor
#define MOTOR_A_IN1 19
#define MOTOR_A_IN2 18
#define MOTOR_B_IN1 4
#define MOTOR_B_IN2 2
#define MAX_PWM_SPEED 2047 // Kecepatan maksimum motor

// Konfigurasi pin tombol
#define BUTTON_MOTOR_PIN 13 // Tombol untuk mengontrol motor

// Konfigurasi pin sensor suhu
#define TEMP_SENSOR_PIN 33

// Konfigurasi pin sensor suara
#define SOUND_SENSOR_PIN 32

// Konfigurasi pin indikator LED (Green)
#define LED_TRANSMIT_INDICATOR_PIN 14

// Status kontrol
byte motorState = 0;          // Status pergerakan motor

// ESP-NOW
// uint8_t masterAddress[] = {0x10, 0x06, 0x1C, 0xF6, 0x43, 0xAC};
uint8_t masterAddress[] = {0xF8, 0xB3, 0xB7, 0x45, 0x3F, 0x94};
typedef struct {
    double obsvFrequency = 0;
    float temperature = 0;
} ESPNowMessage;

ESPNowMessage espNowData; // Struct untuk menyimpan data dari ESP-NOW
esp_now_peer_info_t peerInfo; // Deklarasi peerInfo

// Konfigurasi sensor suhu
OneWire oneWire(TEMP_SENSOR_PIN);
DallasTemperature temperatureSensor(&oneWire);

// Konfigurasi FFT
const uint16_t FFT_SAMPLES = 2048;
const double FFT_SAMPLING_FREQ = 2048;

double fftReal[FFT_SAMPLES] = {};
double fftImag[FFT_SAMPLES] = {};
unsigned int samplingPeriodUs;
unsigned long microsTimestamp;

ArduinoFFT<double> FFT = ArduinoFFT<double>(fftReal, fftImag, FFT_SAMPLES, FFT_SAMPLING_FREQ);

Button2 buttonMotor(BUTTON_MOTOR_PIN, INPUT_PULLUP);
void setup() {
    Serial.begin(115200);

    pinMode(LED_TRANSMIT_INDICATOR_PIN, OUTPUT);

    // Inisialisasi pin motor
    ledcAttach(MOTOR_A_IN1, 1000, 12);
    ledcAttach(MOTOR_A_IN2, 1000, 12);
    ledcAttach(MOTOR_B_IN1, 1000, 12);
    ledcAttach(MOTOR_B_IN2, 1000, 12);
    stopMotor();

    buttonMotor.setDebounceTime(50);
    buttonMotor.setTapHandler(handleMotor);

    // Inisialisasi ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("Gagal menginisialisasi ESP-NOW.");
        return;
    }
    esp_now_register_send_cb(onESPNowDataSent);

    // Konfigurasi peer
    memcpy(peerInfo.peer_addr, masterAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Gagal menambahkan peer");
        return;
    }

    temperatureSensor.begin();
    samplingPeriodUs = round(1000000 * (1.0 / FFT_SAMPLING_FREQ));
    xTaskCreate(processData, "processData", 4096, NULL, 1, NULL);
}

void loop() {
    buttonMotor.loop();
}

// Fungsi callback untuk ESP-NOW
void onESPNowDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Fungsi callback untuk buttonMotor
void handleMotor(Button2 &btn) {
    motorState = (motorState + 1) % 4;
    Serial.print("Mode motor saat ini: ");
    Serial.println(motorState);

    switch (motorState) {
        case 0: stopMotor(); break;
        case 1: moveForward(); break;
        case 2: stopMotor(); break;
        case 3: moveBackward(); break;
    }
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

// Fungsi untuk menghitung frekuensi dominan
double calculateDominantFrequency() {
    microsTimestamp = micros();
    for (int i = 0; i < FFT_SAMPLES; i++) {
        fftReal[i] = analogRead(SOUND_SENSOR_PIN);
        fftImag[i] = 0;

        while (micros() - microsTimestamp < samplingPeriodUs) {
            // Tunggu waktu sampling berikutnya
        }
        microsTimestamp += samplingPeriodUs;
    }

    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();

    return FFT.majorPeak();
}

// Fungsi POST untuk dijalankan di task RTOS
void processData(void *parameter) {
    while (true) {
        // Ambil data suhu dan frekuensi dominan
        digitalWrite(LED_TRANSMIT_INDICATOR_PIN, HIGH);
        espNowData.obsvFrequency = calculateDominantFrequency();  // Hitung frekuensi dominan
        temperatureSensor.requestTemperatures();
        espNowData.temperature = temperatureSensor.getTempCByIndex(0); // Ambil suhu
        Serial.printf("%f Hz, %f C\n",  espNowData.obsvFrequency,  espNowData.temperature);
        esp_err_t result = esp_now_send(masterAddress, (uint8_t *)&espNowData, sizeof(espNowData));
        // Serial.println((result == ESP_OK) ? "Pengiriman berhasil" : "Pengiriman gagal");
        if (result != ESP_OK) {
            Serial.println(result);
        }
        digitalWrite(LED_TRANSMIT_INDICATOR_PIN, LOW);
        vTaskDelay(2000 / portTICK_PERIOD_MS); // Jeda untuk efisiensi CPU
    }
}
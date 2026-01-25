#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "secrets.h"
#include "devices.h"
#include "switchbot_api.h"
#include "ui.h"

// Tab5 WiFi SDIO2 ピン設定
// ESP32-P4とESP32-C6間のSDIO通信用
#define TAB5_SDIO_CLK  12
#define TAB5_SDIO_CMD  13
#define TAB5_SDIO_D0   11
#define TAB5_SDIO_D1   10
#define TAB5_SDIO_D2   9
#define TAB5_SDIO_D3   8
#define TAB5_C6_RST    15

// WiFi接続中の表示
void showConnecting() {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(3);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString("Connecting to WiFi...", M5.Display.width() / 2, M5.Display.height() / 2);
}

// WiFi接続エラー表示
void showWiFiError() {
    M5.Display.fillScreen(TFT_RED);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(2);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString("WiFi Connection Failed", M5.Display.width() / 2, M5.Display.height() / 2 - 30);
    M5.Display.drawString("Check SSID and Password", M5.Display.width() / 2, M5.Display.height() / 2 + 30);
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    Serial.begin(115200);
    Serial.println("M5Stack Tab5 SwitchBot Bulb Controller");

    // WiFi接続
    showConnecting();

    // Tab5用SDIOピン設定（ESP32-C6との通信用）
    // arduino-esp32 3.2.1以上でWiFi.setPins()が利用可能
    WiFi.setPins(TAB5_SDIO_CLK, TAB5_SDIO_CMD, TAB5_SDIO_D0, TAB5_SDIO_D1, TAB5_SDIO_D2, TAB5_SDIO_D3, TAB5_C6_RST);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int retryCount = 0;
    const int maxRetries = 30;  // 30秒タイムアウト

    while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
        delay(1000);
        retryCount++;
        Serial.printf("Connecting... (%d/%d)\n", retryCount, maxRetries);

        // 接続中のドット表示
        M5.Display.print(".");
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi connection failed");
        showWiFiError();
        while (1) {
            delay(1000);
        }
    }

    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // NTP時刻同期
    configTime(9 * 3600, 0, "ntp.nict.jp", "pool.ntp.org");
    Serial.println("Waiting for NTP sync...");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nNTP synced");

    // SwitchBot API初期化
    switchbotApiInit();

    // UI初期化
    uiInit();

    // 初回の温湿度取得
    if (switchbotMeterStatus(meter.deviceId, meter.temperature, meter.humidity)) {
        meter.valid = true;
        uiUpdateMeter();
    }

    // 起動時に全電球の状態を取得
    uiRefreshAllBulbStatus();
}

// 温湿度更新間隔（ミリ秒）
#define METER_UPDATE_INTERVAL 60000
static unsigned long lastMeterUpdate = 0;

void loop() {
    uiUpdate();

    // 定期的に温湿度を更新
    unsigned long now = millis();
    if (now - lastMeterUpdate >= METER_UPDATE_INTERVAL) {
        lastMeterUpdate = now;
        if (switchbotMeterStatus(meter.deviceId, meter.temperature, meter.humidity)) {
            meter.valid = true;
            uiUpdateMeter();
        }
    }

    delay(10);  // CPU負荷軽減
}

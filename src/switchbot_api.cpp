#include "switchbot_api.h"
#include "secrets.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "mbedtls/md.h"
#include "mbedtls/base64.h"

// Base64(HMAC-SHA256(secret, token + t + nonce))
static String makeSign(const String& token, const String& secret, const String& t, const String& nonce) {
    String data = token + t + nonce;

    unsigned char hmacOut[32];
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, info, 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secret.c_str(), secret.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)data.c_str(), data.length());
    mbedtls_md_hmac_finish(&ctx, hmacOut);
    mbedtls_md_free(&ctx);

    // base64 encode
    unsigned char b64[64];
    size_t b64len = 0;
    mbedtls_base64_encode(b64, sizeof(b64), &b64len, hmacOut, sizeof(hmacOut));
    return String((char*)b64, b64len);
}

// 簡易nonce生成
static String makeNonce() {
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    char buf[32];
    snprintf(buf, sizeof(buf), "%08lx%08lx", (unsigned long)r1, (unsigned long)r2);
    return String(buf);
}

// SwitchBot APIにコマンドを送信
static bool sendCommand(const String& deviceId, const String& command, const String& parameter) {
    if (deviceId.isEmpty()) {
        Serial.println("Error: deviceId is empty");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    String url = "https://api.switch-bot.com/v1.1/devices/" + deviceId + "/commands";

    // t は13桁ミリ秒（UNIXタイムスタンプ）
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t epochMs = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    String t = String(epochMs);
    String nonce = makeNonce();
    String sign = makeSign(SWITCHBOT_TOKEN, SWITCHBOT_SECRET, t, nonce);

    String body = "{\"command\":\"" + command + "\",\"parameter\":\"" + parameter + "\",\"commandType\":\"command\"}";

    Serial.printf("Sending command to %s: %s (param: %s)\n", deviceId.c_str(), command.c_str(), parameter.c_str());

    if (!https.begin(client, url)) {
        Serial.println("Failed to begin HTTPS");
        return false;
    }

    https.addHeader("Content-Type", "application/json; charset=utf8");
    https.addHeader("Authorization", SWITCHBOT_TOKEN);
    https.addHeader("t", t);
    https.addHeader("nonce", nonce);
    https.addHeader("sign", sign);

    int code = https.POST(body);
    String resp = https.getString();
    https.end();

    Serial.printf("HTTP %d\n%s\n", code, resp.c_str());
    return (code >= 200 && code < 300);
}

void switchbotApiInit() {
    // 将来の拡張用
}

bool switchbotBulbPower(const String& deviceId, bool on) {
    return sendCommand(deviceId, on ? "turnOn" : "turnOff", "default");
}

bool switchbotBulbBrightness(const String& deviceId, int brightness) {
    // 明るさは1-100の範囲
    brightness = constrain(brightness, 1, 100);
    return sendCommand(deviceId, "setBrightness", String(brightness));
}

bool switchbotMeterStatus(const String& deviceId, float& temperature, int& humidity) {
    if (deviceId.isEmpty()) {
        Serial.println("Error: deviceId is empty");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    String url = "https://api.switch-bot.com/v1.1/devices/" + deviceId + "/status";

    // t は13桁ミリ秒（UNIXタイムスタンプ）
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t epochMs = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    String t = String(epochMs);
    String nonce = makeNonce();
    String sign = makeSign(SWITCHBOT_TOKEN, SWITCHBOT_SECRET, t, nonce);

    Serial.printf("Getting status for %s\n", deviceId.c_str());

    if (!https.begin(client, url)) {
        Serial.println("Failed to begin HTTPS");
        return false;
    }

    https.addHeader("Authorization", SWITCHBOT_TOKEN);
    https.addHeader("t", t);
    https.addHeader("nonce", nonce);
    https.addHeader("sign", sign);

    int code = https.GET();
    String resp = https.getString();
    https.end();

    Serial.printf("HTTP %d\n%s\n", code, resp.c_str());

    if (code < 200 || code >= 300) {
        return false;
    }

    // 簡易JSONパース（temperatureとhumidityを抽出）
    int tempIdx = resp.indexOf("\"temperature\":");
    int humIdx = resp.indexOf("\"humidity\":");

    if (tempIdx < 0 || humIdx < 0) {
        Serial.println("Failed to parse response");
        return false;
    }

    // temperature の値を抽出
    int tempStart = tempIdx + 14;
    int tempEnd = resp.indexOf(",", tempStart);
    if (tempEnd < 0) tempEnd = resp.indexOf("}", tempStart);
    String tempStr = resp.substring(tempStart, tempEnd);
    temperature = tempStr.toFloat();

    // humidity の値を抽出
    int humStart = humIdx + 11;
    int humEnd = resp.indexOf(",", humStart);
    if (humEnd < 0) humEnd = resp.indexOf("}", humStart);
    String humStr = resp.substring(humStart, humEnd);
    humidity = humStr.toInt();

    Serial.printf("Parsed: temp=%.1f, humidity=%d\n", temperature, humidity);
    return true;
}

bool switchbotBulbStatus(const String& deviceId, bool& powerState, int& brightness) {
    if (deviceId.isEmpty()) {
        Serial.println("Error: deviceId is empty");
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    String url = "https://api.switch-bot.com/v1.1/devices/" + deviceId + "/status";

    // t は13桁ミリ秒（UNIXタイムスタンプ）
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t epochMs = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    String t = String(epochMs);
    String nonce = makeNonce();
    String sign = makeSign(SWITCHBOT_TOKEN, SWITCHBOT_SECRET, t, nonce);

    Serial.printf("Getting bulb status for %s\n", deviceId.c_str());

    if (!https.begin(client, url)) {
        Serial.println("Failed to begin HTTPS");
        return false;
    }

    https.addHeader("Authorization", SWITCHBOT_TOKEN);
    https.addHeader("t", t);
    https.addHeader("nonce", nonce);
    https.addHeader("sign", sign);

    int code = https.GET();
    String resp = https.getString();
    https.end();

    Serial.printf("HTTP %d\n%s\n", code, resp.c_str());

    if (code < 200 || code >= 300) {
        return false;
    }

    // 簡易JSONパース（powerとbrightnessを抽出）
    int powerIdx = resp.indexOf("\"power\":");
    int brightnessIdx = resp.indexOf("\"brightness\":");

    if (powerIdx < 0 || brightnessIdx < 0) {
        Serial.println("Failed to parse bulb response");
        return false;
    }

    // power の値を抽出（"on" or "off"）
    int powerStart = resp.indexOf("\"", powerIdx + 8) + 1;
    int powerEnd = resp.indexOf("\"", powerStart);
    String powerStr = resp.substring(powerStart, powerEnd);
    powerState = (powerStr == "on");

    // brightness の値を抽出
    int brightStart = brightnessIdx + 13;
    int brightEnd = resp.indexOf(",", brightStart);
    if (brightEnd < 0) brightEnd = resp.indexOf("}", brightStart);
    String brightStr = resp.substring(brightStart, brightEnd);
    brightness = brightStr.toInt();

    Serial.printf("Parsed: power=%s, brightness=%d\n", powerState ? "on" : "off", brightness);
    return true;
}

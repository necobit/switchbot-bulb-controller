#ifndef DEVICES_H
#define DEVICES_H

#include <Arduino.h>

// 電球デバイス構造体
struct BulbDevice
{
    String deviceId; // SwitchBot デバイスID
    String name;     // 表示名
    bool powerState; // 電源状態
    int brightness;  // 明るさ（1-100）
};

// 電球の数
#define NUM_BULBS 4

// 温湿度計デバイス構造体
struct MeterDevice
{
    String deviceId;    // SwitchBot デバイスID
    String name;        // 表示名
    float temperature;  // 温度（℃）
    int humidity;       // 湿度（%）
    bool valid;         // データ有効フラグ
};

// 温湿度計
inline MeterDevice meter = {"CA323435166C", "温湿度計 6C", 0.0f, 0, false};

// 電球デバイス配列（初期設定）
// deviceId は GET /v1.1/devices で取得できます
inline BulbDevice bulbs[NUM_BULBS] = {
    {"94A99076A08A", "寝室ライト", false, 100},
    {"94A990794BC2", "和室ライト", false, 100},
    {"84FCE6B54B5E", "風呂ライト", false, 100},
    {"84FCE6F438D6", "キッチンライト", false, 100}};

#endif // DEVICES_H

#ifndef SWITCHBOT_API_H
#define SWITCHBOT_API_H

#include <Arduino.h>

// SwitchBot API初期化
void switchbotApiInit();

// 電球の電源制御
// deviceId: デバイスID
// on: true=ON, false=OFF
// 戻り値: 成功=true, 失敗=false
bool switchbotBulbPower(const String& deviceId, bool on);

// 電球の明るさ制御
// deviceId: デバイスID
// brightness: 明るさ（1-100）
// 戻り値: 成功=true, 失敗=false
bool switchbotBulbBrightness(const String& deviceId, int brightness);

// 温湿度計のステータス取得
// deviceId: デバイスID
// temperature: 取得した温度を格納
// humidity: 取得した湿度を格納
// 戻り値: 成功=true, 失敗=false
bool switchbotMeterStatus(const String& deviceId, float& temperature, int& humidity);

#endif // SWITCHBOT_API_H

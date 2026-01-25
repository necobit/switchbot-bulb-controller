# SwitchBot Bulb Controller for M5Stack Tab5

M5Stack Tab5 (ESP32-P4) で SwitchBot スマート電球を制御するアプリケーション。

## 機能

- **電球制御**: 4つのSwitchBot電球のON/OFF、明るさ調整
- **温湿度表示**: SwitchBot温湿度計のデータをヘッダーに表示（60秒ごとに更新）
- **省電力モード**: 1分間操作がないと画面オフ、タッチで復帰
- **タッチUI**: 直感的なタッチ操作によるスライダー・ボタン

## ハードウェア

- M5Stack Tab5 (ESP32-P4 + ESP32-C6)
- SwitchBot Color Bulb（最大4個）
- SwitchBot 温湿度計（オプション）

## セットアップ

### 1. secrets.h の作成

`include/secrets.h` を作成し、以下の内容を設定:

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// WiFi設定
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASS "your-wifi-password"

// SwitchBot API認証情報（SwitchBotアプリから取得）
#define SWITCHBOT_TOKEN "your-switchbot-token"
#define SWITCHBOT_SECRET "your-switchbot-secret"

#endif
```

### 2. devices.h の編集

`include/devices.h` でデバイスIDと名前を設定:

```cpp
inline BulbDevice bulbs[NUM_BULBS] = {
    {"DEVICE_ID_1", "電球1", false, 100},
    {"DEVICE_ID_2", "電球2", false, 100},
    {"DEVICE_ID_3", "電球3", false, 100},
    {"DEVICE_ID_4", "電球4", false, 100}
};

inline MeterDevice meter = {"METER_DEVICE_ID", "温湿度計", 0.0f, 0, false};
```

デバイスIDはSwitchBot APIの `GET /v1.1/devices` で取得できます。

### 3. ビルド & アップロード

```bash
# ビルド
pio run

# アップロード
pio run --target upload

# シリアルモニター
pio device monitor
```

## プロジェクト構成

```
├── src/
│   ├── main.cpp          # メインエントリポイント
│   ├── ui.cpp            # UI描画・タッチ処理
│   ├── ui.h
│   ├── switchbot_api.cpp # SwitchBot API通信
│   └── switchbot_api.h
├── include/
│   ├── devices.h         # デバイス設定
│   └── secrets.h         # 認証情報（gitignore）
└── platformio.ini        # PlatformIO設定
```

## 依存ライブラリ

- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5GFX](https://github.com/m5stack/M5GFX)

## 注意事項

- Tab5のWiFiはESP32-C6で処理されるため、`WiFi.mode(WIFI_OFF)` は使用不可
- SwitchBot API v1.1はHMAC-SHA256署名が必要
- NTP時刻同期が必要（API認証のタイムスタンプ用）

## ライセンス

MIT License

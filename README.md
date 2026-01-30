# SwitchBot Bulb Controller for M5Stack Tab5

M5Stack Tab5 (ESP32-P4) で SwitchBot スマート電球を制御するアプリケーション。

## 機能

- **電球制御**: 4つのSwitchBot電球のON/OFF、明るさ調整
- **温湿度表示**: SwitchBot温湿度計のデータをヘッダーに表示（60秒ごとに更新）
- **省電力モード**: 30秒間操作がないと画面オフ、タッチで復帰
- **タッチUI**: 直感的なタッチ操作によるスライダー・ボタン

## ハードウェア

- M5Stack Tab5 (ESP32-P4 + ESP32-C6)
- SwitchBot Color Bulb（最大4個）
- SwitchBot 温湿度計（オプション）

## セットアップ

### 1. SwitchBot API トークン・シークレットの取得

SwitchBotアプリから認証情報を取得します:

1. SwitchBotアプリを開く
2. **プロフィール** → **設定** → **アプリバージョン** を10回タップ（開発者モード有効化）
3. **開発者向けオプション** が表示される
4. **トークン** と **シークレットキー** をコピー

> 参考: [SwitchBot API v1.1 公式ドキュメント](https://github.com/OpenWonderLabs/SwitchBotAPI)

### 2. secrets.h の作成

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

### 3. デバイスIDの取得

デバイスIDはSwitchBot APIで取得します。以下のcurlコマンドで確認できます:

```bash
# 認証ヘッダーを生成してデバイス一覧を取得
TOKEN="your-token"
SECRET="your-secret"
NONCE=$(uuidgen)
T=$(date +%s000)
SIGN=$(echo -n "${TOKEN}${T}${NONCE}" | openssl dgst -sha256 -hmac "${SECRET}" -binary | base64)

curl -s "https://api.switch-bot.com/v1.1/devices" \
  -H "Authorization: ${TOKEN}" \
  -H "t: ${T}" \
  -H "nonce: ${NONCE}" \
  -H "sign: ${SIGN}" | jq
```

> 参考: [SwitchBot API - Get Device List](https://github.com/OpenWonderLabs/SwitchBotAPI#get-device-list)

### 4. devices.h の編集

`include/devices.h` で取得したデバイスIDと名前を設定:

```cpp
inline BulbDevice bulbs[NUM_BULBS] = {
    {"DEVICE_ID_1", "電球1", false, 100},
    {"DEVICE_ID_2", "電球2", false, 100},
    {"DEVICE_ID_3", "電球3", false, 100},
    {"DEVICE_ID_4", "電球4", false, 100}
};

inline MeterDevice meter = {"METER_DEVICE_ID", "温湿度計", 0.0f, 0, false};
```

### 5. ビルド & アップロード

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

## Tab5 ビルド環境のトラブルシューティング

### esptool エラー（click ライブラリの互換性問題）

ビルド時に以下のエラーが出る場合:

```
TypeError: ParamType.get_metavar() missing 1 required positional argument: 'ctx'
*** [.pio/build/m5stack-tab5/bootloader.bin] Error 1
```

**原因**: PlatformIOのPython環境にある `click` ライブラリが新しすぎて `esptool` と互換性がない

**解決策**:
```bash
~/.platformio/penv/bin/pip install 'click<8.2'
```

### フラッシュサイズ不足（日本語フォント使用時）

日本語フォント（lgfxJapanGothicなど）を使用するとフラッシュ容量を超える場合:

```
Error: The program size (1362473 bytes) is greater than maximum allowed (1310720 bytes)
```

**解決策**: `platformio.ini` でパーティションを変更:

```ini
board_build.partitions = huge_app.csv
```

### Tab5 の WiFi アーキテクチャ

Tab5は以下の構成になっています:

```
ESP32-P4 (メインCPU) ←--SDIO--→ ESP32-C6 (WiFi/BT)
```

- WiFiはESP32-C6で処理され、ESP-Hostedフレームワークで通信
- `WiFi.mode(WIFI_OFF)` で完全オフにすると再初期化不可
- `WiFi.disconnect()` / `WiFi.reconnect()` のみ使用可能
- WiFiピン設定が必要: `WiFi.setPins(CLK, CMD, D0, D1, D2, D3, RST)`

### platformio.ini の設定例

```ini
[env:m5stack-tab5]
platform = https://github.com/pioarduino/platform-espressif32.git#54.03.21
board = esp32-p4-evboard
board_build.mcu = esp32p4
framework = arduino
board_build.partitions = huge_app.csv

build_flags =
    -DBOARD_HAS_PSRAM
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1

lib_deps =
    https://github.com/m5stack/M5Unified.git
    https://github.com/m5stack/M5GFX.git
```

## 注意事項

- Tab5のWiFiはESP32-C6で処理されるため、`WiFi.mode(WIFI_OFF)` は使用不可
- SwitchBot API v1.1はHMAC-SHA256署名が必要
- NTP時刻同期が必要（API認証のタイムスタンプ用）

## ライセンス

MIT License

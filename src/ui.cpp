#include "ui.h"
#include "switchbot_api.h"
#include "devices.h"

// 画面サイズ
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// UIレイアウト定数
#define HEADER_HEIGHT 80
#define PANEL_MARGIN 20
#define PANEL_WIDTH ((SCREEN_WIDTH - PANEL_MARGIN * 5) / 4)
#define PANEL_HEIGHT (SCREEN_HEIGHT - HEADER_HEIGHT - PANEL_MARGIN * 2)
#define BUTTON_HEIGHT 60
#define SLIDER_HEIGHT 40
#define SLIDER_MARGIN 30

// 色定義
#define COLOR_BG 0x1082           // 暗い青
#define COLOR_PANEL 0x2945        // パネル背景
#define COLOR_HEADER 0x001F       // 青
#define COLOR_ON 0x07E0           // 緑
#define COLOR_OFF 0xF800          // 赤
#define COLOR_SLIDER_BG 0x4208    // スライダー背景
#define COLOR_SLIDER_FG 0xFFE0    // スライダー前景（黄色）
#define COLOR_TEXT 0xFFFF         // 白
#define COLOR_DISABLED 0x6B4D     // グレー

// タッチ状態
static int activeSlider = -1;     // ドラッグ中のスライダーインデックス
static unsigned long lastApiCall = 0;
#define API_DEBOUNCE_MS 500       // API呼び出しのデバウンス時間

// 画面オフ機能
static unsigned long lastTouchTime = 0;
static bool screenOff = false;
#define SCREEN_OFF_TIMEOUT_MS 60000  // 1分間操作がなければ画面オフ

// 状態取得機能
static unsigned long lastStatusUpdate = 0;
static bool operationOccurred = false;  // 操作が行われたかどうか
#define STATUS_UPDATE_INTERVAL_MS 10000  // 操作後10秒ごとに状態取得

// パネルの座標計算
static int getPanelX(int index) {
    return PANEL_MARGIN + index * (PANEL_WIDTH + PANEL_MARGIN);
}

static int getPanelY() {
    return HEADER_HEIGHT + PANEL_MARGIN;
}

// ボタンの座標計算
static int getButtonX(int index) {
    return getPanelX(index) + 20;
}

static int getButtonY() {
    return getPanelY() + 60;
}

static int getButtonWidth() {
    return PANEL_WIDTH - 40;
}

// スライダーの座標計算
static int getSliderX(int index) {
    return getPanelX(index) + SLIDER_MARGIN;
}

static int getSliderY() {
    return getButtonY() + BUTTON_HEIGHT + 40;
}

static int getSliderWidth() {
    return PANEL_WIDTH - SLIDER_MARGIN * 2;
}

// ヘッダー描画
static void drawHeader() {
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.setTextDatum(ML_DATUM);
    M5.Display.drawString("SwitchBot Controller", 20, HEADER_HEIGHT / 2);

    // 温湿度表示（右側）
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextDatum(MR_DATUM);
    if (meter.valid) {
        char tempHumStr[32];
        snprintf(tempHumStr, sizeof(tempHumStr), "%.1fC  %d%%", meter.temperature, meter.humidity);
        M5.Display.drawString(tempHumStr, SCREEN_WIDTH - 20, HEADER_HEIGHT / 2);
    } else {
        M5.Display.drawString("--C  --%", SCREEN_WIDTH - 20, HEADER_HEIGHT / 2);
    }
}

// 電球パネル描画
static void drawBulbPanel(int index) {
    BulbDevice& bulb = bulbs[index];
    int x = getPanelX(index);
    int y = getPanelY();
    bool enabled = !bulb.deviceId.isEmpty();

    // パネル背景
    M5.Display.fillRoundRect(x, y, PANEL_WIDTH, PANEL_HEIGHT, 10, COLOR_PANEL);

    // 電球名（日本語フォント）
    M5.Display.setFont(&fonts::lgfxJapanGothic_16);
    M5.Display.setTextColor(enabled ? COLOR_TEXT : COLOR_DISABLED);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString(bulb.name.c_str(), x + PANEL_WIDTH / 2, y + 25);

    // ON/OFFボタン
    int btnX = getButtonX(index);
    int btnY = getButtonY();
    int btnW = getButtonWidth();

    uint16_t btnColor;
    if (!enabled) {
        btnColor = COLOR_DISABLED;
    } else {
        btnColor = bulb.powerState ? COLOR_ON : COLOR_OFF;
    }

    M5.Display.fillRoundRect(btnX, btnY, btnW, BUTTON_HEIGHT, 8, btnColor);
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.drawString(bulb.powerState ? "ON" : "OFF", btnX + btnW / 2, btnY + BUTTON_HEIGHT / 2);

    // スライダー背景
    int sliderX = getSliderX(index);
    int sliderY = getSliderY();
    int sliderW = getSliderWidth();

    M5.Display.fillRoundRect(sliderX, sliderY, sliderW, SLIDER_HEIGHT, 5, COLOR_SLIDER_BG);

    // スライダー値
    if (enabled && bulb.powerState) {
        int fillW = (sliderW * bulb.brightness) / 100;
        M5.Display.fillRoundRect(sliderX, sliderY, fillW, SLIDER_HEIGHT, 5, COLOR_SLIDER_FG);
    }

    // スライダーハンドル
    if (enabled) {
        int handleX = sliderX + (sliderW * bulb.brightness) / 100 - 10;
        M5.Display.fillCircle(handleX + 10, sliderY + SLIDER_HEIGHT / 2, 15, COLOR_TEXT);
    }

    // 明るさ表示
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextColor(enabled ? COLOR_TEXT : COLOR_DISABLED);
    char brightnessStr[16];
    if (enabled && bulb.powerState) {
        snprintf(brightnessStr, sizeof(brightnessStr), "%d%%", bulb.brightness);
    } else {
        strcpy(brightnessStr, bulb.powerState ? "100%" : "OFF");
    }
    M5.Display.drawString(brightnessStr, x + PANEL_WIDTH / 2, sliderY + SLIDER_HEIGHT + 30);

    // デバイスID未設定の警告
    if (!enabled) {
        M5.Display.setFont(&fonts::lgfxJapanGothic_12);
        M5.Display.setTextColor(COLOR_DISABLED);
        M5.Display.drawString("(ID未設定)", x + PANEL_WIDTH / 2, sliderY + SLIDER_HEIGHT + 50);
    }
}

// UI全体描画
static void drawUI() {
    M5.Display.fillScreen(COLOR_BG);
    drawHeader();
    for (int i = 0; i < NUM_BULBS; i++) {
        drawBulbPanel(i);
    }
}

// ボタンタッチ判定
static int checkButtonTouch(int tx, int ty) {
    int btnY = getButtonY();

    if (ty < btnY || ty > btnY + BUTTON_HEIGHT) {
        return -1;
    }

    for (int i = 0; i < NUM_BULBS; i++) {
        int btnX = getButtonX(i);
        int btnW = getButtonWidth();
        if (tx >= btnX && tx <= btnX + btnW) {
            return i;
        }
    }
    return -1;
}

// スライダータッチ判定
static int checkSliderTouch(int tx, int ty) {
    int sliderY = getSliderY();

    if (ty < sliderY - 20 || ty > sliderY + SLIDER_HEIGHT + 20) {
        return -1;
    }

    for (int i = 0; i < NUM_BULBS; i++) {
        int sliderX = getSliderX(i);
        int sliderW = getSliderWidth();
        if (tx >= sliderX - 20 && tx <= sliderX + sliderW + 20) {
            return i;
        }
    }
    return -1;
}

// スライダー値を計算
static int calculateSliderValue(int index, int tx) {
    int sliderX = getSliderX(index);
    int sliderW = getSliderWidth();

    int value = ((tx - sliderX) * 100) / sliderW;
    return constrain(value, 1, 100);
}

void uiInit() {
    M5.Display.setRotation(1);  // 横向き
    lastTouchTime = millis();   // タッチ時刻初期化
    drawUI();
}

void uiUpdate() {
    M5.update();

    auto touch = M5.Touch.getDetail();
    unsigned long now = millis();

    // 画面オフ中にタッチされたら復帰
    if (screenOff) {
        if (touch.wasPressed()) {
            screenOff = false;
            lastTouchTime = now;
            M5.Display.setBrightness(128);
            // 画面復帰時に即時状態取得
            uiRefreshAllBulbStatus();
        }
        return;  // 画面オフ中は他の処理をスキップ
    }

    // 1分間操作がなければ画面オフ
    if (now - lastTouchTime >= SCREEN_OFF_TIMEOUT_MS) {
        screenOff = true;
        M5.Display.setBrightness(0);
        return;
    }

    if (touch.wasPressed()) {
        lastTouchTime = now;  // タッチ時刻更新
        int tx = touch.x;
        int ty = touch.y;

        // ボタンタッチ判定
        int btnIndex = checkButtonTouch(tx, ty);
        if (btnIndex >= 0 && !bulbs[btnIndex].deviceId.isEmpty()) {
            // 電源トグル
            bulbs[btnIndex].powerState = !bulbs[btnIndex].powerState;
            drawBulbPanel(btnIndex);

            // API呼び出し
            switchbotBulbPower(bulbs[btnIndex].deviceId, bulbs[btnIndex].powerState);
            lastApiCall = now;
            operationOccurred = true;  // 操作フラグを設定
            lastStatusUpdate = now;    // タイマーをリセット
            return;
        }

        // スライダータッチ開始
        int sliderIndex = checkSliderTouch(tx, ty);
        if (sliderIndex >= 0 && !bulbs[sliderIndex].deviceId.isEmpty() && bulbs[sliderIndex].powerState) {
            activeSlider = sliderIndex;
            bulbs[sliderIndex].brightness = calculateSliderValue(sliderIndex, tx);
            drawBulbPanel(sliderIndex);
        }
    }

    if (touch.isHolding() && activeSlider >= 0) {
        lastTouchTime = now;  // タッチ時刻更新
        int tx = touch.x;
        int newBrightness = calculateSliderValue(activeSlider, tx);

        if (newBrightness != bulbs[activeSlider].brightness) {
            bulbs[activeSlider].brightness = newBrightness;
            drawBulbPanel(activeSlider);
        }
    }

    if (touch.wasReleased() && activeSlider >= 0) {
        lastTouchTime = now;  // タッチ時刻更新
        // スライダー操作終了、API呼び出し
        if (now - lastApiCall >= API_DEBOUNCE_MS) {
            switchbotBulbBrightness(bulbs[activeSlider].deviceId, bulbs[activeSlider].brightness);
            lastApiCall = now;
            operationOccurred = true;  // 操作フラグを設定
            lastStatusUpdate = now;    // タイマーをリセット
        }
        activeSlider = -1;
    }

    // 操作後10秒ごとに状態取得
    if (operationOccurred && (now - lastStatusUpdate >= STATUS_UPDATE_INTERVAL_MS)) {
        uiRefreshAllBulbStatus();
        operationOccurred = false;  // 一度取得したらフラグをクリア
    }
}

void uiUpdateBulbState(int index, bool powerState, int brightness) {
    if (index < 0 || index >= NUM_BULBS) return;

    bulbs[index].powerState = powerState;
    bulbs[index].brightness = brightness;
    drawBulbPanel(index);
}

void uiUpdateMeter() {
    drawHeader();
}

void uiRefreshAllBulbStatus() {
    Serial.println("Refreshing all bulb status...");
    for (int i = 0; i < NUM_BULBS; i++) {
        if (!bulbs[i].deviceId.isEmpty()) {
            bool powerState;
            int brightness;
            if (switchbotBulbStatus(bulbs[i].deviceId, powerState, brightness)) {
                bulbs[i].powerState = powerState;
                bulbs[i].brightness = brightness;
                drawBulbPanel(i);
                Serial.printf("Bulb %d: power=%s, brightness=%d\n", i, powerState ? "on" : "off", brightness);
            }
        }
    }
    lastStatusUpdate = millis();
}

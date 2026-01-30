#include "ui.h"
#include "switchbot_api.h"
#include "devices.h"

// バックライト制御用
#define BACKLIGHT_MAX 255
#define BACKLIGHT_DIM 0

static void setBacklight(uint8_t brightness)
{
    M5.Display.setBrightness(brightness);
}

// 画面サイズ
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// UIレイアウト定数
#define HEADER_HEIGHT 80
#define PANEL_MARGIN 20
#define PANEL_WIDTH ((SCREEN_WIDTH - PANEL_MARGIN * 5) / 4)
#define PANEL_HEIGHT (SCREEN_HEIGHT - HEADER_HEIGHT - PANEL_MARGIN * 2)
#define BUTTON_HEIGHT 150
#define SLIDER_HEIGHT 40
#define SLIDER_MARGIN 30

// 色定義
#define COLOR_BG 0x1082
#define COLOR_PANEL 0x2945
#define COLOR_HEADER 0x001F
#define COLOR_ON 0x07E0
#define COLOR_OFF 0xF800
#define COLOR_SLIDER_BG 0x4208
#define COLOR_SLIDER_FG 0xFFE0
#define COLOR_TEXT 0xFFFF
#define COLOR_DISABLED 0x6B4D

// パネル用スプライト（1つを再利用）
static M5Canvas panelSprite(&M5.Display);

// スライダードラッグ状態（自前実装）
static int activeSlider = -1;
static int sliderStartX = 0;
static bool sliderDragged = false;
#define SLIDER_DRAG_THRESHOLD 20

// API呼び出し制御
static unsigned long lastApiCall = 0;
#define API_DEBOUNCE_MS 500

// ボタン長押し検出用
static int pressedButtonIndex = -1;
static unsigned long buttonPressStartTime = 0;
#define BUTTON_LONG_PRESS_MS 500

// 画面オフ機能
static unsigned long lastTouchTime = 0;
static bool screenDimmed = false;
static unsigned long wakeUpTime = 0;
#define SCREEN_OFF_TIMEOUT_MS 30000
#define WAKE_UP_IGNORE_MS 300

// 状態取得機能
static unsigned long lastStatusUpdate = 0;
static bool operationOccurred = false;
#define STATUS_UPDATE_INTERVAL_MS 10000

// 遅延OFF機能
static int pendingOffBulbIndex = -1;
static unsigned long pendingOffStartTime = 0;
#define BULB_OFF_DELAY_MS 5000

// 座標計算
static int getPanelX(int index) { return PANEL_MARGIN + index * (PANEL_WIDTH + PANEL_MARGIN); }
static int getPanelY() { return HEADER_HEIGHT + PANEL_MARGIN; }
static int getButtonX(int index) { return getPanelX(index) + 20; }
static int getButtonY() { return getPanelY() + 60; }
static int getButtonWidth() { return PANEL_WIDTH - 40; }
static int getSliderX(int index) { return getPanelX(index) + SLIDER_MARGIN; }
static int getSliderY() { return getButtonY() + BUTTON_HEIGHT + 40; }
static int getSliderWidth() { return PANEL_WIDTH - SLIDER_MARGIN * 2; }

// ヘッダー描画（直接描画）
static void drawHeader()
{
    M5.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COLOR_HEADER);
    M5.Display.setTextColor(COLOR_TEXT);
    M5.Display.setTextSize(1);
    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextDatum(ML_DATUM);
    M5.Display.drawString("SwitchBot Controller", 20, HEADER_HEIGHT / 2);

    M5.Display.setFont(&fonts::FreeSansBold18pt7b);
    M5.Display.setTextDatum(MR_DATUM);
    if (meter.valid)
    {
        char tempHumStr[32];
        snprintf(tempHumStr, sizeof(tempHumStr), "%.1fC  %d%%", meter.temperature, meter.humidity);
        M5.Display.drawString(tempHumStr, SCREEN_WIDTH - 20, HEADER_HEIGHT / 2);
    }
    else
    {
        M5.Display.drawString("--C  --%", SCREEN_WIDTH - 20, HEADER_HEIGHT / 2);
    }
}

// 電球パネル描画（スプライト使用）
static void drawBulbPanel(int index)
{
    BulbDevice &bulb = bulbs[index];
    int panelX = getPanelX(index);
    int panelY = getPanelY();
    bool enabled = !bulb.deviceId.isEmpty();

    // スプライトに描画
    panelSprite.fillSprite(COLOR_PANEL);
    panelSprite.fillRoundRect(0, 0, PANEL_WIDTH, PANEL_HEIGHT, 10, COLOR_PANEL);

    // 電球名
    panelSprite.setTextSize(1);
    panelSprite.setFont(&fonts::lgfxJapanGothic_28);
    panelSprite.setTextColor(enabled ? COLOR_TEXT : COLOR_DISABLED);
    panelSprite.setTextDatum(MC_DATUM);
    panelSprite.drawString(bulb.name.c_str(), PANEL_WIDTH / 2, 35);

    // ON/OFFボタン（パネル内座標）
    int btnX = 20;
    int btnY = 60;
    int btnW = PANEL_WIDTH - 40;
    uint16_t btnColor = !enabled ? COLOR_DISABLED : (bulb.powerState ? COLOR_ON : COLOR_OFF);

    panelSprite.fillRoundRect(btnX, btnY, btnW, BUTTON_HEIGHT, 8, btnColor);
    panelSprite.setFont(&fonts::FreeSansBold18pt7b);
    panelSprite.setTextColor(COLOR_TEXT);
    panelSprite.drawString(bulb.powerState ? "ON" : "OFF", btnX + btnW / 2, btnY + BUTTON_HEIGHT / 2);

    // スライダー（パネル内座標）
    int sliderX = SLIDER_MARGIN;
    int sliderY = btnY + BUTTON_HEIGHT + 40;
    int sliderW = PANEL_WIDTH - SLIDER_MARGIN * 2;

    panelSprite.fillRoundRect(sliderX, sliderY, sliderW, SLIDER_HEIGHT, 5, COLOR_SLIDER_BG);

    if (enabled && bulb.powerState)
    {
        int fillW = (sliderW * bulb.brightness) / 100;
        panelSprite.fillRoundRect(sliderX, sliderY, fillW, SLIDER_HEIGHT, 5, COLOR_SLIDER_FG);
    }

    if (enabled)
    {
        int handleX = sliderX + (sliderW * bulb.brightness) / 100;
        panelSprite.fillCircle(handleX, sliderY + SLIDER_HEIGHT / 2, 15, COLOR_TEXT);
    }

    // 明るさ表示
    panelSprite.setFont(&fonts::FreeSansBold12pt7b);
    panelSprite.setTextColor(enabled ? COLOR_TEXT : COLOR_DISABLED);
    char brightnessStr[16];
    if (enabled && bulb.powerState)
    {
        snprintf(brightnessStr, sizeof(brightnessStr), "%d%%", bulb.brightness);
    }
    else
    {
        strcpy(brightnessStr, bulb.powerState ? "100%" : "OFF");
    }
    panelSprite.drawString(brightnessStr, PANEL_WIDTH / 2, sliderY + SLIDER_HEIGHT + 40);

    if (!enabled)
    {
        panelSprite.setFont(&fonts::lgfxJapanGothic_20);
        panelSprite.setTextColor(COLOR_DISABLED);
        panelSprite.drawString("(ID未設定)", PANEL_WIDTH / 2, sliderY + SLIDER_HEIGHT + 70);
    }

    // スプライトを画面に転送
    panelSprite.pushSprite(panelX, panelY);
}

// UI全体描画
static void drawUI()
{
    M5.Display.fillScreen(COLOR_BG);
    drawHeader();
    for (int i = 0; i < NUM_BULBS; i++)
    {
        drawBulbPanel(i);
    }
}

// ボタンタッチ判定
static int checkButtonTouch(int tx, int ty)
{
    int btnY = getButtonY();
    if (ty < btnY || ty > btnY + BUTTON_HEIGHT)
        return -1;

    for (int i = 0; i < NUM_BULBS; i++)
    {
        int btnX = getButtonX(i);
        int btnW = getButtonWidth();
        if (tx >= btnX && tx <= btnX + btnW)
            return i;
    }
    return -1;
}

// スライダータッチ判定
static int checkSliderTouch(int tx, int ty)
{
    int sliderY = getSliderY();
    if (ty < sliderY - 20 || ty > sliderY + SLIDER_HEIGHT + 20)
        return -1;

    for (int i = 0; i < NUM_BULBS; i++)
    {
        int sliderX = getSliderX(i);
        int sliderW = getSliderWidth();
        if (tx >= sliderX - 20 && tx <= sliderX + sliderW + 20)
            return i;
    }
    return -1;
}

// スライダー値を計算
static int calculateSliderValue(int index, int tx)
{
    int sliderX = getSliderX(index);
    int sliderW = getSliderWidth();
    int value = ((tx - sliderX) * 100) / sliderW;
    return constrain(value, 1, 100);
}

void uiInit()
{
    M5.Display.setRotation(1);

    // パネル用スプライト作成（1パネル分のみ）
    panelSprite.createSprite(PANEL_WIDTH, PANEL_HEIGHT);

    lastTouchTime = millis();
    drawUI();
}

void uiUpdate()
{
    M5.update();

    auto touch = M5.Touch.getDetail();
    unsigned long now = millis();

    // 減光中にタッチされたら復帰
    if (screenDimmed)
    {
        if (touch.wasPressed())
        {
            screenDimmed = false;
            lastTouchTime = now;
            wakeUpTime = now;
            setBacklight(BACKLIGHT_MAX);
            uiRefreshAllBulbStatus();
        }
        return;
    }

    // 復帰直後はタッチを無視
    if (wakeUpTime > 0 && (now - wakeUpTime < WAKE_UP_IGNORE_MS))
        return;
    wakeUpTime = 0;

    // 画面減光
    if (now - lastTouchTime >= SCREEN_OFF_TIMEOUT_MS)
    {
        if (!screenDimmed)
        {
            screenDimmed = true;
            setBacklight(BACKLIGHT_DIM);
        }
        return;
    }

    int tx = touch.x;
    int ty = touch.y;

    // タッチ開始
    if (touch.wasPressed())
    {
        lastTouchTime = now;

        // ボタンタッチ判定
        int btnIndex = checkButtonTouch(tx, ty);
        if (btnIndex >= 0 && !bulbs[btnIndex].deviceId.isEmpty())
        {
            pressedButtonIndex = btnIndex;
            buttonPressStartTime = now;
        }

        // スライダータッチ開始
        int sliderIndex = checkSliderTouch(tx, ty);
        if (sliderIndex >= 0 && !bulbs[sliderIndex].deviceId.isEmpty() && bulbs[sliderIndex].powerState)
        {
            activeSlider = sliderIndex;
            sliderStartX = tx;
            sliderDragged = false;
            Serial.printf("[DEBUG] Slider touch start: index=%d, startX=%d\n", sliderIndex, tx);
        }
    }

    // タッチ中（isPressed で継続的に追跡）
    if (activeSlider >= 0 && touch.isPressed())
    {
        lastTouchTime = now;
        int dragDistance = abs(tx - sliderStartX);

        if (dragDistance >= SLIDER_DRAG_THRESHOLD)
        {
            sliderDragged = true;
            int newBrightness = calculateSliderValue(activeSlider, tx);
            if (newBrightness != bulbs[activeSlider].brightness)
            {
                bulbs[activeSlider].brightness = newBrightness;
                drawBulbPanel(activeSlider);
                Serial.printf("[DEBUG] Dragging: x=%d, brightness=%d\n", tx, newBrightness);
            }
        }
    }

    // タッチリリース
    if (touch.wasReleased())
    {
        lastTouchTime = now;

        // スライダー操作完了
        if (activeSlider >= 0)
        {
            Serial.printf("[DEBUG] Released: dragged=%d\n", sliderDragged);
            if (sliderDragged && (now - lastApiCall >= API_DEBOUNCE_MS))
            {
                switchbotBulbBrightness(bulbs[activeSlider].deviceId, bulbs[activeSlider].brightness);
                lastApiCall = now;
                operationOccurred = true;
                lastStatusUpdate = now;
                Serial.printf("[DEBUG] API called: brightness=%d\n", bulbs[activeSlider].brightness);
            }
            activeSlider = -1;
            sliderDragged = false;
        }

        // ボタンリリース
        if (pressedButtonIndex >= 0)
        {
            pressedButtonIndex = -1;
        }
    }

    // ボタン長押し判定
    if (touch.isHolding() && pressedButtonIndex >= 0)
    {
        lastTouchTime = now;
        int btnIndex = checkButtonTouch(tx, ty);
        if (btnIndex == pressedButtonIndex && (now - buttonPressStartTime >= BUTTON_LONG_PRESS_MS))
        {
            bool wasOn = bulbs[pressedButtonIndex].powerState;

            if (pendingOffBulbIndex == pressedButtonIndex)
                pendingOffBulbIndex = -1;

            bulbs[pressedButtonIndex].powerState = !bulbs[pressedButtonIndex].powerState;
            drawBulbPanel(pressedButtonIndex);

            if (wasOn)
            {
                pendingOffBulbIndex = pressedButtonIndex;
                pendingOffStartTime = now;
            }
            else
            {
                switchbotBulbPower(bulbs[pressedButtonIndex].deviceId, true);
                lastApiCall = now;
                operationOccurred = true;
                lastStatusUpdate = now;
            }
            pressedButtonIndex = -1;
        }
    }

    // 遅延OFF処理
    if (pendingOffBulbIndex >= 0 && (now - pendingOffStartTime >= BULB_OFF_DELAY_MS))
    {
        int idx = pendingOffBulbIndex;
        pendingOffBulbIndex = -1;
        if (!bulbs[idx].powerState)
        {
            switchbotBulbPower(bulbs[idx].deviceId, false);
            lastApiCall = now;
            operationOccurred = true;
            lastStatusUpdate = now;
        }
    }

    // 操作後の状態取得
    if (operationOccurred && (now - lastStatusUpdate >= STATUS_UPDATE_INTERVAL_MS))
    {
        uiRefreshAllBulbStatus();
        operationOccurred = false;
    }
}

void uiUpdateBulbState(int index, bool powerState, int brightness)
{
    if (index < 0 || index >= NUM_BULBS)
        return;

    bulbs[index].powerState = powerState;
    bulbs[index].brightness = brightness;
    drawBulbPanel(index);
}

void uiUpdateMeter()
{
    drawHeader();
}

void uiRefreshAllBulbStatus()
{
    Serial.println("Refreshing all bulb status...");
    for (int i = 0; i < NUM_BULBS; i++)
    {
        if (!bulbs[i].deviceId.isEmpty())
        {
            bool powerState;
            int brightness;
            if (switchbotBulbStatus(bulbs[i].deviceId, powerState, brightness))
            {
                bulbs[i].powerState = powerState;
                bulbs[i].brightness = brightness;
                drawBulbPanel(i);
                Serial.printf("Bulb %d: power=%s, brightness=%d\n", i, powerState ? "on" : "off", brightness);
            }
        }
    }
    lastStatusUpdate = millis();
}

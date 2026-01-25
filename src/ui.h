#ifndef UI_H
#define UI_H

#include <M5Unified.h>
#include "devices.h"

// UI初期化
void uiInit();

// UI描画（メインループで呼び出す）
void uiUpdate();

// 電球の状態を更新
void uiUpdateBulbState(int index, bool powerState, int brightness);

// 温湿度表示を更新
void uiUpdateMeter();

#endif // UI_H

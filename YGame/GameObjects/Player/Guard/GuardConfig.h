#pragma once

struct GuardConfig {
    float   startup = 4.0f;      // ボタンを押してからシールドが有効になるまで
    float   active = 20.0f;      // 通常ガードが有効な長さ
    float   parryStart = 3.0f;   // active 内のパリィ開始フレーム (0〜active-1)
    float   parryEnd = 5.0f;     // active 内のパリィ終了フレーム
    float   recovery = 12.0f;    // 失敗時の硬直
};

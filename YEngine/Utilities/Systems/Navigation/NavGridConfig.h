#pragma once

// C++
#include <string>

// Engine
#include <Loaders/Json/Use/AutoJson.h>

/// <summary>
/// NavGrid の設定をデータドリブン化するコンフィグクラス。
///
/// FieldScene.cpp にハードコードされていた値を JSON に移行する。
///   - グリッド範囲 (worldMin/Max X/Z)
///   - セルサイズ (cellSize)
///   - エージェント半径 (agentRadius) ← Unityの Agent Radius 相当
///   - NavMeshパス再計算間隔 (pathRefreshInterval)
///
/// 使い方:
///   NavGridConfig cfg;
///   cfg.LoadFromFile("Resources/Json/Navigation/NavGridConfig.json");
///   navGrid_.Initialize(cfg.worldMinX, cfg.worldMaxX, ...);
///
/// エディターから変更 → Rebake → 即反映 が可能。
/// </summary>
struct NavGridConfig
{
    // ── グリッド範囲（フィールドの歩行可能エリア） ──────────────────────
    float worldMinX = -100.0f;
    float worldMaxX = 100.0f;
    float worldMinZ = -100.0f;
    float worldMaxZ = 100.0f;

    // ── セルサイズ（小さいほど精度が上がるがメモリ・ベイク時間が増える） ──
    // 推奨: 0.5f〜2.0f。建物の幅(2m前後)に対して1m以下が望ましい
    float cellSize = 1.0f;

    // ── Erosion（エージェント半径） ──────────────────────────────────────
    // 障害物セルの周囲このメートル分も通行不可にする。
    // Unityの NavMeshAgent.radius に相当。
    // 敵の体の半径に合わせて設定する（細い通路を通り抜けるかどうかが変わる）
    float agentRadius = 0.5f;

    // ── A*の最大ノード探索数（重い処理の安全弁） ───────────────────────
    // 大きいほど遠くまで経路を探せるが処理が重くなる
    int maxSearchNodes = 2000;

    // ── 経路再計算間隔（秒） ─────────────────────────────────────────────
    // Chase中にプレイヤーが動いたとき何秒ごとに経路を再計算するか
    // 短いほど追跡が正確になるが処理負荷が増える
    float pathRefreshInterval = 0.3f;

    // ── JSONファイルパス ─────────────────────────────────────────────────
    static constexpr const char* kDefaultPath =
        "Resources/Json/Navigation/NavGridConfig.json";

    // ── AutoJson ────────────────────────────────────────────────────────
    // メンバ変数をAutoJsonに登録してファイル保存・読み込み・ImGui表示を自動化する
    AutoJson aj_;

    NavGridConfig()
    {
        aj_.Add("worldMinX", &worldMinX)
            .Add("worldMaxX", &worldMaxX)
            .Add("worldMinZ", &worldMinZ)
            .Add("worldMaxZ", &worldMaxZ)
            .Add("cellSize", &cellSize)
            .Add("agentRadius", &agentRadius)
            .Add("maxSearchNodes", &maxSearchNodes)
            .Add("pathRefreshInterval", &pathRefreshInterval);
    }

    // ── Public AutoJson インターフェース ─────────────────────────────────
    // ラッパーを挟まず aj_ を直接呼ぶ。SaveToFile / LoadFromFile / ShowImGui が使える。
    //   cfg.aj_.SaveToFile();
    //   cfg.aj_.LoadFromFile();
    //   cfg.aj_.ShowImGui();
};
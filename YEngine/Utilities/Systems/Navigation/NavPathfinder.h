#pragma once

// C++
#include <vector>
#include <functional>

// Math
#include "Vector3.h"

// Navigation
#include "NavGrid.h"

/// <summary>
/// グリッドNavGridを使ったA*経路探索クラス
///
/// NavGrid上でA*を実行し、ワールド座標のウェイポイント列を返す。
/// UnityのNavMeshAgent.SetDestination() に相当する。
///
/// 特徴:
///   - 8方向移動（斜め45度も通れる）
///   - ヒューリスティック: ユークリッド距離
///   - 簡易Funnel: 直線上に障害物がなければ中間ウェイポイントをスキップ
///     → A*そのままだと壁際でジグザグするのを防ぐ
///
/// 使い方:
///   NavPathfinder pf;
///   pf.SetNavGrid(&grid);
///   auto path = pf.FindPath(enemyPos, playerPos);
///   // path は Vector3 のリスト（次の目標地点の列）
/// </summary>
class NavPathfinder
{
public:
    NavPathfinder() = default;
    ~NavPathfinder() = default;

    // NavGridを設定（FieldScene初期化時に呼ぶ）
    void SetNavGrid(NavGrid* grid) { grid_ = grid; }
    const NavGrid* GetNavGrid() const { return grid_; }

    // ────────────────────────────────────────────────────────────────────
    // 経路探索
    // ────────────────────────────────────────────────────────────────────

    /// <summary>
    /// A*で from → to の経路を探索してワールド座標のウェイポイント列を返す。
    /// 経路が見つからない場合は空のvectorを返す。
    ///
    /// maxNodes: 探索ノード数の上限（重い処理を防ぐ安全弁、デフォルト2000）
    /// </summary>
    std::vector<Vector3> FindPath(const Vector3& from, const Vector3& to,
                                  int maxNodes = 2000) const;

    /// <summary>
    /// 同上。グリッド座標で受け取るバージョン（内部処理用）
    /// </summary>
    std::vector<Vector3> FindPath(const NavGrid::GridPos& fromGrid,
                                  const NavGrid::GridPos& toGrid,
                                  int maxNodes = 2000) const;

    // ────────────────────────────────────────────────────────────────────
    // Funnel（経路スムージング）
    // ────────────────────────────────────────────────────────────────────

    /// <summary>
    /// A*の出力パスに対し、視線が通るウェイポイントをスキップして
    /// 直線的な経路に変換する（簡易String-Pulling）。
    /// Unrealの Funnel Algorithm の簡略版。
    /// </summary>
    std::vector<Vector3> SmoothPath(const std::vector<Vector3>& rawPath) const;

private:
    // ────────────────────────────────────────────────────────────────────
    // A*内部
    // ────────────────────────────────────────────────────────────────────

    struct Node {
        NavGrid::GridPos pos;
        float g = 0.0f;  // スタートからのコスト
        float h = 0.0f;  // ゴールまでのヒューリスティック
        float f() const { return g + h; }
        int   parentIdx = -1; // 親ノードのインデックス（経路再構築用）
    };

    static float Heuristic(const NavGrid::GridPos& a, const NavGrid::GridPos& b);

    // 8方向の隣接セル（斜め移動のコスト = √2 ≈ 1.414）
    static constexpr int kDirCount = 8;
    static constexpr int kDirX[8] = { 1, -1, 0,  0, 1, -1,  1, -1 };
    static constexpr int kDirZ[8] = { 0,  0, 1, -1, 1,  1, -1, -1 };
    static constexpr float kDirCost[8] = {
        1.0f, 1.0f, 1.0f, 1.0f,       // 軸方向
        1.414f, 1.414f, 1.414f, 1.414f // 斜め方向
    };

    NavGrid* grid_ = nullptr;
};

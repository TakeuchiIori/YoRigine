#pragma once

// C++
#include <vector>
#include <cstdint>

// Math
#include "Vector3.h"
#include "Shape/AABB.h"

// Engine
#include <Collision/Core/CollisionTypeIdDef.h>

/// <summary>
/// グリッドベースの歩行可能マップ
///
/// フィールド全体を cellSize_ × cellSize_ のセルに分割し、
/// kNavObstacle コライダーが重なるセルを「通行不可」としてベイクする。
///
/// Unity の Navigation Static / NavMesh Bake に相当するが、
/// ボクセル変換なしでAABBを直接グリッドに投影するため実装が単純。
///
/// 使い方:
///   NavGrid grid;
///   grid.Initialize(-50, 50, -50, 50, 1.0f);   // 範囲とセルサイズ設定
///   grid.Bake(objectManager);                    // シーンロード後に1回呼ぶ
///   bool ok = grid.IsWalkable(3, 7);             // セル単位
///   bool ok = grid.IsWalkableWorld({5.0f,0,3.f}); // ワールド座標
/// </summary>
class ObjectManager;

class NavGrid
{
public:
    // ────────────────────────────────────────────────────────────────────
    // セル情報
    // ────────────────────────────────────────────────────────────────────
    struct Cell {
        bool walkable = true; // 通行可能か
    };

    // グリッド座標（整数）
    struct GridPos {
        int x = 0;
        int z = 0;

        bool operator==(const GridPos& o) const { return x == o.x && z == o.z; }
    };

    NavGrid() = default;
    ~NavGrid() = default;

    // ────────────────────────────────────────────────────────────────────
    // 初期化 / ベイク
    // ────────────────────────────────────────────────────────────────────

    /// <summary>
    /// グリッド範囲とセルサイズを設定する（ベイク前に必ず呼ぶ）
    /// </summary>
    /// <param name="worldMinX">ワールドX最小値</param>
    /// <param name="worldMaxX">ワールドX最大値</param>
    /// <param name="worldMinZ">ワールドZ最小値</param>
    /// <param name="worldMaxZ">ワールドZ最大値</param>
    /// <param name="cellSize">1セルのワールド単位サイズ（推奨: 1.0f）</param>
    void Initialize(float worldMinX, float worldMaxX,
                    float worldMinZ, float worldMaxZ,
                    float cellSize = 1.0f);

    /// <summary>
    /// ObjectManager の kNavObstacle コライダーをグリッドに投影して通行不可セルを確定する。
    /// シーンロード後・オブジェクト配置変更後に呼ぶ（毎フレーム不要）。
    /// </summary>
    void Bake(ObjectManager* objectManager);

    /// <summary>
    /// 単一のAABBを障害物としてグリッドに追加する（Bake の部分更新用）
    /// </summary>
    void MarkObstacle(const AABB& worldAABB, bool obstacle = true);

    /// <summary>
    /// グリッドを全セル歩行可能にリセットする
    /// </summary>
    void Reset();

    // ────────────────────────────────────────────────────────────────────
    // 座標変換
    // ────────────────────────────────────────────────────────────────────

    /// ワールド座標 → グリッド座標（範囲外はクランプ）
    GridPos WorldToGrid(const Vector3& worldPos) const;

    /// グリッド座標 → ワールド座標（セル中心）
    Vector3 GridToWorld(const GridPos& gp) const;

    // ────────────────────────────────────────────────────────────────────
    // クエリ
    // ────────────────────────────────────────────────────────────────────

    bool IsWalkable(int gx, int gz) const;
    bool IsWalkable(const GridPos& gp) const { return IsWalkable(gp.x, gp.z); }
    bool IsWalkableWorld(const Vector3& worldPos) const;

    /// グリッドが初期化済みか
    bool IsInitialized() const { return !cells_.empty(); }

    // ────────────────────────────────────────────────────────────────────
    // XZ平面レイキャスト（視線遮蔽判定に使う）
    // ────────────────────────────────────────────────────────────────────

    /// <summary>
    /// from → to へのXZ平面レイが kNavObstacle セルを通過するかを判定する。
    /// 通過しない（＝視線が通る）なら true を返す。
    /// Bresenhamアルゴリズムで実装。
    /// </summary>
    bool HasLineOfSight(const Vector3& from, const Vector3& to) const;


public:
    // ────────────────────────────────────────────────────────────────────
    // アクセッサ
    // ────────────────────────────────────────────────────────────────────

    int GetWidthCells()  const { return widthCells_; }
    int GetDepthCells()  const { return depthCells_; }
    float GetCellSize()  const { return cellSize_; }
    float GetWorldMinX() const { return worldMinX_; }
    float GetWorldMinZ() const { return worldMinZ_; }

    // 全セルを取得
    const std::vector<Cell>& GetCells() const { return cells_; }

    // エージェント半径（Erosionで使う）
    void  SetAgentRadius(float r) { agentRadius_ = r; }
    float GetAgentRadius() const  { return agentRadius_; }

private:
    // ────────────────────────────────────────────────────────────────────
    // 内部ユーティリティ
    // ────────────────────────────────────────────────────────────────────

    int  CellIndex(int gx, int gz) const { return gz * widthCells_ + gx; }
    bool InBounds (int gx, int gz) const {
        return gx >= 0 && gx < widthCells_ && gz >= 0 && gz < depthCells_;
    }

    /// AABBが重なるセル範囲を計算してコールバックを呼ぶ
    template<typename Fn>
    void ForEachOverlappingCell(const AABB& worldAABB, Fn fn) const;

    /// Erosion：障害物セルの周囲 agentRadius_ セル分も通行不可にする
    void ApplyErosion();

private:
    // ────────────────────────────────────────────────────────────────────
    // メンバ変数
    // ────────────────────────────────────────────────────────────────────

    std::vector<Cell> cells_;      // [z * widthCells_ + x]

    float worldMinX_  = -50.0f;
    float worldMinZ_  = -50.0f;
    float worldMaxX_  =  50.0f;
    float worldMaxZ_  =  50.0f;
    float cellSize_   =   1.0f;
    float agentRadius_=   0.5f;   // Erosionで使うエージェント半径（メートル）

    int widthCells_   = 0;        // X方向セル数
    int depthCells_   = 0;        // Z方向セル数
};

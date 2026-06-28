#pragma once
#include <Vector3.h>

/// <summary>
/// 視線遮蔽判定システム
/// kNavObstacle / kStaticWall が設定されたコライダーを
/// 自動的に遮蔽オブジェクトとして扱う。
///
/// 呼び出し側は「見えるか/見えないか」だけを知ればよい。
/// どのオブジェクトが遮蔽するかはこのクラスが管理する。
/// </summary>
class VisionSystem
{
public:
    // ============================================================
    // from → to の視線が障害物に遮られていないか判定する。
    // 遮られていない（見える）場合 true を返す。
    // ============================================================
    static bool HasLineOfSight(const Vector3& from, const Vector3& to);
};


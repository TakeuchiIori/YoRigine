#pragma once
// ===========================================================
// VfxGeometryParams.h
//
// ジオメトリの型 ID とパラメータ（variant）だけを持つ軽量ヘッダ。
// VfxGeometry / ProceduralMeshBase に依存しないので、
// VfxEffectAsset など「データだけ欲しい」側から安全に include できる
// （VfxGeometryTypes.h は Desc（生成関数）まで含むため循環を招く）。
// ===========================================================
#include <variant>
#include <cstdint>

namespace YoRigine {

    // ── 型 ID ─────────────────────────────────────────────────
    enum class VfxGeometryType : int
    {
        Sphere = 0,
        Cone,
        Ring,
        Plane,
        Disc,        // 塗りつぶし円（地面向き）。バフエリア/AoE/射程マーカー用
        RimCurtain,  // 円の縁に立つ縦の帯（円筒側面）。縁から上へ燃え上がる炎などに使う
        LobeCluster, // 小球(塊)を多数寄せ集めた「もくもく」。爆発煙・キノコ雲の頭/柱に使う
        // ↑ 新規は必ず末尾に追加すること（JSON は variant index を保存するため）
    };

    // ── 各ジオメトリのパラメータ（variant の要素） ────────────
    struct SphereGeomParams
    {
        float radius  = 1.5f;
        int   rings   = 18;
        int   sectors = 28;
    };

    struct ConeGeomParams
    {
        float radius   = 1.0f;
        float height   = 2.0f;
        int   segments = 24;
    };

    struct RingGeomParams
    {
        float outerRadius = 3.0f;
        float innerRatio  = 0.7f;
        int   segments    = 32;
        bool  billboard   = true; ///< false で rotation の平面に生成（地面デカール）
    };

    struct PlaneGeomParams
    {
        float size      = 1.0f;
        bool  billboard = true;
    };

    struct DiscGeomParams
    {
        float radius    = 1.0f;   ///< 単位半径（実半径 = radius * element scale）
        int   segments  = 48;     ///< 円周分割数（滑らかさ）
        bool  billboard = false;  ///< true=完全カメラ向き（毎フレームカメラを追う）。通常 false
        // 向きは度数で直接指定（quaternion を通さない・カメラ非依存の world 固定）
        float pitchDeg  = 0.0f;   ///< 地面(水平)からの起き上がり角。0=完全に地面水平 / 90=垂直
        float yawDeg    = 0.0f;   ///< 水平面内の向き（傾ける方向を回す）。放射模様なので見た目影響は小
    };

    struct RimCurtainGeomParams
    {
        float radius       = 1.0f;  ///< 単位半径（実半径 = radius * element scale）。Disc と一致させて縁に立てる
        float heightRatio  = 0.25f; ///< 立ち上がり高さ = radius * element scale * heightRatio（半径に比例）
        int   segments     = 64;    ///< 円周分割数
    };

    // 小球(塊)を多数寄せ集めて「もくもく」した塊感を作るジオメトリ。
    // lobeCount を増やすほど塊の数が増えて爆発煙らしくなる。
    // age(progress) 駆動で塊が時間差でポップ膨張する（stagger で順番具合を調整）。
    struct LobeClusterGeomParams
    {
        float    radius     = 1.5f;   ///< クラスタ全体の半径（実半径 = radius * element scale）
        int      lobeCount  = 8;      ///< 塊(もくもく)の数 ★
        float    lobeRadius = 0.55f;  ///< 各塊の半径（クラスタ半径に対する比）
        float    lobeJitter = 0.35f;  ///< 各塊の大きさ・位置のばらつき(0..1)
        float    stagger    = 0.5f;   ///< 時間差ポップ(0=全部同時に膨張 / 1=順番に湧き出る)
        int      rings      = 6;      ///< 各塊の縦分割（低めでOK。塊が多いので）
        int      sectors    = 8;      ///< 各塊の横分割
        uint32_t seed       = 1337;   ///< 乱数シード（配置を固定してチラつかせない）
    };

    /// ジオメトリパラメータの型安全な union（要素順は VfxGeometryType と一致させること）
    using VfxGeometryParams = std::variant<
        SphereGeomParams, ConeGeomParams, RingGeomParams, PlaneGeomParams, DiscGeomParams,
        RimCurtainGeomParams, LobeClusterGeomParams>;

} // namespace YoRigine

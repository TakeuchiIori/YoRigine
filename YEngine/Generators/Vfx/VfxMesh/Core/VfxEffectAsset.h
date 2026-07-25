#pragma once
// ===========================================================
// VfxEffectAsset.h
//
// Trail と LightVolume のパラメータをまとめるデータ構造
// JSON にシリアライズして保存・ホットリロードに対応
//
// ★ UE Niagara Ribbon Trail 参考の拡張パラメータ追加済み
// ===========================================================
#include "MathFunc.h"
#include "Quaternion.h"
#include <string>
#include <vector>
#include "Loaders/Json/EnumUtils.h" // BlendMode
#include "VfxModule.h"              // データ駆動モジュール
#include "VfxGeometryParams.h"      // 二軸合成: ジオメトリ params（軽量）
#include "VfxMaterialParams.h"      // 二軸合成: マテリアル params（軽量）

namespace YoRigine {

    struct VfxEvalState; // 前方宣言（EvaluateModules 用。定義は VfxEvalState.h）

    // -------------------------------------------------------
    // Trail パラメータ
    // -------------------------------------------------------

    enum class TrailShapeType : int
    {
        Flat      = 0,
        Arc       = 1,
        Fan       = 2,
        Custom    = 3,
        Primitive = 4,   // 3D プリミティブ (Box/Sphere/...)
    };

    // 3D プリミティブ種別
    enum class PrimitiveType : int
    {
        Box      = 0,
        Sphere   = 1,
        Capsule  = 2,
        Cone     = 3,
        Cylinder = 4,
        Torus    = 5,
    };

    // プリミティブの配置モード
    enum class PrimitivePlacement : int
    {
        Static          = 0,   // 1個固定 (装飾メッシュ用途)
        BeadAlongTrail  = 1,   // 軌跡の各点にスタンプ (ビーズ状)
    };

    // 3D プリミティブの形状/配置パラメータ
    struct PrimitiveSpec
    {
        PrimitiveType      type      = PrimitiveType::Box;
        PrimitivePlacement placement = PrimitivePlacement::Static;

        // ---- 共通サイズ ----
        Vector3 halfExtents = { 0.5f, 0.5f, 0.5f };   // Box の半辺長
        float   radius      = 0.5f;                   // Sphere/Capsule/Cone/Cylinder/Torus
        float   height      = 1.0f;                   // Capsule/Cone/Cylinder
        float   tubeRadius  = 0.15f;                  // Torus

        // ---- 分割数 ----
        int     latSegments  = 16;   // Sphere/Capsule の縦
        int     lonSegments  = 16;   // Sphere/Capsule/Cone/Cylinder の横
        int     ringSegments = 12;   // Torus の細い側の分割

        // ---- BeadAlongTrail モード用 ----
        float   stampScale   = 1.0f;
        float   stampSpacing = 0.0f;   // 0 = 平滑化後の全点に置く
        bool    scaleByAge   = true;   // 寿命で縮小フェード
    };


    struct TrailEffectParam
    {
        // --- 基本 ---
        float widthStart   = 0.3f;
        float widthEnd     = 0.0f;
        float lifetime     = 0.5f;
        int   maxPoints    = 512;

        Vector4 colorStart = { 1.f, 0.8f, 0.f, 1.f };
        Vector4 colorEnd   = { 1.f, 0.3f, 0.f, 0.f };

        BlendMode blendMode      = BlendMode::kBlendModeAdd;
        float     uvScrollSpeed  = 0.5f;

        std::string texturePath      = "";
        std::string noiseTexturePath = "";

        // --- 断面形状 ---
        TrailShapeType shapeType     = TrailShapeType::Flat;
        int            widthSegments = 1;
        float          arcAngleDeg   = 120.0f;

        bool  crescentShape = true;
        float thickness     = 0.1f;
        std::vector<Vector2> customVertices;
        int splineSubdivisions = 4;

        // パラメータ

        // --- グロー / エッジ ---
        float   emissiveIntensity = 1.0f;  // 発光強度 (0=消灯 / 1=そのまま / >1=強発光)
        float   softness       = 0.15f;    // エッジソフトフェード幅
        float   glowPower      = 1.5f;     // 中心コアグロー強度
        float   fresnelStrength = 1.0f;    // エッジフレネルグロー強度
        float   trailSharpness  = 2.0f;    // エッジシャープネス
        Vector4 rimColor       = { 0.4f, 0.7f, 1.f, 1.f }; // エッジリムカラー

        // --- ノイズ歪み ---
        float distortion    = 0.0f;     // UV 歪み強度 (0=OFF)
        float noiseOctaves  = 2.0f;     // FBM オクターブ数 (1-4)

        // --- エネルギーライン (UE の電気/魔力ライン) ---
        float energyIntensity = 1.5f;   // エネルギーライン輝度
        float energySpeed     = 2.5f;   // エネルギーラインスクロール速度

        // --- スパークル ---
        float sparkleAmount = 0.4f;     // スパークル輝度 (0=OFF)
        float sparkleSpeed  = 4.0f;     // スパークルアニメ速度

        // --- カラーウェーブ ---
        float colorWaveFreq = 3.0f;     // 色波の周波数
        float colorWaveAmp  = 0.15f;    // 色波の振幅 (0=OFF)

        // --- 幅ウェーブ (慣性感) ---
        float widthWaveAmp  = 0.05f;    // 幅のサイン波振幅 (0=OFF)
        float widthWaveFreq = 8.0f;     // 幅のサイン波周波数

        // --- ディゾルブ (溶けて消える) ---
        // age(頂点の経過時間) が進むほどノイズしきい値を押し上げ、縁から侵食して discard する。
        float   dissolveStrength  = 0.0f;                      // 0=OFF / 大きいほど早く溶ける
        float   dissolveEdgeWidth = 0.08f;                     // 侵食エッジの光る帯幅
        Vector4 dissolveEdgeColor = { 2.0f, 0.7f, 0.15f, 1.f };// 侵食エッジのHDR発光色 (Bloom用に >1 可)

        // --- 3D プリミティブ (shapeType == Primitive の時に参照) ★NEW ---
        PrimitiveSpec primitive;
    };

    // -------------------------------------------------------
    // LightVolume パラメータ
    // -------------------------------------------------------
    struct LightVolumeEffectParam
    {
        Vector3 halfExtents  = { 2.f, 1.5f, 5.f };
        Vector4 color        = { 1.f, 0.9f, 0.f, 0.15f };
        float   intensity    = 1.0f;
        float   edgeFade     = 0.15f;
        float   depthFade    = 1.0f;
        float   noiseTiling  = 2.0f;
        float   noiseStrength = 0.0f;
        float   beamStrength = 0.0f;
        float   beamRadius   = 0.35f;
        float   beamPower    = 2.0f;
        float   beamGlow     = 1.0f;
        bool    isEnable     = true;
    };

    // -------------------------------------------------------
    // NoiseVolume パラメータ (球状ノイズボリューム。煙/炎/霧/オーラに使う)
    // -------------------------------------------------------
    struct SmokeEffectParam
    {
        Vector4 color        = { 0.6f, 0.4f, 1.2f, 1.0f }; // rgb(>1でBloom), a=濃度基準
        Vector4 smokeColor   = { 0.08f, 0.08f, 0.09f, 1.0f }; // 爆発後に遷移する煙色(暗いグレー)
        float   riseSpeed    = 1.2f;   // 爆発後に煙が上昇する速さ
        float   radius       = 1.5f;
        float   noiseScale   = 4.0f;
        float   noiseStrength = 0.9f;
        float   scrollSpeed  = 0.3f;
        float   fresnelPower = 2.5f;
        float   density      = 1.0f;
        float   noiseOctaves = 4.0f;
        float   rimIntensity = 2.0f;   // リム発光（太陽フレア風 / Bloom 用）
        bool    isEnable     = true;

        // 従来の破裂挙動（ワンショット時の自動ポップ膨張/上昇/火球→煙のシェーダ遷移）を使うか。
        // false にするとモジュール（ScaleOverLife / ColorOverLife / Rise 等）だけで動きを組める。
        bool    builtInBurstMotion = true;
    };

    // -------------------------------------------------------
    // LightningBolt パラメータ (プロシージャル稲妻)
    // -------------------------------------------------------
    struct LightningEffectParam
    {
        Vector4 color        = { 0.6f, 0.8f, 1.5f, 1.0f }; // 芯(コア)の色（HDR / Bloom）
        Vector4 glowColor    = { 0.6f, 0.8f, 1.5f, 1.0f }; // 外側グローの色（HDR）2色
        Vector4 branchColor  = { 0.6f, 0.8f, 1.5f, 1.0f }; // 枝の色（HDR）
        float   length       = 4.0f;   // 稲妻の全長
        float   width        = 0.18f;  // リボン幅
        float   jitter       = 0.6f;   // ジグザグの振れ幅
        int     segments     = 24;     // 折れ線の分割数
        int     branches     = 3;      // 枝分かれ本数
        float   branchJitter = 0.5f;   // 枝の振れ幅
        float   flickerRate  = 18.0f;  // 1秒あたりの形変化回数（明滅）
        float   glowPower    = 2.0f;   // 中心グロー（芯の細さ）
        float   coreWidth        = 0.30f; // 芯の太さ/実体感(0..1)
        float   solidness        = 0.0f;  // 透明感を減らす(0=従来 / 1=べた塗り寄り)
        float   outlineIntensity = 0.0f;  // 枝/縁のアウトライン強調

        // 方向・曲線（エディタプレビュー / 既定の伸ばし方向）
        Vector3 direction    = { 0.0f, 1.0f, 0.0f }; // 稲妻を伸ばす向き（既定=上＝縦）
        float   bendAmount   = 0.0f;   // 曲げ量（0=直線 / 中点を横へふくらませて弧にする）
        bool    isEnable     = true;
    };

    // -------------------------------------------------------
    // ShockwaveRing パラメータ (爆発の衝撃波リング)
    // -------------------------------------------------------
    struct ShockwaveEffectParam
    {
        Vector4 color     = { 1.5f, 0.9f, 0.4f, 1.0f }; // 暖色 HDR
        float   radius    = 3.0f;   // 最大半径
        float   duration  = 0.6f;   // 1サイクルの秒数（膨張→消滅）
        float   thickness = 0.15f;  // リングの太さ
        bool    isEnable  = true;
    };

    // -------------------------------------------------------
    // VFXエレメントインスタンス（部品1個分）
    //
    // Trail 以外のエレメントは「種類＋パラメータ＋動き」を1エントリとして
    // 好きな数だけ積める。同じ種類を複数（稲妻3本・衝撃波2重など）も可能。
    // -------------------------------------------------------
    enum class VfxElementType : int
    {
        LightVolume   = 0,
        NoiseVolume   = 1,
        LightningBolt = 2,
        ShockwaveRing = 3,
    };

    // エディタ表示などに使う種類名
    const char* VfxElementTypeName(VfxElementType type);

    // エレメント種別 → モジュールの適用対象（All 指定のモジュール振り分け用）
    VfxModuleTarget ModuleTargetFor(VfxElementType type);

    // エレメントの表現方式
    //   Composed   … Geometry × Material の2軸合成（新: 幅広い表現）
    //   Monolithic … 専用メッシュ（Lightning / LightVolume など特殊効果）
    enum class VfxElementKind : int
    {
        Composed   = 0,
        Monolithic = 1,
    };

    struct VfxElement
    {
        // ── 共通 ─────────────────────────────────────────────
        std::string label   = "";                        // エディタ表示名（空なら種類名）
        bool        enabled = true;
        Vector3     offset  = { 0.f, 0.f, 0.f };          // エフェクト基準位置からのオフセット
        Quaternion  rotation = Quaternion::Identity();    // 向き（Cone / 非ビルボード Ring・Plane で使用）

        // ブレンドモード上書き。
        //   -1                  = マテリアル既定（PSO にベイクされた本来のブレンド）を使う
        //   0..(kCount未満)      = BlendMode（None/Normal/Add/Subtract/Multiply/Screen）で明示上書き
        // HDR で加算(Add)が重なって飽和する場合に Normal/Screen 等へ変えて合成を抑えるためのもの。
        int         blendModeOverride = -1;

        VfxElementKind kind = VfxElementKind::Composed;

        // ── Composed（kind == Composed） ─────────────────────
        // variant の index が種別（Sphere/Cone/... , Noise/Shockwave/...）
        VfxGeometryParams geom = SphereGeomParams{};
        VfxMaterialParams mat  = NoiseMatParams{};

        // ── Monolithic（kind == Monolithic） ─────────────────
        // 専用メッシュ。type が LightningBolt / LightVolume を指す。
        VfxElementType         type = VfxElementType::LightningBolt;
        LightVolumeEffectParam lightVolume;
        LightningEffectParam   lightning;

        // ── 旧アセット移行用（読み取り専用・Composed へ変換される） ──
        // 旧 NoiseVolume/ShockwaveRing の JSON を読むために残す。
        // 実行時は Composed（geom/mat）が使われる。
        SmokeEffectParam       smoke;
        ShockwaveEffectParam   shockwave;

        // ── このエレメントにだけ効く動き（全体 modules に加算で適用） ──
        std::vector<VfxModule> modules;

        // Composed の種別ヘルパ（variant index ↔ 型 ID）
        VfxGeometryType GeometryType() const { return static_cast<VfxGeometryType>(geom.index()); }
        VfxMaterialType MaterialType() const { return static_cast<VfxMaterialType>(mat.index()); }
    };

    // -------------------------------------------------------
    // まとめアセット
    // -------------------------------------------------------
    struct VfxEffectAsset
    {
        std::string       name = "NewEffect";
        TrailEffectParam  trail;
        bool useTrail       = true;

        // 寿命を無限にする（ワンショット寿命/Lifetime を無視してループし続ける）。
        // バフエリア等「Stop するまで消えないエフェクト」向け。Spawn 時に loop を強制 true にする。
        bool loopForever    = false;

        // Trail 以外のエレメント。同じ種類を複数積める。
        std::vector<VfxElement> elements;

        // データ駆動の動き（エフェクト全体）。0個=従来挙動。
        // target で「煙だけ」「稲妻だけ」のように適用先を絞れる。
        std::vector<VfxModule> modules;

        // ワンショット寿命(秒)。Lifetime モジュール（全体/エレメント個別どちらでも）が
        // あればその duration の最大値、無ければ既存アセット互換のヒューリスティック。
        float OneShotDuration() const;

        void SaveToJson(const std::string& filePath) const;
        bool LoadFromJson(const std::string& filePath);
    };

    // modules のうち target（または All）に一致するものを評価し、
    // 共有状態(position/scale/端点)へ反映する唯一の関数。
    // Spawner / Editor の双方がこれを呼ぶことで動きを一致させる。
    void EvaluateModuleList(const std::vector<VfxModule>& modules,
                            VfxModuleTarget target, VfxEvalState& state);

    // エフェクト全体の modules を target で絞って評価（従来 API）
    void EvaluateModules(const VfxEffectAsset& asset, VfxModuleTarget target, VfxEvalState& state);

    // エレメント1個分の評価。
    // 全体 modules（対象一致分）→ インスタンス固有 modules の順に適用する。
    void EvaluateElementModules(const VfxEffectAsset& asset,
                                  const VfxElement& sub, VfxEvalState& state);

} // namespace YoRigine

# VFX拡張設計 — GPUパーティクル × VfxMesh で「攻撃演出/りゅうせいぐん」まで表現できる基盤にする

コード調査（`YEngine/Generators/{Vfx,GPUParticle,Particle,Composite}`）に基づく現状分析と、不足点を埋めるためのクラス設計案。既存の命名・レイヤー構成（`Handle` facade + `Manager`/`Spawner` シングルトン + JSON アセット + `ProceduralMeshBase` 派生）にそのまま乗せられる形で提案する。

## 1. 現状（棚卸し）

このエンジンのVFXは既に4層構造。

- **CPU パーティクル (`YParticle`)**: `IUpdateModule` 群（Spawn/Update）を JSON でモジュール合成する Niagara 風の粒子システム。`UpdateSubEmitter`（親の死亡/継続で子システムを発生）、`UpdateAttractor`/`UpdateForceField`/`UpdateVortex`/`UpdateCollision`（平面のみ）など物理モジュールも揃っている。
- **GPU パーティクル (`GpuEmitManager`/`GPUEmitter`)**: Compute Shader常駐。エミッタ形状は Sphere/Box/Triangle/Cone/**Mesh**（表面/内部/エッジ）。1粒子の描画形状も Plane 以外に Box/Ring/Cylinder/Sphere/Cone/Fan を選べる。トレイル（子粒子）もGPU側で生成。
- **VfxMesh（プロシージャルメッシュ）**: `ProceduralMeshBase` を共通基底に、`TrailMesh`（斬撃軌跡。Flat/Arc/Fan/Custom/**Primitive(Box/Sphere/Capsule/Cone/Cylinder/Torus)**）、`LightningMesh`（ジグザグ稲妻）、`ShockwaveMesh`（衝撃波リング）、`VolumeSmokeMesh`（レイマーチ煙/オーラ球）、`LightVolumeMesh`（光の柱/ビーム）がある。`VfxEffectAsset`が「Trail + 複数エレメント + `VfxMotion`（データ駆動の動き: Move/Rise/Pulse/ScaleOverLife/ColorOverLife/FadeInOut/Accelerate/Orbit/Shake/Visibility/Flicker/BeamPulse）」を1本にまとめる。
- **Composite層 (`CompositeEffectManager`)**: Particle + VfxMesh + GPU + Sound を名前参照だけで束ね、`EffectHandle::PlayOneShot(name, pos)` 1行で全部連動させる入口統一レイヤーが**既にある**。

率直に言って、単発の「斬撃・被弾・爆発」演出パーツの引き出しはかなり多い。今回のゴール（りゅうせいぐんのような「発生源→移動→着弾」を伴う攻撃）で詰まるのは、**個々のエフェクト表現力ではなく、それらを「ゲーム世界の出来事」に接続する配線が無いこと**。

## 2. 不足点

### 2.1 GPUパーティクルに「衝突」も「イベント通知」も無い（最大の穴）
`UpdateParticle.CS.hlsl` を見る限り、GPU粒子の物理は速度積分＋重力のみで、地面判定・衝突コールバック・CPUへの死亡イベント通知が一切ない。`UpdateCollision`（CPU側 `YParticle` の平面バウンド）に相当するものがGPU側に無く、しかもCPU→GPUの一方向で完結しているため、**「粒子がどこで消えたか」をゲームコードが知る手段がない**。りゅうせいぐんは「隕石が着弾した地点にクレーター/ダメージ判定/シェイクを起こす」演出が本体なので、ここが解決しないと見た目だけの雨が降るだけになる。

### 2.2 「実体を持つ発射物」という概念が存在しない
GPU/CPUパーティクルはどちらも純粋な描画物で、`CollisionManager` を通した当たり判定に参加しない。一方 `BaseObject` 系のゲームオブジェクトはコライダーを持てるが、VFX層とは無関係。つまり **「見た目は隕石、実体はコライダーを持つ移動体」を1個のクラスで表現する場所が無い**。今は斬撃(`PlayerSword`)のような「プレイヤーに追従する視覚専用トレイル」しか実例が無く、「独立して空間を移動し、着弾判定を持つ攻撃」の型が未整備。

### 2.3 VfxMeshが「その場に留まる」表現に偏っている
`TrailMesh`/`Lightning`/`Shockwave`/`Smoke`/`LightVolume` はいずれも「基準点（または始点終点）＋その場でのアニメ」。`VfxMotion` の `Move`/`Accelerate` はエフェクト全体の基準位置を動かせるが、**弧を描いて落下し姿勢も変えながら飛ぶ「弾道」の記述**（開始点・目標点・頂点の高さ・回転・到達までの秒数）を持つものが無い。今の枠組みだと「降ってくる隕石本体の軌道」をデータ駆動で表現できず、都度C++で書くことになる。

### 2.4 直線的な「ビーム」表現が無い
`LightningMesh`はジグザグ稲妻専用。チャージレーザーや光の柱の"閃光ビーム"のような、**始点→終点をまっすぐ結ぶ実体感のあるリボン**（コア+グロー、UVスクロールで発射感、着弾フラッシュ）がVfxMeshに無い。`LightVolumeMesh`のbeam系パラメータ（`beamStrength`/`beamRadius`）はOBBボリュームの中の擬似ビームで、狙った2点を結ぶ用途とは別物。

### 2.5 着弾痕（デカール）が無い
`grep -rli decal` は空振り。地面に焼け跡・クレーター・ひび割れを投影するデカール機構が存在しない。りゅうせいぐんのような「多数着弾」演出は、着弾の瞬間だけの閃光/衝撃波では地面に何も残らず単調になりやすい。

### 2.6 GPU⇄VfxMesh⇄Compositeの「発火トリガ」が疎結合すぎる
`CompositeEffectManager` は Particle/VfxMesh/GPU を**同時にPlayするだけ**で、各系統は独立した寿命を進む。「GPU粒子（隕石トレイル）が特定の位置に来た/消えた瞬間にVfxMeshの爆発を鳴らす」といった、**系統をまたいだイベント連鎖**が組めない。`UpdateSubEmitter`はCPUパーティクル内で完結するローカルな仕組みで、Composite層まで届いていない。

### 2.7 カメラ演出（シェイク/ヒットストップ）がVFX層から宣言できない
`AttackCameraComponent`/`GameTime` にシェイク・タイムスケール機構はあるが、`CompositeEffectAsset` にその参照が無いため、**攻撃のたびにゲームコード側で個別に呼び出しを書く**運用になっている。りゅうせいぐんのように着弾ごとに小さいシェイクを何度も入れたいケースほど、データ駆動でないと破綻しやすい。

### 2.8 同時多発時のパフォーマンス制御が手薄
`VfxMeshSpawner` は `kMaxActiveVfx=256` の固定プールで上限自体はあるが、"何を優先して間引くか"（画面外/遠距離/優先度）の概念が無い。`VolumeSmokeMesh`はレイマーチで重いことがワークログにも明記されており、りゅうせいぐんで隕石ごとに煙ボリュームを生成すると簡単に破綻する。GPU側も1エミッタ`kMaxParticles=16384`だが、同時に多数のグループを立てた時の総量ガードが無い。

---

## 3. 設計案

不足を7本のピースで埋める。既存レイヤーの流儀（`Handle`+`Manager`+JSON+`ProceduralMeshBase`）を踏襲し、既存クラスへの変更は最小限にする。

### 3.1 `TrajectoryCurve` — 弾道のデータ表現（2.3対応）
新規: `YEngine/Generators/Vfx/VfxMesh/Core/VfxTrajectory.h`

```cpp
namespace YoRigine {

// 開始点→目標点への弾道。放物線 or ベジエで途中の高さ・横ブレを持たせる。
struct TrajectorySpec
{
    Vector3 start = {};
    Vector3 end   = {};
    float   apexHeight   = 4.0f;   // 弧の頂点の高さ（0=直線）
    float   lateralNoise = 0.0f;   // 横方向のランダムなブレ幅（見た目のばらつき）
    float   duration     = 0.8f;   // start→end 到達までの秒数
    VfxEase ease         = VfxEase::EaseInQuad; // 落下の加速感などに流用

    // t(0..1) から位置と進行方向（姿勢用）を評価
    void Evaluate(float t, Vector3& outPos, Vector3& outDir) const;
};

} // namespace YoRigine
```
`VfxMotion` に新タイプ `FollowTrajectory`（`TrajectorySpec` を1つ紐付け）を追加すれば、既存の `VfxEffectAsset`/`VfxMeshSpawner` の評価パイプライン（`EvaluateMotionList`）にそのまま乗る。「隕石の落下軌道」も「投擲武器の弧」もこれ1つで賄える。

### 3.2 `ProjectileObject` — 実体を持つ発射物（2.2/2.1対応の本丸）
新規: `YGame/GameObjects/Vfx/ProjectileObject.h`（`BaseObject`派生。既存の「Adding GameObject」手順に準拠）

```cpp
class ProjectileObject : public BaseObject
{
public:
    struct Spec
    {
        YoRigine::TrajectorySpec trajectory;
        std::string travelVfx;     // 見た目: Composite名（トレイル+GPU粒子など） 例:"MeteorTravel"
        std::string impactVfx;     // 着弾時: Composite名 例:"MeteorImpact"
        std::string groundDecal;   // 任意: 着弾デカール名
        uint32_t    collisionTypeId = 0; // ColliderFactory で使う typeId（kNavObstacle等と同じ枠組み）
        float       impactRadius = 1.5f; // 着弾判定/範囲ダメージに使う半径
        CameraShakeProfile* shakeOnImpact = nullptr; // 任意
        float       hitStopMsOnImpact = 0.0f;
    };

    void Initialize(Camera* camera) override;
    void InitCollision() override;   // OBB/Sphereコライダーを Spec.collisionTypeId で生成
    void InitJson() override {}
    void Update() override;          // trajectory_.Evaluate(t) で移動、Draw用の見た目Handleに位置を反映
    void Draw() override {}          // 実体は Composite/GPU側が描画するため空実装で良い

    void OnEnterCollision(BaseObject* other) override; // 着弾判定（地面/敵/StaticWall）
    bool IsFinished() const { return finished_; }

private:
    void OnImpact(const Vector3& hitPos);

    Spec    spec_;
    float   age_ = 0.f;
    bool    finished_ = false;
    CompositeInstance travelHandle_; // travelVfx を Play し、毎フレーム SetPosition
};
```

- 移動は`TrajectorySpec::Evaluate`のCPU評価（GPU常駐にしない）なので、`CollisionManager::Raycast`や`OnEnterCollision`にそのまま参加できる＝2.1/2.2を同時に解決。
- 見た目は既存の`CompositeEffectManager`をそのまま呼ぶだけなので、新しい描画コードは書かない。
- 地面コライダーが無い問題（CLAUDE.mdのGotcha）は、`ProjectileObject`側で下向き`Raycast`（`CollisionManager::Raycast`）を使うか、`kNavObstacle`と同様の専用`typeId`（例: `kGroundProbe`）を`Ground`に後付けする対応が別途必要。りゅうせいぐん実装の前提条件としてここは要確認。

### 3.3 `ProjectileSpawner` — 群体発生の指揮官（りゅうせいぐん本体）
新規: `YGame/GameObjects/Vfx/ProjectileSpawner.h`（シングルトンではなく、攻撃を持つオブジェクト側が保有するコンポーネント）

```cpp
class ProjectileSpawner
{
public:
    struct WaveSpec
    {
        int      count = 12;
        Vector3  spawnAreaCenter{};
        float    spawnAreaRadius = 8.0f;   // 発生源をこの円内にランダム散布
        float    spawnHeight = 15.0f;
        Vector3  targetAreaCenter{};
        float    targetAreaRadius = 6.0f;  // 着弾もこの円内にランダム散布
        float    spawnIntervalMin = 0.05f; // 着弾/発生の時間差でザザザ感を出す
        float    spawnIntervalMax = 0.25f;
        ProjectileObject::Spec projectileSpec;
    };

    void Start(const WaveSpec& wave);  // 内部でタイマー駆動、count個をstaggerして生成
    void Update(float dt);
    bool IsFinished() const;

private:
    WaveSpec wave_;
    float    timer_ = 0.f;
    int      spawnedCount_ = 0;
    std::vector<std::unique_ptr<ProjectileObject>> active_;
};
```
JSON化（`Resources/Json/YAttacks/MeteorShower.json`等）すれば、他の攻撃データ（`ComboTypes.h`のAttackData群）と同じ感覚でデザイナーが個数・範囲・間隔を調整できる。

### 3.4 `BeamMesh` — 直線ビーム（2.4対応）
新規: `YEngine/Generators/Vfx/VfxMesh/Effects/BeamMesh.{h,cpp}`。`LightningMesh`/`ShockwaveMesh`と同じ型（`ProceduralMeshBase`派生、`VfxElementType::Beam`追加、`CreatePSO_VfxMeshBeam`追加）。

```cpp
struct BeamEffectParam {
    Vector4 coreColor  = {1.5f, 1.2f, 0.6f, 1.f}; // HDR
    Vector4 glowColor  = {1.5f, 0.6f, 0.2f, 1.f};
    float   width       = 0.3f;
    float   coreWidth   = 0.25f;
    float   uvScrollSpeed = 3.0f;   // 発射感（テクスチャが端から端へ流れる）
    float   impactFlareSize = 1.0f; // 終点の閃光の大きさ
    bool    isEnable = true;
};
class BeamMesh : public ProceduralMeshBase {
public:
    void Initialize();
    void SetEndpoints(const Vector3& start, const Vector3& end);
    void ApplyParam(const BeamEffectParam& p) { param_ = p; }
    void Drive(const VfxEvalState& state) override { SetEndpoints(state.boltStart, state.boltEnd); }
    void Update(float dt) override;
    void Draw(ID3D12GraphicsCommandList* cmdList) override;
private:
    BeamEffectParam param_;
};
```
`LightningMesh`との違いは「ジッター無し・端点固定・UVスクロールで"発射→到達"の勢いを出す」ことに特化する点。チャージ攻撃/必殺技のレーザーに直接使える。

### 3.5 `DecalMesh` — 着弾痕（2.5対応）
新規: `YEngine/Generators/Vfx/VfxMesh/Effects/DecalMesh.{h,cpp}`。地面法線に沿わせた平面を1枚生成し、寿命でディゾルブ消滅（`TrailEffectParam`のdissolve実装と同じ手法を流用可能）。

```cpp
struct DecalEffectParam {
    Vector4 color = {0.1f,0.1f,0.1f,0.8f}; // 焦げ跡など
    float   radius = 1.5f;
    float   lifetime = 6.0f;     // この秒数かけてフェード/ディゾルブ
    std::string texturePath;     // ひび割れ/クレーターのテクスチャ
    bool    isEnable = true;
};
class DecalMesh : public ProceduralMeshBase {
public:
    void Initialize();
    void SetTransform(const Vector3& hitPos, const Vector3& hitNormal, float radius);
    void ApplyParam(const DecalEffectParam& p) { param_ = p; }
    void Update(float dt) override;
    void Draw(ID3D12GraphicsCommandList* cmdList) override;
private:
    DecalEffectParam param_;
    Vector3 normal_ = {0,1,0};
};
```
`VfxElementType::Decal`として`VfxEffectAsset`に混ぜられるようにし、`ProjectileObject::OnImpact`が`hitNormal`（Raycastの結果）を渡して`VfxMeshSpawner::Spawn`する。

### 3.6 `CompositeEffectAsset`への演出フック追加（2.6/2.7対応）
既存`CompositeEffectAsset`（`YEngine/Generators/Composite/CompositeEffectManager.h`）に3フィールド追加するだけで済む、最小コストの拡張。

```cpp
struct CompositeEffectAsset {
    // ...既存フィールド...
    std::string        cameraShakeProfile;  // 空なら無し。名前でCameraShakeライブラリを引く
    float               hitStopMs = 0.0f;    // 0=無し
    std::string         onImpactComposite;   // 任意: この複合エフェクト自体が「子の複合エフェクト」を連鎖起動（2.6の弱い版）
};
```
`CompositeEffectManager::PlayOneShot`内で`cameraShakeProfile`/`hitStopMs`が設定されていれば`GameTime`/`AttackCameraComponent`の既存APIを呼ぶだけ。**新しい系統を作らず、既存4系統に「カメラ」を5本目の薄い参照として足す**イメージ。りゅうせいぐんの着弾ごとの微シェイクは、この1行追加で`MeteorImpact.json`に`"cameraShakeProfile":"SmallImpact"`と書くだけになる。

より本格的な系統間チェイン（GPU粒子の特定粒が消えた瞬間にVfxMeshを起動、等）が要る場合は2.1のGPUイベント通知が前提になるため、まずは2.1と3.2（`ProjectileObject`がCPU側でage/衝突を握る設計）で「GPU常駐粒子に頼らず済ませる」方が投資対効果が高い。GPU側のReadbackベースのイベント通知は将来的な選択肢として保留を推奨（実装コストが高い割に、りゅうせいぐん用途は3.2で代替できるため）。

### 3.7 `VfxBudget` — 同時発生数の間引き（2.8対応）
新規: `YEngine/Generators/Vfx/VfxBudget.h`（軽量シングルトン）

```cpp
class VfxBudget {
public:
    static VfxBudget* GetInstance();
    // 高コスト要素（Smoke/LightVolume等）はSpawn前にこれで許可を取る
    bool RequestSlot(VfxCostTier tier, const Vector3& worldPos, Camera* camera);
    void BeginFrame(); // カウンタリセット
private:
    int activeHeavy_ = 0;
    static constexpr int kMaxHeavyPerFrame = 8; // 例: レイマーチ煙の同時上限
};
```
`VfxMeshSpawner::Spawn`（NoiseVolume要素を含む場合）と`GpuEmitManager::EmitGroups`（多数同時発火時）の入り口で`RequestSlot`を挟み、超過分は「軽量な代替（Shockwave+Decalのみ、Smoke無し）」にフォールバックする。りゅうせいぐんのような「同時多発かつ各弾が重いボリューム煙を持つ」ケースの保険。

---

## 4. 優先順位案

1. **3.2 `ProjectileObject` + 3.1 `TrajectoryCurve`** — りゅうせいぐんの骨格。これが無いと他が意味を持たない。地面コライダー不在（CLAUDE.md記載のGotcha）の解消も同時に必要。
2. **3.6 Composite演出フック** — 既存構造体にフィールド3つ足すだけで演出の質が大きく上がる。コストが最も低い。
3. **3.5 `DecalMesh`** — 着弾痕。りゅうせいぐん・爆発全般の見栄えに効く。既存Trail dissolveの実装を流用できるので実装コストは中程度。
4. **3.4 `BeamMesh`** — 攻撃演出全般（チャージ技等）に汎用的に効くが、りゅうせいぐん単体では必須でない。
5. **3.3 `ProjectileSpawner`** — 3.2ができてからJSON化・デザイナー向け調整項目を整える。
6. **3.7 `VfxBudget`** — 動くようになってから、実機でパフォーマンス実測して閾値を決める（早すぎる最適化を避ける）。
7. GPU側イベント通知（2.1の本格対応/Readback） — 保留。3.2で代替できるので、GPU常駐の大量弾（例: 100発以上の雑魚隕石）が本当に必要になった時にだけ着手。

各ピースは独立して着手可能で、既存の`EffectHandle`/`VfxMeshHandle`/`GpuParticleHandle`/`CompositeEffectManager`のAPIパターンをそのまま踏襲しているため、既存コードへの破壊的変更はほぼ無い（`CompositeEffectAsset`への3フィールド追加のみが唯一の既存構造体変更）。

---

## 5. ユーザーからの懸念点への回答

上記レビューを受けて出た4つの懸念について、追加でコードを調査した上で個別に回答する。

### 5.1「爆発を作るたびにSmokeのような新規クラスを増やさないといけない」問題

これは半分正しく半分誤解がある。実際に調べると、**見た目のバリエーション（色/ノイズ強度/膨張速度/大きさ違いの"別の爆発"）は既にC++クラス追加なしでJSON+`VfxMeshEditor`だけで作れる**（`VfxEffectAsset`に`NoiseVolume`/`LightningBolt`/`ShockwaveRing`を好きな数だけ積んで`VfxMotion`で動きを付ける、というのが「新しい爆発」を作る本来のワークフロー）。新規C++クラスが要るのは「今まで無かった**シェーダアルゴリズム/ジオメトリの種類**」を追加する時だけ（Lightningのジグザグ生成、Smokeのレイマーチなど）。

ただし、以下2点は実際にギャップがあり、「新規クラスを書かないと表現できない場面」を不必要に増やしている。

1. **`PrimitiveSpec`（Box/Sphere/Capsule/Cone/Cylinder/Torus）がTrailの中にしか置けない。** `VfxElement`は`lightVolume`/`smoke`/`lightning`/`shockwave`の4種しか持てず、単体の破片・岩塊・リング状オブジェクトを「トレイル無しで」爆発の一部として置けない。
   → **対応**: `VfxElementType::Primitive`を追加し、`VfxElement`に`PrimitiveSpec primitive`フィールドを足す。描画は新規`PrimitiveMesh : ProceduralMeshBase`（analyticな形状生成は`TrailMesh`の`PrimitiveSpec`生成コードをそのまま流用できるので実質1クラス）。これで「岩塊を複数個、爆発の中心からバラバラに飛ばす」のような表現がC++追加なしで組めるようになる。
2. **新しいVfxElementTypeを追加するコストが高い。** ワークログを見る限り、Lightning/Shockwave/Smokeはそれぞれ「`Effects/X.{h,cpp}`＋シェーダ`VfxMesh_X.PS.hlsl`＋`YPipelineManager::CreatePSO_VfxMeshX`＋`VfxEffectAsset`にパラメータ構造体追加＋`VfxMeshEditor`にタブ追加」という**5箇所を手で触る定型作業**になっている。新しい"表現の引き出し"を追加するハードルそのものを下げないと、この状態はいくら待っても解消しない。
   → **対応**: `YParticleModuleFactory`の`REGISTER_UPDATE_MODULE`と同じ発想で、`VfxElementType`ごとのファクトリ登録マクロ（`REGISTER_VFX_ELEMENT(Type, MeshClass, ParamStruct, PSOName)`）を用意し、`VfxMeshSpawner`/`VfxMeshEditor`のswitch文をレジストリ参照に置き換える。新しい種類を足す時に触るのは「新クラス1個＋シェーダ1個」だけにする（Editorタブ・Spawnerのswitch分岐は自動化）。

まとめると、「爆発の見た目バリエーション」自体はデータ駆動で既にできるが、「新しい種類の部品」を追加するコストが高いのが本当の問題。5.1-2で下げる。

### 5.2 トレイル/形状をBlenderで作りたい

朗報として、**この仕組みは既に半分存在する**。`GPUEmitter::SetMeshParams(Model* model, ...)`はassimp経由でロードした`Model`（`.obj`/`.gltf`をBlenderからエクスポートしたもの）をパーティクル発生源（表面/内部/エッジ）として使える。つまりGPUパーティクルの発生形状は既にBlenderで作れる。

一方`TrailMesh`（斬撃軌跡など）の断面形状は`TrailShapeType`（Flat/Arc/Fan/Custom/**Primitive**）で、`Primitive`はanalyticな6形状のみ、`Custom`も`std::vector<Vector2> customVertices`という2D頂点リストの手打ちで、**インポートしたメッシュを断面やビーズ形状に使えない**。

→ **対応**: `PrimitiveType`に`ImportedMesh`を追加し、`PrimitiveSpec`に`std::string modelPath`を持たせる。`TrailMesh`/新設`PrimitiveMesh`が`ModelManager::LoadModel`経由で取得した`Model`の頂点/インデックスを直接コピーして使う（`GPUEmitter::UpdateMeshTriangleData`が三角形リストをCPU側にキャッシュしている処理と同じやり方が流用できる）。これで「Blenderで作った岩・破片・剣のトレイル断面をエクスポートしてそのまま使う」が、GPUパーティクルだけでなくVfxMesh側でも可能になる。既存のModelロードパイプラインに乗るので、新しいアセットパイプラインを作る必要は無い。

### 5.3 複合エフェクトの位置/寿命がバラバラでゲーム側から統一制御できない

`CompositeEffectManager.cpp`を見ると、`PlayOneShot`/`Play`は各子（`particleEffect`/`vfxMeshAssets`/`gpuEmitterGroup`/`sounds`）をそれぞれ独立に`Play`しているだけで、**寿命はそれぞれのJSON側に埋め込まれた値がそのまま使われる**。また`offset`/`scale`は`vfxMeshAssets`にしか無く、`particleEffect`と`gpuEmitterGroup`は位置オフセットもスケールも持てない（常に同じ座標に生成される）。ゲーム側が呼び出し時に持てるのは`position`だけ。これが「バラバラで統一できない」の正体。

→ **対応**: 2段階で直す。

1. **全チャイルドにoffset/scale/delayを持たせる。** `CompositeVfxRef`と同じ形の参照構造体を`particleEffect`/`gpuEmitterGroup`にも用意する（現状`std::string`1本なので`vector<CompositeParticleRef>`/`vector<CompositeGpuRef>`へ拡張。単一想定を崩さないよう既存の単数フィールドは後方互換で残し、複数子が要る時だけ配列を使う設計にする）。
   ```cpp
   struct CompositeChildRef {
       std::string asset;
       Vector3     offset = {0,0,0};
       float       scale  = 1.0f;
       float       delay  = 0.0f;      // Play呼び出しからの遅延(秒)
   };
   ```
2. **Play呼び出し側でマスター寿命/スケールを渡せるようにする。** `CompositeEffectManager::PlayOneShot`/`Play`に`CompositePlayParams{ float scale=1.0f; float durationScale=1.0f; }`を追加。`durationScale`は各子の`Handle`に寿命スケールを渡す新しい引数（`EffectHandle`/`VfxMeshHandle`/`GpuParticleHandle`それぞれに`SetDurationScale(float)`相当を追加。VfxMesh側は`ActiveEffect::age`の進み方に`1/durationScale`を掛けるだけなので実装コストは低い。CPUパーティクル/GPUパーティクルは`lifeTime`パラメータへの乗算で対応）。

   これで`CompositeEffectManager::PlayOneShot("MeteorImpact", pos, {.scale=2.0f, .durationScale=1.5f})`のように、**呼び出し側1箇所で全系統をまとめて拡大・スロー**にできるようになる。りゅうせいぐんで隕石ごとに着弾エフェクトの大きさをランダム化する、といった用途に直結する。

### 5.4 GPU/VfxMeshの当たり判定はCollisionManagerに足すべきか

結論: **粒度によって答えが変わる。ひとまとめにCollisionManagerへ足すのは避けるべき。**

`CollisionManager`を読むと、これは「持続的に存在するコライダー同士を毎フレームBroadPhase(UniformGrid)→NarrowPhaseで突き合わせ、Enter/Stay/Exitを発火し、CCDでトンネリングを防ぐ」ための仕組みで、`AddCollider`/`RemoveCollider`された`BaseCollider`（＝寿命の長いゲームオブジェクトの当たり判定）を前提に作られている。ここに「GPU上で16384個回っている粒子1個1個」を継続的なコライダーとして登録するのは筋が悪い（GPU→CPU同期コストが跳ね上がるうえ、Enter/Exit・接触猶予・CCDのどれも粒子には過剰な機能）。

一方で`Raycast`/`RaycastMasked`はあるが、**「この位置半径Rの中に誰がいるか」を今すぐ1回だけ聞く一時的な範囲検索（オーバーラップクエリ）はまだ存在しない**（`AddCollider`して毎フレーム判定させ続けるパターンしか無い）。りゅうせいぐんの着弾ダメージのような「一瞬だけ範囲を調べたい」用途にはこれが無いと結局重い持続コライダーを使うことになってしまう。

→ **設計方針（粒度で使い分け）**:

| 粒度 | 使う仕組み | 理由 |
|---|---|---|
| GPU粒子1個1個の地面バウンド/消滅 | **CollisionManagerを使わない**。Compute Shader内で地面高さ(または将来ハイトフィールドテクスチャ)との比較のみ（既存CPU版`UpdateCollision`のGPU版）。 | 数が多すぎてCPU側コライダーと同期する意味が無い。見た目の物理でありゲームプレイ判定ではない。 |
| 隕石本体・ビーム・衝撃波などの「攻撃判定」 | **CollisionManagerに新設する一時的なオーバーラップクエリ**（`CollisionManager::QuerySphere(center, radius, layerMask) -> vector<BaseCollider*>`、`QueryOBB`も同様）。内部は`grid_`(UniformGrid)で候補を絞り、`DispatchShapePair`相当のNarrowPhaseを1回投げるだけで、Enter/Exit・接触猶予・CCDなどの持続状態は一切持たない。 | 「今この瞬間、この範囲に誰がいるか」を都度確認するだけの用途に対して、既存のBroadPhase資産(UniformGrid)を再利用しつつ持続コストを払わずに済む。 |
| 落下する隕石本体（`ProjectileObject`）の地面/壁への着弾検知 | **既存の持続コライダー経路**（`ColliderFactory::Create`＋`OnEnterCollision`）。 | 高速で移動する実体1個の着弾検知なので、既にある`SweepCCDColliders`（トンネリング防止）の恩恵を受けたい。数も攻撃1回につきせいぜい数十個なので持続コストは無視できる。 |

つまり、`CollisionManager`を直接拡張するのは「一時オーバーラップクエリ」の追加だけに留め、GPU粒子そのものはCollisionManagerの外（Compute Shader内で完結する軽量判定）に置く、という整理にする。これなら既存のBaseObject/コライダー資産の意味づけを壊さずに済む。

### 5.5 りゅうせいぐんのタイムライン表示

良いニュースから: **`DopeSheetEditor`は既に汎用部品として作られており、`DopeSheetTypes.h`の`TrackType`には`Effect`/`Sound`/`CameraShake`/`Event`が最初から用意されている。** データ構造(`DopeTrack`/`DopeKey`)もImGui非依存の素のstructなので、Release/Debug問わず保持・評価に使える。UIを新規に作る必要は無い。

ただし実際に使われているのは`YGame/GameObjects/Player/Combo/AttackFrameConverter.h`経由の`AttackHitbox`/`CancelWindow`の2トラックだけで、`Effect`/`Sound`/`CameraShake`トラックは**型として定義されているだけで、これを保持するデータフィールドも、これを読んでランタイムでイベント発火する仕組みも存在しない**。`AttackingCombatState.cpp`が見ているのも`hitStart`/`hitEnd`という区間ウィンドウだけで、「特定フレームで1回だけ何かを起動する」という点イベントの発火処理そのものがコードベースに無い。「最適化が必要そう」という直感は半分正しいが、正確には**最適化ではなく機能が未実装**というのが実態。

→ **対応**: `AttackFrameConverter`と同じパターンを踏襲した新規レイヤーを追加する。

1. **データ**: 既存の`std::vector<DopeSheet::DopeTrack>`をそのまま「エフェクトシーケンス」のシリアライズ形式として採用する（`DopeKey::value`をComposite名のインデックス、`DopeKey::tag`をComposite名文字列に使うなど、既存フィールドの範囲で表現できる）。`CompositeEffectAsset`か、りゅうせいぐん専用の`ProjectileSpawner::WaveSpec`（前回提案）のどちらかに`std::vector<DopeSheet::DopeTrack> timeline`を持たせる。
2. **編集**: `AttackDataEditor::DrawDopeSheet`と全く同じ構成で`WaveSpecEditor`（or 汎用`EffectSequenceEditor`）を作り、`DopeSheetEditor::Draw`をそのまま呼ぶ。トラックは1本（例: "Spawn"）で、りゅうせいぐん30発なら**そのトラックにキーを30個置くだけ**（トラックを30本作る必要はない）。この使い方であれば`DopeSheetEditor`側の追加最適化は恐らく不要（今の実装は「1トラックにキーが多数」のケースは元々想定内）。トラックが数十本を超えて重くなるケース（例: 個々の隕石ごとに専用トラックを分ける、など）だけ避ければ良い。
3. **ランタイム**: 新規`EffectSequencePlayer`（小さいクラス。`AttackingCombatState`のフレーム管理と同じ発想）。
   ```cpp
   class EffectSequencePlayer {
   public:
       void Start(const std::vector<DopeSheet::DopeTrack>& tracks, int fps, const Vector3& origin);
       void Update(float dt); // ageを進め、まだ発火していないkeyでage>=frame/fpsのものを発火
       bool IsFinished() const;
   private:
       struct FiredState { std::vector<std::vector<bool>> firedPerTrackKey; };
       // Effectトラック: DopeKey.tag = Composite名 → CompositeEffectManager::PlayOneShot(tag, origin + …)
       // Soundトラック : DopeKey.tag = Audioパス
       // CameraShakeトラック: DopeKey.value = 強度 など
       float age_ = 0.f; int fps_ = 60; ...
   };
   ```
   `ProjectileSpawner`はこの`EffectSequencePlayer`を1個持ち、"Spawn"キーを踏むたびに`ProjectileObject`を1体生成する形に作り替えると、乱数スタッガーで済ませていた発生タイミングを**手で調整・目視確認できる**ようになり、まさに要望の「発射地点/発射タイミング/着弾タイミング/再生時間をタイムラインで確認したい」に直結する。着弾タイミングは`TrajectorySpec::duration`から自動算出して"Impact"トラックに逆算表示（読み取り専用キー）すれば、発射と着弾の両方が1画面で見える。

この5.5の設計により、`AttackFrameConverter`で実証済みのパターン（DopeTrack ⇄ ゲームデータ の相互変換＋既存Editorウィジェットの再利用）をVFX側にも横展開する形になり、新規UIコストはほぼゼロ、必要なのは「データ保持＋ランタイム発火」の2点だけになる。

---

## 6. CPU/GPU/VfxMesh の設計思想の違い、いつ手を入れるか

### 6.1 3系統は「合成の粒度」が根本的に違う

| | YParticle (CPU) | GPUParticle | VfxMesh |
|---|---|---|---|
| 実行主体 | CPU（粒子ループ） | Compute Shader | CPU(頂点生成)＋PS |
| 合成の単位 | 粒子1個の振る舞い | エミッタ全体の固定機能パイプライン | 「メッシュ生成アルゴリズム」の選択＋要素の動き |
| 実装方式 | `IUpdateModule`/`ISpawnModule`の**ポリモーフィズム**を`YParticleSystem`が`vector<shared_ptr<...>>`で保持し毎フレーム順に呼ぶ（Niagaraのモジュールスタックそのもの） | `UpdateParticle.CS.hlsl`という**単一の固定カーネル**に全物理が直書きされ、`ParticleParameters`という数値構造体で挙動を調整するのみ。エミッタ形状(Sphere/Box/Cone/Mesh)も`switch`文で選ぶ固定選択肢 | `LightningMesh`/`ShockwaveMesh`/`VolumeSmokeMesh`等、**形状ごとに別クラス・別シェーダ**。ただし各要素の"動き"は`VfxMotion`という列挙体リストで駆動しており、実質Niagara風モジュールと同じ発想 |
| 新しい振る舞いの追加コスト | 新規`IUpdateModule`実装クラス1個＋`REGISTER_UPDATE_MODULE`（既存に無関係に追加可能） | `UpdateParticle.CS.hlsl`本体を直接編集（既存ロジックとの衝突・肥大化のリスクあり） | 新規`ProceduralMeshBase`派生クラス＋シェーダ＋PSO登録＋Editor統合（5.1で述べた5箇所） |

CPU側だけが「Niagara風モジュール合成」なのは意図的な違いというより、**HLSLに実行時ポリモーフィズムが無い**という制約からくる必然。GPU側で仮想関数のvtableに相当するものは作れないため、CPU側と全く同じ設計は持ち込めない。VfxMeshは「形状生成」という質的に異なるアルゴリズム同士を合成しようとしている時点でNiagaraのモジュールモデル（数値パラメータの積み重ね）とは前提が違う——ジグザグ稲妻とレイマーチ煙は補間可能な"パラメータ違い"ではなく別アルゴリズムなので、ここが「クラスで持つ」設計になるのは自然で、Unreal Niagara自身もRibbon/Mesh Particle/Light Particleは別レンダラ実装であってモジュールでは表現していない。

### 6.2 GPU側だけは改善余地がある

「違いが自然」とはいえ、GPUParticleの**単一巨大カーネル**は将来的に破綻しやすい設計ではある。今`UpdateParticle.CS.hlsl`にVortex/Attractor/ForceField/Turbulence（CPU版に既にある物理）を足そうとすると、1ファイルに分岐がどんどん積み上がる。

対応案は2段階：

1. **安価な折衷案（機能フラグ分岐）**: `ParticleParameters`にビットフラグを足し、カーネル内で`if (flags & FEATURE_VORTEX) { ... }`のように分岐する。エミッタ単位でフラグは全粒子共通（＝同じワープ内で分岐が揃う）なので、警戒されがちな「GPUの分岐コスト」はここでは実質発生しない。実装コストは低いが、カーネル1本に機能を足し続ける根本問題は解消しない。
2. **本命（マルチパス化）**: `GPUEmitter::Dispatch()`は現状`EmitCS`1回＋別途`UpdateCS`1回という構成（`UpdateParticle.CS.hlsl`）。ここに「有効な機能ごとに小さなComputeパスを追加ディスパッチする」形にすると、CPU側の「モジュールを1個ずつ足す」体験にかなり近づく（`VortexCS`/`ForceFieldCS`/`GroundCollisionCS`等を`ComputeShaderManager`に個別登録し、エミッタが持つ"有効パスリスト"の順に`Dispatch`する）。Unreal NiagaraのGPUシミュレーションも実質これと同じ考え方（グラフを実行時ではなくコンパイル時に一連のステージへ変換する）。デメリットはパスを増やすほどUAVバリア＋Dispatch呼び出しのオーバーヘッドが積み重なること。`kMaxParticles=16384`程度なら数パス増える分には実測ベースでほぼ問題にならないと予想されるが、体感/実測での確認は必要。

どちらを選ぶにせよ、**GPU粒子に新しい物理挙動を足す要求が実際に出てから**着手すべきタイプの改修（6.3参照）。

### 6.3 いつ設計に着手すべきか

優先順位は前回の3→4→5→1→2の流れに、今回の6.2を「1（オーサリング改善）」の一部として組み込む形になる。ただし6.2はリスク・コストが他と一段違うので、明確に時期を分けて考えるのを勧める。

- **今すぐ着手して良い（低リスク・高確度）**: 5.3（Composite offset/scale/delay統一）、5.4（CollisionManagerへの一時オーバーラップクエリ追加）、5.5（EffectSequencePlayer新設）、5.1の`VfxElementType::Primitive`追加。いずれも既存クラスの機能追加であり、既存の挙動を壊さない。りゅうせいぐんを作る上でも直接的に効く。
- **りゅうせいぐん実装が具体的に進み、GPU粒子に「地面バウンド」「渦を巻かせたい」等の**具体的な要求が出た時点で着手**: 6.2（GPUのマルチパス化）。今の時点でこれを想像だけで設計すると、実際に必要な機能セットとズレた抽象化を先に作ってしまうリスクが高い（YAGNI）。まずは6.2をやらずに済む範囲（`ProjectileObject`をCPU側の実体として持たせ、GPU粒子は純粋に見た目だけを担当させる、という前回5.4の整理）でりゅうせいぐんを一通り動かしてみて、それでも「GPU粒子自体に新しい物理が要る」と分かった段階で着手するのが安全。
- **さらに後（新しい形状の追加要求が3つ目・4つ目と続いた段階）**: 5.1-2（VfxElementTypeのファクトリ登録マクロ化）。今はLightning/Shockwave/Smoke/LightVolumeの4種類なので、まだ「毎回5箇所触るのがつらい」と言えるほどの回数ではない。次に新しい形状（例: Beam/Decal/Primitive）を2〜3個足す実績を積んでから、共通化のパターンを固めた方が、先に作ったファクトリ抽象が実際のニーズに合わなくて手戻りする事態を避けられる。

まとめると、**5.3/5.4/5.5とPrimitive要素追加は今着手して良い基盤整備、GPUのマルチパス化とVfxElementTypeファクトリ化は「具体的な2件目・3件目の要求が出てから」の後追い設計にする**、という時期の切り分けを推奨する。

---

## 7. エフェクト量産の前に何からやるか（着手順序）

「大量生産する前に直す」という観点で並べ替えると、優先度は**「後から直すと量産済みの全アセットに手戻りが波及するもの」を最優先**にするのが基本方針になる。

### Phase 0（最優先・量産を始める前に必ず） — 5.3 Composite統一
`CompositeVfxRef`相当のoffset/scale/delayをparticle/gpuにも揃え、`Play`/`PlayOneShot`に`scale`/`durationScale`を渡せるようにする。**ここが一番後回しにできない理由**: これから作る全ての攻撃エフェクトはComposite JSON上に定義される前提なので、この仕組みを直す前に量産すると、量産した全JSONを後で作り直す羽目になる。逆にここさえ直っていれば、以降のエフェクト追加は素直にJSONを増やすだけで済む。

### Phase 1（量産と並行で早めに） — 5.4 当たり判定クエリ / 5.1-1 Primitive要素
- `CollisionManager::QuerySphere`等の一時オーバーラップクエリ。攻撃エフェクトは「ダメージを与える」役目とセットなので、これが無いまま量産すると攻撃ごとにダメージ判定を個別実装する羽目になる（Compositeと同じ「後で全部直す」パターン）。
- `VfxElementType::Primitive`。今回の発端である「爆発のたびに新規クラスを書く」を直接軽減し、量産フェーズの体感速度に一番効く。

### Phase 2（りゅうせいぐんなど"移動して着弾する"攻撃に着手する時） — 3.1/3.2 TrajectoryCurve + ProjectileObject
全ての攻撃に要るわけではない（其の場で発生する斬撃/爆発はPhase 0/1だけで量産できる）。「発射→着弾」系の攻撃を最初に1つ作るタイミングで着手すれば良い。5.5 EffectSequencePlayerは、このPhase 2をやってみて「タイミング調整がつらい」と実感してから足すので十分（先に作ると使われないまま複雑さだけ増えるリスクがある）。

### Phase 3（デマンド駆動・急がない） — GPUマルチパス化 / VfxElementTypeファクトリ化 / Model-importトレイル / BeamMesh・DecalMesh
6.3で述べた通り、具体的な要求が2〜3件積み上がってから着手。今すぐ設計すると実際のニーズとズレる可能性が高い。

**結論**: 今日から手を付けるべきは **Phase 0（Composite統一）**。ここだけは量産前に必ず終わらせ、Phase 1の2点は量産の初速を上げるために並行して進めるのが良い。

### 7.1 補足: Phase 1で寿命(lifetime)も揃えるべきか

「揃える」には性質の違う2つの意味があり、混同すると設計を誤る。

- **(a) 全体のテンポを比例で伸縮する** — Phase 0の`durationScale`が担当。「このComposite全体を1.5倍ゆっくり再生」のような一括調整で、各子の**相対的な長さの比**は保つ。
- **(b) 全ての子を同じ時刻に終わらせる（寿命を強制的に一致させる）** — これは**やらない方がいい**。爆発なら「衝撃波は0.3秒でパッと消え、煙は3秒かけて漂う」のように**意図的に寿命をズラす**のが良い演出であり、強制的に揃えるとむしろ表現の幅が狭まる（今回の目的そのものと逆行する）。`CompositeInstance::IsActive()`が「どれか1つでも生きていればtrue」なのは正しい挙動なので、ここは今のままで良い。

一方で、Phase 1の当たり判定クエリ（5.4）には**別の意味での"揃える"が必要**になる。今のままだと「ダメージ判定をいつ発火するか」を決める基準が無く、実装時に「Playした瞬間」か「特定の子エフェクトの寿命に便乗する」のどちらかになりがちだが、後者は「あの子エフェクトの寿命を変えたらダメージタイミングまでズレた」という事故を生む（見た目の調整とゲームプレイのタイミングが暗黙結合してしまう）。

→ **対応**: `CompositeEffectAsset`に見た目の寿命とは独立した`hitDelay`（Playからダメージクエリ発火までの秒数）と`hitRadius`/`damageLayerMask`を持たせ、5.4で追加する`CollisionManager::QuerySphere`をこの`hitDelay`経過時点で呼ぶ。見た目側の寿命（各子のJSON）とは完全に切り離した、ダメージ専用の時刻を1個だけ持たせる形にする。これなら煙の長さを演出調整で変えてもダメージタイミングは変わらないし、逆にダメージタイミングを合わせたい時は`hitDelay`だけ触ればよい。5.5の`EffectSequencePlayer`が入ればこの1個の値は複数キーに拡張できるが、Phase 1時点ではこの最小追加で十分。

まとめ: **比例スケール(a)は揃える(Phase 0で対応済み)、強制同期(b)は揃えない、ダメージ発火時刻だけは独立した専用フィールドで揃える**、というのがPhase 1での結論。

### 7.2 補足: 「攻撃が終わる前にエフェクトが終わる」を防ぐ最小保証寿命

これは7.1の(a)(b)とも別物で、**「エフェクトの寿命 ≥ 攻撃の長さ」という片方向の下限保証**が要件になる。攻撃側の尺より長い分には（煙が漂い続ける等）問題なく、短くなることだけがまずい。これは強制同期(b)ではなく、Phase 0の`durationScale`をベースにした自動計算で実現できる。

**ケースを2つに分ける**:

1. **ループ型（`Play(loop=true)`＋手動`Stop()`）**: これは元々「攻撃状態が終わるまで生かす」設計そのものなので、寿命フィールドは不要。`AttackingCombatState`（や各コンボの終了処理）が確実に`Stop()`を呼んでいるかどうかの**運用上の徹底**の問題であり、正しく呼ばれていれば既に「攻撃が終わる前にエフェクトが終わる」ことは起きない。ここに新しい仕組みは足さず、Exit処理の`Stop()`漏れが無いかのレビュー観点として扱う。
2. **ワンショット型（`PlayOneShot`）で「攻撃中はずっと見えていてほしい」もの**: こちらが今回の本題。VfxMeshEditorでプレビュー単体で尺を決めて保存したアセットが、実際の攻撃の総フレーム数より短いまま量産される事故が起きうる。

→ **対応**: `CompositeEffectAsset`に**全チャイルドの自然な最大寿命を返す`NaturalDuration()`**を足し、`PlayOneShot`/`Play`に`minDuration`（攻撃側の要求尺）を渡せるようにする。内部では

```cpp
float natural = asset.NaturalDuration();
float scale = (minDuration > 0.0f && natural > 0.0f)
    ? std::max(1.0f, minDuration / natural)
    : 1.0f;
// このscaleを7.1で作るdurationScale経路にそのまま渡す
```

という**「足りない時だけ伸ばす」片方向の自動スケール**にする。すでに攻撃より長いアセットは触らない＝7.1(b)で否定した「強制同期」にはならない。呼び出し側は`CompositeEffectManager::PlayOneShot("MeteorImpact", pos, /*minDuration*/ atk.totalFrames / (float)atk.fps)`のように、**既存の`AttackData::totalFrames`/`fps`をそのまま渡すだけ**で済む（新しい尺データを別途持つ必要がない）。

`NaturalDuration()`の集計は子システムごとに材料が揃っているかがバラバラ:

- **VfxMesh**: `VfxEffectAsset::OneShotDuration()`が既にあるのでそのまま使える。
- **GPUParticle**: `GpuEmitManager`に既に`GetGroupMaxLifetime(const EmitterGroup*)`という同種の見積り関数がある（linger時間の算出に使っている private 関数）。これを`public`な`EstimateGroupNaturalDuration(groupName)`として露出すればそのまま転用できる。
- **YParticle（CPU）**: 同等の見積り関数がまだ無い。`SpawnLifeTime`モジュールの寿命レンジを見て概算する`float YParticleSystem::EstimateNaturalDuration() const`を新規に足す必要がある（完全に正確でなくて良く、「下限保証のための概算」で十分）。

まとめ: 揃えるのは寿命そのものではなく、**「攻撃の尺を下回らないようにする自動引き伸ばし」**。Phase 0のdurationScale機構をそのまま使い回せるので、Phase 0/1の実装に**追加の小さな1機能**として乗せるのが妥当（別フェーズに切り出すほどの規模ではない）。

---

## 8. 実装状況（2026-07-11時点）

Phase 0 と Phase 1 の「当たり判定クエリ」部分を実装済み。以下、実際に変更したファイルと、意図的にスコープ外にした部分を記録する。**MSBuildでのビルド確認はできていない（Windows/DirectX12ツールチェーンがこちらの作業環境に無いため）。実機でのDebugビルド確認をお願いしたい。**

### 実装したもの

- **`CollisionManager::QuerySphere`**（`Collision/Core/CollisionManager.h/.cpp`）: 5.4で設計した一時オーバーラップクエリ。`colliders_`線形走査＋`ComputeWorldAABB`とのSphere-AABB判定という最小実装（`UniformGrid`のブロードフェーズ高速化はせず、まずは正しさ優先）。
- **`VfxMeshSpawner`のtimeScale対応**（`Vfx/VfxMesh/Runtime/VfxMeshSpawner.h/.cpp`）: `ActiveEffect::timeScale`を追加し、`UpdateEffect`内で`dt`をtimeScale倍してから`age`加算・各Mesh Update呼び出しに使う。`Spawn`/`SpawnBolt`/`SetTimeScale`/`GetAsset`（NaturalDuration集計用）を追加。
- **`VfxMeshHandle`**: `Play`/`PlayOneShot`/`PlayBolt`にtimeScale引数（末尾・既定1.0）を追加、`SetTimeScale`を追加。
- **`GpuEmitManager::EstimateGroupNaturalDuration`**: 既存private `GetGroupMaxLifetime`のpublicラッパー。
- **`CompositeEffectManager`/`CompositeEffectAsset`**（Phase 0の本体）:
  - `particleOffset`/`gpuOffset`を追加（vfxMeshAssetsは元々offsetを持っていたので、これでparticle/gpu/vfxMesh全系統がオフセットを持てるようになった）。
  - `hitDelay`/`hitRadius`/`hitLayerMask`を追加。`Update(float dt)`を新設し、`hitDelay`経過後に`CollisionManager::QuerySphere`を1回だけ呼ぶ（結果は`PlayParams::onHitQuery`コールバックへ渡すのみ。ダメージ適用はゲーム側の責務のまま＝CompositeEffectManagerはHP/ダメージAPIを知らない設計を維持）。
  - `cameraShakeProfile`/`hitStopMs`を**データ項目として**追加（後述の通り発火配線は未実装）。
  - `PlayParams{ minDuration, onHitQuery }`付きの`Play`/`PlayOneShot`オーバーロードを追加（既存の2引数版は内部でこれらに委譲、後方互換）。`minDuration`指定時はVfxMesh子だけ`NaturalDuration()`との比較で自動的にtimeScaleを掛けて引き伸ばす（7.2の設計通り、強制同期はしない・足りない時だけ伸ばす）。
  - `CompositeEffectAsset::NaturalDuration()`を追加（VfxMeshは正確値、GPUは概算値、CPUパーティクルは後述の理由で0固定）。
- **配線**: `GameScene.cpp`/`DevelopScene.cpp`の`Update()`に`CompositeEffectManager::GetInstance()->Update(dt)`を追加（`VfxMeshSpawner::Update`と同じ場所、`CollisionManager::Update()`より後）。

### 意図的にスコープ外にしたもの（今回未実装）

- **CPUパーティクル(`particleEffect`)のNaturalDuration/timeScale**: `YParticleSystem`は名前ごとの共有シングルトンで、`PlayOneShot`はその共有バッファに撃ち込むだけ（呼び出しごとの専用インスタンスが無い）。そのため「この1回の再生だけ寿命を伸ばす」が構造的にできない。ワークログにも既出の「System=定義/インスタンス=粒バッファの分離が必要」という設計債務が前提になるため、今回は着手せず`NaturalDuration()`はGPU/VfxMeshだけを見る。
- **particle/gpuの複数子対応**: 今回はもともとの単数フィールド（`particleEffect`/`gpuEmitterGroup`）にoffsetだけ足す最小変更にとどめた。1つのComposite内で同系統を複数（例: GPU粒子を2グループ重ねる）持たせたい場合は、`vfxMeshAssets`と同じ配列パターンへの拡張が別途必要。
- **子ごとのdelay（発生タイミングのずらし）**: 3.6/5.3で触れた「オフセット・スケール・delay」のうちdelayは見送った。実装には毎フレームtickする待ち行列＋`Stop()`との競合処理（delay中にStopされた場合に後から生成してしまわないためのフラグ管理）が要り、hitDelay用に作った待ち行列基盤を流用すれば追加は難しくないが、今回の直接の要望（位置・寿命の統一）には必須でなかったため次回に回した。
- **カメラシェイク/ヒットストップの実発火**: `cameraShakeProfile`/`hitStopMs`はデータフィールドとして追加したのみ。`Camera::Shake(time, min, max)`は存在するが、名前付き「プロファイル」レジストリの有無や、`CompositeEffectManager`からアクティブなカメラへどう参照を渡すかが未確認だったため、誤った配線をするより明示的にTODOとして残す判断をした。
- **`VfxElementType::Primitive`・VfxElementTypeファクトリ化・GPUマルチパス化**: 6.3/7で「デマンド駆動」と位置付けた通り、今回のスコープには含めていない。

### 次にやること

1. **Debugビルドで確認**: 特に`CollisionManager::QuerySphere`（Sphere-AABB判定・layerMask/ignoreTypeIDsの扱い）と`CompositeEffectManager`のJSON後方互換（旧形式JSONに`particleOffset`等が無くても既定値で読めるか）を重点的に見てほしい。
2. **`ProjectileObject`/`TrajectoryCurve`（Phase 2）**: りゅうせいぐん本体。`hitDelay`/`onHitQuery`はここから使う想定（着弾時に`PlayParams::onHitQuery`でダメージを適用するコールバックを渡す形）。
3. **子ごとのdelay**: hitDelay用の待ち行列パターンを流用して追加。

---

## 9. GPUパーティクル: アクセラレーションフィールド（元気玉のような収束エフェクト用）

CPU側`YParticle`には`UpdateAttractor`/`UpdateForceField`/`UpdateVortex`という「空間内の力場」モジュールが既にあるが、GPU側（`UpdateParticle.CS.hlsl`）には重力とトレイル生成しかなく、空間領域に基づく加速・収束が一切無い。元気玉のように「散らばった粒がある一点/領域へ加速しながら集まる」演出はGPU側の物理モデルにこの機能を足さないと作れない。6.3で示した「GPU粒子に新しい物理挙動を足す具体的要求が出た段階で着手する」に該当するので、ここで設計する。

### 9.1 必要な機能

**フィールドの形状**: AABB / Sphere の2種（既存の`EmitterShape`と同じ感覚）。

**フィールドの効果モード**:
- `DirectionalAccel`: 指定方向へ一定加速度（風・噴射などに使える汎用版）
- `ConvergeToCenter`: フィールド中心へ向かって加速（元気玉の核心機能）
- `RadialRepel`: 中心から外側へ加速（オプション。爆風/衝撃波の粒子演出にも転用できる）

**元気玉を作るのに`ConvergeToCenter`単体では足りない点**:
1. **中心付近でのオーバーシュート/振動対策**が要る。単純に「中心方向へ加速」だけだと、中心を通り過ぎて反対側へ飛び出し往復振動する。`maxSpeed`（速度上限）と、中心に近づくほど加速度を弱める`falloff`カーブが要る。
2. **渦を巻きながら収束する見た目**が欲しくなるはず（元気玉はまっすぐ一直線に集まるより、少し渦を巻きながら吸い込まれる方がそれらしい）。これは`ConvergeToCenter`に「中心方向ベクトルと直交する接線方向の速度成分」を`spiralStrength`で混ぜることで実現できる（CPU版`UpdateVortex`の考え方をConvergeに部分的に混ぜ込む形）。
3. **中心に到達した粒をどう終わらせるか**。到達点で急に消えると点滅して見えるので、`killRadius`（この半径以下になったら死亡）＋子システムのトレイル（既存の`EmitterTrailParams`）を併用し、「吸い込まれる軌跡だけ光の尾を引いて本体は静かに消える」ようにするのが自然。中心の「本体が育っていく」感じは、GPU粒子とは別にVfxMeshの`VolumeSmokeMesh`か`LightVolumeMesh`をCompositeで重ね、チャージ時間に応じてスケールが育つ`VfxMotion(ScaleOverLife)`を仕込めば表現できる（GPU粒子側に「中心の球を育てる」機能を持たせる必要はない）。

### 9.2 データ設計

```cpp
// C++側 (GpuParticleParams.h に追加)
enum class GpuFieldShape : uint32_t { AABB = 0, Sphere = 1 };
enum class GpuFieldMode  : uint32_t { DirectionalAccel = 0, ConvergeToCenter = 1, RadialRepel = 2 };

struct GpuForceFieldParams {
    GpuFieldShape shape   = GpuFieldShape::Sphere;
    Vector3       center  = { 0.f, 0.f, 0.f };
    Vector3       halfExtents = { 5.f, 5.f, 5.f };  // AABB用
    float         radius  = 5.0f;                    // Sphere用

    GpuFieldMode  mode    = GpuFieldMode::ConvergeToCenter;
    Vector3       direction = { 0.f, 1.f, 0.f };      // DirectionalAccel用
    float         strength = 10.0f;                   // 加速度の大きさ

    float         falloff      = 0.5f;   // 0=一定 / >0で中心に近いほど加速度を弱める
    float         spiralStrength = 0.0f; // Converge時の接線方向成分（渦を巻きながら収束）
    float         maxSpeed     = 0.0f;   // 0=無制限。中心付近でのオーバーシュート防止
    float         killRadius   = 0.0f;   // 0=無効。この半径以下で粒を消滅させる（Converge用）
};
```

1エミッタグループにつき最大数個（例: 4個）まで持てるようにし、`GpuEmitManager::EmitterGroup`に`std::vector<GpuForceFieldParams> forceFields;`として追加、JSON/エディタで編集できるようにする。

### 9.3 GPU側の実装方針

- **新しいDispatchパスは作らない**: フィールド数は高々数個で、しかも同じディスパッチ内の全粒子が同じフィールドリストを参照する（＝ワープ内で分岐が揃う、6.2で書いた「安価な折衷案」がそのまま当てはまる）。既存の`UpdateParticle.CS.hlsl`の重力適用の直後に、フィールド数分のforループを足すだけで済む。
- 追加リソース: `StructuredBuffer<GpuForceField> g_ForceFields`（フィールド配列）＋ 個数を`PerFrame`かフィールド専用の小さいCBに追加。`GPUParticle::DispatchUpdate`のルートシグネチャ（`ParticleUpdateCS`、現在ルートパラメータ0〜5使用）に新しいSRVディスクリプタテーブルを1本追加する必要がある（`ComputeShaderManager.cpp`のルートシグネチャ定義を変更）。ここはDirectX12のルートシグネチャ変更＝バインド順序を間違えると即クラッシュ/化けるので、他の変更より慎重にやる必要がある。
- HLSL側の力場適用（概念コード）:
  ```hlsl
  for (uint f = 0; f < g_ForceFieldCount; f++) {
      GpuForceField field = g_ForceFields[f];
      float3 toCenter = field.center - particle.translate;
      float dist = length(toCenter);
      bool inside = (field.shape == SHAPE_SPHERE)
          ? dist <= field.radius
          : all(abs(particle.translate - field.center) <= field.halfExtents);
      if (!inside) continue;

      float3 accelDir = (field.mode == MODE_DIRECTIONAL) ? normalize(field.direction)
                        : (dist > 0.001) ? toCenter / dist : float3(0,0,0);
      if (field.mode == MODE_REPEL) accelDir = -accelDir;

      float falloffScale = (field.falloff > 0.0001)
          ? lerp(1.0, saturate(dist / field.radius), field.falloff) : 1.0;

      float3 accel = accelDir * field.strength * falloffScale;
      if (field.mode == MODE_CONVERGE && field.spiralStrength > 0.0001) {
          float3 tangent = normalize(cross(accelDir, float3(0,1,0)));
          accel += tangent * field.strength * field.spiralStrength;
      }
      particle.velocity += accel * dt;

      if (field.maxSpeed > 0.0001) {
          float sp = length(particle.velocity);
          if (sp > field.maxSpeed) particle.velocity *= field.maxSpeed / sp;
      }
      if (field.mode == MODE_CONVERGE && field.killRadius > 0.0001 && dist <= field.killRadius) {
          isActive = 0; // 通常の死亡処理へ（FreeListへ返却）
      }
  }
  ```

### 9.4 元気玉の作り方（この機能が揃った前提のレシピ）

1. **発生**: `EmitterShape::Sphere`で大きめの半径（例: 半径8〜10）から粒を散発的に発生させる。色は明るいHDRの黄〜白。
2. **収束フィールド**: 元気玉本体の中心座標を`center`、`radius`をSphere発生範囲より少し大きめに設定した`ConvergeToCenter`フィールドを1つ配置。`strength`と`falloff`でチャージの「加速していく感じ」を調整（falloffを効かせると外側はゆっくり、中心に近づくほど速く吸い込まれる）。
3. **渦**: `spiralStrength`を0.2〜0.5あたりから調整し、まっすぐ収束ではなく少し巻き込むように。
4. **終端処理**: `killRadius`を中心近くの小さい値に設定し、到達したら消える。既存の`EmitterTrailParams`（`isTrail=true`）を有効にして、各粒が飛来中に光の尾を引くようにすると密度感が出る。
5. **中心の本体**: 別途Composite側で`VolumeSmokeMesh`か`LightVolumeMesh`を中心に置き、チャージ時間ぶんの`VfxMotion(ScaleOverLife)`で徐々に膨らませる。GPU収束粒子はあくまで「集まってくる光」担当、中心の球本体はVfxMesh担当、という役割分担にする（既存の「点/スパーク→YParticle・高数量→GPU・継続ボリューム→VfxMesh」という使い分け方針そのまま）。
6. **発射**: チャージ完了後、収束フィールドを止め、`TrajectoryCurve`（3.1）を使った`ProjectileObject`（3.2）として本体を対象へ飛ばす。飛翔中の光跡はGPUトレイル、着弾はCompositeの`ExplosionShockwave`＋`hitDelay`ダメージクエリ、という形で今回実装した仕組み一式にそのまま繋がる。

### 9.5 実装時の注意

ルートシグネチャ変更を伴うため、他の機能追加より一段リスクが高い。着手する際は次の順でやるのが安全:
1. まず`GpuForceFieldParams`と数個までの配列をC++側だけで用意し、JSON/エディタで編集できるようにする（GPU側は素通し）。
2. `ParticleUpdateCS`のルートシグネチャに1つだけSRVテーブルを足し、`DispatchUpdate`のバインドを対応させる（他のルートパラメータのインデックスは変更しない＝末尾に追加）。
3. HLSL側でフィールド数0のガード（`g_ForceFieldCount == 0`なら何もしないループ）を必ず入れ、既存エフェクト（フィールド未設定）の見た目が一切変わらないことを確認してから、実際に元気玉用のテストデータで検証する。

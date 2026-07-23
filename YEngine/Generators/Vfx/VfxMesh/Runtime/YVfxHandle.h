#pragma once
// ===========================================================
// YVfxHandle.h
//
// ゲームコードから VfxMesh 系エフェクトをワンライナーで
// 再生・操作する軽量ハンドル。
// 内部では VfxMeshSpawner に ID で問い合わせるだけなので
// コピー・ムーブが自由にできる。
//
// 使い方:
//   // ワンショット爆発
//   YVfxHandle::PlayOneShot("Explosion", blastPos, 1.5f);
//
//   // ループ稲妻（毎フレーム端点を追従）
//   boltHandle_ = YVfxHandle::PlayBolt("Lightning", startPos, endPos, true);
//   boltHandle_.SetEndpoints(swordBase, swordTip);
//   boltHandle_.Stop();
//
//   // スモーク（位置指定）
//   smokeHandle_ = YVfxHandle::Play("Smoke", hitPos, 2.0f, true);
//   smokeHandle_.SetPosition(newPos);
// ===========================================================
#include "Vector3.h"
#include <string>
#include <cstdint>

class YVfxHandle
{
public:
    YVfxHandle() = default;
    ~YVfxHandle() = default;

    // ── ファクトリ ────────────────────────────────────────────

    /// 位置 + スケール指定で再生する（スモーク・衝撃波など球系向け）
    /// timeScale: age の進み方の倍率。<1 で寿命を引き伸ばす
    static YVfxHandle Play(const std::string& assetName,
                           const Vector3&     position,
                           float              scale     = 1.0f,
                           bool               loop      = false,
                           float              timeScale = 1.0f);

    /// ハンドルを返さないワンショット再生（戻り値不要の場合）
    static void PlayOneShot(const std::string& assetName,
                            const Vector3&     position,
                            float              scale     = 1.0f,
                            float              timeScale = 1.0f);

    /// 再生するエフェクト・出す位置・寿命(秒)を指定してその場で 1 発発生させる。
    /// この関数を呼ぶたびに新しいエフェクトが Emit される（撃ちっぱなし / fire-and-forget）。
    ///   assetName : 再生するエフェクト名（ScanDirectory 済みのアセット）
    ///   position  : 発生位置（ワールド座標）
    ///   lifetime  : 寿命(秒)。この秒数でバースト進捗 0→1 が完結し自動で破棄される。
    ///               <=0 を渡すとアセット既定の寿命を使う。
    ///   scale     : 大きさ（省略時 1.0）
    static void Emit(const std::string& assetName,
                     const Vector3&     position,
                     float              lifetime,
                     float              scale = 1.0f);

    /// 始点・終点指定で再生する（稲妻など方向性エフェクト向け）
    static YVfxHandle PlayBolt(const std::string& assetName,
                               const Vector3&     start,
                               const Vector3&     end,
                               bool               loop      = false,
                               float              timeScale = 1.0f);

    // ── 操作 ─────────────────────────────────────────────────

    /// エフェクトの位置を更新する
    void SetPosition(const Vector3& pos);

    /// エフェクトのスケールを更新する
    void SetScale(float scale);

    /// 方向性エフェクト（稲妻など）の始点・終点を更新する
    void SetEndpoints(const Vector3& start, const Vector3& end);

    /// age の進み方の倍率を変更する（<1 でスローモーション）
    void SetTimeScale(float timeScale);

    /// エフェクトを即座に停止・破棄する
    void Stop();

    // ── クエリ ───────────────────────────────────────────────

    /// ハンドルが有効（Spawn に成功しているか）
    bool IsValid() const { return id_ != 0; }

    /// エフェクトがまだ生存しているか（ワンショット完了後は false）
    bool IsAlive() const;

private:
    YVfxHandle(uint32_t id) : id_(id) {}

    uint32_t id_ = 0; ///< VfxMeshSpawner が発行する一意 ID（0=無効）

    friend class VfxMeshSpawner;
};

#pragma once

#include "../TriggerAction.h"
#include "Vector3.h"

#include <functional>
#include <string>

// ============================================================
// OpenGateAction
//   指定の敵グループが撃破された通知を受け取ると、
//   名前で参照したターゲット PlacedObject の transform を
//   "閉じている状態" から "開いている状態" へ滑らかに補間する。
//
//   開放アニメはコード駆動 (アニメクリップを使わない):
//     - openOffsetPosition / openOffsetRotationDeg / openOffsetScale を
//       元の値に対する "差分" として、duration 秒で線形補間する。
//     - 補間中はターゲットの worldTransform が毎フレーム更新される
//       (collider も自動追従する)。
//
//   完了時に collider を無効化し、onGateOpened_ を発火する。
//
//   閉位置の永続化:
//     - closedPosition_ / Rotation / Scale をトリガー JSON に保存する。
//     - 起動時の RestoreClosed() で target を強制的にこの pose へ戻す。
//     - これにより、ゲーム中に動いた transform が誤って Field.json へ
//       保存されてしまっても、次回起動時に初期 (閉) 状態に戻る。
// ============================================================
class OpenGateAction : public TriggerAction {
public:
	enum class Phase {
		Waiting,    // 撃破通知待ち
		Opening,    // 補間中
		Opened,     // 完了
	};

	// 必須: 対象オブジェクトの nameTag と、条件となる敵グループ名。
	// requiredCount: 撃破回数の閾値 (デフォルト 1)。
	OpenGateAction(std::string targetName, std::string requiredGroup, int requiredCount = 1);

	// 開放完了時のコールバック (必要なら Scene 側で NavMesh 更新などを渡す)
	void SetOnGateOpened(std::function<void()> cb) { onGateOpened_ = std::move(cb); }

	// 外部システムの撃破通知から叩く
	void NotifyEnemyDefeated(const std::string& group);

	void Update(float deltaTime) override;

	nlohmann::json SerializeToJson() const override;
	std::string    GetTypeName() const override { return "OpenGate"; }
	bool           NeedsSpatialPlacement() const override { return false; }

	// エディタ用 setter / getter
	void SetTargetName    (const std::string& s) { targetName_    = s; }
	void SetRequiredGroup (const std::string& s) { requiredGroup_ = s; }
	void SetRequiredCount (int n)                { requiredCount_ = (n > 0 ? n : 1); }
	void SetOpenOffsetPosition(const Vector3& v) { openOffsetPosition_    = v; }
	void SetOpenOffsetRotationDeg(const Vector3& v) { openOffsetRotationDeg_ = v; }
	void SetOpenOffsetScale(const Vector3& v)    { openOffsetScale_       = v; }
	void SetOpenDuration  (float t)              { openDuration_ = (t > 0.0f ? t : 0.0001f); }

	// 閉位置の手動設定 (ターゲット現在位置を「閉」として記録するエディタボタンから使う)
	void SetClosedPose(const Vector3& pos, const Vector3& rot, const Vector3& scale);
	bool HasClosedPose() const { return closedCaptured_; }
	const Vector3& GetClosedPosition() const { return closedPosition_; }
	const Vector3& GetClosedRotation() const { return closedRotation_; }
	const Vector3& GetClosedScale()    const { return closedScale_; }

	const std::string& GetTargetName()    const { return targetName_; }
	const std::string& GetRequiredGroup() const { return requiredGroup_; }
	const Vector3&     GetOpenOffsetPosition()    const { return openOffsetPosition_; }
	const Vector3&     GetOpenOffsetRotationDeg() const { return openOffsetRotationDeg_; }
	const Vector3&     GetOpenOffsetScale()       const { return openOffsetScale_; }
	float              GetOpenDuration()          const { return openDuration_; }

	// プレビュー: 撃破条件を無視して即座に開放アニメを再生する
	void TriggerPreview();

	// 「閉」位置 (= 初期 pose) へターゲットを強制的に戻す。
	// EventTriggerLoader 完了直後、毎回呼ぶことでゲーム再起動時に初期化される。
	// closedCaptured_ が false の場合は target の現在 transform を「閉」として捕捉する。
	void RestoreClosed();

	// 状態リセット (エディタ用)。RestoreClosed と同じ動作 (target を閉位置へ戻して Waiting に)
	void Reset();

	Phase GetPhase() const { return phase_; }
	int   GetCurrentCount() const { return currentCount_; }
	int   GetRequiredCount() const { return requiredCount_; }

private:
	void BeginOpening();
	void TickOpening(float dt);
	void CaptureClosedPoseFromTarget(); // target の現在 transform を closed として記録

private:
	std::string targetName_;
	std::string requiredGroup_;
	std::function<void()> onGateOpened_;

	Phase phase_ = Phase::Waiting;
	int   requiredCount_ = 1;
	int   currentCount_  = 0;
	bool  defeatSignal_  = false;

	// 開放モーション (元 = 閉位置に対する差分)
	Vector3 openOffsetPosition_    = { 0.0f, 0.0f, 0.0f };
	Vector3 openOffsetRotationDeg_ = { 0.0f, 0.0f, 0.0f };
	Vector3 openOffsetScale_       = { 0.0f, 0.0f, 0.0f };
	float   openDuration_          = 1.0f;

	// 閉位置 (永続化される)
	bool    closedCaptured_  = false;
	Vector3 closedPosition_  = {};
	Vector3 closedRotation_  = {};
	Vector3 closedScale_     = { 1.0f, 1.0f, 1.0f };

	// 補間中の内部状態
	float   elapsed_         = 0.0f;
	int     cachedTargetId_  = -1; // 取得済みターゲットの ObjectManager ID
};

#pragma once
#include <map>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map> // ターゲット登録用に必要
#include "Virtuals/VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>

// ============================================================
// カメラディレクタークラス
// 全ての仮想カメラを管理し、切り替えや補間（ブレンド）を行う
// ============================================================
class CameraDirector
{
public:
	// ============================================================
	// 設定用構造体
	// ============================================================
	struct BlendSettings {
		bool useBlending = true;     // 補間を行うかどうか
		float duration = 0.5f;       // 補間にかかる時間
		int easingType = 0;          // 0: Linear, 1: EaseInOut など
	};

public:
	// ============================================================
	// 基本関数
	// ============================================================
	static CameraDirector* GetInstance();

	void Initialize();
	void Update(float deltaTime);

	// ============================================================
	// カメラ管理操作
	// ============================================================
	void AddCamera(const std::string& name, std::shared_ptr<VirtualCamera> camera);
	std::shared_ptr<VirtualCamera> GetCamera(const std::string& name);
	void SetPriority(const std::string& name, int priority);

	// 補間をせずに現在のカメラに強制スナップする
	void SnapToActiveCamera();

	// ============================================================
	// ターゲット管理（プレイヤーや敵の座標をカメラが探せるようにする）
	// ============================================================
	void RegisterTarget(const std::string& name, const WorldTransform* transform) {
		targetRegistry_[name] = transform;
	}

	const WorldTransform* FindTarget(const std::string& name) const {
		if (targetRegistry_.count(name)) return targetRegistry_.at(name);
		return nullptr;
	}

public:
	// ============================================================
	// アクセッサ・設定
	// ============================================================
	void SetBlendSettings(const BlendSettings& s) { blendSettings_ = s; }
	void SetBlendDuration(float duration) { blendDuration_ = duration; }
	void SetEnableBlending(bool enable) { enableBlending_ = enable; }

	bool IsBlendingEnabled() const { return enableBlending_; }

	const Matrix4x4& GetViewProjectionMatrix() const { return finalVP_; }
	const Matrix4x4& GetViewMatrix() const { return activeCamera_->GetViewMatrix(); }

	const std::shared_ptr<VirtualCamera>& GetActiveCamera() const { return activeCamera_; }
	const std::shared_ptr<VirtualCamera>& GetPrevCamera() const { return prevCamera_; }

	float GetFovY() const { return currentFovY_; }
	const Vector3& GetActiveCameraPos() const { return activeCameraPos_; }
	const Vector3& GetActiveCameraRot() const { return activeCameraRot_; }

	const std::map<std::string, std::shared_ptr<VirtualCamera>>& GetAllCameras() const { return cameras_; }

private:
	// ============================================================
	// 内部処理・禁止事項
	// ============================================================
	CameraDirector() = default;
	~CameraDirector() = default;
	CameraDirector(const CameraDirector&) = delete;
	CameraDirector& operator=(const CameraDirector&) = delete;

	void RefreshActiveCamera();

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	std::map<std::string, std::shared_ptr<VirtualCamera>> cameras_;

	std::shared_ptr<VirtualCamera> activeCamera_ = nullptr; // 現在メインとなっているカメラ
	std::shared_ptr<VirtualCamera> prevCamera_ = nullptr;   // 1つ前のカメラ（補間用）

	std::unordered_map<std::string, const WorldTransform*> targetRegistry_;
	BlendSettings blendSettings_;

	// 補間（ブレンド）用
	bool isBlending_ = false;
	float blendTimer_ = 0.0f;
	float blendDuration_ = 1.0f;
	bool enableBlending_ = true;

	// 最終的な出力パラメータ
	Matrix4x4 finalVP_;
	float currentFovY_;
	Vector3 activeCameraPos_;
	Vector3 activeCameraRot_;
};
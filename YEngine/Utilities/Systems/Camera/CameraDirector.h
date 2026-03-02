#pragma once
#include <map>
#include <string>
#include <memory>
#include <vector>
#include "Virtuals/VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>

class CameraDirector
{
public:

	// --- 補間設定 ---
	struct BlendSettings {
		bool useBlending = true;     // そもそも補間するか
		float duration = 0.5f;       // 補間時間
		// 0: Linear, 1: EaseInOut ... などのID
		int easingType = 0;
	};
	void SetBlendSettings(const BlendSettings& s) { blendSettings_ = s; }

	///************************* 基本関数 *************************///
	static CameraDirector* GetInstance();

	void Initialize();
	void Update(float deltaTime);

	// カメラの登録
	void AddCamera(const std::string& name, std::shared_ptr<VirtualCamera> camera);

	// 名前でカメラを取得
	std::shared_ptr<VirtualCamera> GetCamera(const std::string& name);

	// 優先度の変更
	void SetPriority(const std::string& name, int priority);

	// ブレンド時間の設定
	void SetBlendDuration(float duration) { blendDuration_ = duration; }

	// 補間をせずに現在のカメラに固定する
	void SnapToActiveCamera();

	// --- ターゲット管理 ---
	// オブジェクト側が「自分はここ（ポインタ）にいるよ」と登録する
	void RegisterTarget(const std::string& name, const WorldTransform* transform) {
		targetRegistry_[name] = transform;
	}

	// カメラ側が「名前」から「ポインタ」を探すために使う
	const WorldTransform* FindTarget(const std::string& name) const {
		if (targetRegistry_.count(name)) return targetRegistry_.at(name);
		return nullptr;
	}
public:
	///************************* アクセッサ *************************///
	// 最終的な描画に使う行列とFOV
	const Matrix4x4& GetViewProjectionMatrix() const { return finalVP_; }
	const Matrix4x4& GetViewMatrix() const {return activeCamera_->GetViewMatrix();}
	const std::shared_ptr<VirtualCamera>& GetActiveCamera() const { return activeCamera_; }
	const std::shared_ptr<VirtualCamera>& GetPrevCamera() const { return prevCamera_; }

	float GetFovY() const { return currentFovY_; }
	const Vector3& GetActiveCameraPos() const { return activeCameraPos_; }
	Vector3 GetActiveCameraRot() const { return activeCameraRot_; }
	// 全カメラ取得
	const std::map<std::string, std::shared_ptr<VirtualCamera>>& GetAllCameras() const { return cameras_; }

	// 補間設定
	void SetEnableBlending(bool enable) { enableBlending_ = enable; }
	bool IsBlendingEnabled() const { return enableBlending_; }

private:
	///************************* 内部処理 *************************///
	CameraDirector() = default;
	~CameraDirector() = default;
	CameraDirector(const CameraDirector&) = delete;
	CameraDirector& operator=(const CameraDirector&) = delete;

	// 一番優先度が高いカメラを探してセットする
	void RefreshActiveCamera();

private:
	///************************* メンバ変数 *************************///
	std::map<std::string, std::shared_ptr<VirtualCamera>> cameras_;

	std::shared_ptr<VirtualCamera> activeCamera_ = nullptr; // 今メインのカメラ
	std::shared_ptr<VirtualCamera> prevCamera_ = nullptr;   // 1つ前のカメラ（補間用）

	std::unordered_map<std::string, const WorldTransform*> targetRegistry_;
	BlendSettings blendSettings_;

	// 補間（ブレンド）用
	bool isBlending_ = false;
	float blendTimer_ = 0.0f;
	float blendDuration_ = 1.0f; // 1秒かけて切り替える（エディタで調整可能にする）
	bool enableBlending_ = true;

	// 最終的な出力
	Matrix4x4 finalVP_;
	float currentFovY_;
	Vector3 activeCameraPos_;
	Vector3 activeCameraRot_;
};


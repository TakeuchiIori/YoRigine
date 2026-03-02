#pragma once

// C++
#include <memory>
#include <functional>
#include <string>

// Engine
#include "Systems/Camera/Camera.h"
#include "Player/Player.h"
#include "Drawer/LineManager/Line.h"

// App
#include "SceneDataStructures.h"

/// <summary>
/// サブシーン基底クラス
/// </summary>
class BaseSubScene {
public:
	using TransitionCallback = std::function<void(const SubSceneTransitionRequest&)>;

	BaseSubScene(const std::string& name) : sceneName_(name) {}
	virtual ~BaseSubScene() = default;

	///************************* 基本関数（純粋仮想） *************************///
	virtual void Initialize(Camera* camera, Player* player) = 0;
	virtual void Update() = 0;
	virtual void DrawObject() = 0;
	virtual void DrawNonOffscreen() = 0;
	virtual void DrawShadow() = 0;
	virtual void DrawLine() = 0;
	virtual void DrawUI() = 0;
	virtual void Finalize() = 0;

	///************************* ライフサイクル *************************///

	// シーンに入る時
	virtual void OnEnter() {
		isActive_ = true;
	}

	// シーンから出る時
	virtual void OnExit() {
		isActive_ = false;
	}

	// 一時停止（他のシーンがアクティブになった時）
	virtual void OnPause() {
		isPaused_ = true;
	}

	// 再開（再びアクティブになった時）
	virtual void OnResume() {
		isPaused_ = false;
	}

	///************************* 遷移リクエスト *************************///

	void RequestTransition(const SubSceneTransitionRequest& request) {
		if (transitionCallback_) {
			transitionCallback_(request);
		}
	}

	// よく使う遷移のヘルパー関数
	void RequestBattleTransition(const BattleTransitionData& data) {
		SubSceneTransitionRequest request;
		request.type = SubSceneTransitionType::TO_BATTLE;
		request.transitionData = new BattleTransitionData(data);
		RequestTransition(request);
	}

	void RequestFieldTransition(const FieldReturnData& data) {
		SubSceneTransitionRequest request;
		request.type = SubSceneTransitionType::TO_FIELD;
		request.transitionData = new FieldReturnData(data);
		RequestTransition(request);
	}

	///************************* アクセッサ *************************///

	void SetTransitionCallback(TransitionCallback callback) {
		transitionCallback_ = callback;
	}

	bool IsActive() const { return isActive_; }
	bool IsPaused() const { return isPaused_; }
	std::string GetSceneName() const { return sceneName_; }

	// カメラモード管理
	virtual void SetCameraMode(CameraMode mode) { currentCameraMode_ = mode; }
	virtual CameraMode GetCameraMode() const { return currentCameraMode_; }

protected:
	///************************* メンバ変数 *************************///

	std::string sceneName_;
	Camera* sceneCamera_ = nullptr;
	Player* player_ = nullptr;

	CameraMode currentCameraMode_ = CameraMode::FOLLOW;

	bool isActive_ = false;
	bool isPaused_ = false;

	TransitionCallback transitionCallback_;
};
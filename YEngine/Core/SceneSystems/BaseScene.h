#pragma once

// Engine
#include <memory>
#include <Systems/Camera/Camera.h>
#include <Systems/Camera/CameraManager.h>
#include <Systems/GameTime/GameTime.h>
#include <Collision/Core/CollisionManager.h>
#include <Particle/ParticleManager.h>
#include <ModelManipulator/ModelManipulator.h>
#include <LightManager/LightManager.h>


// 前方宣言
class SceneManager;

/// <summary>
/// メインシーンの基底クラス
/// </summary>
class BaseScene
{
public:
	///************************* 基本的な関数 *************************///
	// 初期化
	virtual void Initialize() = 0;
	// 終了
	virtual void Finalize() = 0;
	// 更新
	virtual void Update() = 0;
	// 描画
	virtual void Draw() = 0;
	// PostEffectを掛けたくないものを描画
	virtual void DrawNonOffscreen() = 0;
	// 影描画
	virtual void DrawShadow() = 0;
	// デストラクタ
	virtual ~BaseScene() = default;
	// ビュープロジェクション行列を取得
	virtual Matrix4x4 GetViewProjection() = 0;
public:
	///************************* アクセッサ *************************///
	virtual void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }
	const std::string& GetName() const { return sceneName_; }
protected:
	///************************* 継承関で使用 *************************///
	SceneManager* sceneManager_ = nullptr;
	BaseScene(const std::string& name) : sceneName_(name) {}

	///************************* 共通処理 *************************///
	void InitializeCommon();
	void UpdateCommon();
	void DrawCommonObject();
	void DrawCommonParticles();
private:
	///************************* メンバ変数 *************************///
	std::string sceneName_;
	// カメラ
	std::shared_ptr<Camera> sceneCamera_;
	CameraManager cameraManager_;

};


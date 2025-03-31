#pragma once
// C++
#include <memory>

// Engine
#include "WinApp./WinApp.h"
#include "Debugger./ImGuiManager.h"
#include "SrvManager./SrvManager.h"
#include "DX./DirectXCommon.h"
#include "Loaders./Texture/TextureManager.h"
#include "Sprite./SpriteCommon.h"
#include "CoreScenes./Manager/SceneManager.h"
#include "Object3D/Object3dCommon.h"
#include "Collision/Core/CollisionManager.h"
#include "LightManager./LightManager.h"
#include "Drawer./LineManager/LineManager.h"
#include "Loaders./Model./ModelManager.h"
#include "Systems./Input/Input.h"
#include "Systems./Audio/Audio.h"
#include "Corescenes./Factory/AbstractSceneFactory.h"
#include "Debugger./LeakChecker.h"
#include "PipelineManager/SkinningManager.h"
#include "PipelineManager/PipelineManager.h"

// ゲーム全体
class Framework
{
public: // メンバ関数

	/// <summary>
	/// 初期化
	/// </summary>
	virtual void Initialize();

	/// <summary>
	/// 終了
	/// </summary>
	virtual void Finalize();

	/// <summary>
	/// 更新
	/// </summary>
	virtual void Update();

	/// <summary>
	/// 描画
	/// </summary>
	virtual void Draw() = 0;

	/// <summary>
	/// 終了フラグのチェック
	/// </summary>
	/// <returns></returns>
	virtual bool IsEndRequst() { return winApp_->ProcessMessage(); }

	// 呼び出さないとリークするぞ
	virtual ~Framework() = default;

	/// <summary>
	/// 実行
	/// </summary>
	void Run();
protected:
	// 基本的なゲームのコンポーネント
	DirectXCommon* dxCommon_;
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;

	//  ポインタ
	WinApp* winApp_ = nullptr;
	Input* input_ = nullptr;
	Audio* audio_ = nullptr;
	ImGuiManager* imguiManager_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
	Object3dCommon* object3dCommon_ = nullptr;
	TextureManager* textureManager_ = nullptr;
	ModelManager* modelManager_ = nullptr;
	CollisionManager* collisionManager_ = nullptr;;
	LightManager* lightManager_ = nullptr;
	LineManager* lineManager_ = nullptr;
	SkinningManager* skinningManager_ = nullptr;
	PipelineManager* pipelineManager_ = nullptr;
private:
	
	// ゲーム終了フラグ
	bool endRequst_ = false;

};


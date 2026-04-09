#pragma once
// C++
#include <memory>

// Engine
#include "WinApp./WinApp.h"
#include "Debugger./ImGuiManager.h"
#include "SrvManager.h"
#include "DirectXCommon.h"
#include "Loaders./Texture/TextureManager.h"
#include "Sprite./SpriteCommon.h"
#include "Object3D/Object3dCommon.h"
#include "Collision/Core/CollisionManager.h"
#include "LightManager./LightManager.h"
#include "Drawer./LineManager/LineManager.h"
#include "ModelManager.h"
#include "Systems./Input/Input.h"
#include "Systems./Audio/Audio.h"
#include "ComputeShaderManager/ComputeShaderManager.h"
#include "Debugger/DebugConsole.h"
#include "Debugger/D3DResourceLeakChecker.h"
#include "Object3D/ObjectManager.h"
/// <summary>
/// 全体の枠組みを管理するクラス
/// </summary>
class Framework
{
public:
	///************************* 基本関数 *************************///

	virtual void Initialize();
	virtual void Finalize();
	virtual void Update();
	virtual void Draw() = 0;
	virtual bool IsEndRequst() { return winApp_->ProcessMessage(); }

	// 呼び出さないとリークするぞ
	virtual ~Framework() = default;
	void Run();
	WinApp* GetWinApp() { return winApp_; }
protected:
	///************************* 継承間で使用 *************************///
	YoRigine::DirectXCommon* dxCommon_;
	WinApp* winApp_ = nullptr;
	YoRigine::Input* input_ = nullptr;
	YoRigine::Audio* audio_ = nullptr;
	ImGuiManager* imguiManager_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
	Object3dCommon* object3dCommon_ = nullptr;
	TextureManager* textureManager_ = nullptr;
	ModelManager* modelManager_ = nullptr;
	YoRigine::CollisionManager* collisionManager_ = nullptr;;
	YoRigine::LightManager* lightManager_ = nullptr;
	LineManager* lineManager_ = nullptr;
	ComputeShaderManager* computeShaderManager_ = nullptr;
#ifdef USE_IMGUI
	DebugConsole* debugConsole_;
#endif
private:
	///************************* 内部処理 *************************///
	// ゲーム終了フラグ
	bool endRequst_ = false;

};


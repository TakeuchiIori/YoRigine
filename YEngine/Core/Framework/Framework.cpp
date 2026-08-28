#include "Framework.h"
#include "OffScreen/OffScreen.h"
#include "PipelineManager/YPipelineManager.h"
#include "Systems/Input/InputActionMap.h"
/// <summary>
/// フレームワーク全体の初期化
/// </summary>
void Framework::Initialize() {
  //-----------------------------------------
  // ウィンドウ生成
  //-----------------------------------------
  winApp_ = WinApp::GetInstance();
  winApp_->Initialize();

  //-----------------------------------------
  // 入力システム
  //-----------------------------------------
  input_ = YoRigine::Input::GetInstance();
  input_->Initialize(winApp_);

  //-----------------------------------------
  // DirectX 初期化
  //-----------------------------------------
  dxCommon_ = YoRigine::DirectXCommon::GetInstance();
  dxCommon_->Initialize(winApp_);

#ifdef USE_IMGUI
  //-----------------------------------------
  // デバッグコンソール
  //-----------------------------------------
  debugConsole_ = DebugConsole::GetInstance();
  debugConsole_->Initialize();
#endif

  //-----------------------------------------
  // オーディオ
  //-----------------------------------------
  audio_ = YoRigine::Audio::GetInstance();
  audio_->Initialize();

  //-----------------------------------------
  // ImGui
  //-----------------------------------------
  imguiManager_ = ImGuiManager::GetInstance();
  imguiManager_->Initialize(winApp_, dxCommon_);

  //-----------------------------------------
  // テクスチャマネージャ
  //-----------------------------------------
  textureManager_ = TextureManager::GetInstance();
  textureManager_->Initialize(dxCommon_, dxCommon_->GetSrvManager());

  //-----------------------------------------
  // パイプラインマネージャ
  //-----------------------------------------

  YPipelineManager::GetInstance()->Initialize();

  //-----------------------------------------
  // コンピュートシェーダーマネージャ
  //-----------------------------------------
  computeShaderManager_ = ComputeShaderManager::GetInstance();
  computeShaderManager_->Initialize();

  //-----------------------------------------
  // スプライト共通部
  //-----------------------------------------
  spriteCommon_ = SpriteCommon::GetInstance();
  spriteCommon_->Initialize(dxCommon_);

  //-----------------------------------------
  // 3D オブジェクト共通部
  //-----------------------------------------
  object3dCommon_ = Object3dCommon::GetInstance();
  object3dCommon_->Initialize(dxCommon_);

  //-----------------------------------------
  // ライト管理
  //-----------------------------------------
  lightManager_ = YoRigine::LightManager::GetInstance();
  lightManager_->SetOffScreen(OffScreen::GetInstance());
  lightManager_->Initialize();

  //-----------------------------------------
  // モデル管理
  //-----------------------------------------
  modelManager_ = ModelManager::GetInstance();
  modelManager_->Initialze(dxCommon_);

  //-----------------------------------------
  // 衝突判定マネージャ
  //-----------------------------------------
  collisionManager_ = YoRigine::CollisionManager::GetInstance();

  //-----------------------------------------
  // ライン描画
  //-----------------------------------------
  lineManager_ = LineManager::GetInstance();
  lineManager_->Initialize();

  //-----------------------------------------
  // オブジェクト管理
  //-----------------------------------------
  ObjectManager::GetInstance()->SetCollisionManager(collisionManager_);
  ObjectManager::GetInstance()->Initialize();
}

/// <summary>
/// フレームワーク全体の終了処理
/// </summary>
void Framework::Finalize() {
#ifdef _DEBUG
  if (debugConsole_) {
    debugConsole_->Finalize();
  }
#endif

  ObjectManager::GetInstance()->Finalize();
  YPipelineManager::GetInstance()->Finalize();
  lightManager_->Finalize();
  computeShaderManager_->Finalize();
  textureManager_->Finalize();
  imguiManager_->Finalize();
  audio_->Finalize();
  dxCommon_->Finalize();
  input_->Finalize();
  winApp_->Finalize();

  winApp_ = nullptr;
}

/// <summary>
/// フレーム更新処理
/// </summary>
void Framework::Update() {
  // 入力は毎フレーム最初に更新
  input_->Update();

  // アクションマップは生入力の直後に更新する。
  // 以降のゲーム更新は全て同じ入力スナップショットを見ることになる。
  YoRigine::InputActionMap::GetInstance()->Update();

  // ゲームオブジェクト更新
  ObjectManager::GetInstance()->Update();

  // ポイント/スポットライトを GPU バッファへ同期（全シーン共通）
  lightManager_->TransferData();
}

/// <summary>
/// メインループ開始
/// </summary>
void Framework::Run() {
  // 初期化
  Initialize();

  while (true) {
    //-----------------------------------------
    // 更新
    //-----------------------------------------
    Update();

    // 終了リクエスト
    if (IsEndRequst()) {
      break;
    }

    //-----------------------------------------
    // 描画
    //-----------------------------------------
    Draw();
  }

  // 終了処理
  Finalize();
}

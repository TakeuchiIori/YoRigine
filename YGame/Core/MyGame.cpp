#include "MyGame.h"
// ParticleManager は削除済み。YParticleManager + EffectHandle を使用。
#include "Mesh/MeshPrimitive.h"
#include "Editor/Editor.h"
#include "Systems/GameTime/GameTime.h"
#include "Systems/Cinematic/CinematicManager.h"
#include <ModelManipulator/ModelManipulator.h>
#include <PipCamera/PipCameraSystem.h>
#include "OffScreen/PostEffectManager.h"
#include "Material/ToonSettings.h"
#include "Material/OutlineSettings.h"
#include <Systems/UI/UIManager.h>
#include <Systems/Text/TextTextureBaker.h>
#include "GPUParticle/GpuEmitManager.h"
#include <Object3D/BaseObjectManager.h>

#include "Particle/YParticleManager.h"
#include "Particle/YEmitterGroupEditor.h"

#include "Particle/YEmitterGroupManager.h"
#include "Vfx/VfxMesh/VfxMeshSpawner.h"
#include "Composite/CompositeEffectManager.h"
/// <summary>
/// ゲーム全体の初期化処理（起動時に一度だけ実行）
/// </summary>
void MyGame::Initialize() {

	//------------------------------------------------------------
	// 基盤・シーン管理の初期化
	//------------------------------------------------------------
	Framework::Initialize();
	//texturePreloader_.PreloadDirectory("Resources/Textures/");
	audio_->PreloadAllInPath("Resources/Audio/");

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->SetTransitionFactory(std::make_unique<FadeTransitionFactory>());
	SceneManager::GetInstance()->Initialize();

	//------------------------------------------------------------
	// オフスクリーン / ポストエフェクト初期化
	//------------------------------------------------------------
	offScreen_ = OffScreen::GetInstance();
	offScreen_->Initialize();

	PostEffectManager::GetInstance()->Initialize();

	YParticleManager::GetInstance().Initialize(dxCommon_->GetSrvManager(), 50000);

	// エフェクトのロード
	// バンドルファイル（Resources/Json/YEffects/*.json）に移行済みのものは
	// LoadEffectBundle() で System と Group をまとめてロードできる:
	// YParticleManager::GetInstance().LoadEffectBundle("Resources/Json/YEffects/EnemyHit.json");
	// バンドルファイルはエディタの「エミッタグループ → バンドル保存」ボタンで作成する。

	// System を先に全ロード（Group が名前参照するため）→ Group を後にロード。
	// JSON を追加するだけで自動ロードされる。手動羅列・ロード漏れ
	// （以前 EnemyHit3 がロードされず無言で出ていなかった）を構造的に廃止。
	YParticleManager::GetInstance().ScanDirectory("Resources/Json/YParticleSystems/");
	YEmitterGroupManager::GetInstance().ScanDirectory("Resources/Json/YEmitterGroups/");

	// 1エフェクト=1ファイル（systems+groups 同梱）。これ単体で完結し
	// 参照切れが起きない。エフェクトはエディタからこの形式で保存していく（移行先）。
	YParticleManager::GetInstance().ScanEffectBundles("Resources/Json/YEffects/");
	//------------------------------------------------------------
	// パーティクル関連の初期化（YParticle に完全移行済み）
	//------------------------------------------------------------
	YoRigine::GpuEmitManager::GetInstance()->Initialize();

	VfxMeshSpawner::GetInstance()->Initialize();
	VfxMeshSpawner::GetInstance()->ScanDirectory("Resources/Json/VfxMesh/");

	// 複合エフェクト（Particle+VfxMesh+GPU+Sound を名前で束ねる層）を自動ロード。
	// Particle/GPU の Scan より後（子アセットが先に存在している必要があるため）。
	CompositeEffectManager::GetInstance()->ScanDirectory("Resources/Json/YComposites/");

	// モデル操作関連の初期化
	YoRigine::ModelManipulator::GetInstance()->Initialize();

	// BaseObject 一括管理マネージャ
	BaseObjectManager::GetInstance()->Initialize();

	// PiP カメラサブシステム
	PipCameraSystem::GetInstance()->Initialize();

	// トゥーン（全オブジェクト共通設定）
	ToonSettings::GetInstance()->Initialize();

	// 輪郭線（全オブジェクト共通設定・インバートハル）
	OutlineSettings::GetInstance()->Initialize();

	// 演出マネージャ（letterbox UI + Sequencer 駆動）
	YoRigine::CinematicManager::GetInstance()->Initialize();

#ifdef USE_IMGUI
	//------------------------------------------------------------
	// エディター初期化とUI登録
	//------------------------------------------------------------
	Editor::GetInstance()->Initialize();

	// シーン変更コールバック登録
	Editor::GetInstance()->SetSceneChangeCallback(
		[](const std::string& sceneName) {
			if(sceneName == "Develop") SceneManager::GetInstance()->ChangeSceneImmediate(sceneName);
			else SceneManager::GetInstance()->ChangeScene(sceneName);
		}
	);

	Editor::GetInstance()->SetGizmoDrawCallback([]() {
		YoRigine::ModelManipulator::GetInstance()->DrawGizmo();
		YEmitterGroupEditor::GetInstance().DrawGizmo();
		});

	// 各種ImGuiツール登録
	Editor::GetInstance()->RegisterGameUI("ゲーム時間管理", &YoRigine::GameTime::ImGui);
	// ParticleEditor は旧システム専用のため削除済み。YParticleEditor を使用。
	Editor::GetInstance()->RegisterGameUI("モデル操作", []() { YoRigine::ModelManipulator::GetInstance()->DrawImGui(); });
	Editor::GetInstance()->RegisterGameUI("ポストエフェクト", []() { PostEffectManager::GetInstance()->ImGui(); });
	Editor::GetInstance()->RegisterGameUI("トゥーン", []() { ToonSettings::GetInstance()->ImGui(); });
	Editor::GetInstance()->RegisterGameUI("輪郭線", []() { OutlineSettings::GetInstance()->ImGui(); });
	Editor::GetInstance()->RegisterGameUI("JSON管理", &YoRigine::JsonManager::ImGuiManager);
	Editor::GetInstance()->RegisterGameUI("UI管理", []() { YoRigine::UIManager::GetInstance()->ImGuiDebug(); });
	Editor::GetInstance()->RegisterGameUI("テキストベイク", []() { YoRigine::TextTextureBaker::ImGuiPanel(); });
	Editor::GetInstance()->RegisterGameUI("ログ", []() { Editor::GetInstance()->DrawLog(); });
	Editor::GetInstance()->RegisterGameUI("オーディオ詳細", [this]() { audio_->ShowDebugWindow(); });
	Editor::GetInstance()->RegisterGameUI("オーディオ設定", [this]() { audio_->ShowSettingsWindow(); });
	Editor::GetInstance()->RegisterGameUI("PiP カメラ", []() { PipCameraSystem::GetInstance()->DrawImGuiWindow(); });
#endif

	//------------------------------------------------------------
	// 初期シーン設定
	//------------------------------------------------------------
#if defined(DEVELOP_BUILD)
	SceneManager::GetInstance()->ChangeScene("Develop"); // Develop構成: エンジン機能テスト用シーン。Playerを生成しないのでゲームデータは保存されない
#elif defined(_DEBUG)
	SceneManager::GetInstance()->ChangeScene("Game");   // デバッグ時はゲームシーン
#else
	SceneManager::GetInstance()->ChangeScene("Title");  // 製品版はタイトルシーン
#endif
}

/// <summary>
/// ゲーム終了時の解放処理
/// </summary>
void MyGame::Finalize() {
	SceneManager::GetInstance()->Finalize();
	VfxMeshSpawner::GetInstance()->Finalize();
	YoRigine::ModelManipulator::GetInstance()->Finalize();

#ifdef USE_IMGUI
	Editor::GetInstance()->Finalize();
#endif

	Framework::Finalize();
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
void MyGame::Update() {

	//------------------------------------------------------------
	// ImGui受付開始
	//------------------------------------------------------------
	imguiManager_->Begin();
#ifdef USE_IMGUI
	ImGuizmo::BeginFrame();
#endif
	//texturePreloader_.FlushPendingUploads(10);

#ifdef USE_IMGUI
	Editor::GetInstance()->Draw();
#endif

	//------------------------------------------------------------
	// ゲーム本体の更新処理
	//------------------------------------------------------------
	Framework::Update();
	SceneManager::GetInstance()->Update();
	PipCameraSystem::GetInstance()->Update();
	YoRigine::CinematicManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime());

	//------------------------------------------------------------
	// ImGui受付終了
	//------------------------------------------------------------
	imguiManager_->End();
}

/// <summary>
/// 描画処理
/// </summary>
void MyGame::Draw() {


	YoRigine::ModelManipulator::GetInstance()->DrawPickPass();

	//------------------------------------------------------------
	// オフスクリーン描画
	//------------------------------------------------------------
	dxCommon_->PreDrawShadow();
	SceneManager::GetInstance()->DrawShadow();
	dxCommon_->PreDrawOffScreen();
	dxCommon_->GetSrvManager()->PreDraw();

	// シーン描画
	SceneManager::GetInstance()->Draw();

	//------------------------------------------------------------
	// PiP (Picture-in-Picture) 2nd 描画パス
	// 有効時のみ: シーンカメラの行列を PiP カメラに差し替えて 3D だけ再描画。
	// per-object 定数バッファ (WorldTransform::WVP) は GPU 上に 1 本しかなく、
	// メインパスと PiP パスの両方で同じ CB を書き換えるとコマンドリスト末尾実行時に
	// 最後の書き込みだけが残って両パスとも同じ角度になる (CB stomp)。
	// そのため、メインパスをここで一度 GPU に流し切ってから PiP を組む。
	//------------------------------------------------------------
	{
		auto* pip = PipCameraSystem::GetInstance();
		auto* scene = SceneManager::GetInstance()->GetScene();
		Camera* sceneCam = scene ? scene->GetSceneCamera() : nullptr;
		if (pip->IsEnabled() && sceneCam) {
			// メインパスを真に GPU 完了まで待つ (シェーダは gCamera.viewProjection を
			// 使うので、メイン draw が GPU で CB を読み終える前に PiP の値で上書きしてはいけない)
			dxCommon_->FlushAndWait();

			// PiP パス本体: Camera CB を PiP の値に書き換えて専用 RT へ 3D 再描画
			pip->ApplyToCamera(sceneCam);
			dxCommon_->PreDrawPip(pip->GetRTName(), pip->GetDSVName(),
				pip->GetWidth(), pip->GetHeight());
			scene->DrawScene3DOnly();
			dxCommon_->EndPipPass(pip->GetRTName());

			// Restore より先に FlushAndWait。
			// PiP draws の GPU 実行が完了する前に Restore で CB を上書きすると
			// PiP も Scene VP で描画されてしまう (アングルが変わらない)。
			dxCommon_->FlushAndWait();

			// ここで初めて Scene の値に戻す (後続 PostEffect/UI が読む CB を復元)
			pip->RestoreCamera(sceneCam);
		}
	}

	//------------------------------------------------------------
	// ポストエフェクト描画
	//------------------------------------------------------------
	dxCommon_->PreDraw();
	offScreen_->SetProjection(SceneManager::GetInstance()->GetScene()->GetViewProjection());
	PostEffectManager::GetInstance()->Draw();

	//------------------------------------------------------------
	// 通常描画 + UI
	//------------------------------------------------------------
	dxCommon_->DepthBarrier();
	SceneManager::GetInstance()->DrawNonOffscreen();
	// 映画風レターボックスは全シーン共通で最後（ImGui の手前）に描画
	YoRigine::CinematicManager::GetInstance()->Draw();
	dxCommon_->CopyBackBufferToFinalResult();
	imguiManager_->Draw();

	// フレーム終了
	dxCommon_->PostDraw();
}
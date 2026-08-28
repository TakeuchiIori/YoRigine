#include "MyGame.h"
// ParticleManager は削除済み。YParticleManager + EffectHandle を使用。
#include "Mesh/MeshPrimitive.h"
#include "Editor/Editor.h"
#include "Editor/Tools/ImGuiStudio.h"
#include "Systems/GameTime/GameTime.h"
#include "Systems/Cinematic/CinematicManager.h"
#include <SceneEditor/SceneEditor.h>
#include <PipCamera/PipCameraSystem.h>
#include "OffScreen/PostEffectManager.h"
#include "Material/ToonSettings.h"
#include "Material/OutlineSettings.h"
#include <Systems/UI/UIManager.h>
#include <Systems/Text/TextTextureBaker.h>
#include <Systems/Tutorial/TutorialManager.h>
#include "GPUParticle/YGpuEmitManager.h"
#include <Object3D/BaseObjectManager.h>
#include <Drawer/InstancedObject3d.h>
#include <Loaders/Texture/TextureManager.h>
#include <ModelManager.h>
#include <Collision/Core/CollisionManager.h>
#include <Collision/Core/CollisionEditor.h>

#include "Particle/YParticleManager.h"
#include "Particle/YEmitterGroupEditor.h"

#include "Particle/YEmitterGroupManager.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshSpawner.h"
#include "Composite/CompositeEffectManager.h"
#include "Trigger/WaypointManager.h"
#include "LightManager/LightManager.h"
#include "DsvManager.h"
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
	YoRigine::YGpuEmitManager::GetInstance()->SetTextureManager(TextureManager::GetInstance());
	YoRigine::YGpuEmitManager::GetInstance()->SetModelManager(ModelManager::GetInstance());
	YoRigine::YGpuEmitManager::GetInstance()->Initialize();

	VfxMeshSpawner::GetInstance()->Initialize();
	VfxMeshSpawner::GetInstance()->ScanDirectory("Resources/Json/VfxMesh/");

	WaypointManager::GetInstance()->SetVfxMeshSpawner(VfxMeshSpawner::GetInstance());

	// 複合エフェクト（Particle+VfxMesh+GPU+Sound を名前で束ねる層）を自動ロード。
	// Particle/GPU の Scan より後（子アセットが先に存在している必要があるため）。
	CompositeEffectManager::GetInstance()->SetVfxMeshSpawner(VfxMeshSpawner::GetInstance());
	CompositeEffectManager::GetInstance()->SetYGpuEmitManager(YoRigine::YGpuEmitManager::GetInstance());
	CompositeEffectManager::GetInstance()->SetAudio(audio_);
	CompositeEffectManager::GetInstance()->SetCollisionManager(YoRigine::CollisionManager::GetInstance());
	CompositeEffectManager::GetInstance()->SetYParticleManager(&YParticleManager::GetInstance());
	CompositeEffectManager::GetInstance()->SetYEmitterGroupManager(&YEmitterGroupManager::GetInstance());
	CompositeEffectManager::GetInstance()->ScanDirectory("Resources/Json/YComposites/");

	// モデル操作関連の初期化
	YoRigine::SceneEditor::GetInstance()->Initialize();

	// BaseObject 一括管理マネージャ
#ifdef USE_IMGUI
	BaseObjectManager::GetInstance()->SetEditor(Editor::GetInstance());
#endif
	BaseObjectManager::GetInstance()->SetInstancedObject3d(InstancedObject3d::GetInstance());
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
		YoRigine::SceneEditor::GetInstance()->DrawGizmo();
		YEmitterGroupEditor::GetInstance().DrawGizmo();
		});

	// 各種ImGuiツール登録
	Editor::GetInstance()->RegisterGameUI(
		"ゲーム時間管理", &YoRigine::GameTime::ImGui,
		"AllScene", "デバッグ");
	YoRigine::CollisionEditor::GetInstance()->Initialize();
	Editor::GetInstance()->RegisterGameUI(
		"当たり判定Editor",
		[]() { YoRigine::CollisionEditor::GetInstance()->DrawImGui(); },
		"AllScene", "システム");
	ImGuiStudio::GetInstance()->Initialize();
	Editor::GetInstance()->RegisterGameUI(
		"ImGui Studio",
		[]() { ImGuiStudio::GetInstance()->Draw(); },
		"AllScene", "システム");
	// TutorialManager::DrawEditor は USE_IMGUI でのみ存在するため、登録ごと囲む。
#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI(
		"チュートリアル",
		[]() { YoRigine::TutorialManager::GetInstance()->DrawEditor(); },
		"AllScene", "システム");
#endif
	// ParticleEditor は旧システム専用のため削除済み。YParticleEditor を使用。
	Editor::GetInstance()->RegisterGameUI(
		"モデル操作",
		[]() { YoRigine::SceneEditor::GetInstance()->DrawImGui(); },
		"AllScene", "シーン", true);
	Editor::GetInstance()->RegisterGameUI(
		"ポストエフェクト", []() { PostEffectManager::GetInstance()->ImGui(); },
		"AllScene", "レンダリング");
	Editor::GetInstance()->RegisterGameUI(
		"トゥーン", []() { ToonSettings::GetInstance()->ImGui(); },
		"AllScene", "レンダリング");
	Editor::GetInstance()->RegisterGameUI(
		"輪郭線", []() { OutlineSettings::GetInstance()->ImGui(); },
		"AllScene", "レンダリング");
	Editor::GetInstance()->RegisterGameUI(
		"JSON管理", &YoRigine::JsonManager::ImGuiManager,
		"AllScene", "システム");
	Editor::GetInstance()->RegisterGameUI(
		"UI管理", []() { YoRigine::UIManager::GetInstance()->ImGuiDebug(); },
		"AllScene", "システム");
	Editor::GetInstance()->RegisterGameUI(
		"テキストベイク", []() { YoRigine::TextTextureBaker::ImGuiPanel(); },
		"AllScene", "システム");
	Editor::GetInstance()->RegisterGameUI(
		"ログ", []() { Editor::GetInstance()->DrawLog(); },
		"AllScene", "デバッグ", true);
	Editor::GetInstance()->RegisterGameUI(
		"オーディオ詳細", [this]() { audio_->ShowDebugWindow(); },
		"AllScene", "システム");
	Editor::GetInstance()->RegisterGameUI(
		"オーディオ設定", [this]() { audio_->ShowSettingsWindow(); },
		"AllScene", "システム");
	Editor::GetInstance()->RegisterGameUI(
		"PiP カメラ", []() { PipCameraSystem::GetInstance()->DrawImGuiWindow(); },
		"AllScene", "レンダリング");
#endif

	//------------------------------------------------------------
	// SceneManager の依存先マネージャ注入（初回 ChangeScene より前に必要）
	//------------------------------------------------------------
	SceneManager::GetInstance()->SetPostEffectManager(PostEffectManager::GetInstance());
	SceneManager::GetInstance()->SetBaseObjectManager(BaseObjectManager::GetInstance());

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
	WaypointManager::GetInstance()->Finalize();
	YoRigine::YGpuEmitManager::GetInstance()->Finalize();
	CompositeEffectManager::GetInstance()->Finalize();
	VfxMeshSpawner::GetInstance()->Finalize();
	YoRigine::SceneEditor::GetInstance()->Finalize();

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

	// チュートリアルはシーン更新の後。
	// 説明パネルのスプライトを UIManager::UpdateAll（シーン側UIが呼ぶ）より後に
	// 差し込む必要があるため、この順序を崩さないこと。
	YoRigine::TutorialManager::GetInstance()->Update();
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


	YoRigine::SceneEditor::GetInstance()->DrawPickPass();

	//------------------------------------------------------------
	// オフスクリーン描画
	//------------------------------------------------------------
	// カスケードシャドウ：カスケードごとにスライスをクリアしてシーンを影描画する。
	// SetCurrentCascade で影パスの gLight（ShadowDrawPreference / InstancedObject3d が読む）を切り替える。
	for (uint32_t cascade = 0; cascade < YoRigine::DsvManager::kShadowCascadeCount; ++cascade) {
		YoRigine::LightManager::GetInstance()->SetCurrentCascade(cascade);
		dxCommon_->PreDrawShadow(cascade);
		SceneManager::GetInstance()->DrawShadow();
	}
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
		YoRigine::Camera* sceneCam = scene ? scene->GetSceneCamera() : nullptr;
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

	// チュートリアルも全シーン共通で自前描画する。
	// シーンごとのUI描画（GameUI は特定レイヤーしか描かない）に依存させると、
	// 出るシーンと出ないシーンができてしまうため。
	YoRigine::TutorialManager::GetInstance()->Draw();
	dxCommon_->CopyBackBufferToFinalResult();
	imguiManager_->Draw();

	// フレーム終了
	dxCommon_->PostDraw();
}

#include "GameScene.h"
// Engine
#include "CoreScenes./Manager./SceneManager.h"
#include "Systems./Input./Input.h"
#include "Systems/Audio/Audio.h"
#include "Loaders./Texture./TextureManager.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "PipelineManager/SkinningManager.h"
#include "Loaders/Model/Model.h"
#include "Collision/CollisionManager.h"
#include <Systems/GameTime/HitStop.h>
#include <cstdlib>
#include <ctime>

#ifdef _DEBUG
#include "imgui.h"
#endif // DEBUG
#include "LightManager/LightManager.h"
#include "Sprite/SpriteCommon.h"
#include <Systems/GameTime/GameTIme.h>

/// <summary>
/// 初期化処理
/// </summary>
void GameScene::Initialize()
{
	srand(static_cast<unsigned int>(time(nullptr))); // 乱数シード設定
	// カメラの生成
	sceneCamera_ = cameraManager_.AddCamera();


	CollisionManager::GetInstance()->Initialize();
	// 線
	line_ = std::make_unique<Line>();
	line_->Initialize();
	line_->SetCamera(sceneCamera_.get());

	boneLine_ = std::make_unique<Line>();
	boneLine_->Initialize();
	boneLine_->SetCamera(sceneCamera_.get());


	//// コマンドパターン
	//inputHandler_ = std::make_unique<InputHandleMove>();

	//inputHandler_->AssignMoveFrontCommandPressKeyW();
	//inputHandler_->AssignMoveBehindCommandPressKeyS();
	//inputHandler_->AssignMoveRightCommandPressKeyD();
	//inputHandler_->AssignMoveLeftCommandPressKeyA();

	GameTime::GetInstance()->Initialize();

	mpInfo_ = std::make_unique<MapChipInfo>();
	mpInfo_->Initialize();
	mpInfo_->SetCamera(sceneCamera_.get());

	followCamera_.Initialize();
	debugCamera_.Initialize();
	// 各オブジェクトの初期化
	player_ = std::make_unique<Player>(mpInfo_->GetMapChipField());
	player_->Initialize(sceneCamera_.get());
	Vector3 playerPosition = mpInfo_->GetMapChipField()->GetMapChipPositionByIndex(1, 16);
	player_->SetPosition(playerPosition);
	followCamera_.SetTarget(player_.get()->GetWorldTransform());

	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(sceneCamera_.get());
	enemyManager_->SetPlayer(player_.get());

	// 地面
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_.get());

	// test
	test_ = std::make_unique<Object3d>();
	test_->Initialize();
	test_->SetModel("walk.gltf", true);
	//test_->SetModel("sneakWalk.gltf", true);
	testWorldTransform_.Initialize();




	// 初期カメラモード設定
	cameraMode_ = CameraMode::FOLLOW;

	// パーティクル


	emitterPosition_ = Vector3{ 0.0f, 0.0f, 0.0f }; // エミッタの初期位置
	particleCount_ = 1;
	particleEmitter_[0] = std::make_unique<ParticleEmitter>("Circle", emitterPosition_, particleCount_);
	particleEmitter_[0]->Initialize();



	//// オーディオファイルのロード（例: MP3）
	//soundData = Audio::GetInstance()->LoadAudio(L"Resources./images./harpohikunezumi.mp3");
	//// オーディオの再生
	//sourceVoice = Audio::GetInstance()->SoundPlayAudio(soundData);
	//// 音量の設定（0.0f ～ 1.0f）
	//Audio::GetInstance()->SetVolume(sourceVoice, 0.8f); // 80%の音量に設定


	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize("Resources/Textures/KoboRB.png");
	sprite_->SetSize(Vector2{ 1280.0f,720.0f });
	sprite_->SetTextureSize(Vector2{ 1280,720 });

	ParticleManager::GetInstance()->SetCamera(sceneCamera_.get());

	InitializeOcclusionQuery();

	uiBase_ = std::make_unique<UIBase>("UIButton");
	uiBase_->Initialize("Resources/JSON/UI/Button.json");

	uiSub_ = std::make_unique<UIBase>("UISub");
	uiSub_->Initialize("Resources/JSON/UI/Sub.json");
}

/// <summary>
/// 更新処理
/// </summary>
void GameScene::Update()
{
	if (Input::GetInstance()->TriggerKey(DIK_LCONTROL) || Input::GetInstance()->IsPadTriggered(0, GamePadButton::RT)) {
		isDebugCamera_ = !isDebugCamera_;  // 現在の状態を反転
	}


	//particleEmitter_[0]->SetPosition(player_->GetPosition());


	//iCommand_ = inputHandler_->HandleInput();

	//if (this->iCommand_) {
	//    iCommand_->Exec(*player_.get());
	//}

	// パーティクル更新
	mpInfo_->Update();
	
	
	// スポーンタイマーを更新

	// objの更新
	if (!isDebugCamera_) {
	player_->Update();
	}
	enemyManager_->Update();
	
	player_->JsonImGui();
	//followCamera_.JsonImGui();

	// enemy_->Update();
	ground_->Update();
	test_->UpdateAnimation();


	particleEmitter_[0]->Emit();

	ParticleManager::GetInstance()->Update();
	// カメラ更新
	UpdateCameraMode();
	UpdateCamera();




	// ParticleManager::GetInstance()->UpdateParticlePlayerWeapon(weaponPos);
	ShowImGui();


	// particleEmitter_[1]->Update();

	JsonManager::ImGuiManager();
	// ワールドトランスフォーム更新
	testWorldTransform_.UpdateMatrix();
	cameraManager_.UpdateAllCameras();
	CollisionManager::GetInstance()->UpdateCollision();

	//=====================================================//
	/*                  これより下は触るな危険　　　　　　　   　*/
	//=====================================================//

	// ライティング
	LightManager::GetInstance()->ShowLightingEditor();


	sprite_->Update();
	uiBase_->Update();

	uiSub_->Update();

}


/// <summary>
/// 描画処理
/// </summary>
void GameScene::Draw()
{
	//---------
	// 3D
	//---------
	Object3dCommon::GetInstance()->DrawPreference();
	LightManager::GetInstance()->SetCommandList();
	DrawObject();
	///line_->UpdateVertices(start_, end_);
	///line_->DrawLine();

	//---------
	// Animation
	//---------
	SkinningManager::GetInstance()->DrawPreference();
	LightManager::GetInstance()->SetCommandList();
	DrawAnimation();
	





}

void GameScene::DrawOffScreen()
{
	//----------
	// Particle
	//----------
	ParticleManager::GetInstance()->Draw();

	//----------
	// Sprite
	//----------
	SpriteCommon::GetInstance()->DrawPreference();
	DrawSprite();

	//----------
	// Line
	//----------
	DrawLine();

}

void GameScene::DrawObject()
{
	CollisionManager::GetInstance()->Draw();
	mpInfo_->Draw();
	// オクルージョンクエリ開始
	uint32_t queryIndex = 0;
	player_->Draw();
	// Ground のオクルージョンチェック
	commandList_->BeginQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, queryIndex);
	ground_->Draw();
	commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, queryIndex);
	enemyManager_->Draw();

	//ResolvedOcclusionQuery();

}

void GameScene::DrawSprite()
{
	//sprite_->Draw();
	uiBase_->Draw();
	uiSub_->Draw();
}

void GameScene::DrawAnimation()
{
	// Player のオクルージョンチェック
	uint32_t queryIndex = 1;
	commandList_->BeginQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, queryIndex);
	test_->Draw(sceneCamera_.get(), testWorldTransform_);
	commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, queryIndex);
	queryIndex++;
	ResolvedOcclusionQuery();
	
}

void GameScene::DrawLine()
{
	// 骨描画
	if (test_ && test_->GetModel()->GetSkeleton().joints.size() > 0) {
		test_->DrawSkeleton(test_->GetModel()->GetSkeleton(), *boneLine_);
		boneLine_->DrawLine();
	}
}


/// <summary>
/// 解放処理
/// </summary>
void GameScene::Finalize()
{
	cameraManager_.RemoveCamera(sceneCamera_);
}


void GameScene::UpdateCameraMode()
{
#ifdef _DEBUG
	ImGui::Begin("Camera Mode");
	if (ImGui::Button("DEFAULT Camera")) {
		cameraMode_ = CameraMode::DEFAULT;
	}
	if (ImGui::Button("Follow Camera")) {
		cameraMode_ = CameraMode::FOLLOW;
	}
	if (ImGui::Button("Top-Down Camera")) {
		cameraMode_ = CameraMode::TOP_DOWN;
	}
	if (ImGui::Button("Debug Camera")) {
		cameraMode_ = CameraMode::DEBUG;
	}
	ImGui::End();
#endif
}

void GameScene::UpdateCamera()
{
	switch (cameraMode_)
	{
	case CameraMode::DEFAULT:
	{
		sceneCamera_->DefaultCamera();
	}
	break;
	case CameraMode::FOLLOW:
	{

		followCamera_.Update();
		sceneCamera_->viewMatrix_ = followCamera_.matView_;
		sceneCamera_->transform_.translate = followCamera_.translate_;
		sceneCamera_->transform_.rotate = followCamera_.rotate_;

		sceneCamera_->UpdateMatrix();
	}
	break;
	case CameraMode::TOP_DOWN:
	{
		//Vector3 topDownPosition = Vector3(0.0f, 100.0f, 0.0f);
		//sceneCamera_->SetTopDownCamera(topDownPosition + player_->GetPosition());

		topDownCamera_.SetTarget(player_.get()->GetWorldTransform());
		topDownCamera_.Update();
		sceneCamera_->viewMatrix_ = topDownCamera_.matView_;
		sceneCamera_->transform_.translate = topDownCamera_.translate_;
		sceneCamera_->transform_.rotate = topDownCamera_.rotate_;

		sceneCamera_->UpdateMatrix();
	}
	break;
	case CameraMode::DEBUG:
	{
		if (isDebugCamera_) {
			sceneCamera_->SetFovY(debugCamera_.GetFov());
			debugCamera_.Update();
			sceneCamera_->viewMatrix_ = debugCamera_.matView_;
			sceneCamera_->transform_.translate = debugCamera_.translate_;
			sceneCamera_->transform_.rotate = debugCamera_.rotate_;
			sceneCamera_->UpdateMatrix();
		}
	}
	break;

	default:
		break;
	}
}

void GameScene::ShowImGui()
{
#ifdef _DEBUG
	ImGui::Begin("FPS");
	ImGui::Text("FPS:%.1f", ImGui::GetIO().Framerate);
	ImGui::End();
	ImGui::Begin("Emitter");
	ImGui::DragFloat3("Emitter Position", &emitterPosition_.x, 0.1f);
	// particleEmitter_[0]->SetPosition(emitterPosition_);

	 // パーティクル数の表示と調整
	ImGui::Text("Particle Count: %.0d", particleCount_); // 現在のパーティクル数を表示
	if (ImGui::Button("Increase Count")) {
		particleCount_ += 1; // パーティクル数を増加
	}
	if (ImGui::Button("Decrease Count")) {
		if (particleCount_ > 1) { // パーティクル数が1未満にならないように制限
			particleCount_ -= 1;
		}
	}
	//particleEmitter_[0]->SetCount(particleCount_);


	ImGui::End();
	ImGui::Begin("Occlusion Query");
	ImGui::Text("Occlusion Ground: %lld", occlusionResults_[0]);
	ImGui::Text("Occlusion AnimationModel: %lld", occlusionResults_[1]);
	ImGui::End();

#endif // _DEBUG
}

void GameScene::CheckAllCollisions() {

	//// 衝突マネージャーのリセット
	//CollisionManager::GetInstance()->Reset();

	//// コライダーをリストに登録
	//CollisionManager::GetInstance()->AddCollider(player_.get());

	//// コライダーリストに登録
	//CollisionManager::GetInstance()->AddCollider(player_->GetPlayerWeapon());


	//// CollisionManager::GetInstance()->AddCollider(enemy_.get());

	// // 衝突判定と応答
	//CollisionManager::GetInstance()->CheckAllCollisions();

}

void GameScene::InitializeOcclusionQuery()
{
    ID3D12Device* device = DirectXCommon::GetInstance()->GetDevice().Get();
	commandList_ = DirectXCommon::GetInstance()->GetCommandList().Get();
	
	// クエリの数をオブジェクト数に合わせる
	occlusionResults_.resize(queryCount_, 0);

	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.Count = queryCount_;
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
	queryHeapDesc.NodeMask = 0;

	// クエリヒープ作成
	HRESULT hr = device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap_));
	if (FAILED(hr)) {
		throw std::runtime_error("Failed to create Query Heap.");
	}

	// クエリ結果格納用のバッファを作成
	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = sizeof(UINT64) * queryCount_; // クエリ数に応じたサイズ
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_READBACK };

	hr = device->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&queryResultBuffer_)
	);

	if (FAILED(hr)) {
		throw std::runtime_error("Failed to create Query Result Buffer.");
	}
}

void GameScene::BeginOcclusionQuery(UINT queryIndex)
{
	commandList_->BeginQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, queryIndex);
}

void GameScene::EndOcclusionQuery(UINT queryIndex)
{
	commandList_->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_OCCLUSION, queryIndex);
}

void GameScene::ResolvedOcclusionQuery()
{
	// クエリ結果をリソースにコピー
	commandList_->ResolveQueryData
	(
		queryHeap_.Get(),
		D3D12_QUERY_TYPE_OCCLUSION,
		0,
		queryCount_,
		queryResultBuffer_.Get(),
		0
	);

	// 結果をマッピング
	void* mappedData = nullptr;
	D3D12_RANGE readRange = { 0, sizeof(UINT64) * queryCount_ };
	if (SUCCEEDED(queryResultBuffer_->Map(0, &readRange, &mappedData)))
	{
		UINT64* queryData = static_cast<UINT64*>(mappedData);
		for (uint32_t i = 0; i < queryCount_; i++) {
			occlusionResults_[i] = queryData[i]; // 結果を保存
		}
		queryResultBuffer_->Unmap(0, nullptr);
	}
}

#include "Player.h"
// Engine
#include "Loaders./Model./ModelManager.h"
#include "Object3D./Object3dCommon.h"
#include "Collision/CollisionTypeIdDef.h"


#ifdef _DEBUG
#include "imgui.h" 
#endif // _DEBUG



void Player::Initialize(Camera* camera)
{
	camera_ = camera;
	// OBject3dの初期化
	base_ = std::make_unique<Object3d>();
	base_->Initialize();
	base_->SetModel("cube.obj");

	weapon_ = std::make_unique<PlayerWeapon>();
	weapon_->Initialize(camera_);

	shadow_ = std::make_unique<Object3d>();
	shadow_->Initialize();
	shadow_->SetModel("Shadow.obj");


	WS_.Initialize();

	WS_.translation_.y = 0.1f;
	// その他初期化
	input_ = Input::GetInstance();
	moveSpeed_ = 0.25f;
	worldTransform_.Initialize();
	worldTransform_.translation_.y = 1.0f;
	worldTransform_.translation_.z = -50.0f;
	weapon_->SetParent(worldTransform_);
	// TypeIDの設定
	//BaseCollider::SetCamera(camera_);
	//OBBCollider::Initialize();
	//BaseCollider::SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));

	InitCollision();

	particleEmitter_ = std::make_unique<ParticleEmitter>("PlayerParticle", worldTransform_.translation_, 5);
    particleEmitter_->Initialize();

	InitJson();

	timeID_ = "Player";
	gameTime_ = GameTime::GetInstance();
	gameTime_->RegisterObject(timeID_);
}

void Player::InitCollision()
{
	// OBB
	obbCollider_ = std::make_unique<OBBCollider>();
	obbCollider_->SetTransform(&worldTransform_);
	obbCollider_->SetCamera(camera_);
	obbCollider_->Initialize();
	obbCollider_->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));

	// メンバ関数ポインタの登録
	obbCollider_->SetOnEnterCollision([this](BaseCollider* self, BaseCollider* other) {
		this->OnEnterCollision(self, other);
		});
	obbCollider_->SetOnCollision([this](BaseCollider* self, BaseCollider* other) {
		this->OnCollision(self, other);
		});
	obbCollider_->SetOnExitCollision([this](BaseCollider* self, BaseCollider* other) {
		this->OnExitCollision(self, other);
		});

	//aabbCollider_ = std::make_unique<AABBCollider>();
	//aabbCollider_->SetTransform(&worldTransform_);
	//aabbCollider_->SetCamera(camera_);
	//aabbCollider_->Initialize();
	//aabbCollider_->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	//aabbCollider_->SetOnEnterCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnEnterCollision(self, other);
	//	});
	//aabbCollider_->SetOnCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnCollision(self, other);
	//	});
	//aabbCollider_->SetOnExitCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnExitCollision(self, other);
	//	});

	
	//sphereCollider_ = std::make_unique<SphereCollider>();
	//sphereCollider_->SetTransform(&worldTransform_);
	//sphereCollider_->SetCamera(camera_);
	//sphereCollider_->Initialize();
	//sphereCollider_->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	//sphereCollider_->SetOnEnterCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnEnterCollision(self, other);
	//	});
	//sphereCollider_->SetOnCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnCollision(self, other);
	//	});
	//sphereCollider_->SetOnExitCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnExitCollision(self, other);
	//	});


}

void Player::Update()
{


	Move();

	CameraShake();

	WS_.translation_.x = worldTransform_.translation_.x;
	WS_.translation_.z = worldTransform_.translation_.z;


	
    particleEmitter_->UpdateEmit("PlayerParticle", worldTransform_.translation_, 3);

	UpdateWorldTransform();

	
	weapon_->Update();

#ifdef _DEBUG
	ShowCoordinatesImGui();
#endif // _DEBUG
	//OBBCollider::Update();
	obbCollider_->Update();
	//aabbCollider_->Update();
	//sphereCollider_->Update();
}

void Player::Draw()
{
	base_->Draw(camera_,worldTransform_);
	shadow_->Draw(camera_, WS_);
	weapon_->Draw(camera_);
}

void Player::DrawCollision()
{
	//OBBCollider::Draw();
	obbCollider_->Draw();
	//aabbCollider_->Draw();
	//sphereCollider_->Draw();
	//weapon_->DrawCollision();
}

void Player::UpdateWorldTransform()
{
	// ワールドトランスフォームの更新
	worldTransform_.SetAnchorPoint(anchorPoint_);
	worldTransform_.UpdateMatrix();
	WS_.UpdateMatrix();

}

void Player::Move()
{

	if (!isDash_ && weapon_->GetIsDashAttack()) {
		isDash_ = true;
	}

	// キーボードで移動
	if (!isJumping_ && !weapon_->GetIsJumpAttack() || !isDash_) {
#ifdef _DEBUG
		MoveKey();
#endif // _DEBUG

		if (Input::GetInstance()->IsRightStickMoving()) {
			Rotate();
			
		}
		else {
			MoveController();
		}
		
	}
	Jump();
	if (isDash_) {
		Dash();
	}
}

void Player::Rotate()
{
	XINPUT_STATE joyState;
	Input::GetInstance()->GetJoystickState(0, joyState);
	// スティックの入力値を正規化
	Vector3 move = { (float)joyState.Gamepad.sThumbLX / SHRT_MAX * rotrateSpeed_ , 0.0f, (float)joyState.Gamepad.sThumbLY / SHRT_MAX * rotrateSpeed_  };
	// moveベクトルを正規化して方向を得る
	if (move.x > 0 && move.y > 0 && move.z > 0) {
		move = Normalize(move) * rotrateSpeed_ ;
	}
	// カメラの回転行列を抽出
	Matrix4x4 cameraRotationMatrix = MakeRotateMatrixXYZ(camera_->transform_.rotate);
	Vector3 cameraDirection = Vector3(
		cameraRotationMatrix.m[2][0], // カメラのZ軸方向のX成分
		cameraRotationMatrix.m[2][1], // カメラのZ軸方向のY成分
		cameraRotationMatrix.m[2][2]  // カメラのZ軸方向のZ成分
	);
	// 左スティックの方向を基にプレイヤーの回転を計算（Y軸回転）
	float stickAngle = std::atan2(move.x, move.z); // XZ平面の角度を計算
	worldTransform_.rotation_ = Vector3(0.0f, stickAngle, 0.0f);

	// 入力値を正規化して方向を得る
	move = Normalize(move) * rotrateSpeed_ ;

	// カメラの向いている方向ベクトルのXZ平面内の角度を計算
	float angle = std::atan2(cameraDirection.x, cameraDirection.z);
	// プレイヤーの回転を設定（Y軸周りのみ）
	worldTransform_.rotation_ = Vector3(0.0f, angle, 0.0f);
}

void Player::MoveController()
{
	deltaTime_ = gameTime_->GetDeltaTime(timeID_);

	// ヒットストップ中は動きを停止
	if (deltaTime_ <= 0.001f) {
		return;
	}
	XINPUT_STATE state;
	if (!Input::GetInstance()->GetJoystickState(0, state)) {
		// 速度減衰
		velocity_ = velocity_ * 0.9f;
		worldTransform_.translation_ += velocity_;
		return;
	}

	// 左スティックの入力を取得
	float leftStickX = state.Gamepad.sThumbLX / 32768.0f;
	float leftStickY = state.Gamepad.sThumbLY / 32768.0f;

	// デッドゾーン処理
	if (std::fabs(leftStickX) < 0.1f) leftStickX = 0.0f;
	if (std::fabs(leftStickY) < 0.1f) leftStickY = 0.0f;

	if (leftStickX == 0.0f && leftStickY == 0.0f) {
		// 入力がない場合は減速
		velocity_ = velocity_ * 0.9f;
		worldTransform_.translation_ += velocity_;
		return;
	}

	// カメラの回転行列を取得
	Matrix4x4 cameraRotationMatrix = MakeRotateMatrixXYZ(camera_->transform_.rotate);

	// カメラの方向ベクトルを取得
	Vector3 cameraForward = {
		cameraRotationMatrix.m[2][0],
		0.0f,
		cameraRotationMatrix.m[2][2]
	};

	Vector3 cameraRight = {
		cameraRotationMatrix.m[0][0],
		0.0f,
		cameraRotationMatrix.m[0][2]
	};

	// 移動方向の計算
	Vector3 moveDirection = cameraForward * leftStickY + cameraRight * leftStickX;

	// 方向の正規化
	if (moveDirection.x != 0.0f || moveDirection.z != 0.0f) {
		moveDirection = Normalize(moveDirection);
	}

	// プレイヤーの回転
	worldTransform_.rotation_.y = atan2f(moveDirection.x, moveDirection.z);

	// 目標速度の計算（入力の強さに応じた速度）
	float inputMagnitude = std::sqrt(leftStickX * leftStickX + leftStickY * leftStickY);
	inputMagnitude = std::min(inputMagnitude, 1.0f);  // 入力の大きさを1以下に制限

	// 現在の速度を計算
	velocity_ = moveDirection * moveSpeed_ * inputMagnitude * deltaTime_;

	// 速度の大きさを制限
	float speed = Length(velocity_);
	if (speed > maxMoveSpeed_) {
		velocity_ = Normalize(velocity_) * maxMoveSpeed_;
	}

	// 位置の更新
	worldTransform_.translation_ += velocity_;


}


void Player::MoveKey()
{


	// プレイヤーの移動方向を計算するベクトル
	Vector3 direction = { 0.0f, 0.0f, 0.0f };
	// 前回の速度を減衰させる
	velocity_ = velocity_ * 0.9f;

	// キーボードの入力で移動と方向の決定
	if (input_->PushKey(DIK_W)) {
		direction.z += 1.0f; // Z軸正方向に進む
	}
	if (input_->PushKey(DIK_A)) {
		direction.x -= 1.0f; // X軸負方向に進む
	}
	if (input_->PushKey(DIK_S)) {
		direction.z -= 1.0f; // Z軸負方向に進む
	}
	if (input_->PushKey(DIK_D)) {
		direction.x += 1.0f; // X軸正方向に進む
	}

	direction = Normalize(direction);

	if (weapon_->GetIsJumpAttack()) {
		worldTransform_.translation_ += direction;
	}

	// 方向ベクトルを正規化して一定速度にする
	if (direction.x != 0.0f || direction.z != 0.0f) {
		direction = Normalize(direction);
		// 入力がある場合、新しい速度を設定
		velocity_ = direction * moveSpeed_;
	}

	// 新しい位置を計算（まだ適用しない）
	Vector3 newPosition = worldTransform_.translation_ + velocity_;

	// マップチップとの衝突判定と解決
	mpCollision_.DetectAndResolveCollision(
		colliderRect_,  // 衝突判定用矩形
		newPosition,    // 更新される位置（衝突解決後）
		velocity_,      // 更新される速度
		MapChipCollision::CollisionFlag::All,  // すべての方向をチェック
		[this](const CollisionInfo& info) {
			// 衝突時の処理（例：特殊ブロック対応）
			HandleBlockCollision(info);
		}
	);


	// 衝突解決後の位置を適用
	worldTransform_.translation_ = newPosition;
}




void Player::Jump()
{
	// ジャンプ開始
	if (!isJumping_ && weapon_->GetIsJumpAttack()) {
		isJumping_ = true;
		jumpTime_ = 0.0f; // ジャンプ時間をリセット
	}

	if (isJumping_) {
		// ジャンプ時間を更新
		jumpTime_ += 0.016f; // 仮に1フレーム16msと仮定 (60FPS)

		// 正規化した時間 t (0.0f〜1.0f)
		float t = jumpTime_ / jumpDuration_;

		if (t <= 1.0f) { // ジャンプ期間内
			// 高さの計算 (EaseInOutQuad)
			if (t < 0.5f) {
				// 上昇 (EaseIn)
				float normalizedT = t * 2.0f; // 0.0〜0.5を0.0〜1.0に変換
				float height = jumpHeight_ * (normalizedT * normalizedT);
				worldTransform_.translation_.y = groundY_ + height;
			}
			else {
				// 下降 (EaseOut) + 速度係数
				float normalizedT = (t - 0.5f) * 2.0f; // 0.5〜1.0を0.0〜1.0に変換
				float height = jumpHeight_ * (1.0f - (normalizedT * normalizedT) * fallSpeedFactor_);
				worldTransform_.translation_.y = groundY_ + height;
			}

			// 強制着地条件
			if (worldTransform_.translation_.y <= 1.0f) {
				worldTransform_.translation_.y = 1.0f; // 地面に固定
				isJumping_ = false;                       // ジャンプ終了
			}
		}
		else {
			// ジャンプ終了
			worldTransform_.translation_.y = 1.0f; // 地面に戻す
			isJumping_ = false;                       // ジャンプ終了
		}
	}
}

void Player::Dash()
{
	if (isDash_) {
		// ダッシュ時間を更新
		dashTime_ += 0.016f; // 仮に1フレーム16ms (60FPS)

		// 正規化した時間 t (0.0f〜1.0f)
		float t = dashTime_ / dashDuration_;

		if (t <= 1.0f) {
			// 移動方向を計算（Y軸の回転に基づいて計算）
			Vector3 forwardDirection = {
				sinf(worldTransform_.rotation_.y), // Y軸の回転に応じた前方方向
				0.0f,
				cosf(worldTransform_.rotation_.y)
			};

			// ダッシュ速度に基づいて補間移動 (EaseOutExpo)
			float dashFactor = (t == 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * t); // EaseOutExpo
			worldTransform_.translation_ += forwardDirection * (dashSpeed_ * dashFactor * 0.016f);
		}
		else {
			// ダッシュ終了
			isDash_ = false;
			dashTime_ = 0.0f; // ダッシュ時間をリセット
		}
	}
}

void HandleBlockCollision(const CollisionInfo& info) {
	// 衝突したブロックの種類に応じた処理
	switch (info.blockType) {
	case MapChipType::kBlock:
		// 通常ブロックの処理
		break;

		// 将来的に追加される特殊ブロックの処理
		// case MapChipType::kDamageBlock:
		//     TakeDamage(10);
		//     break;
		// case MapChipType::kJumpBlock:
		//     velocity_.y = 10.0f;  // ジャンプさせる
		//     break;

	default:
		break;
	}

	// 衝突方向に応じた処理
	if (info.direction == 4) {  // 下方向の衝突 = 着地
		//isGrounded_ = true;
	}
}

void Player::CameraShake()
{
	if (isShake_) {
		//camera_->Shake(1.0f, 1.0f, 5.0f);
		isShake_ = false;
	}
}

void Player::MoveFront()
{    // 移動方向を計算（Y軸の回転に基づいて計算）
	Vector3 forwardDirection = {
		sinf(worldTransform_.rotation_.y), // Y軸の回転に応じた前方方向
		0.0f,
		cosf(worldTransform_.rotation_.y)
	};

	// キーボード入力による移動
	if (input_->PushKey(DIK_W)) {
		// 前進（進行方向に沿って前方に移動）
		worldTransform_.translation_ += forwardDirection * moveSpeed_;
	}


}

void Player::MoveBehind()
{
	// 移動方向を計算（Y軸の回転に基づいて計算）
	Vector3 forwardDirection = {
		sinf(worldTransform_.rotation_.y), // Y軸の回転に応じた前方方向
		0.0f,
		cosf(worldTransform_.rotation_.y)
	};
	if (input_->PushKey(DIK_S)) {
		// 後退（進行方向に沿って後方に移動）
		worldTransform_.translation_ -= forwardDirection * moveSpeed_;
	}

}

void Player::MoveRight()
{
	Vector3 rightDirection = {
	cosf(worldTransform_.rotation_.y),
	0.0f,
	-sinf(worldTransform_.rotation_.y)
	};

	if (input_->PushKey(DIK_D)) {
		// 右に移動（進行方向に垂直な右方向に移動）
		worldTransform_.translation_ += rightDirection * moveSpeed_;
	}
}

void Player::MoveLeft()
{
	Vector3 rightDirection = {
	cosf(worldTransform_.rotation_.y),
	0.0f,
	-sinf(worldTransform_.rotation_.y)
	};

	if (input_->PushKey(DIK_A)) {
		// 左に移動（進行方向に垂直な左方向に移動）
		worldTransform_.translation_ -= rightDirection * moveSpeed_;
	}
}

Vector3 Player::GetWorldPosition()
{
	Vector3 worldPos;
	worldPos.x = worldTransform_.matWorld_.m[3][0];
	worldPos.y = worldTransform_.matWorld_.m[3][1];
	worldPos.z = worldTransform_.matWorld_.m[3][2];

	return worldPos;
}

void Player::ShowCoordinatesImGui()
{
#ifdef _DEBUG
	// ImGuiウィンドウを利用してプレイヤーの座標を表示
	ImGui::Begin("Player Editor");
	ImGui::Text("DrawCall");
	ImGui::Checkbox("Enable Draw", &isDrawEnabled_);
	ImGui::Checkbox("Update", &isUpdate_);

	ImGui::DragFloat("deltaTime_", &deltaTime_, 0.01f, 0.0f, 10.0f);
	ImGui::End();
#endif

}

void Player::InitJson()
{
	jsonManager_ = std::make_unique<JsonManager>("Player","Resources./JSON");
	jsonManager_->SetCategory("Objects");
	jsonManager_->Register("Translation", &worldTransform_.translation_);
	jsonManager_->Register("Rotate", &worldTransform_.rotation_);
	jsonManager_->Register("Scale", &worldTransform_.scale_);
	jsonManager_->Register("Use Anchor Point", &worldTransform_.useAnchorPoint_);
	jsonManager_->Register("AnchorPoint", &anchorPoint_);
	jsonManager_->Register("Speed", &moveSpeed_);
	jsonManager_->Register("JumpHeight", &jumpHeight_);
	jsonManager_->Register("Rotate Speed", &rotrateSpeed_);

	jsonCollider_ = std::make_unique<JsonManager>("PlayerCollider", "Resources/Json/Colliders");
	obbCollider_->InitJson(jsonCollider_.get());
	//aabbCollider_->InitJson(jsonCollider_.get());
}


void Player::OnEnterCollision(BaseCollider* self, BaseCollider* other)
{
}

void Player::OnCollision(BaseCollider* self, BaseCollider* other)
{
}

void Player::OnExitCollision(BaseCollider* self, BaseCollider* other)
{
}

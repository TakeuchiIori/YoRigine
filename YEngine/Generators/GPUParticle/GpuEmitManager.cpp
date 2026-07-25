#include "GpuEmitManager.h"

// C++
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

// Engine
#include <Loaders/Json/ConversionJson.h>
#include <Loaders/Texture/TextureManager.h>
#include <ModelManager.h>
#include <Debugger/Logger.h>
#include "Systems/GameTime/GameTime.h"
#include <IconsFontAwesome5.h>
#include "GpuEmitterJson.h"   // GpuForceFieldParams の to_json/from_json
#include <cassert>
#ifdef USE_IMGUI
#include "GpuEmitManagerEditorUI.h"
#endif

namespace YoRigine {
	// ImGui用の形状名一覧
	const char* GpuEmitManager::shapeNames_[] = {
		"円形",
		"箱形",
		"三角形",
		"コーン",
		"メッシュ"
	};

	/// <summary>
	/// GpuEmitManager シングルトン取得
	/// </summary>
	GpuEmitManager* GpuEmitManager::GetInstance()
	{
		static GpuEmitManager instance;
		return &instance;
	}

	/// <summary>
	/// 初期化（カメラ登録のみ）
	/// </summary>
	void GpuEmitManager::Initialize()
	{
		assert(textureManager_ && "GpuEmitManager : SetTextureManager() を先に呼ぶこと");
		assert(modelManager_ && "GpuEmitManager : SetModelManager() を先に呼ぶこと");

#ifdef USE_IMGUI
		// --- テクスチャブラウザ ---
		textureBrowser_ = FileBrowser(
			"Resources/Textures/",
			{ ".png", ".jpg", ".dds" },
			FileBrowser::DisplayMode::Grid
		);
		// サムネイルプロバイダ: TextureManager 経由で GPU ハンドルを返す
		textureBrowser_.SetThumbnailProvider([this](const std::string& path) -> ImTextureID {
			textureManager_->LoadTexture(path);
			auto handle = textureManager_->GetsrvHandleGPU(path);
			return handle.ptr != 0 ? static_cast<ImTextureID>(handle.ptr) : 0;
			});
		// 選択時コールバック: テクスチャパスを入力バッファに反映
		textureBrowser_.SetOnFileSelected([this](const std::string& path) {
			strncpy_s(newEmitterTexturePath_, path.c_str(), sizeof(newEmitterTexturePath_) - 1);
			showTextureBrowser_ = false;
			});

		// --- JSON ブラウザ ---
		jsonBrowser_ = FileBrowser(
			"Resources/Json/GpuEmitters/",
			{ ".json" },
			FileBrowser::DisplayMode::List
		);
		// 選択時コールバック: パスをセーブ/ロードバッファに反映
		jsonBrowser_.SetOnFileSelected([this](const std::string& path) {
			strncpy_s(saveFilePath_, path.c_str(), sizeof(saveFilePath_) - 1);
			selectedJsonFilePath_ = path;
			selectedGroupName_.clear();
			selectedEmitterName_.clear();
			newGroupName_[0] = '\0';
			});

		// エミッタ形状のライン可視化（Debugのみ）
		gizmoLine_ = std::make_unique<YoRigine::Line>();
		gizmoLine_->Initialize();
		gizmoLine_->SetCamera(camera_);
#endif

		modelManager_->LoadModel("Resources/Models/Cube", "Cube.obj");
		LoadAllEmitters();
	}

	/// <summary>
	/// アプリ終了時に借用ポインタを手放す
	/// </summary>
	void GpuEmitManager::Finalize()
	{
		textureManager_ = nullptr;
		modelManager_ = nullptr;
	}

	/// <summary>
	/// 全エミッターを更新 (グループ内をループ)
	/// </summary>
	void GpuEmitManager::Update()
	{
		float deltaTime = GameTime::GetDeltaTime(TimeChannel::Vfx);

		// グループ全体をループ
		for (auto& [groupName, groupData] : groups_) {
			if (!groupData->isActive) continue;

			// 発生中(isPlaying) か、発生停止後の余韻(lingerTimer>0)がある間は粒子シミュレーションを回す。
			// これにより「停止後も既存粒子が自然に消える」「発生中でなくても撃ったワンショットが動く」が成立する。
			const bool emitting = groupData->isPlaying;
			const bool simulating = emitting || groupData->lingerTimer > 0.0f;
			if (!simulating) continue;

			// システム時間の更新
			if (emitting) {
				groupData->currentTime += deltaTime;
			} else {
				// 余韻中はエミッションせず、寿命ぶんだけ回して自然消滅を待つ
				groupData->lingerTimer -= deltaTime;
			}

			// グループ内のエミッターをループ
			for (auto& [emitterName, emitterData] : groupData->emitters) {
				if (emitterData->isActive && emitterData->emitter) {
					// 継続発生は再生中のみ。停止/余韻中はバースト要求だけが発生する
					emitterData->emitter->SetContinuousEmit(emitting);
					// グループ原点＋ローカルオフセットをワールド発生位置として毎フレーム反映（追従）
					emitterData->emitter->SetEmitWorldPosition(
						groupData->translate + GetEmitterLocalOffset(emitterData.get()));
					// フォースフィールドもグループ原点に追従させる（center はグループ相対）
					emitterData->emitter->SetForceFields(emitterData->forceFields, groupData->translate);
					emitterData->emitter->Update(deltaTime);
				}
			}

			// システムの自動終了判定 (Durationが0より大きく、再生時間がDurationを超えたら停止)
			if (emitting && groupData->systemDuration > 0.0f && groupData->currentTime >= groupData->systemDuration) {
				StopEmitterGroup(groupName);
			}
		}
	}

	/// <summary>
	/// 全エミッター描画 (グループ内をループ)
	/// </summary>
	void GpuEmitManager::Draw()
	{
		for (auto& [groupName, groupData] : groups_) {
			if (groupData->isActive) {
				for (auto& [emitterName, emitterData] : groupData->emitters) {
					if (emitterData->isActive && emitterData->emitter) {
						emitterData->emitter->Draw();
					}
				}
			}
		}

#ifdef USE_IMGUI
		// エミッタ形状のライン可視化（選択中グループ）
		if (showEmitterGizmos_ && gizmoLine_) {
			DrawEmitterGizmos();
		}
#endif
	}

#ifdef USE_IMGUI
	/// <summary>
	/// 選択中グループの各エミッタ形状をライン描画（選択エミッタ=黄 / それ以外=シアン）
	/// </summary>
	void GpuEmitManager::DrawEmitterGizmos()
	{
		gizmoLine_->Reset();

		EmitterGroup* group = GetGroup(selectedGroupName_);
		if (!group) return;

		// --- 非選択エミッタ（シアン）---
		for (auto& [name, e] : group->emitters) {
			if (!e || name == selectedEmitterName_) continue;
			RegisterEmitterGizmo(e.get(), group->translate + GetEmitterLocalOffset(e.get()));
		}
		gizmoLine_->SetColor({ 0.2f, 0.8f, 1.0f, 1.0f });
		gizmoLine_->DrawLine();

		// --- 選択エミッタ（黄・強調）---
		auto it = group->emitters.find(selectedEmitterName_);
		if (it != group->emitters.end() && it->second) {
			RegisterEmitterGizmo(it->second.get(), group->translate + GetEmitterLocalOffset(it->second.get()));
			gizmoLine_->SetColor({ 1.0f, 0.9f, 0.1f, 1.0f });
			gizmoLine_->DrawLine();
		}

		// --- フォースフィールド範囲（マゼンタ）---
		for (auto& [name, e] : group->emitters) {
			if (!e) continue;
			RegisterForceFieldGizmos(e.get(), group->translate);
		}
		gizmoLine_->SetColor({ 1.0f, 0.3f, 1.0f, 1.0f });
		gizmoLine_->DrawLine();

		// --- killRadius（赤・消滅範囲）---
		for (auto& [name, e] : group->emitters) {
			if (!e) continue;
			for (const auto& ff : e->forceFields) {
				if (!ff.isEnable) continue;
				if (ff.mode == GpuFieldMode::ConvergeToCenter && ff.killRadius > 0.0001f) {
					gizmoLine_->DrawSphere(group->translate + ff.center, ff.killRadius, 12);
				}
			}
		}
		gizmoLine_->SetColor({ 1.0f, 0.15f, 0.15f, 1.0f });
		gizmoLine_->DrawLine();
	}

	/// <summary>
	/// 単一エミッタのフォースフィールド範囲を gizmoLine_ に線分登録（DrawLine は呼び出し側が発行）
	/// </summary>
	void GpuEmitManager::RegisterForceFieldGizmos(const EmitterData* e, const Vector3& groupOrigin)
	{
		if (!e) return;

		for (const auto& ff : e->forceFields) {
			if (!ff.isEnable) continue;

			const Vector3 center = groupOrigin + ff.center;

			// フィールドの外形
			if (ff.shape == GpuFieldShape::Sphere) {
				gizmoLine_->DrawSphere(center, ff.radius, 16);
			} else {
				gizmoLine_->DrawAABB(center - ff.halfExtents, center + ff.halfExtents);
			}

			// モードの向きが分かるガイド線
			switch (ff.mode) {
			case GpuFieldMode::DirectionalAccel:
			{
				// 加速方向の矢印
				Vector3 d = ff.direction;
				float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
				d = (len < 1e-4f) ? Vector3{ 0.0f, 1.0f, 0.0f } : d * (1.0f / len);
				const float guide = (ff.shape == GpuFieldShape::Sphere) ? ff.radius : ff.halfExtents.y;
				gizmoLine_->RegisterLine(center, center + d * guide);
				break;
			}
			case GpuFieldMode::ConvergeToCenter:
			case GpuFieldMode::RadialRepel:
			{
				// 中心マーカー（十字）
				const float m = 0.3f;
				gizmoLine_->RegisterLine(center - Vector3{ m, 0, 0 }, center + Vector3{ m, 0, 0 });
				gizmoLine_->RegisterLine(center - Vector3{ 0, m, 0 }, center + Vector3{ 0, m, 0 });
				gizmoLine_->RegisterLine(center - Vector3{ 0, 0, m }, center + Vector3{ 0, 0, m });
				break;
			}
			}
		}
	}

	/// <summary>
	/// 単一エミッタの形状を gizmoLine_ に線分登録（DrawLine は呼び出し側が発行）
	/// </summary>
	void GpuEmitManager::RegisterEmitterGizmo(const EmitterData* e, const Vector3& worldPos)
	{
		if (!e) return;

		switch (e->shape) {
		case EmitterShape::Sphere:
			gizmoLine_->DrawSphere(worldPos, e->sphereParams.radius, 16);
			break;

		case EmitterShape::Box:
		{
			// size はフルサイズ（shader は ±size*0.5 で分布）
			Vector3 half = e->boxParams.size * 0.5f;
			gizmoLine_->DrawAABB(worldPos - half, worldPos + half);
			break;
		}

		case EmitterShape::Cone:
		{
			const auto& cp = e->coneParams;
			const Vector3 apex = worldPos;

			// 方向を正規化
			Vector3 d = cp.direction;
			float dlen = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
			d = (dlen < 1e-4f) ? Vector3{ 0.0f, 1.0f, 0.0f } : d * (1.0f / dlen);

			const Vector3 baseC = apex + d * cp.height;

			// 方向に垂直な基底 (r, u) を作る
			Vector3 up = (std::fabs(d.y) < 0.999f) ? Vector3{ 0.0f, 1.0f, 0.0f } : Vector3{ 1.0f, 0.0f, 0.0f };
			Vector3 r = { up.y * d.z - up.z * d.y, up.z * d.x - up.x * d.z, up.x * d.y - up.y * d.x };
			float rlen = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
			if (rlen > 1e-4f) r = r * (1.0f / rlen);
			Vector3 u = { d.y * r.z - d.z * r.y, d.z * r.x - d.x * r.z, d.x * r.y - d.y * r.x };

			const int seg = 16;
			const float twoPi = 6.28318530718f;
			Vector3 prev{};
			for (int i = 0; i <= seg; ++i) {
				float a = twoPi * float(i) / float(seg);
				Vector3 p = baseC + (r * std::cos(a) + u * std::sin(a)) * cp.radius;
				if (i > 0) gizmoLine_->RegisterLine(prev, p);
				prev = p;
				if (i % 4 == 0) gizmoLine_->RegisterLine(apex, p); // 側面のガイド線
			}
			break;
		}

		case EmitterShape::Triangle:
		{
			// shader は translate を使わず v1/v2/v3 を直接使うため、グループ原点だけを基準にする
			const Vector3 base = worldPos - e->triangleParams.translate;
			const auto& tp = e->triangleParams;
			gizmoLine_->RegisterLine(base + tp.v1, base + tp.v2);
			gizmoLine_->RegisterLine(base + tp.v2, base + tp.v3);
			gizmoLine_->RegisterLine(base + tp.v3, base + tp.v1);
			break;
		}

		case EmitterShape::Mesh:
			// メッシュ境界の取得は重いので、発生位置に小さなマーカー球のみ
			gizmoLine_->DrawSphere(worldPos, 0.5f, 8);
			break;
		}
	}
#endif
	
	/// <summary>
	/// エミッターのパラメータを更新
	/// </summary>
	/// <param name="emitterData"></param>
	void GpuEmitManager::UpdateParticleParams(EmitterData* emitterData)
	{
		if (!emitterData || !emitterData->emitter) return;

		auto* emitter = emitterData->emitter.get();
		auto& params = emitterData->particleParams;
		emitter->SetParticleParameters(params);
		// フォースフィールドは Update() で毎フレーム（グループ原点込みで）転送する
	}

	/// <summary>
	/// エミッションを強制発生
	/// </summary>
	/// <param name="groupName"></param>
	/// <param name="position"></param>
	/// <param name="count"></param>
	void GpuEmitManager::EmitGroups(const std::string& groupName, const Vector3& position, float count)
	{
		// グループを検索
		auto groupIt = groups_.find(groupName);
		if (groupIt == groups_.end()) {
			Logger("GpuEmitManager::EmitGroups() : エミッターグループが見つかりません");
			return;
		}

		// グループ内の全エミッターに対してエミッションを発生
		// 各エミッタは指定位置＋自身のローカルオフセットで発生（グループ内の相対配置を保持）
		auto& groupData = groupIt->second;
		groupData->translate = position;
		for (auto& [emitterName, emitterData] : groupData->emitters) {
			if (emitterData->isActive && emitterData->emitter) {
				// count < 0 は「各エミッタが持つ設定値を使う」の意味
				float emitCount = count;
				if (emitCount < 0.0f) {
					switch (emitterData->shape) {
					case EmitterShape::Sphere:   emitCount = emitterData->sphereParams.count;   break;
					case EmitterShape::Box:      emitCount = emitterData->boxParams.count;      break;
					case EmitterShape::Triangle: emitCount = emitterData->triangleParams.count; break;
					case EmitterShape::Cone:     emitCount = emitterData->coneParams.count;     break;
					case EmitterShape::Mesh:     emitCount = emitterData->meshParams.count;     break;
					}
				}
				emitterData->emitter->EmitAtPosition(position + GetEmitterLocalOffset(emitterData.get()), emitCount);
			}
		}
		// ワンショットは isPlaying を立てず、粒子が寿命を全うするまで linger でシミュレーションを継続させる
		groupData->lingerTimer = std::max(groupData->lingerTimer, GetGroupMaxLifetime(groupData.get()));
	}

	/// <summary>
	/// グループ内の最大パーティクル寿命を取得（発生停止後の linger 時間見積り用）
	/// </summary>
	float GpuEmitManager::GetGroupMaxLifetime(const EmitterGroup* group) const
	{
		float maxLife = 0.5f; // 最低保証（空グループ等でも最小限ティックする）
		if (!group) return maxLife;
		for (const auto& [name, e] : group->emitters) {
			if (!e) continue;
			const float life = e->particleParams.lifeTime + e->particleParams.lifeTimeVariance;
			maxLife = std::max(maxLife, life);
			// パーティクル自身のトレイル寿命も考慮
			if (e->particleParams.child.isTrail) {
				maxLife = std::max(maxLife, life + e->particleParams.child.lifeTime);
			}
		}
		return maxLife;
	}

	/// <summary>
	/// グループの「自然な寿命」の見積り（秒）。GetGroupMaxLifetime の public wrapper。
	/// Composite::NaturalDuration() の集計に使う。存在しないグループは 0 を返す（=不明扱い）。
	/// </summary>
	float GpuEmitManager::EstimateGroupNaturalDuration(const std::string& groupName) const
	{
		auto it = groups_.find(groupName);
		if (it == groups_.end() || !it->second) return 0.0f;
		return GetGroupMaxLifetime(it->second.get());
	}

	/// <summary>
	/// エミッターの形状ごとのローカル発生位置（オフセット）を取得
	/// </summary>
	Vector3 GpuEmitManager::GetEmitterLocalOffset(const EmitterData* emitterData) const
	{
		if (!emitterData) return { 0.0f, 0.0f, 0.0f };
		switch (emitterData->shape) {
		case EmitterShape::Sphere:   return emitterData->sphereParams.translate;
		case EmitterShape::Box:      return emitterData->boxParams.translate;
		case EmitterShape::Triangle: return emitterData->triangleParams.translate;
		case EmitterShape::Cone:     return emitterData->coneParams.translate;
		case EmitterShape::Mesh:     return emitterData->meshParams.translate;
		}
		return { 0.0f, 0.0f, 0.0f };
	}

	/// <summary>
	/// グループのワールド原点を移動
	/// </summary>
	void GpuEmitManager::SetGroupPosition(const std::string& groupName, const Vector3& pos)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (group) group->translate = pos;
	}

	/// <summary>
	/// グループが存在するか
	/// </summary>
	bool GpuEmitManager::HasGroup(const std::string& groupName) const
	{
		return groups_.count(groupName) > 0;
	}

	/// <summary>
	/// グループが再生中か
	/// </summary>
	bool GpuEmitManager::IsGroupPlaying(const std::string& groupName) const
	{
		auto it = groups_.find(groupName);
		return it != groups_.end() && it->second->isPlaying;
	}
	/// <summary>
	/// エミッター管理用の ImGui 描画。UI本体は GpuEmitManagerEditorUI に分離済み (神クラス対策)。
	/// </summary>
	void GpuEmitManager::DrawImGui()
	{
#ifdef USE_IMGUI
		GpuEmitManagerEditorUI::DrawImGui(*this);
#endif
	}




	/// <summary>
	/// 新しいエミッターを作成
	/// </summary>
	GpuEmitManager::EmitterData* GpuEmitManager::CreateEmitter(const std::string& groupName, const std::string& emitterName, std::string& texturePath, EmitterShape shape)
	{
		EmitterGroup* group = GetGroup(groupName);
		// グループが存在しない
		if (!group) return nullptr;

		// 同名のエミッターが存在する
		if (group->emitters.count(emitterName)) return nullptr;

		//-------------------- 値保存をしてから生成 --------------------//
		auto newEmitterData = std::make_unique<EmitterData>();
		newEmitterData->name = emitterName;
		newEmitterData->shape = shape;
		newEmitterData->isActive = true;
		newEmitterData->texturePath = texturePath;
		newEmitterData->emitter = std::make_unique<GPUEmitter>();

		// GPUEmitter の初期化
		newEmitterData->emitter->Initialize(camera_, texturePath);
		newEmitterData->emitter->SetEmitterShape(shape);

		// デフォルトパラメータ反映
		UpdateEmitterParams(newEmitterData.get());

		// グループのコンテナに追加
		group->emitters[emitterName] = std::move(newEmitterData);
		return group->emitters[emitterName].get();
	}

	/// <summary>
	/// エミッターを 1 つ削除
	/// </summary>
	void GpuEmitManager::DeleteEmitter(const std::string& groupName, const std::string& emitterName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group) return;

		if (group->emitters.count(emitterName)) {
			group->emitters.erase(emitterName);
			if (selectedEmitterName_ == emitterName) {
				selectedEmitterName_.clear();
			}
		}
	}

	/// <summary>
	/// 全エミッター削除
	/// </summary>
	void GpuEmitManager::DeleteAllEmitters()
	{
		for (auto& [name, data] : groups_) {
			data->emitters.clear();
		}
	}

	GpuEmitManager::EmitterGroup* GpuEmitManager::GetGroup(const std::string& groupName)
	{
		if (groups_.count(groupName)) {
			return groups_.at(groupName).get();
		}
		return nullptr;
	}

	GpuEmitManager::EmitterGroup* GpuEmitManager::CreateEmitterGroup(const std::string& groupName, const std::string& sourceFilePath)
	{
		// 1. グループ名で重複がないかチェック（非致命: 既存を返さず nullptr。ログのみ）
		if (groups_.count(groupName)) {
			Logger("GpuEmitManager: グループ名 '" + groupName + "' は既に存在するため作成をスキップ");
			return nullptr;
		}

		// 2. 由来ファイルパスを決定（引数優先→エディタ選択中→最後はグループ名から自動決定）
		//    これによりエディタで JSON を手選択しなくても新規グループを作成・保存できる
		std::string source = !sourceFilePath.empty() ? sourceFilePath : selectedJsonFilePath_;
		if (source.empty()) {
			source = "Resources/Json/GpuEmitters/" + groupName + ".json";
		}

		// 3. 新しいグループを作成
		auto newGroup = std::make_unique<EmitterGroup>();
		newGroup->name = groupName;
		newGroup->sourceFilePath = source;

		// 4. groups_ に追加
		auto [it, inserted] = groups_.emplace(groupName, std::move(newGroup));
		if (inserted) {
			Logger("Group created: " + groupName + " (File: " + source + ")");
			return it->second.get();
		}

		return nullptr;
	}
	
	void GpuEmitManager::DeleteEmitterGroup(const std::string& groupName)
	{
		auto it = groups_.find(groupName);
		if (it == groups_.end()) {
			Logger("GpuEmitManager: 削除対象グループ '" + groupName + "' が見つかりません");
			return;
		}

		EmitterGroup* group = it->second.get();
		const std::string sourceFile = group->sourceFilePath;

		// グループ内の全エミッターを破棄
		group->emitters.clear();

		// グループを groups_ から削除
		groups_.erase(it);

		Logger("Group deleted: " + groupName + " (File: " + sourceFile + ")");

		// 選択状態のリセット
		if (selectedGroupName_ == groupName) {
			selectedGroupName_ = "";
			selectedEmitterName_ = "";
		}
	}

	void GpuEmitManager::DeleteAllEmitterGroups()
	{
		// グループコンテナ全体をクリア
		groups_.clear();

		// 選択中のグループ名とエミッター名をクリアしてリセット
		selectedGroupName_.clear();
		selectedEmitterName_.clear();
	}

	void GpuEmitManager::PlayEmitterGroup(const std::string& groupName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group)return;

		// 再生中じゃない場合のみ処理する
		if (!group->isPlaying)
		{
			group->isPlaying = true;
			group->currentTime = 0.0f;
			group->lingerTimer = 0.0f;
			// グループ内のエミッターを再生（発生状態をリセットしてクリーンスタート）
			for (auto& [name, data] : group->emitters)
			{
				if (data->emitter) {
					data->emitter->Reset();
				}
			}
		}
	}

	void GpuEmitManager::StopEmitterGroup(const std::string& groupName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group)return;

		if (!group->isPlaying) return;

		group->isPlaying = false;
		group->currentTime = 0.0f;

		// 停止時は Reset() で粒子を即消滅させず、linger でシミュレーションを続けて自然に消えるのを待つ
		// （剣トレイル等を Stop したとき既存粒子がプツッと消えるのを防ぐ）
		group->lingerTimer = std::max(group->lingerTimer, GetGroupMaxLifetime(group));
	}

	void GpuEmitManager::StopAllEmitterGroups()
	{
		for (auto& [name, group] : groups_) {
			group->isPlaying = false;
			group->currentTime = 0.0f;
			group->lingerTimer = 0.0f;
		}
	}

	void GpuEmitManager::SetCamera(YoRigine::Camera* camera)
	{
		// GpuEmitManager 自身のカメラポインタを更新
		camera_ = camera;
#ifdef USE_IMGUI
		if (gizmoLine_) gizmoLine_->SetCamera(camera);
#endif
		// 全グループをループ
		for (auto& [groupName, groupData] : groups_) {
			for (auto& [emitterName, emitterDataPtr] : groupData->emitters) {
				if (emitterDataPtr) {
					GPUEmitter* emitter = emitterDataPtr->emitter.get();
					if (emitter) {
						emitter->SetCamera(camera);
					}
				}
			}
		}
	}

	/// <summary>
	/// エミッター名からデータ取得
	/// </summary>
	GpuEmitManager::EmitterData* GpuEmitManager::GetEmitter(const std::string& groupName, const std::string& emitterName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group) return nullptr;

		if (group->emitters.count(emitterName)) {
			return group->emitters.at(emitterName).get();
		}
		return nullptr;
	}

	/// <summary>
	/// 全エミッター名リスト取得
	/// </summary>
	std::vector<std::string> GpuEmitManager::GetEmitterNames() const
	{
		std::vector<std::string> names;
		for (const auto& [groupName, groupPtr] : groups_) {
			for (const auto& [emitterName, _] : groupPtr->emitters) {
				names.push_back(emitterName);
			}
		}
		return names;
	}

	/// <summary>
	/// ロード済みグループ名の一覧
	/// </summary>
	std::vector<std::string> GpuEmitManager::GetGroupNames() const
	{
		std::vector<std::string> names;
		names.reserve(groups_.size());
		for (const auto& [name, group] : groups_) {
			names.push_back(name);
		}
		return names;
	}

	/// <summary>
	/// 指定エミッターが存在するか
	/// </summary>
	bool GpuEmitManager::HasEmitter(const std::string& emitterName) const
	{
		for (const auto& [groupName, groupPtr] : groups_) {
			if (groupPtr->emitters.count(emitterName)) {
				return true;
			}
		}
		return false;
	}

	/// <summary>
	/// エミッターの形状パラメータを GPUEmitter に反映
	/// </summary>
	void GpuEmitManager::UpdateEmitterParams(EmitterData* emitterData)
	{
		if (!emitterData || !emitterData->emitter) {
			return;
		}

		switch (emitterData->shape)
		{
		case EmitterShape::Sphere:
		{
			const auto& p = emitterData->sphereParams;
			emitterData->emitter->UpdateSphereParams(
				p.translate, p.radius, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Box:
		{
			const auto& p = emitterData->boxParams;
			emitterData->emitter->UpdateBoxParams(
				p.translate, p.size, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Triangle:
		{
			const auto& p = emitterData->triangleParams;
			emitterData->emitter->UpdateTriangleParams(
				p.v1, p.v2, p.v3, p.translate, p.count, p.emitInterval
			);
		}
		break;

		case EmitterShape::Cone:
		{
			const auto& p = emitterData->coneParams;
			emitterData->emitter->UpdateConeParams(
				p.translate, p.direction, p.radius, p.height, p.count, p.emitInterval
			);
		}
		break;
		case EmitterShape::Mesh:
		{
			const auto& p = emitterData->meshParams;
			emitterData->emitter->UpdateMeshParams(
				p.model, p.translate, p.scale, p.rotation,
				p.count, p.emitInterval, p.emitMode
			);
		}
		break;
		}
	}

	/// <summary>
	/// JSON ファイルとしてエミッター情報を保存
	/// </summary>
	bool GpuEmitManager::SaveToFile(const std::string& filepath)
	{
		try
		{
			nlohmann::json json = ToJson();

			// 必要ならディレクトリ作成
			std::filesystem::path path(filepath);
			std::filesystem::create_directories(path.parent_path());

			std::ofstream file(filepath);
			if (!file.is_open()) {
				return false;
			}

			file << json.dump(4);
			return true;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error saving emitters: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// グループ単位で、そのグループ自身の sourceFilePath に保存する。
	/// 1ファイル1グループを基本とし、複数グループが1ファイルに混ざるのを防ぐ。
	/// </summary>
	bool GpuEmitManager::SaveGroupToFile(const std::string& groupName)
	{
		EmitterGroup* group = GetGroup(groupName);
		if (!group) {
			Logger("GpuEmitManager::SaveGroupToFile : グループ '" + groupName + "' が見つかりません");
			return false;
		}

		// 保存先が未設定なら名前から自動決定
		if (group->sourceFilePath.empty()) {
			group->sourceFilePath = "Resources/Json/GpuEmitters/" + groupName + ".json";
		}

		try
		{
			nlohmann::json json;
			json["version"] = "1.0";
			json["groups"] = nlohmann::json::array();
			json["groups"].push_back(ToJsonGroup(group));

			std::filesystem::path path(group->sourceFilePath);
			if (path.has_parent_path()) {
				std::filesystem::create_directories(path.parent_path());
			}

			std::ofstream file(group->sourceFilePath);
			if (!file.is_open()) {
				Logger("GpuEmitManager::SaveGroupToFile : ファイルを開けません " + group->sourceFilePath);
				return false;
			}

			file << json.dump(4);
			Logger("GpuEmitManager: グループ '" + groupName + "' を保存 → " + group->sourceFilePath);
			return true;
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error saving group: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// JSON ファイルからエミッター情報を読み込み
	/// </summary>
	bool GpuEmitManager::LoadFromFile(const std::string& filepath)
	{
		try
		{
			std::ifstream file(filepath);

			if (!file.is_open()) {
				return false;
			}

			nlohmann::json json;
			file >> json;

			return FromJson(json, filepath);
		}
		catch (const std::exception& e)
		{
			std::cerr << "Error loading emitters: " << e.what() << std::endl;
			return false;
		}
	}


	/// <summary>
	/// ディレクトリ内全てを読み込む
	/// </summary>
	/// <param name="directory"></param>
	/// <returns></returns>
	bool GpuEmitManager::LoadAllEmitters(const std::string& directory)
	{
		ScanJsonDirectory(directory);
		bool allSucceeded = true; // 初期値は成功

		// スキャン結果のファイルパスをループ
		for (const auto& filepath : availableJsonFiles_) {

			// 各JSONファイルを個別に読み込み、エミッターとして登録
			if (LoadFromFile(directory + filepath)) {
				// 成功
				Logger("読み込み成功: " + filepath);
			}
			else {
				// 失敗
				Logger("読み込み失敗: " + filepath);
				allSucceeded = false; // 失敗フラグを立てる
			}
		}

		// 処理結果を返す
		return allSucceeded;
	}
	/// <summary>
	/// 全エミッターを JSON 化
	/// </summary>
	nlohmann::json GpuEmitManager::ToJson() const
	{
		nlohmann::json json;
		json["version"] = "1.0";
		json["groups"] = nlohmann::json::array();

		for (const auto& [groupName, groupPtr] : groups_)
		{
			json["groups"].push_back(ToJsonGroup(groupPtr.get()));
		}
		return json;
	}

	/// <summary>
	/// 単一グループを JSON 化（グループ単位保存・全体保存の両方から使う）
	/// </summary>
	/// <summary>
	/// 1エミッタの全シリアライズ対象を AutoJson ツリーへ登録する（保存・読込の単一情報源）。
	/// 調整パラメータを増やすときはここに .Add() を1行足すだけ。
	/// key 名は既存JSONと一致させ後方互換を保つ（child.* は "trailXxx" にフラット化）。
	/// </summary>
	void GpuEmitManager::BuildEmitterSchema(EmitterData& e,
		AutoJson& root, AutoJson& sphere, AutoJson& box, AutoJson& tri,
		AutoJson& cone, AutoJson& mesh, AutoJson& particle, AutoJson& particleMesh) const
	{
		auto& sp = e.sphereParams;
		sphere.Add("translate", &sp.translate).Add("radius", &sp.radius)
			.Add("count", &sp.count).Add("emitInterval", &sp.emitInterval);

		auto& bp = e.boxParams;
		box.Add("translate", &bp.translate).Add("size", &bp.size)
			.Add("count", &bp.count).Add("emitInterval", &bp.emitInterval);

		auto& tp = e.triangleParams;
		tri.Add("v1", &tp.v1).Add("v2", &tp.v2).Add("v3", &tp.v3)
			.Add("count", &tp.count).Add("emitInterval", &tp.emitInterval);

		auto& cp = e.coneParams;
		cone.Add("translate", &cp.translate).Add("direction", &cp.direction)
			.Add("radius", &cp.radius).Add("height", &cp.height)
			.Add("count", &cp.count).Add("emitInterval", &cp.emitInterval);

		// meshParams: model ポインタだけは名前で特別扱いするため schema には含めない
		auto& mp = e.meshParams;
		mesh.Add("translate", &mp.translate).Add("scale", &mp.scale)
			.Add("rotation", &mp.rotation).Add("count", &mp.count)
			.Add("emitInterval", &mp.emitInterval).Add("emitMode", &mp.emitMode);

		// particleParams: child(トレイル) は "trailXxx" にフラット化して既存JSON互換
		auto& pp = e.particleParams;
		particle.Add("lifeTime", &pp.lifeTime).Add("lifeTimeVariance", &pp.lifeTimeVariance)
			.Add("startScale", &pp.startScale).Add("startScaleVariance", &pp.startScaleVariance)
			.Add("endScale", &pp.endScale).Add("endScaleVariance", &pp.endScaleVariance)
			.Add("rotation", &pp.rotation).Add("rotationVariance", &pp.rotationVariance)
			.Add("rotationSpeed", &pp.rotationSpeed).Add("rotationSpeedVariance", &pp.rotationSpeedVariance)
			.Add("velocity", &pp.velocity).Add("velocityVariance", &pp.velocityVariance)
			.Add("startColor", &pp.startColor).Add("startColorVariance", &pp.startColorVariance)
			.Add("endColor", &pp.endColor).Add("endColorVariance", &pp.endColorVariance)
			.Add("gravity", &pp.gravity).Add("isBillboard", &pp.isBillboard)
			.Add("isTrail", &pp.child.isTrail).Add("isInheritScale", &pp.child.isInheritScale)
			.Add("trailLifeTime", &pp.child.lifeTime).Add("trailEmissionCount", &pp.child.emissionCount)
			.Add("trailMinDistance", &pp.child.minDistance)
			.Add("trailStartScale", &pp.child.startScale).Add("trailEndScale", &pp.child.endScale);

		auto& pm = e.particleMeshParams;
		particleMesh.Add("width", &pm.width).Add("height", &pm.height).Add("depth", &pm.depth)
			.Add("outerRadius", &pm.outerRadius).Add("innerRadius", &pm.innerRadius)
			.Add("radius", &pm.radius).Add("angleDegree", &pm.angleDegree)
			.Add("divide", &pm.divide).Add("subdivisions", &pm.subdivisions);

		// ルート: スカラ + enum + フォースフィールド配列 + 各グループ
		root.Add("name", &e.name).Add("shape", &e.shape)
			.Add("isActive", &e.isActive).Add("textureFilePath", &e.texturePath)
			.Add("particleMeshShape", &e.particleMeshShape)
			.Add("forceFields", &e.forceFields);
		root.AddGroup("sphereParams", sphere)
			.AddGroup("boxParams", box)
			.AddGroup("triangleParams", tri)
			.AddGroup("coneParams", cone)
			.AddGroup("meshParams", mesh)
			.AddGroup("particleParams", particle)
			.AddGroup("particleMeshParams", particleMesh);
	}

	/// <summary>
	/// エミッタ1つ → JSON。model ポインタだけは名前(modelName)で書き出す特別扱い。
	/// </summary>
	nlohmann::json GpuEmitManager::SerializeEmitter(EmitterData& e) const
	{
		AutoJson root, sphere, box, tri, cone, mesh, particle, particleMesh;
		BuildEmitterSchema(e, root, sphere, box, tri, cone, mesh, particle, particleMesh);

		nlohmann::json j;
		root.Save(j);
		// 特別扱い: YoRigine::Model* はポインタなので名前で保存（schema 外の唯一の項目）
		j["meshParams"]["modelName"] = e.meshParams.model ? e.meshParams.model->GetName() : "";
		return j;
	}

	/// <summary>
	/// JSON → 生成済みエミッタ。model は modelName から解決する。
	/// </summary>
	void GpuEmitManager::DeserializeEmitter(EmitterData& e, const nlohmann::json& j) const
	{
		AutoJson root, sphere, box, tri, cone, mesh, particle, particleMesh;
		BuildEmitterSchema(e, root, sphere, box, tri, cone, mesh, particle, particleMesh);
		root.Load(j);

		// 特別扱い: modelName から YoRigine::Model* を解決
		if (j.contains("meshParams")) {
			const std::string modelName = j["meshParams"].value("modelName", std::string{});
			if (!modelName.empty()) {
				assert(modelManager_ && "GpuEmitManager : SetModelManager() を先に呼ぶこと");
				e.meshParams.model = modelManager_->FindModel(modelName);
			}
		}
	}

	nlohmann::json GpuEmitManager::ToJsonGroup(const EmitterGroup* groupPtr) const
	{
		nlohmann::json groupJson;
		if (!groupPtr) return groupJson;
		{
			// ------------------------------------------------------------
			// エミッターグループのパラメータをシリアライズ
			// ------------------------------------------------------------
			groupJson["groupName"] = groupPtr->name;
			groupJson["isActive"] = groupPtr->isActive;
			groupJson["isPlaying"] = groupPtr->isPlaying;
			groupJson["currentTime"] = groupPtr->currentTime;
			groupJson["systemDuration"] = groupPtr->systemDuration;
			groupJson["translate"] = groupPtr->translate;
			groupJson["emitters"] = nlohmann::json::array(); // グループ内のエミッター配列

			// 各エミッタは AutoJson スキーマ（BuildEmitterSchema）で一括シリアライズ
			for (const auto& [emitterName, e] : groupPtr->emitters)
			{
				groupJson["emitters"].push_back(SerializeEmitter(*e));
			}
		}
		return groupJson;
	}


	/// <summary>
	/// JSON からエミッター情報を復元
	/// </summary>
	bool GpuEmitManager::FromJson(const nlohmann::json& json, const std::string& sourceFilePath)
	{
		try
		{

			bool loaded = false;

			// 新しいグループ形式 ("groups") の読み込みを試みる
			if (json.contains("groups"))
			{
				// グループの配列をループ
				for (const auto& groupJson : json["groups"])
				{
					//------------------------------------------------------------
					// グループ情報の復元
					//------------------------------------------------------------
					// groupName がない場合に備えてデフォルト値 "LoadedGroup" を設定
					std::string groupName = groupJson.value("groupName", "LoadedGroup");

					// グループの作成（由来ファイルパスを明示的に渡す＝起動時ロードでも確実に作成される）
					EmitterGroup* group = CreateEmitterGroup(groupName, sourceFilePath);
					if (!group) continue;

					// グループパラメータの復元
					group->isActive = groupJson.value("isActive", true);
					group->isPlaying = groupJson.value("isPlaying", false);
					group->currentTime = groupJson.value("currentTime", 0.0f);
					group->systemDuration = groupJson.value("systemDuration", 0.0f);

					if (groupJson.contains("translate")) {
						group->translate = groupJson["translate"];
					}

					if (groupJson.contains("emitters")) // エミッター配列があればループ
					{
						for (const auto& j : groupJson["emitters"])
						{
							// 補助関数でエミッターをロード
							LoadEmitterFromJson(groupName, j);
						}
						loaded = true; // グループが存在し、処理が実行された
					}
				}
			}
			return loaded; // ロードできたかどうかを返す
		}
		catch (const std::exception& e)
		{
			// デシリアライズ中にエラーが発生した場合
			std::cerr << "Error parsing JSON: " << e.what() << std::endl;
			return false;
		}
	}

	/// <summary>
	/// JSONから単一のエミッター情報を復元
	/// </summary>
	bool GpuEmitManager::LoadEmitterFromJson(const std::string& groupName, const nlohmann::json& j)
	{
		// エミッタ生成には name/shape/texturePath が先に必要なので、この3つだけ直接読む。
		// 残りの全フィールドは DeserializeEmitter（AutoJson スキーマ）が一括で流し込む。
		std::string  name = j.value("name", "UnnamedEmitter");
		EmitterShape shape = static_cast<EmitterShape>(j.value("shape", 0)); // 0=Sphere
		std::string  texturePath = j.value("textureFilePath", "");

		if (!CreateEmitter(groupName, name, texturePath, shape)) {
			return false;
		}
		auto* e = GetEmitter(groupName, name);
		if (!e) return false;

		// 全パラメータを一括復元（欠損キーは既定値のまま＝後方互換）
		DeserializeEmitter(*e, j);

		// 適用
		UpdateEmitterParams(e);
		UpdateParticleParams(e);
		if (e->emitter) {
			e->emitter->SetParticleMesh(e->particleMeshShape, e->particleMeshParams);
		}
		return true;
	}
	// 画像一覧をスキャン
	void GpuEmitManager::ScanTextureDirectory(const std::string& directory)
	{
		currentTextureDir_ = directory;
		availableTextures_.clear();
		availableFolders_.clear();

		for (auto& p : std::filesystem::directory_iterator(directory))
		{
			if (p.is_directory()) {
				availableFolders_.push_back(p.path().filename().string());
				continue;
			}

			if (!p.is_regular_file()) continue;

			std::string ext = p.path().extension().string();

			std::transform(
				ext.begin(), ext.end(), ext.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); }
			);

			if (ext == ".png" || ext == ".jpg" || ext == ".dds")
			{
				availableTextures_.push_back(p.path().string());
			}
		}
	}
	// JSONファイル一覧をスキャン
	void GpuEmitManager::ScanJsonDirectory(const std::string& directory)
	{
		// ディレクトリが存在しない、またはディレクトリでない場合はクリアして終了
		if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
			availableJsonFiles_.clear();
			return;
		}

		currentJsonDir_ = directory;
		availableJsonFiles_.clear();

		for (auto& p : std::filesystem::directory_iterator(directory))
		{
			// ファイルであること
			if (!p.is_regular_file()) continue;

			// 拡張子が .json であること
			if (p.path().extension().string() == ".json")
			{
				availableJsonFiles_.push_back(p.path().filename().string());
			}
		}
	}
}

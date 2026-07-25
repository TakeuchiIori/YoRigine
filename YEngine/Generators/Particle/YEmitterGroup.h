#pragma once
#include "YParticleEmitter.h"
#include <vector>
#include <string>
#include <memory>
#include <json.hpp>

/// <summary>
/// 複数の YParticleEmitter をまとめて管理するグループ
/// </summary>
class YEmitterGroup {
public:
	// コンストラクタ・デストラクタ
	explicit YEmitterGroup(const std::string& groupName);
	~YEmitterGroup();

	// エミッター管理
	YParticleEmitter& AddEmitter(const std::string& systemName, const Vector3& localOffset = { 0,0,0 });
	YParticleEmitter* GetEmitter(size_t index);
	size_t GetEmitterCount() const;
	void RemoveEmitter(size_t index);

	// 更新
	void Update(float deltaTime);

	// グループ全体への操作
	void EmitAll(int count = -1);
	void SetPosition(const Vector3& pos);
	void SetAutoEmitAll(bool autoEmit);

	// アクセサ
	const std::string& GetName() const;
	void SetName(const std::string& name);

	bool IsActive() const;
	void SetActive(bool active);

	// このグループをワンショット発火したとき、最後の粒が消えるまでの秒数（再生尺）。
	// 各エミッタの対象システムの最大寿命のうち最大値を返す。エミッタ無しなら 0。
	float GetEffectDuration() const;

	const Vector3& GetPosition() const;
	const std::vector<std::unique_ptr<YParticleEmitter>>& GetEmitters() const;

	// JSON保存/読み込み
	nlohmann::json SaveToJson() const;
	void LoadFromJson(const nlohmann::json& j);

private:
	std::string groupName_;
	bool isActive_;
	bool isAutoEmit_;
	Vector3 position_;
	std::vector<std::unique_ptr<YParticleEmitter>> emitters_;
};
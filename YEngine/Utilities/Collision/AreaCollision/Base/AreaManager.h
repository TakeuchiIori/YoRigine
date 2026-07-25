#pragma once

// C++
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

// Engine
#include "BaseArea.h"
#include <Loaders/Json/Use/AutoJson.h>
#include <WorldTransform/WorldTransform.h>

// エリア判定を一括管理するクラス
// 複数のエリアを登録し、更新や描画を一括で行う
class AreaManager
{
public:
	///************************* シングルトン *************************///

	static AreaManager* GetInstance();

	AreaManager();
	~AreaManager() = default;

public:
	///************************* 基本関数 *************************///

	void Initialize();

	// 単一ターゲットの更新 (Enter/Exit を targetKey で識別)
	//   deltaTime: Stay tick 間隔を使うエリア向けの経過秒（毒ダメージ等）。
	void Update(const Vector3& targetPosition, void* targetKey = nullptr, float deltaTime = 0.0f);

	// 複数ターゲットの一括更新。各ターゲットに対し全エリアの Enter/Exit を判定する。
	//   deltaTime: Stay tick 間隔を使うエリア向けの経過秒（敵・プレイヤーへの継続効果）。
	struct AreaTarget {
		void*   key;
		Vector3 position;
	};
	void UpdateTargets(const std::vector<AreaTarget>& targets, float deltaTime = 0.0f);

	// あるターゲットを忘却 (キャラクタ破棄時に呼ぶ)
	void ForgetTarget(void* targetKey);

	void Draw(YoRigine::Line* line,
		const std::initializer_list<std::string>& excludes = {});
	void DrawArea(const std::string& name, YoRigine::Line* line);
	void Reset();

public:
	///************************* エリア管理 *************************///

	void AddArea(const std::string& name, std::shared_ptr<BaseArea> area);
	void RemoveArea(const std::string& name);
	std::shared_ptr<BaseArea> GetArea(const std::string& name);

	void SetAreaActive(const std::string& name, bool active);
	void SetAllAreasActive(bool active);

	// エリア一覧を取得（AreaEditor 用）
	const std::unordered_map<std::string, std::shared_ptr<BaseArea>>& GetAreas() const
	{ return areas_; }

public:
	///************************* JSON 保存・読み込み *************************///

	void SaveAllToFile(const std::string& filepath);
	void LoadAllFromFile(const std::string& filepath);

	// タイプ文字列からエリアを生成するファクトリ（AreaEditor からも使用）
	static std::shared_ptr<BaseArea> CreateArea(const std::string& typeString);

public:
	///************************* オブジェクト登録管理 *************************///

	struct RestrictedObject {
		YoRigine::WorldTransform* worldTransform = nullptr;
		bool            enabled        = true;
		std::string     tag            = "";

		RestrictedObject() = default;
		RestrictedObject(YoRigine::WorldTransform* wt, const std::string& t = "")
			: worldTransform(wt), enabled(true), tag(t) {
		}
	};

	void RegisterObject(YoRigine::WorldTransform* wt, const std::string& tag = "");
	void UnregisterObject(YoRigine::WorldTransform* wt);
	void UpdateSingleObject(YoRigine::WorldTransform* wt);
	void UpdateRestrictedObjects();
	void SetObjectRestrictionEnabled(YoRigine::WorldTransform* wt, bool enabled);
	void ClearAllObjects();

public:
	///************************* 判定関数 *************************///

	bool    IsInsideAnyArea(const Vector3& position) const;
	bool    IsInsideAreaByPurpose(const Vector3& position, AreaPurpose purpose) const;
	Vector3 ClampToNearestArea(const Vector3& position) const;

public:
	///************************* デバッグ設定 *************************///

	void SetDebugDrawEnabled(bool enabled) { isDebugDrawEnabled_ = enabled; }
	bool IsDebugDrawEnabled() const        { return isDebugDrawEnabled_; }

private:
	///************************* コピー禁止 *************************///

	AreaManager(const AreaManager&) = delete;
	AreaManager& operator=(const AreaManager&) = delete;

private:
	///************************* メンバ変数 *************************///

	std::unordered_map<std::string, std::shared_ptr<BaseArea>> areas_;
	bool isDebugDrawEnabled_ = false;
	std::vector<RestrictedObject> restrictedObjects_;

	// ---- JSON インデックス（名前・タイプ・Purposeの並列ベクタ）----
	AutoJson             indexAj_;
	std::vector<std::string> savedNames_;
	std::vector<std::string> savedTypes_;
	std::vector<std::string> savedPurposes_;
};

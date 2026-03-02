#pragma once

// C++
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

// Engine
#include "BaseArea.h"
#include <WorldTransform/WorldTransform.h>

// エリア判定を一括管理するクラス
// 複数のエリアを登録し、更新や描画を一括で行う
class AreaManager
{
public:
	///************************* シングルトン *************************///

	static AreaManager* GetInstance();

	AreaManager() = default;
	~AreaManager() = default;

public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// 更新（すべてのエリアを更新）
	void Update(const Vector3& targetPosition);

	// 描画（すべてのエリアを描画）
	void Draw(Line* line);

	// リセット（すべてのエリアをクリア）
	void Reset();

public:
	///************************* エリア管理 *************************///

	// エリアを追加
	void AddArea(const std::string& name, std::shared_ptr<BaseArea> area);

	// エリアを削除
	void RemoveArea(const std::string& name);

	// エリアを名前で取得
	std::shared_ptr<BaseArea> GetArea(const std::string& name);

	// エリアの有効/無効を切り替え
	void SetAreaActive(const std::string& name, bool active);

	// すべてのエリアの有効/無効を切り替え
	void SetAllAreasActive(bool active);


public:
	///************************* オブジェクト登録管理 *************************///

	// エリア制限対象のオブジェクト構造体
	struct RestrictedObject {
		WorldTransform* worldTransform = nullptr;  // ワールドトランスフォームへのポインタ
		bool enabled = true;                       // 制限の有効/無効
		std::string tag = "";                      // 識別用タグ(デバッグ用)

		RestrictedObject() = default;
		RestrictedObject(WorldTransform* wt, const std::string& t = "")
			: worldTransform(wt), enabled(true), tag(t) {
		}
	};

	// オブジェクトを登録(エリア制限対象にする)
	void RegisterObject(WorldTransform* wt, const std::string& tag = "");

	// オブジェクトの登録を解除
	void UnregisterObject(WorldTransform* wt);
	// 登録されている特定のオブジェクトの位置をエリア内に補正
	void UpdateSingleObject(WorldTransform* wt);
	// 登録されているすべてのオブジェクトの位置をエリア内に補正
	void UpdateRestrictedObjects();

	// 特定のオブジェクトの制限を有効/無効化
	void SetObjectRestrictionEnabled(WorldTransform* wt, bool enabled);

	// すべてのオブジェクトをクリア
	void ClearAllObjects();

public:
	///************************* 判定関数 *************************///

	// 指定位置がいずれかのエリア内にあるかチェック
	bool IsInsideAnyArea(const Vector3& position) const;

	// 指定位置が特定の用途のエリア内にあるかチェック
	bool IsInsideAreaByPurpose(const Vector3& position, AreaPurpose purpose) const;

	// 指定位置を最も近いエリア境界内にクランプ
	Vector3 ClampToNearestArea(const Vector3& position) const;

public:
	///************************* デバッグ設定 *************************///

	// デバッグ描画の有効/無効
	void SetDebugDrawEnabled(bool enabled) { isDebugDrawEnabled_ = enabled; }
	bool IsDebugDrawEnabled() const { return isDebugDrawEnabled_; }

private:
	///************************* コピー禁止 *************************///

	AreaManager(const AreaManager&) = delete;
	AreaManager& operator=(const AreaManager&) = delete;

private:
	///************************* メンバ変数 *************************///

	// 登録されているエリアのマップ（名前 -> エリア）
	std::unordered_map<std::string, std::shared_ptr<BaseArea>> areas_;

	// デバッグ描画フラグ
	bool isDebugDrawEnabled_ = false;

	// エリア制限対象のオブジェクトリスト
	std::vector<RestrictedObject> restrictedObjects_;
};
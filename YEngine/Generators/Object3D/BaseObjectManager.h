#pragma once

// Engine
#include "Object3D/BaseObject.h"
#include "Systems/Camera/Camera.h"

// C++
#include <memory>
#include <vector>
#include <string>
#include <type_traits>
#include <utility>
#include <typeinfo>

// ============================================================
// BaseObjectManager
// ============================================================
// BaseObject を継承したゲームオブジェクト (Player / Enemy / Ground 等) を
// まとめて管理するマネージャ。
//
// 目的:
//   - カメラを 1 回だけ SetCamera しておけば、登録済みオブジェクト全体の
//     Update / Draw を一括で回せるようにして、シーン側の配線を減らす。
//   - 既存の Initialize(Camera*) シグネチャはそのまま使う (A 方式) ので、
//     既存オブジェクトは無改修で乗せられる。
//
// 所有モデル (ハイブリッド):
//   - Add<T>()    … マネージャが unique_ptr で所有する (生成 + camera 注入 + Initialize)。
//   - Register()  … 所有は呼び出し側のまま、駆動 (Update/Draw) だけ委譲する。
// ============================================================
class BaseObjectManager {
public:
	// ============================================================
	// 管理エントリ
	// ============================================================
	// 1 オブジェクト 1 エントリ。owned が非 null なら Add 由来 (マネージャ所有)、
	// null なら Register 由来 (呼び出し側所有)。参照は常に ptr 経由で行う。
	struct Entry {
		BaseObject* ptr = nullptr;             // 参照用 (owned / 外部所有どちらでも有効)
		std::unique_ptr<BaseObject> owned;     // Add 時のみ実体を持つ
		std::string name;                      // 表示名 / 検索キー
		bool pendingDestroy = false;           // フレーム末に除去する予約フラグ
	};

	// ============================================================
	// 基本関数
	// ============================================================
	static BaseObjectManager* GetInstance();

	void Initialize();
	void Finalize();

	// ============================================================
	// カメラ
	// ============================================================
	// 一度セットすれば、以降の Add<T>() で生成するオブジェクトに自動注入される。
	void SetCamera(Camera* camera) { camera_ = camera; }
	Camera* GetCamera() const { return camera_; }

	// ============================================================
	// 登録 / 生成
	// ============================================================
	// オブジェクトを生成してマネージャ所有で登録する。
	// camera 注入 → Initialize(camera) まで行い、生成した T* を返す。
	template<class T, class... Args>
	T* Add(const std::string& name = "", Args&&... args);

	// 既に呼び出し側が所有しているオブジェクトを駆動対象に登録する (所有は移さない)。
	// Initialize は呼び出し側で済ませておくこと。
	void Register(BaseObject* obj, const std::string& name = "");

	// 駆動対象から外す。Add 由来 (マネージャ所有) の場合は実体も破棄される。
	// 反復処理中の呼び出しは避けること。
	void Unregister(BaseObject* obj);

	// 遅延破棄を予約する。実際の除去はフレーム末 (UpdateAll の最後) に行うため、
	// 更新ループ中から安全に呼べる。
	void Destroy(BaseObject* obj);

	// すべてのオブジェクトを破棄してクリアする。
	void ClearAll();

	// ============================================================
	// 一括駆動
	// ============================================================
	// active なオブジェクトのみ処理する。UpdateAll はフレーム末に遅延破棄を実行する。
	void UpdateAll();
	void DrawAll();
	void DrawAnimationAll();
	void DrawCollisionAll();
	void DrawShadowAll();

	// インスタンシング対応の一括描画。
	// IsInstanceable() が true のオブジェクトは InstancedObject3d でまとめ描き、
	// それ以外 (アニメ / 特殊描画) は従来どおり個別 Draw() する。
	// カラーパス。カメラは SetCamera() 済みのものを使う。
	void DrawAllInstanced();
	// 影パス。gLight は呼び出し側で設定済みであること (既存の影描画と同じ前提)。
	void DrawShadowAllInstanced();

	// ============================================================
	// 検索 / アクセッサ
	// ============================================================
	// 名前で最初に一致したオブジェクトを返す。空文字 / 未登録なら nullptr。
	BaseObject* FindByName(const std::string& name);

	// 指定型 (T 派生) の全オブジェクトを抽出して返す。
	template<class T>
	std::vector<T*> GetAllOfType();

	const std::vector<Entry>& GetEntries() const { return entries_; }
	int GetObjectCount() const { return static_cast<int>(entries_.size()); }

	// ============================================================
	// インスペクタ (Debug のみ)
	// ============================================================
	// 登録オブジェクト一覧 + 選択オブジェクトの詳細 (名前 / アクティブ / SRT) を描画する。
	// Release ビルドでは中身が空になる。
	void DrawInspector();

private:
	// ============================================================
	// 内部
	// ============================================================
	BaseObjectManager() = default;
	~BaseObjectManager() = default;
	BaseObjectManager(const BaseObjectManager&) = delete;
	BaseObjectManager& operator=(const BaseObjectManager&) = delete;

	// pendingDestroy が立ったエントリをまとめて除去する。
	void ProcessPendingDestroy();

	static BaseObjectManager* instance_;

	Camera* camera_ = nullptr;            // Add 時に各オブジェクトへ注入するカメラ
	std::vector<Entry> entries_;          // 管理中の全エントリ
	int selectedIndex_ = -1;              // インスペクタで選択中の行 (-1 で未選択)
	bool inspectorRegistered_ = false;    // インスペクタパネルを Editor に登録済みか
};

// ============================================================
// Add (テンプレート実装)
// ============================================================
template<class T, class... Args>
T* BaseObjectManager::Add(const std::string& name, Args&&... args) {
	static_assert(std::is_base_of_v<BaseObject, T>,
		"BaseObjectManager::Add<T> : T は BaseObject を継承している必要があります");

	//------------------------------------------------------------
	// 生成 → 名前付け → camera 注入 & Initialize
	//------------------------------------------------------------
	std::unique_ptr<T> obj = std::make_unique<T>(std::forward<Args>(args)...);
	T* raw = obj.get();
	raw->SetName(name);
	raw->Initialize(camera_);

	//------------------------------------------------------------
	// エントリとして所有を引き取る
	//------------------------------------------------------------
	Entry entry;
	entry.ptr = raw;
	entry.owned = std::move(obj);
	entry.name = name;
	entries_.push_back(std::move(entry));

	return raw;
}

// ============================================================
// GetAllOfType (テンプレート実装)
// ============================================================
template<class T>
std::vector<T*> BaseObjectManager::GetAllOfType() {
	std::vector<T*> result;
	for (auto& entry : entries_) {
		if (!entry.ptr || entry.pendingDestroy) continue;
		if (T* casted = dynamic_cast<T*>(entry.ptr)) {
			result.push_back(casted);
		}
	}
	return result;
}

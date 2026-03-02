#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include "UIBase.h"

///************************* UI管理クラス *************************///
namespace YoRigine {
	class UIManager {
	public:
		///************************* シングルトン *************************///

		// インスタンス取得
		static UIManager* GetInstance();

		///************************* UI基本管理 *************************///

		// UIを追加
		void AddUI(const std::string& id, std::unique_ptr<UIBase> ui);

		// UIを削除
		void RemoveUI(const std::string& id);

		// UIを取得
		UIBase* GetUI(const std::string& id);

		// すべてのUIを取得
		const std::unordered_map<std::string, std::unique_ptr<UIBase>>& GetAllUIs() const {
			return uiElements_;
		}

		// UIが存在するか確認
		bool HasUI(const std::string& id) const;

		// すべてのUIをクリア
		void Clear();

		// UIのIDを変更
		bool RenameUI(const std::string& oldId, const std::string& newId);

		///************************* 一括更新・描画 *************************///

		// すべてのUIを更新
		void UpdateAll();

		// すべてのUIを描画（レイヤー順）
		void DrawAll();

		// 個別に描画
		void Draw(const std::string& id);

		///************************* レイヤー管理 *************************///

		// レイヤーでソート（描画順を更新）
		void SortByLayer();

		// 特定レイヤーの表示/非表示
		void ShowLayer(int layer, bool show);

		// すべてのUIの表示/非表示
		void ShowAll(bool show);

		// 特定レイヤーのUIリストを取得
		std::vector<UIBase*> GetUIsByLayer(int layer);

		///************************* シーン管理 *************************///

		// 現在のUIレイアウトをシーンとして保存
		bool SaveScene(const std::string& sceneName);

		// シーンを読み込み（現在のUIは削除される）
		bool LoadScene(const std::string& sceneName);

		// 利用可能なシーン一覧を取得
		std::vector<std::string> GetAvailableScenes() const;

		// シーンを削除
		bool DeleteScene(const std::string& sceneName);

		///************************* グループ管理 *************************///

		// UIをグループに追加
		void AddToGroup(const std::string& groupName, const std::string& uiId);

		// グループからUIを削除
		void RemoveFromGroup(const std::string& groupName, const std::string& uiId);

		// グループのUIを一括表示/非表示
		void ShowGroup(const std::string& groupName, bool show);

		// グループのUIリストを取得
		std::vector<UIBase*> GetGroup(const std::string& groupName);

		///************************* 検索・フィルタ *************************///

		// 名前で検索
		std::vector<UIBase*> FindByName(const std::string& name);

		// テクスチャパスで検索
		std::vector<UIBase*> FindByTexture(const std::string& texturePath);

		///************************* デバッグ・エディタ *************************///

		// ImGuiデバッグウィンドウ
		void ImGuiDebug();

		// 統計情報構造体
		struct Statistics {
			int totalUIs = 0;
			int visibleUIs = 0;
			int hiddenUIs = 0;
			std::unordered_map<int, int> uisByLayer;
		};

		// 統計情報を取得
		Statistics GetStatistics() const;

	private:
		///************************* 基本構造 *************************///

		UIManager() = default;
		~UIManager() = default;
		UIManager(const UIManager&) = delete;
		UIManager& operator=(const UIManager&) = delete;

		///************************* 管理マップ *************************///

		// UI要素のマップ（ID -> UIBase）
		std::unordered_map<std::string, std::unique_ptr<UIBase>> uiElements_;

		// 描画順序リスト（レイヤーソート後）
		std::vector<std::string> drawOrder_;

		// グループ管理（グループ名 -> UIのIDリスト）
		std::unordered_map<std::string, std::vector<std::string>> groups_;

		// シーン保存ディレクトリ
		static const std::string SCENE_DIRECTORY;

		// UI設定保存ディレクトリ
		static const std::string UI_CONFIG_DIRECTORY;
		///************************* ImGui補助関数 *************************///

		// 改良版テクスチャセレクターを表示
		void DisplayImprovedTextureSelector(UIBase* ui);

		// ディレクトリ内のテクスチャ一覧を表示
		void DisplayTextureDirectory(const std::string& path, const std::string& baseDir, UIBase* ui, const char* filter);

		///************************* ヘルパー関数 *************************///

		// 描画順序を再構築
		void RebuildDrawOrder();

		// 描画順序で前面へ移動（1つ前に）
		void MoveDrawOrderForward(const std::string& uiId);

		// 描画順序で背面へ移動（1つ後ろに）
		void MoveDrawOrderBackward(const std::string& uiId);

		// 一意なIDを生成
		std::string GenerateUniqueID(const std::string& baseName);

		// シーン名からUIConfig保存先ディレクトリを返す
		static std::string GetSceneConfigDir(const std::string& sceneName);


		///************************* ImGui状態管理 *************************///

		// 選択中のUI ID
		std::string selectedUIId_;

		// 全UIリスト表示フラグ
		bool showAllUIsList_ = true;

		// 現在のシーン名
		std::string currentSceneName_;
	};
}
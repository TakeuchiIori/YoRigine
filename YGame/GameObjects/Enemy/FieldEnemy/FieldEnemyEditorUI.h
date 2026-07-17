#pragma once
#ifdef USE_IMGUI

#include <string>
#include <vector>

class FieldEnemyManager;

// FieldEnemyManager のデバッグ情報表示・敵データ/スポーンポイント編集用 ImGui UI。
// USE_IMGUI ガードのため Release ビルドには含まれない。
// FieldEnemyManager から責務を分離するために切り出した (神クラス対策)。
// ギズモ/マーカーライン描画 (DrawSpawnPointGizmoHandles 等) は描画パイプラインと
// 密結合しているため FieldEnemyManager 側に残している。
class FieldEnemyEditorUI {
public:
	// フィールドエネミーエディター全体 (タブUI)。Editor の GameUI 登録から呼ばれる。
	static void ShowEnemyEditor(FieldEnemyManager& manager);

	// マネージャーの状態・アクティブ敵一覧などを表示するデバッグパネル。
	static void ShowDebugInfo(FieldEnemyManager& manager);

private:
	static void ShowEnemyDataEditor(FieldEnemyManager& manager);
	static void ShowSpawnPointEditor(FieldEnemyManager& manager);

	static void CreateNewEnemyData(FieldEnemyManager& manager);
	static std::string GenerateUniqueEnemyDataId(FieldEnemyManager& manager, const std::string& prefix);
	static bool RenameEnemyData(FieldEnemyManager& manager, const std::string& oldId, const std::string& newId);
	static std::vector<std::string> LoadBattleEnemyIdOptions();
	static void DeleteEnemyData(FieldEnemyManager& manager, const std::string& enemyId);

	static void CreateNewSpawnPoint(FieldEnemyManager& manager);
	static bool RenameSpawnPoint(FieldEnemyManager& manager, const std::string& oldId, const std::string& newId);
	static void DeleteSpawnPoint(FieldEnemyManager& manager, const std::string& spawnId);
};

#endif

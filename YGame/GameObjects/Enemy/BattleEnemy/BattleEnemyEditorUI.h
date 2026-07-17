#pragma once
#ifdef USE_IMGUI

class BattleEnemyManager;

// BattleEnemyManager のデバッグ情報表示・敵ベースデータ編集用 ImGui UI。
// USE_IMGUI ガードのため Release ビルドには含まれない。
// BattleEnemyManager から責務を分離するために切り出した (神クラス対策)。
class BattleEnemyEditorUI {
public:
	static void Draw(BattleEnemyManager& manager);
};

#endif

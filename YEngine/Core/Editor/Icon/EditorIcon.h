#pragma once
//=================================================================
// FontAwesome 5 Free — アイコン Unicode 定義 (Modern C++ 版)
// 
// 使用例）if (ImGui::Button((std::string(Icon::Play) +"再生").c_str()))
//=================================================================
namespace Icon {
	static constexpr const char* Play = "\uf04b";			// 再生
	static constexpr const char* Xmark = "\uf00d";			// キャンセル/閉じる
	static constexpr const char* Check = "\uf00c";			// チェックマーク
	static constexpr const char* Danger = "\uf071";			// 警告
	static constexpr const char* Bolt = "\uf0e7";			// 稲妻（高速/パフォーマンス）
	static constexpr const char* Bullhorn = "\uf0a1";		// メガホン（アナウンス/通知）
	static constexpr const char* Refresh = "\uf021";		// 更新/リロード
	static constexpr const char* Stop = "\uf04d";			// 停止
	static constexpr const char* PlusCircle = "\uf055";		// 追加（丸）
	static constexpr const char* Trash = "\uf1f8";			// ゴミ箱（削除）
	static constexpr const char* Cog = "\uf013";			// 歯車（設定）
	static constexpr const char* FloppyDisk = "\uf0c7";		// 保存
	static constexpr const char* FolderOpen = "\uf115";		// フォルダ（開いてる）
	static constexpr const char* CheckCircle = "\uf058";	// チェックマーク（丸）
	static constexpr const char* TimesCircle = "\uf057";	// バツマーク（丸）

    static constexpr const char* Cube = "\uf1b2";			// オブジェクト/3Dモデル
    static constexpr const char* List = "\uf0ca";			// アウトライナー(階層)
    static constexpr const char* Sliders = "\uf1de";		// ディテール(プロパティ)
    static constexpr const char* Folder = "\uf07b";			// コンテンツブラウザ
    static constexpr const char* Search = "\uf002";			// 検索
    static constexpr const char* Copy = "\uf0c5";			// 複製
    static constexpr const char* Puzzle = "\uf12e";			// プレファブ
    static constexpr const char* Plus = "\uf067";			// 追加
    static constexpr const char* Sieve = "\uf0b0";			// フィルター
    static constexpr const char* Eye = "\uf06e";			// 表示/非表示用
    static constexpr const char* Wrench = "\uf0ad";			// ツール (Wrench/レンチ)
    static constexpr const char* File = "\uf15b";			// ファイル

    static constexpr const char* Pause = "\uf04c";			// 一時停止
	static constexpr const char* Infinity = "\uf534";		// ループ (無限大)

	static constexpr const char* ArrowsAlt = "\uf0b2";		// 移動/位置 (Translate)
	static constexpr const char* SyncAlt = "\uf2f1";		// 回転 (Rotate)
	static constexpr const char* ExpandArrowsAlt = "\uf31e";// 拡縮 (Scale)
}
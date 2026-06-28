#pragma once
#include <string>

namespace WinAppUtil {

    // フィルターのタイプ定義
    enum class FilterType {
        All,     // すべて (*.*)
        Json,    // JSON (*.json)
        Texture, // 画像 (*.png; *.jpg)
        Model,   // モデル (*.obj; *.gltf)
    };

    /// <summary>
    /// ファイルを開くダイアログを表示
    /// </summary>
    /// <param name="title">ダイアログのタイトル</param>
    /// <param name="initialDir">初期表示ディレクトリ（相対パス可）</param>
    /// <param name="filter">フィルター（例: "JSON Files\0*.json\0All Files\0*.*\0"）</param>
    /// <returns>選択されたフルパス（キャンセル時は空文字列）</returns>
    std::string OpenFileDialog(
        const std::string& title = "Open File",
        const std::string& initialDir = "",
		FilterType filter = FilterType::All
    );

    /// <summary>
    /// ファイルを保存するダイアログを表示
    /// </summary>
    /// <param name="title">ダイアログのタイトル</param>
    /// <param name="initialDir">初期表示ディレクトリ</param>
    /// <param name="filter">フィルター</param>
    /// <param name="defaultExt">デフォルトの拡張子（例: "json"）</param>
    /// <returns>保存先のフルパス（キャンセル時は空文字列）</returns>
    std::string SaveFileDialog(
        const std::string& title = "Save File",
        const std::string& initialDir = "",
        FilterType filter = FilterType::All,
        const std::string& defaultExt = ""
    );

}


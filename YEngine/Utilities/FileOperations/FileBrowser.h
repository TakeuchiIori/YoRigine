#pragma once

// C++
#include <string>
#include <vector>
#include <functional>
#include <unordered_set>

#ifdef USE_IMGUI
#include <imgui.h>

namespace YoRigine {

    /// <summary>
    /// 汎用ファイルブラウザクラス
    /// ディレクトリスキャンと ImGui 表示を担当する。
    /// テクスチャ / JSON / モデル (.obj, .gltf) など拡張子を自由に指定できる。
    /// </summary>
    class FileBrowser
    {
    public:
        // -----------------------------------------------------------------------
        // 表示モード
        // -----------------------------------------------------------------------
        enum class DisplayMode
        {
            List,       // ファイル名をリスト形式で表示
            Grid,       // サムネイル + グリッド形式で表示（画像向け）
        };

        // -----------------------------------------------------------------------
        // サムネイルプロバイダ
        // テクスチャ以外のファイル種別ではデフォルト実装が nullptr を返す。
        // GPU ハンドルを返す関数をユーザーが差し替えることで任意サムネイルを表示できる。
        // -----------------------------------------------------------------------
        // 引数: ファイルのフルパス  / 戻り値: ImTextureID (0 の場合はアイコン代替)
        using ThumbnailProvider = std::function<ImTextureID(const std::string& fullPath)>;

        // -----------------------------------------------------------------------
        // コールバック型
        // -----------------------------------------------------------------------
        // ファイルが選択されたときに呼ばれる。引数はフルパス。
        using OnFileSelected = std::function<void(const std::string& fullPath)>;

    public:
        // -----------------------------------------------------------------------
        // コンストラクタ
        // -----------------------------------------------------------------------

        /// <summary>
        /// コンストラクタ
        /// </summary>
        /// <param name="rootDirectory">初期ディレクトリ (例: "Resources/Textures/")</param>
        /// <param name="extensions">許可する拡張子リスト (小文字、ドット付き。例: {".png", ".jpg"})</param>
        /// <param name="displayMode">表示モード</param>
        explicit FileBrowser(
            const std::string& rootDirectory = "Resources/",
            const std::vector<std::string>& extensions = {},
            DisplayMode displayMode = DisplayMode::List
        );

        ~FileBrowser() = default;

        // -----------------------------------------------------------------------
        // 設定
        // -----------------------------------------------------------------------

        /// 対応拡張子を設定する (小文字・ドット付き)
        void SetExtensions(const std::vector<std::string>& extensions);

        /// 表示モードを切り替える
        void SetDisplayMode(DisplayMode mode) { displayMode_ = mode; }

        /// グリッドモードのサムネイルサイズを設定する
        void SetThumbnailSize(float size) { thumbnailSize_ = size; }

        /// サムネイルプロバイダを差し替える（テクスチャを独自ロードする場合など）
        void SetThumbnailProvider(ThumbnailProvider provider) { thumbnailProvider_ = std::move(provider); }

        /// ファイル選択時のコールバックを設定する
        void SetOnFileSelected(OnFileSelected callback) { onFileSelected_ = std::move(callback); }

        // -----------------------------------------------------------------------
        // ディレクトリスキャン
        // -----------------------------------------------------------------------

        /// <summary>
        /// 指定ディレクトリをスキャンしてファイル / サブフォルダ一覧を更新する。
        /// 引数を省略すると現在のディレクトリを再スキャンする。
        /// </summary>
        /// <param name="directory">スキャン対象ディレクトリ。空文字の場合は currentDir_ を使用。</param>
        /// <returns>スキャンに成功したか</returns>
        bool Scan(const std::string& directory = "");

        /// <summary>
        /// 親ディレクトリへ移動してスキャンする
        /// </summary>
        void GoUp();

        // -----------------------------------------------------------------------
        // ImGui 描画
        // -----------------------------------------------------------------------

        /// <summary>
        /// ファイルブラウザ全体を Child ウィンドウとして描画する。
        /// ファイルが選択されると OnFileSelected コールバックが呼ばれる。
        /// </summary>
        /// <param name="id">ImGui Child の識別子</param>
        /// <param name="size">Child ウィンドウサイズ (0,0) で自動</param>
        void Draw(const char* id = "FileBrowser", ImVec2 size = ImVec2(0, 350));

        /// <summary>
        /// ファイルリストのみを描画する (Child 枠なし)。
        /// 独自レイアウトに埋め込む場合に使う。
        /// </summary>
        /// <returns>何かファイルが選択されたら true</returns>
        bool DrawFileList();

        // -----------------------------------------------------------------------
        // アクセッサ
        // -----------------------------------------------------------------------

        const std::string& GetCurrentDir() const { return currentDir_; }
        const std::vector<std::string>& GetFiles()   const { return files_; }
        const std::vector<std::string>& GetFolders() const { return folders_; }

        /// 最後に選択されたファイルのフルパスを返す
        const std::string& GetSelectedPath() const { return selectedPath_; }

        /// 手動で選択状態を設定する
        void SetSelectedPath(const std::string& path) { selectedPath_ = path; }

        /// スキャン結果が空かどうか
        bool IsEmpty() const { return files_.empty(); }

    private:
        // -----------------------------------------------------------------------
        // 内部処理
        // -----------------------------------------------------------------------
        bool MatchesFilter(const std::string& extension) const;

        /// グリッドモードでの描画
        bool DrawGrid();

        /// リストモードでの描画
        bool DrawList();

    private:
        // -----------------------------------------------------------------------
        // メンバ変数
        // -----------------------------------------------------------------------
        std::string                         currentDir_;
        std::vector<std::string>            files_;         // フルパス
        std::vector<std::string>            folders_;       // フォルダ名のみ
        std::unordered_set<std::string>     extensions_;    // 小文字・ドット付き

        DisplayMode     displayMode_ = DisplayMode::List;
        float           thumbnailSize_ = 64.0f;

        std::string         selectedPath_;
        ThumbnailProvider   thumbnailProvider_ = nullptr;
        OnFileSelected      onFileSelected_ = nullptr;
    };

} // namespace YoRigine
#endif // USE_IMGUI

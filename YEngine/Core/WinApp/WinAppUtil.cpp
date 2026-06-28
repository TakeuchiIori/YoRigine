#include "WinAppUtil.h"
#include <Windows.h>
#include <commdlg.h>
#include <filesystem>

namespace WinAppUtil {

    // enum を文字列に変換
    static const char* GetFilterString(FilterType type) {
        switch (type) {
        case FilterType::Json:    return "JSON Files\0*.json\0";
        case FilterType::Texture: return "Image Files\0*.png;*.jpg\0";
        case FilterType::Model:   return "Model Files\0*.obj;*.gltf\0";
        case FilterType::All:
        default:                  return "All Files\0*.*\0";
        }
    }

    std::string OpenFileDialog(const std::string& title, const std::string& initialDir, FilterType filter) {
        char filePath[MAX_PATH] = "";
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);

        // enum からフィルター文字列を取得
        ofn.lpstrFilter = GetFilterString(filter);
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title.c_str();

        // 初期ディレクトリの絶対パス解決
        std::string absPath;
        if (!initialDir.empty()) {
            if (std::filesystem::exists(initialDir)) {
                absPath = std::filesystem::absolute(initialDir).string();
                ofn.lpstrInitialDir = absPath.c_str();
            }
        }

        // ファイルが存在することを確認、カレントディレクトリを変更しない
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

        if (GetOpenFileNameA(&ofn)) {
            return std::string(filePath);
        }
        return "";
    }

    std::string SaveFileDialog(const std::string& title, const std::string& initialDir, FilterType filter, const std::string& defaultExt) {
        char filePath[MAX_PATH] = "";
        OPENFILENAMEA ofn{};
        ofn.lStructSize = sizeof(ofn);
        
        // enum からフィルター文字列を取得
        ofn.lpstrFilter = GetFilterString(filter);
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = title.c_str();
        ofn.lpstrDefExt = defaultExt.c_str();

        // 初期ディレクトリの絶対パス解決
        std::string absPath;
        if (!initialDir.empty()) {
            if (std::filesystem::exists(initialDir)) {
                absPath = std::filesystem::absolute(initialDir).string();
                ofn.lpstrInitialDir = absPath.c_str();
            }
        }

        // 上書き確認、カレントディレクトリを変更しない
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

        if (GetSaveFileNameA(&ofn)) {
            return std::string(filePath);
        }
        return "";
    }

}
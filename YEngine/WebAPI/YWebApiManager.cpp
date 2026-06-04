#include "YWebApiManager.h"
#include <curl/curl.h>
#include <iostream>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

YWebApiManager& YWebApiManager::GetInstance() {
    static YWebApiManager instance;
    return instance;
}

bool YWebApiManager::Initialize() {
    if (initialized_) return true;

    // アプリケーション全体で一生に一度のcURLグローバル初期化
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
         AddLog("[YWebApiManager] cURLのグローバル初期化に失敗しました。");
        return false;
    }

    initialized_ = true;
    AddLog( "[YWebApiManager] 初期化に成功しました。");
    return true;
}

void YWebApiManager::Finalize() {
    if (!initialized_) return;

    // cURLのグローバル解放
    curl_global_cleanup();
    initialized_ = false;
    AddLog("[YWebApiManager] シャットダウンしました。");
}

void YWebApiManager::DrawLogWindow() {
    // 上部に便利な操作ボタンを配置
    if (ImGui::Button("Clear")) {
        ClearLogs();
    }
    ImGui::SameLine();
    if (ImGui::Button("Test Request (Local)")) {
        // ボタンを押したらテスト通信が走るようにしておくと超便利！
        SendGetRequest("http://localhost/Backend/api/ranking.php");
    }

    ImGui::Separator();

    // ログ表示部分をスクロールできるように子ウィンドウにする
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

    // ログを一行ずつ描画
    for (const auto& log : logs_) {
        // ログの種類（[Error] や [Request]）によって文字の色を変えるとおしゃれ
        if (log.rfind("[Error]", 0) == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), log.c_str()); // 赤色
        }
        else if (log.rfind("[Request]", 0) == 0) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), log.c_str()); // 青色
        }
        else if (log.rfind("[JSON]", 0) == 0) {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), log.c_str()); // 緑色
        }
        else {
            ImGui::TextUnformatted(log.c_str()); // 通常（白、またはテーマ色）
        }
    }

    // 新しいログが追加されたら自動で一番下までスクロールさせるお守り
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

// cURLがWEBサーバーからデータを少しずつ受信するたびに呼び出される関数
size_t YWebApiManager::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;

    // 引数（userp）として渡された std::string（バッファ）に受信データを追加
    std::string* responseString = static_cast<std::string*>(userp);
    responseString->append(static_cast<char*>(contents), totalSize);

    return totalSize;
}

void YWebApiManager::AddLog(const std::string& logs)
{
	logs_.push_back(logs);
}
nlohmann::json YWebApiManager::SendGetRequest(const std::string& url) {
    if (!initialized_) {
        AddLog("[Error] YWebApiManagerが初期化されていません。");
        return nlohmann::json();
    }

    // 通信開始ログ
    AddLog("[Request] GET -> " + url);

    CURL* curl = curl_easy_init();
    if (!curl) {
        AddLog("[Error] cURLハンドルの作成に失敗しました。");
        return nlohmann::json();
    }

    std::string responseBuffer;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        // 通信失敗ログ
        AddLog("[Error] 通信失敗: " + std::string(curl_easy_strerror(res)));
        return nlohmann::json();
    }

    // 通信成功ログ（文字数が多すぎる場合はダンプを制限してもOK）
    AddLog("[Success] レスポンス受信完了。");

    try {
        nlohmann::json jsonResult = nlohmann::json::parse(responseBuffer);
        // パースしたJSONを綺麗に整形（インデント4）してログに出す
        AddLog("[JSON] \n" + jsonResult.dump(4));
        return jsonResult;
    }
    catch (const nlohmann::json::parse_error& e) {
        // パース失敗ログ
        AddLog("[Error] JSONパース失敗: " + std::string(e.what()));
        AddLog("[Raw Response] " + responseBuffer);
        return nlohmann::json();
    }
}
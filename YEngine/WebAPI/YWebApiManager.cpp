#include "YWebApiManager.h"
#include <iostream>
#include <thread>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

YWebApiManager& YWebApiManager::GetInstance() {
    static YWebApiManager instance;
    return instance;
}

bool YWebApiManager::Initialize() {
    if (initialized_)
        return true;

    // アプリケーション全体で一生に一度のcURLグローバル初期化
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        AddLog("[YWebApiManager] cURLのグローバル初期化に失敗しました。");
        return false;
    }
    curl_ = curl_easy_init();
    if (curl_) {
        initialized_ = true;
        AddLog("[YWebApiManager] 初期化に成功しました。");
        curl_easy_cleanup(curl_);
    }
    curl_global_cleanup();
    return true;
}

void YWebApiManager::Finalize() {
    if (!initialized_)
        return;

    // cURLのグローバル解放
    curl_global_cleanup();
    initialized_ = false;
    AddLog("[YWebApiManager] シャットダウンしました。");
}

void YWebApiManager::DrawLogWindow() {
#ifdef USE_IMGUI
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
    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true,
        ImGuiWindowFlags_HorizontalScrollbar);

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
#endif // USE_IMGUI
}

// cURLがWEBサーバーからデータを少しずつ受信するたびに呼び出される関数
size_t YWebApiManager::WriteCallback(void* contents, size_t size, size_t nmemb,
    void* userp) {
    size_t totalSize = size * nmemb;

    // 引数（userp）として渡された std::string（バッファ）に受信データを追加
    std::string* responseString = static_cast<std::string*>(userp);
    responseString->append(static_cast<char*>(contents), totalSize);

    return totalSize;
}

void YWebApiManager::AddLog(const std::string& logs) { logs_.push_back(logs); }
nlohmann::json
YWebApiManager::SendGetRequest(const std::string& url,
    const std::vector<std::string>& headers) {
    return PerformRequest("GET", url, nullptr, headers);
}

nlohmann::json
YWebApiManager::SendPostRequest(const std::string& url,
    const nlohmann::json& body,
    const std::vector<std::string>& headers) {
    return PerformRequest("POST", url, &body, headers);
}

void YWebApiManager::SendGetRequestAsync(
    const std::string& url, const std::vector<std::string>& headers,
    std::function<void(nlohmann::json)> callback) {
    std::thread([this, url, headers, callback]() {
        nlohmann::json result = PerformRequest("GET", url, nullptr, headers);
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingResults_.push_back([callback, result]() {
            if (callback)
                callback(result);
            });
        }).detach();
}

void YWebApiManager::SendPostRequestAsync(
    const std::string& url, const nlohmann::json& body,
    const std::vector<std::string>& headers,
    std::function<void(nlohmann::json)> callback) {
    std::thread([this, url, body, headers, callback]() {
        nlohmann::json result = PerformRequest("POST", url, &body, headers);
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingResults_.push_back([callback, result]() {
            if (callback)
                callback(result);
            });
        }).detach();
}

void YWebApiManager::SendPostRequestWithStatusAsync(
    const std::string& url, const nlohmann::json& body,
    const std::vector<std::string>& headers,
    std::function<void(nlohmann::json body, long httpStatus)> callback) {
    std::thread([this, url, body, headers, callback]() {
        long httpStatus = 0;
        nlohmann::json result =
            PerformRequest("POST", url, &body, headers, &httpStatus);
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingResults_.push_back([callback, result, httpStatus]() {
            if (callback)
                callback(result, httpStatus);
            });
        }).detach();
}

void YWebApiManager::SendPatchRequestWithStatusAsync(
    const std::string& url, const nlohmann::json& body,
    const std::vector<std::string>& headers,
    std::function<void(nlohmann::json body, long httpStatus)> callback) {
    std::thread([this, url, body, headers, callback]() {
        long httpStatus = 0;
        nlohmann::json result =
            PerformRequest("PATCH", url, &body, headers, &httpStatus);
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingResults_.push_back([callback, result, httpStatus]() {
            if (callback)
                callback(result, httpStatus);
            });
        }).detach();
}

void YWebApiManager::Update() {
    std::vector<std::function<void()>> finished;
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        if (pendingResults_.empty()) {
            return;
        }
        finished.swap(pendingResults_);
    }
    // コールバックはロックの外（メインスレッド）で実行する
    for (auto& invoke : finished) {
        if (invoke) {
            invoke();
        }
    }
}

nlohmann::json YWebApiManager::PerformRequest(
    const std::string& method, const std::string& url,
    const nlohmann::json* body, const std::vector<std::string>& headers,
    long* outHttpStatus) {
    if (!initialized_) {
        // DevelopScene等で明示初期化されていない場合に備えた遅延初期化
        Initialize();
    }

    // 通信開始ログ
    AddLog("[Request] " + method + " -> " + url);

    CURL* curl = curl_easy_init();
    if (!curl) {
        AddLog("[Error] cURLハンドルの作成に失敗しました。");
        return nlohmann::json();
    }

    std::string responseBuffer;
    std::string bodyString;
    curl_slist* headerList = nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body) {
            bodyString = body->dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyString.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                static_cast<long>(bodyString.size()));
        }
    }
    else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (body) {
            bodyString = body->dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyString.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                static_cast<long>(bodyString.size()));
        }
    }

    for (const auto& header : headers) {
        headerList = curl_slist_append(headerList, header.c_str());
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK && outHttpStatus) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        *outHttpStatus = httpCode;
    }

    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        // 通信失敗ログ
        AddLog("[Error] 通信失敗: " + std::string(curl_easy_strerror(res)));
        return nlohmann::json();
    }

    // 通信成功ログ
    AddLog("[Success] レスポンス受信完了。");

    if (responseBuffer.empty()) {
        // Prefer: return=minimal のPOST等、正常でも中身が空のことがある
        return nlohmann::json::array();
    }

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
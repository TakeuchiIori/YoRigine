#pragma once

#include<curl/curl.h>
#include <functional>
#include <json.hpp>
#include <mutex>
#include <string>
#include <vector>

//=========================================================================
// C++とWrbAPIをつなぐクラス
//=========================================================================
class YWebApiManager {
public:
  //=========================================================================
  // シングルトンインスタンスの取得など
  //=========================================================================
  static YWebApiManager &GetInstance();
  YWebApiManager(const YWebApiManager &) = delete;
  YWebApiManager &operator=(const YWebApiManager &) = delete;

  //=========================================================================
  // 基本的な処理
  //=========================================================================
  bool Initialize();
  void Finalize();
  void DrawLogWindow();

  // 非同期リクエストの完了コールバックをメインスレッドで処理する。
  // 毎フレーム（Framework::Update等）から呼び出すこと。
  void Update();

  //=========================================================================
  // 公開関数
  //=========================================================================
  // WebAPIにGETリクエストを送り、パース済みのJSONを返す関数（同期・ブロッキング）
  nlohmann::json SendGetRequest(const std::string &url,
                                const std::vector<std::string> &headers = {});
  // WebAPIにPOSTリクエストを送り、パース済みのJSONを返す関数（同期・ブロッキング）
  nlohmann::json SendPostRequest(const std::string &url,
                                 const nlohmann::json &body,
                                 const std::vector<std::string> &headers = {});

  // 非同期版（別スレッドで通信し、完了後は次の Update()
  // 呼び出し時にメインスレッドで callback を実行する）
  void SendGetRequestAsync(const std::string &url,
                           const std::vector<std::string> &headers,
                           std::function<void(nlohmann::json)> callback);
  void SendPostRequestAsync(const std::string &url, const nlohmann::json &body,
                            const std::vector<std::string> &headers,
                            std::function<void(nlohmann::json)> callback);

  void ClearLogs() { logs_.clear(); }

  //=========================================================================
  // アクセッサ
  //=========================================================================
  const std::vector<std::string> &GetLogs() const { return logs_; }

private:
  //=========================================================================
  // 内部関数
  //=========================================================================
  // 外部からの new や実体化を禁止
  YWebApiManager() = default;
  ~YWebApiManager() = default;

  // GET/POST共通の送受信本体
  nlohmann::json PerformRequest(const std::string &method,
                                const std::string &url,
                                const nlohmann::json *body,
                                const std::vector<std::string> &headers);

  // cURLがデータを受信したときに呼ばれる静的コールバック関数
  static size_t WriteCallback(void *contents, size_t size, size_t nmemb,
                              void *userp);
  // ログを追加
  void AddLog(const std::string &logs);

private:
  //=========================================================================
  // メンバ変数
  //=========================================================================

  // 初期化されたか
  bool initialized_ = false;

  // ログの保存
  std::vector<std::string> logs_;

  // cURLのハンドル
  CURL *curl_ = nullptr;

  // 非同期リクエストの結果待ちキュー（別スレッド→メインスレッドの受け渡し用）
  struct PendingResult {
    std::function<void(nlohmann::json)> callback;
    nlohmann::json result;
  };
  std::mutex pendingMutex_;
  std::vector<PendingResult> pendingResults_;
};

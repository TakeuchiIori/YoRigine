#pragma once

#include <string>
#include <vector>

//=========================================================================
// YWebApiManagerを使った疎通確認用の簡易ミニゲーム。
// 5秒間のクリックチャレンジでスコアを稼ぎ、swgame(/scores)へ送信、
// ランキング(スコア降順)を取得して表示する。
//=========================================================================
class YScoreGameUI {
public:
    static YScoreGameUI& GetInstance();
    YScoreGameUI(const YScoreGameUI&) = delete;
    YScoreGameUI& operator=(const YScoreGameUI&) = delete;

    // 毎フレーム呼び出し（Playing中のカウントダウン処理）
    void Update(float deltaTime);
    // ImGuiパネルの描画。DevelopScene等からRegisterGameUIで登録する
    void DrawImGui();

private:
    YScoreGameUI() = default;
    ~YScoreGameUI() = default;

    enum class State {
        Idle,
        Playing,
        Result,
    };

    // ランキング1件分
    struct RankingEntry {
        int score = 0;
        std::string createdAt;
    };

    void StartGame();
    void SubmitScore();
    void RefreshRanking();

private:
    State state_ = State::Idle;

    // チャレンジ設定
    static constexpr float kChallengeSeconds = 5.0f;
    float timeRemaining_ = 0.0f;
    int clickCount_ = 0;
    int lastScore_ = 0;

    // 送信状態
    bool isSubmitting_ = false;
    bool submitSucceeded_ = false;
    std::string submitMessage_;

    // ランキング
    bool isLoadingRanking_ = false;
    std::vector<RankingEntry> ranking_;
};

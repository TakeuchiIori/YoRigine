#include "YScoreGameUI.h"
#include "YWebApiManager.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
// swgame(Prisma+Express)側のエンドポイント。be1apiと同じ3000番ポートを使うため、
// 同時には起動できない（どちらか一方だけをローカルで動かしておくこと）。
constexpr const char* kScoresUrl = "http://localhost:3000/scores";
constexpr size_t kRankingDisplayCount = 10;
} // namespace

YScoreGameUI& YScoreGameUI::GetInstance() {
    static YScoreGameUI instance;
    return instance;
}

void YScoreGameUI::Update(float deltaTime) {
    if (state_ != State::Playing) {
        return;
    }

    timeRemaining_ -= deltaTime;
    if (timeRemaining_ <= 0.0f) {
        timeRemaining_ = 0.0f;
        lastScore_ = clickCount_;
        state_ = State::Result;
    }
}

void YScoreGameUI::StartGame() {
    state_ = State::Playing;
    clickCount_ = 0;
    timeRemaining_ = kChallengeSeconds;
    submitSucceeded_ = false;
    submitMessage_.clear();
}

void YScoreGameUI::SubmitScore() {
    if (isSubmitting_) {
        return;
    }
    isSubmitting_ = true;
    submitMessage_.clear();

    nlohmann::json body = { {"score", lastScore_} };
    YWebApiManager::GetInstance().SendPostRequestWithStatusAsync(
        kScoresUrl, body, { "Content-Type: application/json" },
        [this](nlohmann::json /*result*/, long status) {
            isSubmitting_ = false;
            submitSucceeded_ = (status >= 200 && status < 300);
            if (submitSucceeded_) {
                submitMessage_ = "送信成功！";
                RefreshRanking();
            } else {
                submitMessage_ = "送信失敗 (status " + std::to_string(status) + ")";
            }
        });
}

void YScoreGameUI::RefreshRanking() {
    if (isLoadingRanking_) {
        return;
    }
    isLoadingRanking_ = true;

    YWebApiManager::GetInstance().SendGetRequestAsync(
        kScoresUrl, {},
        [this](nlohmann::json result) {
            isLoadingRanking_ = false;

            std::vector<RankingEntry> entries;
            if (result.is_array()) {
                for (const auto& item : result) {
                    RankingEntry entry;
                    entry.score = item.value("score", 0);
                    entry.createdAt = item.value("createdAt", "");
                    entries.push_back(entry);
                }
            }

            std::sort(entries.begin(), entries.end(),
                [](const RankingEntry& a, const RankingEntry& b) {
                    return a.score > b.score;
                });
            if (entries.size() > kRankingDisplayCount) {
                entries.resize(kRankingDisplayCount);
            }
            ranking_ = std::move(entries);
        });
}

void YScoreGameUI::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Text("クリックチャレンジ (制限時間 %.0f秒)", kChallengeSeconds);
    ImGui::Separator();

    switch (state_) {
    case State::Idle: {
        if (ImGui::Button("Play", ImVec2(120, 40))) {
            StartGame();
        }
        break;
    }
    case State::Playing: {
        ImGui::Text("残り時間: %.1f秒", timeRemaining_);
        ImGui::Text("クリック数: %d", clickCount_);
        if (ImGui::Button("Click!", ImVec2(160, 80))) {
            clickCount_++;
        }
        break;
    }
    case State::Result: {
        ImGui::Text("スコア: %d", lastScore_);
        ImGui::BeginDisabled(isSubmitting_);
        if (ImGui::Button("Submit")) {
            SubmitScore();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Play Again")) {
            state_ = State::Idle;
        }
        if (!submitMessage_.empty()) {
            ImGui::TextColored(
                submitSucceeded_ ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                "%s", submitMessage_.c_str());
        }
        break;
    }
    }

    ImGui::Separator();
    ImGui::Text("ランキング (上位%zu件)", kRankingDisplayCount);
    ImGui::SameLine();
    ImGui::BeginDisabled(isLoadingRanking_);
    if (ImGui::Button("Refresh")) {
        RefreshRanking();
    }
    ImGui::EndDisabled();

    if (ImGui::BeginTable("RankingTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("順位");
        ImGui::TableSetupColumn("スコア");
        ImGui::TableSetupColumn("日時");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < ranking_.size(); ++i) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%zu", i + 1);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", ranking_[i].score);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(ranking_[i].createdAt.c_str());
        }
        ImGui::EndTable();
    }
#endif // USE_IMGUI
}

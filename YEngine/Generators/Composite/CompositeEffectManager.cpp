#include "CompositeEffectManager.h"

#include "Debugger/Logger.h"

// エディタのドロップダウン用（各系統の名前一覧）
#include "Particle/YParticleManager.h"
#include "Particle/YEmitterGroupManager.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshSpawner.h"
#include "GPUParticle/GpuEmitManager.h"

#include <json.hpp>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>

#ifdef USE_IMGUI
#include <imgui.h>
#include <IconsFontAwesome5.h>
#endif

// ============================================================================
// CompositeInstance（ループ実行インスタンス）
// ============================================================================

void CompositeInstance::SetPosition(const Vector3& pos)
{
    basePos = pos;
    if (particle.IsValid()) particle.SetPosition(pos);
    for (size_t i = 0; i < vfx.size(); ++i) {
        vfx[i].SetPosition(pos + vfxOffsets[i]);
    }
    if (gpu.IsValid()) gpu.SetPosition(pos);
    // 音は Audio に3D位置がないため追従しない（スコープ外）
}

void CompositeInstance::Stop()
{
    if (particle.IsValid()) particle.Stop();
    for (auto& v : vfx) v.Stop();
    if (gpu.IsValid()) gpu.Stop();
    for (auto& s : sounds) s.Stop();
}

bool CompositeInstance::IsActive() const
{
    if (particle.IsValid() && particle.IsActive()) return true;
    for (const auto& v : vfx) if (v.IsAlive()) return true;
    if (gpu.IsValid() && gpu.IsActive()) return true;
    for (const auto& s : sounds) if (s.IsPlaying()) return true;
    return false;
}

// ============================================================================
// CompositeEffectManager
// ============================================================================

CompositeEffectManager* CompositeEffectManager::GetInstance()
{
    static CompositeEffectManager instance;
    return &instance;
}

namespace {
    YoRigine::SoundCategory ParseCategory(const std::string& s)
    {
        if (s == "BGM")     return YoRigine::SoundCategory::BGM;
        if (s == "VOICE")   return YoRigine::SoundCategory::VOICE;
        if (s == "AMBIENT") return YoRigine::SoundCategory::AMBIENT;
        return YoRigine::SoundCategory::SE;
    }
}

void CompositeEffectManager::ScanDirectory(const std::string& dir)
{
    namespace fs = std::filesystem;
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return; // ディレクトリが無ければ何もしない（未使用でも安全）
    }
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == ".json") {
            LoadAsset(entry.path().string());
        }
    }
}

bool CompositeEffectManager::LoadAsset(const std::string& filepath)
{
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;

        nlohmann::json j;
        file >> j;

        CompositeEffectAsset a;
        a.name = j.value("name", std::filesystem::path(filepath).stem().string());
        a.particleEffect = j.value("particleEffect", std::string());
        a.gpuEmitterGroup = j.value("gpuEmitterGroup", std::string());

        if (j.contains("vfxMeshAssets")) {
            for (const auto& v : j["vfxMeshAssets"]) {
                CompositeVfxRef r;
                r.asset = v.value("asset", std::string());
                if (v.contains("offset") && v["offset"].is_array() && v["offset"].size() >= 3) {
                    r.offset = { v["offset"][0].get<float>(), v["offset"][1].get<float>(), v["offset"][2].get<float>() };
                }
                r.scale = v.value("scale", 1.0f);
                if (!r.asset.empty()) a.vfxMeshAssets.push_back(r);
            }
        }

        if (j.contains("sounds")) {
            for (const auto& s : j["sounds"]) {
                CompositeSoundRef r;
                r.path = s.value("path", std::string());
                r.volume = s.value("volume", 1.0f);
                r.category = ParseCategory(s.value("category", std::string("SE")));
                if (!r.path.empty()) a.sounds.push_back(r);
            }
        }

        assets_[a.name] = std::move(a);
        Logger("CompositeEffectManager: ロード -> " + filepath);
        return true;
    }
    catch (const std::exception& e) {
        Logger(std::string("CompositeEffectManager: ロード失敗 ") + filepath + " : " + e.what());
        return false;
    }
}

bool CompositeEffectManager::Has(const std::string& name) const
{
    return assets_.count(name) > 0;
}

bool CompositeEffectManager::SaveAsset(const std::string& name)
{
    auto it = assets_.find(name);
    if (it == assets_.end()) {
        Logger("CompositeEffectManager::SaveAsset : '" + name + "' が見つかりません");
        return false;
    }
    const CompositeEffectAsset& a = it->second;

    try {
        nlohmann::json j;
        j["name"] = a.name;
        j["particleEffect"] = a.particleEffect;
        j["gpuEmitterGroup"] = a.gpuEmitterGroup;

        j["vfxMeshAssets"] = nlohmann::json::array();
        for (const auto& v : a.vfxMeshAssets) {
            j["vfxMeshAssets"].push_back({
                {"asset", v.asset},
                {"offset", { v.offset.x, v.offset.y, v.offset.z }},
                {"scale", v.scale},
                });
        }

        j["sounds"] = nlohmann::json::array();
        for (const auto& s : a.sounds) {
            j["sounds"].push_back({
                {"path", s.path},
                {"volume", s.volume},
                {"category", YoRigine::GetCategoryName(s.category)},
                });
        }

        const std::string filepath = "Resources/Json/YComposites/" + name + ".json";
        std::filesystem::path path(filepath);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream file(filepath);
        if (!file.is_open()) {
            Logger("CompositeEffectManager::SaveAsset : ファイルを開けません " + filepath);
            return false;
        }
        file << j.dump(4);
        Logger("CompositeEffectManager: 保存 -> " + filepath);
        return true;
    }
    catch (const std::exception& e) {
        Logger(std::string("CompositeEffectManager::SaveAsset 失敗: ") + e.what());
        return false;
    }
}

void CompositeEffectManager::PlayOneShot(const std::string& name, const Vector3& pos)
{
    auto it = assets_.find(name);
    if (it == assets_.end()) {
        Logger("CompositeEffectManager::PlayOneShot : '" + name + "' が見つかりません");
        return;
    }
    const CompositeEffectAsset& a = it->second;

    // 各系統を「撃ちっぱなし」で発火（保持不要）
    if (!a.particleEffect.empty()) {
        EffectHandle::PlayOneShot(a.particleEffect, pos);
    }
    for (const auto& v : a.vfxMeshAssets) {
        VfxMeshHandle::PlayOneShot(v.asset, pos + v.offset, v.scale);
    }
    if (!a.gpuEmitterGroup.empty()) {
        GpuParticleHandle::PlayOneShot(a.gpuEmitterGroup, pos);
    }
    for (const auto& s : a.sounds) {
        YoRigine::Audio::GetInstance()->PlayOneShot(s.path, s.volume, s.category);
    }
}

EffectHandle CompositeEffectManager::Play(const std::string& name, const Vector3& pos)
{
    EffectHandle handle;

    auto it = assets_.find(name);
    if (it == assets_.end()) {
        Logger("CompositeEffectManager::Play : '" + name + "' が見つかりません");
        return handle;
    }
    const CompositeEffectAsset& a = it->second;

    auto inst = std::make_shared<CompositeInstance>();
    inst->basePos = pos;

    // Particle 子（ループ）
    if (!a.particleEffect.empty()) {
        inst->particle = EffectHandle::Play(a.particleEffect, pos, /*loop*/true);
    }
    // VfxMesh 子（ループ）
    for (const auto& v : a.vfxMeshAssets) {
        inst->vfx.push_back(VfxMeshHandle::Play(v.asset, pos + v.offset, v.scale, /*loop*/true));
        inst->vfxOffsets.push_back(v.offset);
    }
    // GPU 子（ループ）
    if (!a.gpuEmitterGroup.empty()) {
        inst->gpu = GpuParticleHandle::Play(a.gpuEmitterGroup, pos);
    }
    // ループ音（保持して Stop 連鎖）
    for (const auto& s : a.sounds) {
        inst->sounds.push_back(YoRigine::Audio::GetInstance()->Play(s.path, /*loop*/true, s.volume, s.category));
    }

    handle.systemName_ = name;
    handle.composite_ = inst;
    return handle;
}

// ============================================================================
// エディタ（USE_IMGUI）
// ============================================================================
#ifdef USE_IMGUI

void CompositeEffectManager::ScanSounds()
{
    availableSounds_.clear();
    namespace fs = std::filesystem;
    const std::string dir = "Resources/Audio/";
    std::error_code ec;
    if (fs::exists(dir, ec) && fs::is_directory(dir, ec)) {
        for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
            if (ec) break;
            if (!e.is_regular_file()) continue;
            std::string ext = e.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".wav" || ext == ".mp3" || ext == ".mp4") {
                availableSounds_.push_back(e.path().generic_string());
            }
        }
    }
    std::sort(availableSounds_.begin(), availableSounds_.end());
    soundsScanned_ = true;
}

void CompositeEffectManager::DrawImGui()
{
    if (!soundsScanned_) ScanSounds();

    // (なし)対応の文字列コンボ。選択で value を更新し、変更有無を返す
    auto stringCombo = [](const char* label, std::string& value,
                          const std::vector<std::string>& opts, bool allowNone) -> bool {
        bool changed = false;
        const std::string preview = value.empty() ? "(なし)" : value;
        if (ImGui::BeginCombo(label, preview.c_str())) {
            if (allowNone && ImGui::Selectable("(なし)", value.empty())) { value.clear(); changed = true; }
            for (const auto& o : opts) {
                if (ImGui::Selectable(o.c_str(), o == value)) { value = o; changed = true; }
            }
            ImGui::EndCombo();
        }
        return changed;
    };

    // ── 新規作成 ──
    ImGui::SeparatorText("複合エフェクト（Composite）");
    ImGui::TextDisabled("各系統の名前をドロップダウンで選ぶだけ。手打ち不要でタイプミスなし。");
    ImGui::InputTextWithHint("##newcomp", "新しい複合エフェクト名...", newCompositeName_, sizeof(newCompositeName_));
    ImGui::SameLine();
    ImGui::BeginDisabled(strlen(newCompositeName_) == 0);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.6f, 0.3f, 1.0f));
    if (ImGui::Button(ICON_FA_PLUS " 作成")) {
        std::string nm = newCompositeName_;
        if (!assets_.count(nm)) {
            CompositeEffectAsset a; a.name = nm;
            assets_[nm] = a;
            selectedComposite_ = nm;
        }
        newCompositeName_[0] = '\0';
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();

    // ── 編集対象の選択 ──
    {
        std::vector<std::string> names;
        for (const auto& [k, v] : assets_) names.push_back(k);
        std::sort(names.begin(), names.end());
        const std::string preview = selectedComposite_.empty() ? "(選択してください)" : selectedComposite_;
        if (ImGui::BeginCombo("編集対象", preview.c_str())) {
            for (const auto& n : names) {
                if (ImGui::Selectable(n.c_str(), n == selectedComposite_)) selectedComposite_ = n;
            }
            ImGui::EndCombo();
        }
    }

    auto it = assets_.find(selectedComposite_);
    if (it == assets_.end()) {
        ImGui::TextDisabled("上で複合エフェクトを作成／選択してください。");
        return;
    }
    CompositeEffectAsset& a = it->second;

    ImGui::Separator();

    // ── Particle ──
    {
        std::vector<std::string> opts = YParticleManager::GetInstance().GetAllSystemNames();
        auto groups = YEmitterGroupManager::GetInstance().GetAllGroupNames();
        opts.insert(opts.end(), groups.begin(), groups.end());
        stringCombo("Particle エフェクト", a.particleEffect, opts, true);
    }
    // ── GPU ──
    {
        auto opts = YoRigine::GpuEmitManager::GetInstance()->GetGroupNames();
        stringCombo("GPU グループ", a.gpuEmitterGroup, opts, true);
    }

    // ── VfxMesh（複数）──
    ImGui::SeparatorText("VfxMesh（複数可）");
    {
        auto opts = VfxMeshSpawner::GetInstance()->GetAssetNames();
        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(a.vfxMeshAssets.size()); ++i) {
            ImGui::PushID(i);
            auto& v = a.vfxMeshAssets[i];
            stringCombo("アセット", v.asset, opts, false);
            ImGui::DragFloat3("オフセット", &v.offset.x, 0.05f);
            ImGui::DragFloat("スケール", &v.scale, 0.01f, 0.01f, 100.0f);
            if (ImGui::SmallButton("この行を削除")) removeIdx = i;
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIdx >= 0) a.vfxMeshAssets.erase(a.vfxMeshAssets.begin() + removeIdx);
        if (ImGui::Button(ICON_FA_PLUS " VfxMesh を追加")) a.vfxMeshAssets.push_back({});
    }

    // ── サウンド（複数）──
    ImGui::SeparatorText("サウンド（複数可）");
    {
        static const char* catNames[] = { "SE", "BGM", "VOICE", "AMBIENT" };
        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(a.sounds.size()); ++i) {
            ImGui::PushID(1000 + i);
            auto& s = a.sounds[i];
            stringCombo("ファイル", s.path, availableSounds_, false);
            ImGui::DragFloat("音量", &s.volume, 0.01f, 0.0f, 1.0f);
            int ci = 0;
            switch (s.category) {
            case YoRigine::SoundCategory::BGM:     ci = 1; break;
            case YoRigine::SoundCategory::VOICE:   ci = 2; break;
            case YoRigine::SoundCategory::AMBIENT: ci = 3; break;
            default:                               ci = 0; break;
            }
            if (ImGui::Combo("カテゴリ", &ci, catNames, 4)) {
                s.category = (ci == 1) ? YoRigine::SoundCategory::BGM
                    : (ci == 2) ? YoRigine::SoundCategory::VOICE
                    : (ci == 3) ? YoRigine::SoundCategory::AMBIENT
                    : YoRigine::SoundCategory::SE;
            }
            if (ImGui::SmallButton("この行を削除")) removeIdx = i;
            ImGui::Separator();
            ImGui::PopID();
        }
        if (removeIdx >= 0) a.sounds.erase(a.sounds.begin() + removeIdx);
        if (ImGui::Button(ICON_FA_PLUS " サウンドを追加")) a.sounds.push_back({});
        ImGui::SameLine();
        if (ImGui::SmallButton("音声一覧を再スキャン")) ScanSounds();
    }

    // ── プレビュー / 保存 ──
    ImGui::SeparatorText("プレビュー / 保存");
    ImGui::DragFloat3("再生位置", &previewPos_.x, 0.1f);
    if (ImGui::Button(ICON_FA_PLAY " ワンショット再生")) {
        PlayOneShot(a.name, previewPos_);
    }
    ImGui::SameLine();
    if (ImGui::Button("ループ再生")) {
        previewLoopHandle_ = Play(a.name, previewPos_);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP " 停止")) {
        if (previewLoopHandle_.IsValid()) previewLoopHandle_.Stop();
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.6f, 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.35f, 1.0f));
    if (ImGui::Button(ICON_FA_SAVE " この複合エフェクトを保存", ImVec2(-1, 32))) {
        SaveAsset(a.name);
    }
    ImGui::PopStyleColor(2);
    ImGui::TextDisabled("保存先: Resources/Json/YComposites/%s.json", a.name.c_str());
    ImGui::TextDisabled("ゲーム側: EffectHandle::PlayOneShot(\"%s\", pos)", a.name.c_str());
}
#endif

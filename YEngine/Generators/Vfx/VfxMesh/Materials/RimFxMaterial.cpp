// ===========================================================
// RimFxMaterial.cpp
// ===========================================================
#include "RimFxMaterial.h"
#include <Loaders/Texture/TextureManager.h>
#include <SrvManager.h>
#ifdef USE_IMGUI
#include <imgui.h>
#include "Core/Editor/Widgets/YEditorWidget.h"
#include "FileOperations/FileBrowser.h"
#endif

namespace YoRigine {

static constexpr size_t kCBAlign = 256;
template<typename T>
static constexpr size_t CBSize256() { return (sizeof(T) + kCBAlign - 1) & ~(kCBAlign - 1); }

// テクスチャ未設定時の白フォールバック（GPU Validation エラー回避）
static const std::string kRimFxWhite = "Resources/Textures/white.png";

RimFxMaterial::RimFxMaterial(const RimFxMatParams& param) : param_(param) { EnsureTexture(); }

// ===========================================================
// テクスチャ確保（パスが変わったときだけ TextureManager に読ませる）
// ===========================================================
void RimFxMaterial::EnsureTexture()
{
    auto* tex = TextureManager::GetInstance();
    tex->LoadTexture(kRimFxWhite);
    if (!param_.texturePath.empty() && param_.texturePath != loadedTexPath_) {
        tex->LoadTexture(param_.texturePath);
    }
    loadedTexPath_ = param_.texturePath;
}

// ===========================================================
// t0: gTexNoise（テクスチャ or 白）をバインド
// ===========================================================
void RimFxMaterial::BindResources(ID3D12GraphicsCommandList* cmdList,
                                  const std::unordered_map<std::string, UINT>& idx) const
{
    (void)cmdList; // SrvManager が内部のコマンドリストへバインドする
    auto it = idx.find("gTexNoise");
    if (it == idx.end()) return; // このPSOにテクスチャ入力が無いなら何もしない

    // YParticle / Sprite と同じく SrvManager 経由で SRV インデックスをバインドする。
    auto* srv = SrvManager::GetInstance();
    srv->PreDraw(); // SRV ヒープをコマンドリストへセット（SetDescriptorHeaps）

    auto* tex = TextureManager::GetInstance();
    const std::string& path = param_.texturePath.empty() ? kRimFxWhite : param_.texturePath;
    uint32_t srvIndex = tex->GetTextureIndexByFilePath(path);
    srv->SetGraphicsRootDescriptorTable(it->second, srvIndex);
}

#ifdef USE_IMGUI
// スタイルごとの推奨プリセット（色・速度・密度）。ボタン1つで現在の style に流し込む。
// style は据え置き、見た目パラメータだけ上書きする。
static void ApplyRimFxStylePreset(RimFxMatParams& p)
{
    switch (p.style) {
    default:
    case 0: // 炎
        p.color = { 2.5f, 0.8f, 0.15f, 1.f }; p.tipColor = { 2.2f, 0.2f, 0.04f, 1.f };
        p.riseSpeed = 1.2f; p.noiseScale = 3.0f; p.turbulence = 1.0f; break;
    case 1: // 霊気
        p.color = { 0.30f, 1.20f, 1.60f, 1.f }; p.tipColor = { 0.80f, 0.40f, 1.60f, 1.f };
        p.riseSpeed = 0.6f; p.noiseScale = 2.5f; p.turbulence = 0.8f; break;
    case 2: // 電撃
        p.color = { 0.60f, 1.40f, 2.60f, 1.f }; p.tipColor = { 2.40f, 2.60f, 3.00f, 1.f };
        p.riseSpeed = 1.6f; p.noiseScale = 6.0f; p.turbulence = 1.2f; break;
    case 3: // 花びら
        p.color = { 2.40f, 0.70f, 1.30f, 1.f }; p.tipColor = { 2.60f, 1.80f, 2.00f, 1.f };
        p.riseSpeed = 0.8f; p.noiseScale = 3.0f; p.turbulence = 0.5f; break;
    case 4: // オーラ柱
        p.color = { 1.60f, 1.20f, 2.60f, 1.f }; p.tipColor = { 2.20f, 2.00f, 2.80f, 1.f };
        p.riseSpeed = 1.0f; p.noiseScale = 2.0f; p.turbulence = 0.4f; break;
    case 5: // 毒
        p.color = { 0.60f, 2.00f, 0.30f, 1.f }; p.tipColor = { 1.40f, 2.40f, 0.40f, 1.f };
        p.riseSpeed = 0.7f; p.noiseScale = 3.5f; p.turbulence = 1.3f; break;
    case 6: // 渦
        p.color = { 0.50f, 1.60f, 2.60f, 1.f }; p.tipColor = { 2.40f, 2.40f, 2.80f, 1.f };
        p.riseSpeed = 1.4f; p.noiseScale = 4.0f; p.turbulence = 1.0f; break;
    case 7: // 火の粉
        p.color = { 2.80f, 1.60f, 0.40f, 1.f }; p.tipColor = { 2.60f, 0.90f, 0.20f, 1.f };
        p.riseSpeed = 1.0f; p.noiseScale = 4.0f; p.turbulence = 0.8f; break;
    }
}
#endif

size_t RimFxMaterial::GetCBByteSize() const { return CBSize256<RimFxParamsCB>(); }

// ===========================================================
// 定数バッファ書き込み
// ===========================================================

void RimFxMaterial::FillCB(void* mapped, const VfxMatFillArgs& args) const
{
    auto& cb = *static_cast<RimFxParamsCB*>(mapped);

    // tint（モジュール評価＋インスタンス色）を元の色に乗算。a に強度/不透明度が乗る。
    const auto& t = args.tint;
    cb.color = {
        param_.color.x * t.x,
        param_.color.y * t.y,
        param_.color.z * t.z,
        param_.color.w * t.w
    };
    cb.tipColor = {
        param_.tipColor.x * t.x,
        param_.tipColor.y * t.y,
        param_.tipColor.z * t.z,
        param_.color.w * t.w
    };
    cb.style      = static_cast<float>(param_.style);
    cb.riseSpeed  = param_.riseSpeed;
    cb.noiseScale = param_.noiseScale;
    cb.turbulence = param_.turbulence;
    cb.time       = args.age;
    cb.texStrength = param_.texStrength;
    cb.texTiling   = param_.texTiling;
    cb.texScroll   = param_.texScroll;
}

// ===========================================================
// MaterialRegistry 登録情報
// ===========================================================

VfxMaterialDesc RimFxMaterial::Describe()
{
    VfxMaterialDesc d;
    d.type          = VfxMaterialType::RimFx;
    d.displayName   = "RimFx";
    d.defaultParams = RimFxMatParams{};

    d.create = [](const VfxMaterialParams& p) -> std::unique_ptr<VfxMaterial> {
        return std::make_unique<RimFxMaterial>(std::get<RimFxMatParams>(p));
    };

#ifdef USE_IMGUI
    d.drawUI = [](VfxMaterialParams& params) -> bool {
        auto& p = std::get<RimFxMatParams>(params);
        bool c = false;
        static const char* kStyleNames[] = {
            "0 Flame (炎)", "1 Wisp (霊気)", "2 Electric (電撃)", "3 Petal (花びら)",
            "4 Aura (オーラ柱)", "5 Poison (毒)", "6 Vortex (渦)", "7 Sparkle (火の粉)"
        };
        int s = p.style;
        if (s < 0) s = 0; if (s > 7) s = 7;
        ImGui::SetNextItemWidth(200);
        if (ImGui::Combo("表現##rimfx", &s, kStyleNames, IM_ARRAYSIZE(kStyleNames))) {
            p.style = s; c = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("推奨プリセット##rimfx")) {
            ApplyRimFxStylePreset(p); c = true;
        }
        c |= YEditorWidget::ColorHDR("根本の色(HDR)##rimfx", p.color);
        c |= YEditorWidget::ColorHDR("先端の色(HDR)##rimfx", p.tipColor);
        c |= YEditorWidget::SliderFloat("上昇速度##rimfx",   p.riseSpeed,  0.f, 4.f, "%.2f");
        c |= YEditorWidget::SliderFloat("揺らぎ密度##rimfx", p.noiseScale, 0.5f, 12.f, "%.2f");
        c |= YEditorWidget::SliderFloat("揺らぎ強さ##rimfx", p.turbulence, 0.f, 2.f, "%.2f");
        ImGui::TextDisabled("  8種の縦演出。電撃/渦の本数と各揺らぎは「揺らぎ密度」で調整");

        // ── テクスチャ（任意）: YParticle と同じ FileBrowser で選択 ──────
        ImGui::Separator();
        ImGui::Text("テクスチャ: %s", p.texturePath.empty() ? "(未設定)" : p.texturePath.c_str());

        // FileBrowser はフレーム跨ぎの状態を持つので function-local static で保持。
        // （マテリアル編集は一度に1つなので単一インスタンスで問題ない）
        static YoRigine::FileBrowser s_texBrowser(
            "Resources/Effects/", { ".png", ".jpg", ".dds" },
            YoRigine::FileBrowser::DisplayMode::Grid);
        static bool        s_browserInit = false;
        static bool        s_showBrowser = false;
        static std::string s_picked;
        if (!s_browserInit) {
            s_texBrowser.SetThumbnailProvider([](const std::string& path) -> ImTextureID {
                TextureManager::GetInstance()->LoadTexture(path);
                auto h = TextureManager::GetInstance()->GetsrvHandleGPU(path);
                return h.ptr != 0 ? static_cast<ImTextureID>(h.ptr) : 0;
            });
            s_texBrowser.SetOnFileSelected([](const std::string& path) {
                s_picked = path;
                s_showBrowser = false;
            });
            s_browserInit = true;
        }

        if (ImGui::Button("テクスチャを開く##rimfx")) {
            s_texBrowser.Scan();
            s_showBrowser = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("クリア##rimfxtex")) { p.texturePath.clear(); c = true; }

        if (s_showBrowser) ImGui::OpenPopup("##RimFxTexBrowser");
        ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("##RimFxTexBrowser", &s_showBrowser,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))
        {
            s_texBrowser.Draw("##RimFxTexChild", ImVec2(0, 340));
            ImGui::Separator();
            if (ImGui::Button("キャンセル", ImVec2(-1, 0))) {
                s_showBrowser = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 選択されたら反映（コールバックは Draw 内で発火する）
        if (!s_picked.empty()) { p.texturePath = s_picked; s_picked.clear(); c = true; }

        c |= YEditorWidget::SliderFloat("テクスチャ寄与##rimfx", p.texStrength, 0.f, 1.f, "%.2f");
        c |= YEditorWidget::SliderFloat("テクスチャ密度##rimfx", p.texTiling,   0.1f, 8.f, "%.2f");
        c |= YEditorWidget::SliderFloat("テクスチャ流れ##rimfx", p.texScroll,  -3.f, 3.f, "%.2f");
        return c;
    };
#endif

    return d;
}

} // namespace YoRigine

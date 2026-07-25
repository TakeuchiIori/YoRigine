#include "ImGuiManager.h"
// Engine
#include "DirectXCommon.h"
#include "WinApp/WinApp.h"
#ifdef USE_IMGUI
#include "imgui.h"
#include "imgui_internal.h"
#include <imgui_impl_dx12.cpp>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#endif
#include <iostream>

ImGuiManager *ImGuiManager::GetInstance() {
  static ImGuiManager instance;
  return &instance;
}

void ImGuiManager::Initialize(
    [[maybe_unused]] WinApp *winApp,
    [[maybe_unused]] YoRigine::DirectXCommon *dxCommon) {
#ifdef USE_IMGUI
  dxCommon_ = dxCommon;
  winApp_ = winApp;

  // コンテキスト生成
  ImGui::CreateContext();

  // フォント / IO 設定を先に（フォントビルド前に色設定は不要）
  CustomizeEditor();

  // "Black Gold" テーマ適用
  CustomizeColor();

  // DirectX12 バックエンド初期化
  InitialzeDX12();
#endif
}

void ImGuiManager::Begin() {
#ifdef USE_IMGUI
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
#endif
}

void ImGuiManager::End() {
#ifdef USE_IMGUI
  // 1.91+ では EndFrame() → Render() の順が推奨
  ImGui::EndFrame();
  ImGui::Render();
#endif
}

void ImGuiManager::Draw() {
#ifdef USE_IMGUI
  ID3D12DescriptorHeap *ppHeaps[] = {
      YoRigine::SrvManager::GetInstance()->GetDescriptorHeap()};
  dxCommon_->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

  ImDrawData *drawData = ImGui::GetDrawData();
  if (!drawData || drawData->TotalVtxCount == 0)
    return;

  ImGui_ImplDX12_RenderDrawData(drawData, dxCommon_->GetCommandList().Get());
#endif
}

// =============================================================================
//  DirectX12 バックエンド
// =============================================================================
void ImGuiManager::InitialzeDX12() {
#ifdef USE_IMGUI
  IMGUI_CHECKVERSION();
  ImGui_ImplWin32_Init(winApp_->GetHwnd());

  uint32_t srvIndex = YoRigine::SrvManager::GetInstance()->Allocate();
  HRESULT hr = ImGui_ImplDX12_Init(
      dxCommon_->GetDevice().Get(), dxCommon_->GetBackBufferCount(),
      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
      YoRigine::SrvManager::GetInstance()->GetDescriptorHeap(),
      YoRigine::SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex),
      YoRigine::SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex));
  assert(SUCCEEDED(hr));
#endif
}

// =============================================================================
//  "Black Gold" テーマ
//
//  コンセプト:
//    ・漆黒 (#080604) ベース ― Unreal Editor の深み を参考
//    ・アクセントは 18kt 金色 (Hue≈43°, S≈80%, V≈85%)
//    ・ウィジェット枠線に 極細ゴールドライン でラグジュアリー感
//    ・タブ/タイトルは ダークブラウン でレイヤーを演出
//    ・ホバー/アクティブは 金の明度変化 のみで色相一致を保つ
// =============================================================================
void ImGuiManager::CustomizeColor() {
#ifdef USE_IMGUI
  ImGui::StyleColorsDark();

  ImGuiStyle &s = ImGui::GetStyle();
  ImVec4 *c = s.Colors;

  // -------------------------------------------------------------------------
  //  極限の黒 (Deep Ebony) パレット
  // -------------------------------------------------------------------------
  // 背景は 0.03f〜0.05f に設定。真っ黒(0.0)にすると逆に奥行きが消えるため、
  // わずかに質感を残した「墨色」を採用します。
  const ImVec4 kBgBlack = ImVec4(0.035f, 0.035f, 0.035f, 1.00f); // メイン背景
  const ImVec4 kBgPanel =
      ImVec4(0.055f, 0.055f, 0.055f, 1.00f); // タイトル・パネル
  const ImVec4 kBgInput =
      ImVec4(0.090f, 0.090f, 0.090f, 1.00f); // 入力フィールド

  // アクセントカラー：テック・ブルー
  const ImVec4 kAccentBlue =
      ImVec4(0.120f, 0.450f, 0.880f, 1.00f); // Blenderの選択色に近い青
  const ImVec4 kAccentLight = ImVec4(0.200f, 0.550f, 0.980f, 1.00f); // ホバー用
  const ImVec4 kAccentDim = ImVec4(0.120f, 0.450f, 0.880f, 0.25f);   // 薄い塗り

  const ImVec4 kText = ImVec4(0.920f, 0.920f, 0.920f, 1.00f);    // ほぼ白
  const ImVec4 kTextDim = ImVec4(0.450f, 0.450f, 0.450f, 1.00f); // 非活性

  // =========================================================================
  //  色設定
  // =========================================================================

  // --- テキスト ---
  c[ImGuiCol_Text] = kText;
  c[ImGuiCol_TextDisabled] = kTextDim;
  c[ImGuiCol_TextSelectedBg] = kAccentDim;

  // --- ウィンドウ / 背景 (徹底的に黒く) ---
  c[ImGuiCol_WindowBg] = kBgBlack;
  c[ImGuiCol_ChildBg] = kBgBlack;
  c[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.96f);
  c[ImGuiCol_MenuBarBg] = kBgPanel;
  c[ImGuiCol_TitleBg] = kBgPanel;
  c[ImGuiCol_TitleBgActive] = kBgPanel;
  c[ImGuiCol_TitleBgCollapsed] = kBgBlack;

  // --- フレーム (InputText, Checkbox等) ---
  c[ImGuiCol_FrameBg] = kBgInput;
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  c[ImGuiCol_FrameBgActive] = kAccentDim;

  // --- ボタン ---
  c[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
  c[ImGuiCol_ButtonActive] = kAccentBlue;

  // --- タブ ---
  c[ImGuiCol_Tab] = kBgBlack;
  c[ImGuiCol_TabHovered] = kBgInput;
  c[ImGuiCol_TabActive] = kBgInput;
  c[ImGuiCol_TabUnfocused] = kBgBlack;
  c[ImGuiCol_TabUnfocusedActive] = kBgBlack;

  // --- ヘッダ (TreeNode / Selectable) ---
  c[ImGuiCol_Header] = kAccentDim;
  c[ImGuiCol_HeaderHovered] = kAccentBlue;
  c[ImGuiCol_HeaderActive] = kAccentLight;

  // --- インジケータ (Check / Slider) ---
  c[ImGuiCol_CheckMark] = kAccentLight;
  c[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
  c[ImGuiCol_SliderGrabActive] = kAccentLight;

  // --- 境界線 (Blender風: 境界を目立たせない) ---
  c[ImGuiCol_Separator] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
  c[ImGuiCol_Border] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
  c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);

  // =========================================================================
  //  形状設定 (Blender-Like Solid)
  // =========================================================================
  s.WindowRounding = 0.0f;
  s.FrameRounding = 2.0f;
  s.GrabRounding = 1.0f;
  s.TabRounding = 2.0f;

  s.WindowBorderSize = 1.0f;
  s.FrameBorderSize = 0.0f; // 枠線ではなく色差で表現

  s.ItemSpacing = ImVec2(8.0f, 4.0f);
  s.FramePadding = ImVec2(5.0f, 3.5f);
  s.WindowTitleAlign = ImVec2(0.02f, 0.5f); // 左寄せ
#endif
}

// =============================================================================
//  エディタ設定（IO / フォント）
// =============================================================================
void ImGuiManager::CustomizeEditor() {
#ifdef USE_IMGUI
  ImGuiIO &io = ImGui::GetIO();

  // --------- IO フラグ -----------------------------------------------------
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // エディタ向け挙動
  io.ConfigDockingWithShift = false; // Shift なしでドック可
  io.ConfigDockingAlwaysTabBar = false;
  io.ConfigWindowsMoveFromTitleBarOnly = true; // タイトルバーのみドラッグ

  // ini 保存先
  io.IniFilename = "imgui_editor.ini";

  // --------- アイコン範囲 --------------------------------------------------
  static const ImWchar kIconRanges[] = {0xf000, 0xf8ff, 0};

  // =========================================================================
  //  フォント 1 : ノーマル 14px (日本語 + Font Awesome マージ)
  // =========================================================================
  {
    ImFontConfig jpCfg;
    jpCfg.OversampleH = 2;
    jpCfg.OversampleV = 2;
    jpCfg.PixelSnapH = false;
    jpCfg.MergeMode = false;
    jpCfg.GlyphOffset = ImVec2(0.0f, 1.0f);

    fontNormal_ = io.Fonts->AddFontFromFileTTF(
        "Resources/Fonts/ipaexg.ttf", 14.0f, &jpCfg,
        io.Fonts->GetGlyphRangesJapanese());

    ImFontConfig iconCfg;
    iconCfg.OversampleH = 1;
    iconCfg.OversampleV = 1;
    iconCfg.PixelSnapH = true;
    iconCfg.MergeMode = true;
    iconCfg.GlyphOffset = ImVec2(0.0f, 1.0f);

    io.Fonts->AddFontFromFileTTF("Resources/Fonts/Free-Solid-900.otf", 13.0f,
                                 &iconCfg, kIconRanges);
  }

  // =========================================================================
  //  フォント 2 : ラージ 18px (ツールバー / 大見出し)
  // =========================================================================
  {
    ImFontConfig jpCfg;
    jpCfg.OversampleH = 2;
    jpCfg.OversampleV = 2;
    jpCfg.MergeMode = false;
    jpCfg.GlyphOffset = ImVec2(0.0f, 1.0f);

    fontLarge_ = io.Fonts->AddFontFromFileTTF(
        "Resources/Fonts/ipaexg.ttf", 18.0f, &jpCfg,
        io.Fonts->GetGlyphRangesJapanese());

    ImFontConfig iconCfg;
    iconCfg.OversampleH = 1;
    iconCfg.OversampleV = 1;
    iconCfg.PixelSnapH = true;
    iconCfg.MergeMode = true;
    iconCfg.GlyphOffset = ImVec2(0.0f, 2.0f);

    io.Fonts->AddFontFromFileTTF("Resources/Fonts/Free-Solid-900.otf", 16.0f,
                                 &iconCfg, kIconRanges);
  }

  // =========================================================================
  //  フォント 3 : スモール 11px (ステータスバー / デバッグ情報)
  // =========================================================================
  {
    ImFontConfig jpCfg;
    jpCfg.OversampleH = 2;
    jpCfg.OversampleV = 2;
    jpCfg.MergeMode = false;
    jpCfg.GlyphOffset = ImVec2(0.0f, 0.0f);

    fontSmall_ = io.Fonts->AddFontFromFileTTF(
        "Resources/Fonts/ipaexg.ttf", 11.0f, &jpCfg,
        io.Fonts->GetGlyphRangesJapanese());
  }

  io.Fonts->Build();

#endif // USE_IMGUI
}

void ImGuiManager::Finalize() {
#ifdef USE_IMGUI
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
#endif
}
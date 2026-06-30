#include "TextTextureBaker.h"

// Windows / DirectWrite / Direct2D / WIC
#include <Windows.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <wincodec.h>
#include <wrl.h>

// C++
#include <vector>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <algorithm>

// UI 登録用 / テクスチャ
#include "Systems/UI/UIManager.h"
#include "Systems/UI/UIBase.h"
#include "Loaders/Texture/TextureManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

namespace {

	// ============================================================
	// COM ファクトリ群（遅延生成・プロセス内で使い回す）
	// ============================================================
	struct Factories {
		ComPtr<ID2D1Factory>        d2d;
		ComPtr<IDWriteFactory>      dwrite;
		ComPtr<IWICImagingFactory>  wic;
		bool ok = false;
	};

	Factories& GetFactories() {
		static Factories f;
		static bool tried = false;
		if (tried) return f;
		tried = true;

		// WIC 用に COM を初期化（既に初期化済みなら S_FALSE 等が返るだけで無害）
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);

		if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory), reinterpret_cast<void**>(f.d2d.GetAddressOf())))) {
			return f;
		}
		if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(f.dwrite.GetAddressOf())))) {
			return f;
		}
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(f.wic.GetAddressOf())))) {
			return f;
		}
		f.ok = true;
		return f;
	}

	// UTF-8 → wstring
	std::wstring ToWide(const std::string& s) {
		if (s.empty()) return std::wstring();
		int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		std::wstring w(n > 0 ? n - 1 : 0, L'\0');
		if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
		return w;
	}

	D2D1_COLOR_F ToColor(const Vector4& c) {
		return D2D1::ColorF(c.x, c.y, c.z, c.w);
	}

	// Resources/Fonts 等のディレクトリから .ttf/.otf のファイル名一覧を返す
	std::vector<std::string> ListFontFiles(const std::string& dir) {
		std::vector<std::string> out;
		std::error_code ec;
		if (!std::filesystem::exists(dir, ec)) return out;
		for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
			if (!e.is_regular_file()) continue;
			std::string ext = e.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
			if (ext == ".ttf" || ext == ".otf") out.push_back(e.path().filename().string());
		}
		return out;
	}

	// ============================================================
	// グリフ輪郭ジオメトリ方式のテキストレンダラー
	//   IDWriteTextLayout::Draw から各グリフランごとに呼ばれ、
	//   影 → アウトライン → 塗り の順でジオメトリを描く。
	//   アウトラインはストローク幅を 2 倍にして塗りで内側を潰す＝外側に縁が残る。
	// ============================================================
	class OutlineTextRenderer : public IDWriteTextRenderer {
	public:
		OutlineTextRenderer(ID2D1Factory* d2d, ID2D1RenderTarget* rt,
			ID2D1SolidColorBrush* fill, ID2D1SolidColorBrush* outline,
			ID2D1SolidColorBrush* shadow,
			float outlineWidth, bool useShadow, Vector2 shadowOffset)
			: d2d_(d2d), rt_(rt), fill_(fill), outline_(outline), shadow_(shadow),
			outlineWidth_(outlineWidth), useShadow_(useShadow), shadowOffset_(shadowOffset) {}

		// ---- IDWriteTextRenderer ----
		HRESULT STDMETHODCALLTYPE DrawGlyphRun(void*, FLOAT baselineX, FLOAT baselineY,
			DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN* run,
			const DWRITE_GLYPH_RUN_DESCRIPTION*, IUnknown*) override {
			if (!run || !run->fontFace) return S_OK;

			ComPtr<ID2D1PathGeometry> geo;
			if (FAILED(d2d_->CreatePathGeometry(&geo))) return S_OK;
			ComPtr<ID2D1GeometrySink> sink;
			if (FAILED(geo->Open(&sink))) return S_OK;
			run->fontFace->GetGlyphRunOutline(run->fontEmSize, run->glyphIndices,
				run->glyphAdvances, run->glyphOffsets, run->glyphCount,
				run->isSideways, (run->bidiLevel & 1) != 0, sink.Get());
			sink->Close();

			// ベースライン位置へ平行移動
			ComPtr<ID2D1TransformedGeometry> tg;
			d2d_->CreateTransformedGeometry(geo.Get(),
				D2D1::Matrix3x2F::Translation(baselineX, baselineY), &tg);

			if (useShadow_ && shadow_) {
				ComPtr<ID2D1TransformedGeometry> sg;
				d2d_->CreateTransformedGeometry(geo.Get(),
					D2D1::Matrix3x2F::Translation(baselineX + shadowOffset_.x, baselineY + shadowOffset_.y), &sg);
				rt_->FillGeometry(sg.Get(), shadow_);
			}
			if (outlineWidth_ > 0.0f && outline_) {
				rt_->DrawGeometry(tg.Get(), outline_, outlineWidth_ * 2.0f);
			}
			rt_->FillGeometry(tg.Get(), fill_);
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, const DWRITE_UNDERLINE*, IUnknown*) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH*, IUnknown*) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return S_OK; }

		// ---- IDWritePixelSnapping ----
		HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void*, BOOL* d) override { *d = TRUE; return S_OK; }
		HRESULT STDMETHODCALLTYPE GetCurrentTransform(void*, DWRITE_MATRIX* m) override {
			*m = DWRITE_MATRIX{ 1,0,0,1,0,0 }; return S_OK;
		}
		HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void*, FLOAT* p) override { *p = 1.0f; return S_OK; }

		// ---- IUnknown（スタック確保なので参照カウントは形だけ） ----
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppv) override {
			if (iid == __uuidof(IUnknown) || iid == __uuidof(IDWritePixelSnapping) ||
				iid == __uuidof(IDWriteTextRenderer)) {
				*ppv = this; return S_OK;
			}
			*ppv = nullptr; return E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
		ULONG STDMETHODCALLTYPE Release() override { return 1; }

	private:
		ID2D1Factory* d2d_;
		ID2D1RenderTarget* rt_;
		ID2D1SolidColorBrush* fill_;
		ID2D1SolidColorBrush* outline_;
		ID2D1SolidColorBrush* shadow_;
		float   outlineWidth_;
		bool    useShadow_;
		Vector2 shadowOffset_;
	};

	// ============================================================
	// 任意の .ttf/.otf からフォントコレクション＋ファミリ名を作る（IDWriteFactory5 経由）
	//   失敗時は false（呼び出し側でシステムフォントにフォールバック）
	// ============================================================
	bool BuildCustomFont(IDWriteFactory* base, const std::wstring& filePath,
		ComPtr<IDWriteFontCollection>& outColl, std::wstring& outFamily) {
		ComPtr<IDWriteFactory5> f5;
		if (FAILED(base->QueryInterface(IID_PPV_ARGS(&f5)))) return false;

		ComPtr<IDWriteFontSetBuilder1> builder;
		if (FAILED(f5->CreateFontSetBuilder(&builder))) return false;
		ComPtr<IDWriteFontFile> file;
		if (FAILED(f5->CreateFontFileReference(filePath.c_str(), nullptr, &file))) return false;
		if (FAILED(builder->AddFontFile(file.Get()))) return false;

		ComPtr<IDWriteFontSet> set;
		if (FAILED(builder->CreateFontSet(&set))) return false;
		ComPtr<IDWriteFontCollection1> coll1;
		if (FAILED(f5->CreateFontCollectionFromFontSet(set.Get(), &coll1))) return false;

		// 先頭ファミリの名前を取得
		if (coll1->GetFontFamilyCount() == 0) return false;
		ComPtr<IDWriteFontFamily> family;
		if (FAILED(coll1->GetFontFamily(0, &family))) return false;
		ComPtr<IDWriteLocalizedStrings> names;
		if (FAILED(family->GetFamilyNames(&names))) return false;

		UINT32 idx = 0; BOOL exists = FALSE;
		names->FindLocaleName(L"en-us", &idx, &exists);
		if (!exists) idx = 0;
		UINT32 len = 0; names->GetStringLength(idx, &len);
		outFamily.assign(len, L'\0');
		names->GetString(idx, outFamily.data(), len + 1);

		outColl = coll1;
		return true;
	}

} // namespace

namespace YoRigine {

	bool TextTextureBaker::Bake(const TextBakeParams& params, const std::string& outPngPath) {
		Factories& f = GetFactories();
		if (!f.ok) return false;

		const std::wstring text = ToWide(params.text);
		if (text.empty()) return false;

		// ---- フォント（カスタム .ttf 優先、無ければシステムフォント） ----
		ComPtr<IDWriteFontCollection> coll;     // nullptr = システムコレクション
		std::wstring family = ToWide(params.fontFamily);
		if (!params.fontFilePath.empty()) {
			std::wstring famFromFile;
			if (BuildCustomFont(f.dwrite.Get(), ToWide(params.fontFilePath), coll, famFromFile)) {
				family = famFromFile;
			}
		}

		DWRITE_FONT_WEIGHT weight = params.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
		DWRITE_FONT_STYLE  style = params.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

		ComPtr<IDWriteTextFormat> format;
		if (FAILED(f.dwrite->CreateTextFormat(family.c_str(), coll.Get(), weight, style,
			DWRITE_FONT_STRETCH_NORMAL, params.fontSize, L"", &format))) {
			return false;
		}
		format->SetWordWrapping(params.maxWidth > 0.0f ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
		format->SetTextAlignment(params.align == 1 ? DWRITE_TEXT_ALIGNMENT_CENTER
			: params.align == 2 ? DWRITE_TEXT_ALIGNMENT_TRAILING
			: DWRITE_TEXT_ALIGNMENT_LEADING);

		// ---- レイアウトを作って実寸を測る ----
		const float layoutMaxW = params.maxWidth > 0.0f ? params.maxWidth : 100000.0f;
		ComPtr<IDWriteTextLayout> layout;
		if (FAILED(f.dwrite->CreateTextLayout(text.c_str(), (UINT32)text.size(),
			format.Get(), layoutMaxW, 100000.0f, &layout))) {
			return false;
		}
		DWRITE_TEXT_METRICS m{};
		layout->GetMetrics(&m);

		const float shadowExtent = params.shadow
			? std::max(std::fabs(params.shadowOffset.x), std::fabs(params.shadowOffset.y)) : 0.0f;
		const float margin = params.padding + params.outlineWidth + shadowExtent;

		const float contentW = (params.maxWidth > 0.0f ? params.maxWidth : m.widthIncludingTrailingWhitespace);
		const UINT  bmpW = (UINT)std::ceil(contentW + margin * 2.0f);
		const UINT  bmpH = (UINT)std::ceil(m.height + margin * 2.0f);
		if (bmpW == 0 || bmpH == 0) return false;

		// ---- WIC ビットマップ＋ D2D レンダーターゲット ----
		ComPtr<IWICBitmap> wicBmp;
		if (FAILED(f.wic->CreateBitmap(bmpW, bmpH, GUID_WICPixelFormat32bppPBGRA,
			WICBitmapCacheOnLoad, &wicBmp))) {
			return false;
		}
		D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
			D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			96.0f, 96.0f);
		ComPtr<ID2D1RenderTarget> rt;
		if (FAILED(f.d2d->CreateWicBitmapRenderTarget(wicBmp.Get(), &rtProps, &rt))) {
			return false;
		}

		ComPtr<ID2D1SolidColorBrush> fillBrush, outlineBrush, shadowBrush;
		rt->CreateSolidColorBrush(ToColor(params.fillColor), &fillBrush);
		rt->CreateSolidColorBrush(ToColor(params.outlineColor), &outlineBrush);
		rt->CreateSolidColorBrush(ToColor(params.shadowColor), &shadowBrush);

		OutlineTextRenderer renderer(f.d2d.Get(), rt.Get(), fillBrush.Get(),
			outlineBrush.Get(), shadowBrush.Get(),
			params.outlineWidth, params.shadow, params.shadowOffset);

		rt->BeginDraw();
		rt->Clear(D2D1::ColorF(0, 0, 0, 0));   // 透明背景
		layout->Draw(nullptr, &renderer, margin, margin);
		HRESULT hrEnd = rt->EndDraw();
		if (FAILED(hrEnd)) return false;

		// ---- プリマルチプライ解除（PNG はストレートアルファ） ----
		WICRect rc{ 0, 0, (INT)bmpW, (INT)bmpH };
		ComPtr<IWICBitmapLock> lock;
		if (FAILED(wicBmp->Lock(&rc, WICBitmapLockRead, &lock))) return false;
		UINT cbBuf = 0, stride = 0; BYTE* src = nullptr;
		lock->GetStride(&stride);
		lock->GetDataPointer(&cbBuf, &src);
		std::vector<BYTE> buf(src, src + cbBuf);
		lock.Reset();
		for (UINT y = 0; y < bmpH; ++y) {
			BYTE* row = buf.data() + (size_t)y * stride;
			for (UINT x = 0; x < bmpW; ++x) {
				BYTE* px = row + (size_t)x * 4; // B,G,R,A
				BYTE a = px[3];
				if (a != 0 && a != 255) {
					px[0] = (BYTE)std::min(255, px[0] * 255 / a);
					px[1] = (BYTE)std::min(255, px[1] * 255 / a);
					px[2] = (BYTE)std::min(255, px[2] * 255 / a);
				}
			}
		}

		// ---- 出力先ディレクトリを用意 ----
		std::error_code ec;
		std::filesystem::path outPath(outPngPath);
		if (outPath.has_parent_path()) std::filesystem::create_directories(outPath.parent_path(), ec);

		// ---- PNG エンコード ----
		ComPtr<IWICBitmap> straight;
		if (FAILED(f.wic->CreateBitmapFromMemory(bmpW, bmpH, GUID_WICPixelFormat32bppBGRA,
			stride, cbBuf, buf.data(), &straight))) {
			return false;
		}
		ComPtr<IWICStream> stream;
		if (FAILED(f.wic->CreateStream(&stream))) return false;
		if (FAILED(stream->InitializeFromFilename(ToWide(outPngPath).c_str(), GENERIC_WRITE))) return false;

		ComPtr<IWICBitmapEncoder> encoder;
		if (FAILED(f.wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder))) return false;
		if (FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache))) return false;

		ComPtr<IWICBitmapFrameEncode> frame;
		if (FAILED(encoder->CreateNewFrame(&frame, nullptr))) return false;
		if (FAILED(frame->Initialize(nullptr))) return false;
		frame->SetSize(bmpW, bmpH);
		WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
		frame->SetPixelFormat(&pf);
		if (FAILED(frame->WriteSource(straight.Get(), nullptr))) return false;
		if (FAILED(frame->Commit())) return false;
		if (FAILED(encoder->Commit())) return false;

		return true;
	}

	bool TextTextureBaker::BakeAndRegisterUI(const std::string& uiId, const TextBakeParams& params,
		const std::string& outPngPath, int layer) {
		if (!Bake(params, outPngPath)) return false;

		// 同名再ベイクでも最新画像を反映させる（キャッシュ済みなら作り直す）
		TextureManager::GetInstance()->ReloadTexture(outPngPath);

		auto* mgr = UIManager::GetInstance();
		if (UIBase* existing = mgr->GetUI(uiId)) {
			// 既存 UI があればテクスチャを貼り替えるだけ（サイズは自動追従）
			existing->SetTexture(outPngPath);
			existing->SetLayer(layer);
			existing->SetVisible(true);
			return true;
		}

		auto ui = std::make_unique<UIBase>(uiId);
		ui->Initialize("");               // config 無し → 既定 white で初期化
		ui->SetTexture(outPngPath);       // ベイク画像へ差し替え（AdjustTextureSize で実寸反映）
		ui->SetLayer(layer);
		ui->SetVisible(true);
		mgr->AddUI(uiId, std::move(ui));
		return true;
	}

#ifdef USE_IMGUI
	void TextTextureBaker::ImGuiPanel() {
		static char textBuf[256] = "Hello";
		static char fontFamily[128] = "Meiryo";
		static char fontFile[256] = "";
		static char outName[128] = "Resources/UITex/text.png";
		static char uiId[128] = "bakedText";
		static float fontSize = 64.0f;
		static bool  bold = false, italic = false;
		static Vector4 fill{ 1,1,1,1 };
		static float   outlineW = 4.0f;
		static Vector4 outlineC{ 0,0,0,1 };
		static bool    shadow = false;
		static float   shadowOff[2] = { 3,3 };
		static Vector4 shadowC{ 0,0,0,0.5f };
		static float   padding = 8.0f;
		static int     layer = 0;
		static std::string status;

		static std::string previewPath;          // プレビュー対象のPNGパス

		ImGui::TextUnformatted("テキストを PNG にベイクして UI 用テクスチャにする");

		// ---- プレビュー（最上部・UI登録不要。ベイクした瞬間に表示） ----
		if (!previewPath.empty()) {
			auto* tm = TextureManager::GetInstance();
			try {
				const auto& md = tm->GetMetaData(previewPath);
				float iw = (float)md.width, ih = (float)md.height;
				float avail = ImGui::GetContentRegionAvail().x;
				float maxW = (avail > 32.0f) ? avail : 256.0f;
				float scale = (iw > maxW) ? (maxW / iw) : 1.0f;
				ImVec2 sz(iw * scale, ih * scale);
				// 透明背景の文字が見えるよう、暗いベタ塗りを背面に
				ImVec2 p0 = ImGui::GetCursorScreenPos();
				ImGui::GetWindowDrawList()->AddRectFilled(
					p0, ImVec2(p0.x + sz.x, p0.y + sz.y), IM_COL32(48, 48, 48, 255));
				ImGui::Image((ImTextureID)tm->GetsrvHandleGPU(previewPath).ptr, sz);
				ImGui::Text("プレビュー: %d x %d px", (int)md.width, (int)md.height);
			}
			catch (...) { /* 未ロード等は無視 */ }
		}

		ImGui::Separator();

		ImGui::InputText("テキスト", textBuf, sizeof(textBuf));

		// ---- フォント選択（Resources/Fonts を走査してコンボ表示） ----
		static std::vector<std::string> fontFiles;
		static bool fontsScanned = false;
		static int  fontSel = 0; // 0 =(システムフォント)、1.. = fontFiles[idx-1]
		if (!fontsScanned) { fontFiles = ListFontFiles("Resources/Fonts"); fontsScanned = true; }

		const char* curLabel = (fontSel == 0 || fontSel > (int)fontFiles.size())
			? "(システムフォント)" : fontFiles[fontSel - 1].c_str();
		if (ImGui::BeginCombo("フォント", curLabel)) {
			if (ImGui::Selectable("(システムフォント)", fontSel == 0)) { fontSel = 0; fontFile[0] = '\0'; }
			for (int i = 0; i < (int)fontFiles.size(); ++i) {
				if (ImGui::Selectable(fontFiles[i].c_str(), fontSel == i + 1)) {
					fontSel = i + 1;
					std::string full = "Resources/Fonts/" + fontFiles[i];
					strncpy_s(fontFile, full.c_str(), sizeof(fontFile) - 1);
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("再スキャン")) { fontFiles = ListFontFiles("Resources/Fonts"); fontSel = 0; fontFile[0] = '\0'; }

		// システムフォント選択時のみファミリ名入力を出す
		if (fontSel == 0) {
			ImGui::InputText("フォント名(システム)", fontFamily, sizeof(fontFamily));
		}

		ImGui::DragFloat("サイズ(px)", &fontSize, 1.0f, 4.0f, 512.0f, "%.0f");
		ImGui::Checkbox("太字", &bold); ImGui::SameLine(); ImGui::Checkbox("斜体", &italic);
		ImGui::ColorEdit4("塗り色", &fill.x);

		ImGui::SeparatorText("アウトライン");
		ImGui::DragFloat("太さ(px)", &outlineW, 0.5f, 0.0f, 64.0f, "%.1f");
		ImGui::ColorEdit4("縁色", &outlineC.x);

		ImGui::SeparatorText("影");
		ImGui::Checkbox("影を付ける", &shadow);
		ImGui::DragFloat2("影オフセット", shadowOff, 0.5f, -64.0f, 64.0f, "%.1f");
		ImGui::ColorEdit4("影色", &shadowC.x);

		ImGui::SeparatorText("出力");
		ImGui::DragFloat("余白(px)", &padding, 1.0f, 0.0f, 128.0f, "%.0f");
		ImGui::InputText("出力PNGパス", outName, sizeof(outName));
		ImGui::InputText("UI ID", uiId, sizeof(uiId));
		ImGui::DragInt("レイヤー", &layer, 1, 0, 8);

		auto buildParams = [&]() {
			TextBakeParams p;
			p.text = textBuf;
			p.fontFamily = fontFamily;
			p.fontFilePath = fontFile;
			p.fontSize = fontSize;
			p.bold = bold; p.italic = italic;
			p.fillColor = fill;
			p.outlineWidth = outlineW; p.outlineColor = outlineC;
			p.shadow = shadow; p.shadowOffset = { shadowOff[0], shadowOff[1] }; p.shadowColor = shadowC;
			p.padding = padding;
			return p;
			};

		static std::string lastRegisteredId;     // 位置調整の対象
		static float uiPos[2] = { 640.0f, 360.0f };

		ImGui::Separator();
		if (ImGui::Button("PNG にベイク")) {
			if (Bake(buildParams(), outName)) {
				// 登録なしでも見れるよう、テクスチャだけ読み直してプレビュー対象にする
				TextureManager::GetInstance()->ReloadTexture(outName);
				previewPath = outName;
				status = std::string("ベイク成功: ") + outName;
			} else {
				status = "ベイク失敗";
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("ベイク & UI 登録")) {
			if (BakeAndRegisterUI(uiId, buildParams(), outName, layer)) {
				lastRegisteredId = uiId;
				previewPath = outName;
				if (auto* ui = UIManager::GetInstance()->GetUI(uiId)) {
					ui->SetPosition({ uiPos[0], uiPos[1], 0.0f });
				}
				status = std::string("UI 登録成功: ") + uiId;
			} else {
				status = "失敗";
			}
		}

		if (!status.empty()) {
			ImGui::TextWrapped("%s", status.c_str());
		}

		// ---- 配置調整（登録済みUIをドラッグで動かす。将来ギズモ化） ----
		if (!lastRegisteredId.empty()) {
			if (auto* ui = UIManager::GetInstance()->GetUI(lastRegisteredId)) {
				ImGui::SeparatorText("配置調整");
				ImGui::Text("対象UI: %s", lastRegisteredId.c_str());
				// 外部（UIエディタ等）で動かされた場合に追従
				Vector3 cur = ui->GetPosition();
				uiPos[0] = cur.x; uiPos[1] = cur.y;
				if (ImGui::DragFloat2("位置(px)", uiPos, 1.0f)) {
					ui->SetPosition({ uiPos[0], uiPos[1], cur.z });
				}
				if (ImGui::Button("中央へ")) {
					uiPos[0] = 640.0f; uiPos[1] = 360.0f;
					ui->SetPosition({ uiPos[0], uiPos[1], cur.z });
				}
			}
		}
	}
#endif

} // namespace YoRigine

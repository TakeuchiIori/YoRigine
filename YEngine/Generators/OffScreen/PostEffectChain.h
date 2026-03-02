#pragma once
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include "OffScreen.h"
#include "Vector2.h"
#include "Vector4.h"

// エフェクトの設定情報
struct PostEffectData {
	OffScreen::OffScreenEffectType type;
	std::string name;
	bool enabled = true;

	// 各エフェクト固有のパラメータ
	struct {
		float exposure = 1.0f;                      // ToneMapping用
		float sigma = 2.0f;                         // GaussSmoothing用
		int kernelSize = 3;                         // 各種フィルタ用
		Vector4 outlineColor = { 0,0,0,1 };         // DepthOutline用
		OffScreen::RadialBlurPrams radialBlur;      // RadialBlur用
		OffScreen::DissolveParams dissolve;         // Dissolve用
		OffScreen::ChromaticParams chromatic;       // Chromatic用
		OffScreen::ColorAdjustParams colorAdjust;   // ColorAdjust用
		OffScreen::ToneParams tone;
		OffScreen::ShatterTransitionParams shatter; // ShatterTransition用
	} params;
};

/// <summary>
/// エフェクトのチェーン（連鎖）を管理するクラス
/// 役割：エフェクトの順序管理、追加/削除、パラメータ設定、ImGui表示
/// </summary>
class PostEffectChain
{
public:
	///************************* エフェクト操作 *************************///

	// エフェクトを追加
	int AddEffect(OffScreen::OffScreenEffectType type, const std::string& name = "");

	// エフェクトを削除
	void RemoveEffect(int index);

	// エフェクトを移動
	void MoveEffect(int fromIndex, int toIndex);

	// エフェクトの有効/無効を切り替え
	void SetEffectEnabled(int index, bool enabled);

	// 全エフェクトをクリア
	void ClearAllEffects();

	///************************* アクセッサ *************************///

	// エフェクト数を取得
	size_t GetEffectCount() const { return effects_.size(); }

	// エフェクトデータを取得
	PostEffectData* GetPostEffectData(int index);
	const PostEffectData* GetPostEffectData(int index) const;

	// 有効なエフェクトのインデックス一覧を取得
	std::vector<int> GetEnabledEffectIndices() const;

	// 全エフェクトデータを取得（JSON用）
	const std::vector<std::unique_ptr<PostEffectData>>& GetAllEffects() const { return effects_; }

	// エフェクトデータを設定（JSON用）
	void SetAllEffects(std::vector<std::unique_ptr<PostEffectData>>&& effects);

	///************************* パラメータ更新 *************************///

	// トーンマッピングのパラメータを設定
	void SetToneMappingExposure(int index, float exposure);

	// ガウシアンブラーのパラメータを設定
	void SetGaussianBlurParams(int index, float sigma, int kernelSize);

	// デプスアウトラインのパラメータを設定
	void SetDepthOutlineParams(int index, int kernelSize, const Vector4& color);

	// ラジアルブラーのパラメータを設定
	void SetRadialBlurParams(int index, const OffScreen::RadialBlurPrams& params);

	// ディゾルブのパラメータを設定
	void SetDissolveParams(int index, const OffScreen::DissolveParams& params);

	// クロマチックアバーレーションのパラメータを設定
	void SetChromaticParams(int index, const OffScreen::ChromaticParams& params);

	// 色調整のパラメータを設定
	void SetColorAdjustParams(int index, const OffScreen::ColorAdjustParams& colorParams, const OffScreen::ToneParams& toneParams);

	// 破壊シーン遷移のパラメータを設定
	void SetShatterTransitionParams(int index, const OffScreen::ShatterTransitionParams& params);
	///************************* ImGui表示 *************************///

	// エフェクトリストのImGui表示
	bool DrawEffectListImGui(int& selectedIndex);

	// 選択されたエフェクトのパラメータ編集ImGui
	bool DrawEffectParametersImGui(int selectedIndex);


private:
	///************************* 内部処理 *************************///

	// エフェクト名を自動生成
	std::string GenerateEffectName(OffScreen::OffScreenEffectType type);

	// エフェクトタイプから表示名を取得
	const char* GetEffectTypeName(OffScreen::OffScreenEffectType type) const;

	// デフォルトパラメータを設定
	void SetDefaultParameters(PostEffectData& effect);

	// インデックスが有効かチェック
	bool IsValidIndex(int index) const;

private:
	///************************* メンバ変数 *************************///

	// エフェクトリスト
	std::vector<std::unique_ptr<PostEffectData>> effects_;

	// エフェクト名のカウンタ
	std::unordered_map<OffScreen::OffScreenEffectType, int> effectCounters_;
};

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include "PostEffectChain.h"

namespace YoRigine{
class DirectXCommon;
}

class RtvManager;
class OffScreen;

/// <summary>
/// ポストエフェクト管理クラス
/// </summary>
class PostEffectManager
{
public:
	static PostEffectManager* GetInstance() {
		static PostEffectManager instance;
		return &instance;
	}

	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();
	// 終了処理
	void Finalize();
	void Reset();
	// ImGui表示
	void ImGui();
	// 全エフェクトを適用した描画
	void Draw();

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

	///************************* セーブ/ロード *************************///

	// エフェクトチェーンを保存
	bool SaveEffectChain(const std::string& filename);

	// エフェクトチェーンを読み込み
	bool LoadEffectChain(const std::string& filename);

	// プリセットを保存
	bool SavePreset(const std::string& presetName);

	// プリセットを読み込み
	bool LoadPreset(const std::string& presetName);

	// 利用可能なプリセット一覧を取得
	std::vector<std::string> GetAvailablePresets() const;

	///************************* アクセッサ *************************///

	// エフェクトチェーンを取得
	PostEffectChain* GetEffectChain() { return effectChain_.get(); }

	// エフェクト数を取得
	size_t GetEffectCount() const;

	// エフェクトデータを取得
	PostEffectData* GetEffectData(int index);

private:
	///************************* 内部処理 *************************///

	PostEffectManager() = default;
	~PostEffectManager() = default;
	PostEffectManager(const PostEffectManager&) = delete;
	PostEffectManager& operator=(const PostEffectManager&) = delete;

	// レンダーターゲットの初期化
	void InitializeRenderTargets();

	// エフェクトチェーンを描画
	void RenderEffectChain();

	// エフェクトチェーンをオフスクリーンに適用
	void ApplyEffectParametersToOffScreen(const PostEffectData& effect);

	// リソースの状態遷移を管理
	void TransitionResource(const std::string& rtName, D3D12_RESOURCE_STATES newState);

	// ビューポートとシザー矩形を設定
	void SetViewportAndScissor();

	///************************* JSON処理 *************************///

	// エフェクトチェーンをJSONに変換
	std::string EffectChainToJson() const;

	// JSONからエフェクトチェーンを復元
	bool JsonToEffectChain(const std::string& jsonStr);

	// プリセットディレクトリのパスを取得
	std::string GetPresetDirectory() const;

	// プリセットファイルのパスを生成
	std::string GetPresetFilePath(const std::string& presetName) const;

	// ディレクトリが存在しない場合は作成
	bool EnsureDirectoryExists(const std::string& dirPath) const;


private:
	///************************* メンバ変数 *************************///

	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	RtvManager* rtvManager_ = nullptr;
	OffScreen* offScreen_ = nullptr;

	// エフェクトチェーン（エフェクトの管理）
	std::unique_ptr<PostEffectChain> effectChain_;

	// 中間バッファ用レンダーターゲット名
	std::vector<std::string> intermediateRTNames_;

	// レンダーターゲットの現在の状態を管理
	std::unordered_map<std::string, D3D12_RESOURCE_STATES> rtStates_;

	// ImGui用の選択されたエフェクトインデックス
	int selectedEffectIndex_ = -1;

	// 設定
	static constexpr const char* PRESET_DIRECTORY = "Resources/Json/PostEffectPresets/";
	static constexpr const char* FILE_EXTENSION = ".json";
	static constexpr int MAX_INTERMEDIATE_BUFFERS = 4;
};
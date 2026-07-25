#pragma once
#include "../VirtualCamera.h"
#include "Easing.h"
#include <vector>

// ============================================================
// キーフレームカメラクラス
// 時間軸に沿って複数の座標・回転・FOVを補間してアニメーションする
// ============================================================
class KeyframeCamera : public VirtualCamera {
public:
	// ============================================================
	// 補間モード
	//   Linear     : 隣接2キーで直線補間（Easing 適用）
	//   CatmullRom : 隣接4キーで Catmull-Rom 曲線補間（スプライン経路）
	// ============================================================
	enum class InterpolationMode {
		Linear,
		CatmullRom,
	};

	// ============================================================
	// 構造体定義
	// ============================================================
	struct Keyframe {
		float time;
		Vector3 translate;
		Vector3 rotate;
		float fov;
		Easing::Function easing;
	};

public:
	// ============================================================
	// 基本関数
	// ============================================================
	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;
	void DrawDebug3D(YoRigine::Line& line) override;

	// 指定時刻のキー補間を即時評価して transform_ / fovY_ に反映する
	// （プレビューや外部スクラブから利用）
	void EvaluateAt(float time);

	// ============================================================
	// セーブ・ロード
	// ============================================================
	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;

	// ============================================================
	// キーフレーム操作
	// ============================================================
	void AddKeyframe(float time, const Vector3& pos, const Vector3& rot, float fov, Easing::Function easing);
	void SortKeyframes();

	// 指定 index と index+1 のキーの間に、現在の曲線上をサンプリングした新キーを追加
	void InsertKeyBetween(int beforeIndex);

	// すべての隣接キー間に、現在の曲線をサンプリングした N 個の中間キーを一括追加
	// 元のキーは保持される。N が大きいほど曲線がより細かい制御点で再構成される
	void SubdivideAllSegments(int n);

	// ============================================================
	// 再生制御（外部からの演出駆動用）
	// ============================================================
	void Play()    { isPlaying_ = true; }
	void Stop()    { isPlaying_ = false; }
	void Reset()   { timer_ = 0.0f; }
	bool IsPlaying() const { return isPlaying_; }
	void SetLooping(bool loop) { isLooping_ = loop; }
	float GetTimer() const { return timer_; }
	float GetMaxTime() const { return keyframes_.empty() ? 0.0f : keyframes_.back().time; }

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	std::vector<Keyframe> keyframes_;
	float timer_ = 0.0f;
	bool isPlaying_ = false;
	bool isLooping_ = true;
	float playbackSpeed_ = 1.0f;
	InterpolationMode interpolationMode_ = InterpolationMode::CatmullRom;

	// スムーズ移動モード:
	//   true  → 各キーの easing は無視、globalEasing_ を全体時間に適用 + 各セグメントは Linear
	//           (CatmullRom 曲線が滑らかなので等速で通過し「止まる」現象がなくなる)
	//   false → 各キーの easing を per-segment 適用（従来挙動、ポイントで pause かかる）
	bool useSmoothMotion_ = true;
	Easing::Function globalEasing_ = Easing::Function::EaseInOutQuad;

	// 編集 UX 用
	int selectedKeyIndex_ = -1;     // ImGui で選択中のキー（3D ハイライト用）
	bool showPath_ = true;          // 3D デバッグ描画を出すか
	int pathSegmentSamples_ = 32;   // 1 セグメントあたりのライン描画分割数
};
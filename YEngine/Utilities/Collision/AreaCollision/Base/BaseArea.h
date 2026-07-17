#pragma once

// Math
#include "Vector3.h"

// Graphics
#include "../Graphics/Drawer/LineManager/Line.h"

// System
#include "Systems/Camera/Camera.h"
#include <Loaders/Json/Use/AutoJson.h>

// C++
#include <functional>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <json.hpp>


// エリアの種類を定義
enum class AreaType
{
	Circle,      // 円形エリア
	Rectangle,   // 矩形エリア
	Sphere,      // 球体エリア
	Box,         // 直方体エリア
	Polygon,     // 任意多角形エリア
};

// エリアの用途を定義
enum class AreaPurpose
{
	Boundary,    // 境界制限（プレイヤーを内側に閉じ込める）
	Trigger,     // トリガー（入った時/出た時のイベント）
};

// エリア判定の基底クラス
class BaseArea
{
public:
	///************************* コールバック定義 *************************///

	// targetKey: Update に渡したターゲット識別キー (WorldTransform* やキャラクタポインタ)。
	//            「誰が円内にいるか」をゲーム側で判別してダメージ等を適用するために渡す。
	using AreaEnterCallback = std::function<void(void* targetKey, const Vector3& position)>;
	using AreaExitCallback  = std::function<void(void* targetKey, const Vector3& position)>;
	using AreaStayCallback  = std::function<void(void* targetKey, const Vector3& position)>;

public:
	///************************* デストラクタ *************************///

	virtual ~BaseArea() = default;

public:
	///************************* 純粋仮想関数（継承先で実装） *************************///

	// エリア内かどうかを判定
	virtual bool IsInside(const Vector3& position) const = 0;

	// 位置をエリア内にクランプ（境界制限用）
	virtual Vector3 ClampPosition(const Vector3& position) const = 0;

	// エリア境界までの距離を取得（正の値 = 内側、負の値 = 外側）
	virtual float GetDistanceFromBoundary(const Vector3& position) const = 0;

	// エリアの中心座標を取得
	virtual Vector3 GetCenter() const = 0;

	// デバッグ描画
	virtual void Draw(Line* line) = 0;

	// エリアタイプを取得
	virtual AreaType GetAreaType() const = 0;

	// タイプ文字列を取得（JSON保存・ファクトリ用）
	virtual std::string GetTypeString() const = 0;

public:
	///************************* 共通機能 *************************///

	// 更新処理（位置トラッキングやコールバック判定）
	//   targetKey:  ターゲットを識別するキー (例: WorldTransform* やキャラクタポインタ)。
	//               省略時は legacy 単一ターゲットモード (キーは nullptr)。
	//               異なるターゲットそれぞれの Enter/Exit を独立して追跡する。
	//   deltaTime:  Stay tick 用の経過秒。SetStayTickInterval(>0) 時のみ使用。
	//               0 (既定) の場合 Stay は毎フレーム発火する。
	void Update(const Vector3& targetPosition, void* targetKey = nullptr, float deltaTime = 0.0f);

	// あるターゲットが現在エリア内に居るか
	bool IsTargetInside(void* targetKey) const {
		return insideTargets_.find(targetKey) != insideTargets_.end();
	}
	size_t GetInsideTargetCount() const { return insideTargets_.size(); }

	// ターゲット忘却 (例: キャラクタが破棄されたときに呼ぶ)
	void ForgetTarget(void* targetKey) {
		insideTargets_.erase(targetKey);
		stayTimers_.erase(targetKey);
	}

	// Stay コールバックの発火間隔（秒）。0 = 毎フレーム発火。
	// 例: 毒ダメージを 0.5 秒ごとに入れたいなら 0.5f を設定する。
	void  SetStayTickInterval(float seconds) { stayTickInterval_ = seconds; }
	float GetStayTickInterval() const        { return stayTickInterval_; }

	// 境界に接触しているかチェック（マージン付き）
	bool IsTouchingBoundary(const Vector3& position, float margin = 0.1f) const;

	// エリアから押し戻すベクトルを取得
	Vector3 GetPushBackVector(const Vector3& position) const;

	// 滑らかに境界内に収める（補間係数付き）
	Vector3 SmoothClampPosition(const Vector3& currentPos,
		const Vector3& targetPos,
		float lerpFactor = 0.3f) const;

public:
	///************************* コールバック設定 *************************///

	void SetOnEnterArea(AreaEnterCallback cb) { enterCallback_ = cb; }
	void SetOnExitArea(AreaExitCallback cb)   { exitCallback_  = cb; }
	void SetOnStayArea(AreaStayCallback cb)   { stayCallback_  = cb; }

public:
	///************************* アクセッサ *************************///

	void SetActive(bool active)             { isActive_ = active; }
	bool IsActive() const                   { return isActive_; }

	void SetPurpose(AreaPurpose purpose)    { purpose_ = purpose; }
	AreaPurpose GetPurpose() const          { return purpose_; }

	void SetDebugDrawEnabled(bool enabled)  { isDebugDrawEnabled_ = enabled; }
	bool IsDebugDrawEnabled() const         { return isDebugDrawEnabled_; }

	void SetCamera(Camera* camera)          { camera_ = camera; }

	// AutoJson アクセッサ
	AutoJson& GetAutoJson()                 { return aj_; }
	const AutoJson& GetAutoJson() const     { return aj_; }

protected:
	///************************* AutoJson 登録（派生クラスで override して拡張） *************************///

	// コンストラクタの末尾で必ず呼ぶこと
	virtual void SetupAutoJson()
	{
		aj_.Add("active",    &isActive_)
		   .Add("debugDraw", &isDebugDrawEnabled_);
	}

protected:
	///************************* 内部状態管理 *************************///

	// ターゲットごとの「現在エリア内」状態を独立管理。
	// 旧 wasInside_ は単一 bool で複数ターゲット時に状態破壊が起きていたため廃止。
	std::unordered_set<void*> insideTargets_;

	// ターゲットごとの Stay tick 蓄積時間 (stayTickInterval_ > 0 のとき使用)。
	std::unordered_map<void*, float> stayTimers_;
	float       stayTickInterval_   = 0.0f;   // 0 = 毎フレーム Stay 発火

	bool        isActive_           = true;
	AreaPurpose purpose_            = AreaPurpose::Boundary;
	bool        isDebugDrawEnabled_ = true;
	Camera*     camera_             = nullptr;

	AutoJson    aj_;

private:
	///************************* コールバック保持 *************************///

	AreaEnterCallback enterCallback_;
	AreaExitCallback  exitCallback_;
	AreaStayCallback  stayCallback_;
};

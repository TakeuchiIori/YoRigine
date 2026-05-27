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

	using AreaEnterCallback = std::function<void(const Vector3& position)>;
	using AreaExitCallback  = std::function<void(const Vector3& position)>;
	using AreaStayCallback  = std::function<void(const Vector3& position)>;

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
	void Update(const Vector3& targetPosition);

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

	bool        wasInside_          = false;
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

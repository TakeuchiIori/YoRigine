#pragma once

#include "YParticleManager.h"
#include "Vector3.h"
#include <string>
#include "YParticleSystem.h"
#include "YEmitterShape.h"

/// <summary>
/// パーティクルエミッター
/// 特定のパーティクルシステムを簡単に制御するためのヘルパークラス
/// ゲームオブジェクトにアタッチして使用することを想定
/// </summary>
class YParticleEmitter {
public:
	//=================================================================
	// コンストラクタ
	//=================================================================

	/// <summary>
	/// エミッターの作成
	/// </summary>
	/// <param name="systemName">対象のパーティクルシステム名</param>
	/// <param name="position">初期位置</param>
	YParticleEmitter(const std::string& systemName, const Vector3& position = { 0, 0, 0 });
	~YParticleEmitter() = default;

	//=================================================================
	// 更新
	//=================================================================

	/// <summary>
	/// エミッターの更新
	/// 自動発生が有効な場合、一定間隔でパーティクルを発生
	/// </summary>
	/// <param name="deltaTime">経過時間</param>
	void Update(float deltaTime);

	//=================================================================
	// パーティクル発生
	//=================================================================

	/// <summary>
	/// パーティクルを発生
	/// </summary>
	/// <param name="count">発生数（-1の場合はデフォルト数）</param>
	void Emit(int count = -1);

	/// <summary>
	/// バースト発生（一度に大量発生）
	/// </summary>
	/// <param name="count">発生数</param>
	void EmitBurst(int count);

	/// <summary>
	/// 追従エミット
	/// 指定位置からパーティクルを発生（エミッター位置を使用しない）
	/// </summary>
	/// <param name="position">発生位置</param>
	/// <param name="count">発生数</param>
	void FollowEmit(const Vector3& position, int count = 1);

	/// <summary>
	/// リセット関数
	/// </summary>
	void Reset();

private:

	//=================================================================
	// 内部処理
	//=================================================================

	/// <summary>
	/// 内部に射出する関数
	/// </summary>
	/// <param name="position"></param>
	/// <param name="count"></param>
	void EmitInternal(const Vector3& position, int count);
public:
	//=================================================================
	// アクセサ
	//=================================================================

	// 位置
	void SetPosition(const Vector3& position) { position_ = position; }
	const Vector3& GetPosition() const { return position_; }

	// 発生レート（秒間の発生数）
	void SetEmissionRate(float rate) { emissionRate_ = rate; }
	float GetEmissionRate() const { return emissionRate_; }

	// 1回の発生数
	void SetEmitCount(int count) { emitCount_ = count; }
	int GetEmitCount() const { return emitCount_; }

	// 有効/無効
	void SetActive(bool active) { isActive_ = active; }
	bool IsActive() const { return isActive_; }

	// 自動発生
	void SetAutoEmit(bool autoEmit) { autoEmit_ = autoEmit; }
	bool GetAutoEmit() const { return autoEmit_; }

	// システム名
	const std::string& GetSystemName() const { return systemName_; }
	void SetSystemName(const std::string& name) { systemName_ = name; }

	// 対象システムの取得
	YParticleSystem* GetTargetSystem() const {
		return YParticleManager::GetInstance().GetSystem(systemName_);
	}

	//=================================================================
	// 形状設定メソッド
	//=================================================================

	/// 点（全パーティクルが同一点から放出）
	void SetShapePoint();

	/// 球体（半径固定 / 後方互換）
	void SetShapeSphere(float radius, bool shellOnly = false);

	/// 球体 min〜max 範囲（内側半径〜外側半径）
	/// minRadius = maxRadius で球殻になる
	void SetShapeSphereRange(float minRadius, float maxRadius);

	/// 箱型（サイズ固定 / 後方互換）
	void SetShapeBox(const Vector3& size);

	/// 箱型 min〜max 範囲（内側サイズ〜外側サイズ）
	/// minSize > {0,0,0} で中空ボックスになる
	void SetShapeBoxRange(const Vector3& minSize, const Vector3& maxSize);

	/// コーン（方向付き円錐台）
	/// @param outerAngleDeg 外側の半頂角（度）
	/// @param height        コーン高さ
	/// @param direction     コーン軸方向（正規化不要）
	void SetShapeCone(float outerAngleDeg, float height,
	                  const Vector3& direction = { 0,1,0 });

	/// コーン min〜max 角度範囲（内側〜外側の半頂角）
	void SetShapeConeRange(float innerAngleDeg, float outerAngleDeg,
	                       float height, float minRadius = 0.0f, float maxRadius = 1.0f,
	                       const Vector3& direction = { 0,1,0 });

	YEmitterShape* GetShape() const { return shape_.get(); }

	//=================================================================
	// システムへの直接アクセス（高度な制御用）
	//=================================================================

	/// <summary>
	/// モジュールを追加
	/// </summary>
	template<typename T, typename... Args>
	void AddSpawnModule(Args&&... args) {
		auto* system = GetTargetSystem();
		if (system) {
			auto module = std::make_shared<T>(std::forward<Args>(args)...);
			system->AddSpawnModule(module);
		}
	}

	template<typename T, typename... Args>
	void AddUpdateModule(Args&&... args) {
		auto* system = GetTargetSystem();
		if (system) {
			auto module = std::make_shared<T>(std::forward<Args>(args)...);
			system->AddUpdateModule(module);
		}
	}

	/// <summary>
	/// モジュールを取得
	/// </summary>
	template<typename T>
	std::shared_ptr<T> GetModule(const std::string& moduleName) {
		auto* system = GetTargetSystem();
		if (system) {
			return std::dynamic_pointer_cast<T>(system->GetModule(moduleName));
		}
		return nullptr;
	}

private:
	//=================================================================
	// メンバ変数
	//=================================================================

	std::string systemName_;    // 対象システム名
	Vector3 position_;          // エミッター位置

	// エミッション制御
	std::unique_ptr<YEmitterShape> shape_; // エミッター形状
	float emissionRate_;                    // 秒間発生数
	float emissionTimer_;                   // 発生タイマー
	int emitCount_;                         // 1回の発生数

	// 状態
	bool isActive_;             // 有効フラグ
	bool autoEmit_;             // 自動発生フラグ
};
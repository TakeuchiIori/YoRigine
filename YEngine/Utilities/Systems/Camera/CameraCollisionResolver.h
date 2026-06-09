#pragma once
#include "Vector3.h"
#include "MathFunc.h"
#include "json.hpp"


///************************* カメラの地形めり込みを防止し、滑らかな回避とハイアングル化を行うコンポーネント *************************///
class CameraCollisionResolver {
public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	/// <summary>
	/// 理想のカメラ座標から地形のめり込みを計算し、最終的な安全な座標を返す
	/// </summary>
	/// <param name="idealPos">本来カメラが行きたい座標</param>
	/// <param name="targetPivot">プレイヤーの頭など、レイの始点となる注視点</param>
	/// <returns>壁避け・ハイアングル化を適用した最終座標</returns>
	Vector3 Resolve(const Vector3& idealPos, const Vector3& targetPivot);

	// エディタ設定用
	void DrawDebugGui();
	void Save(nlohmann::json& j) const;
	void Load(const nlohmann::json& j);

	///************************* アクセッサ *************************///
	// オンオフ切り替え
	void SetEnabled(bool enable) { isEnabled_ = enable; }
	bool IsEnabled() const { return isEnabled_; }

	// 無視するコライダーのTypeIDを設定する
	void AddIgnoreTypeID(uint32_t typeID) { ignoreTypeIDs_.push_back(typeID); }
	void ClearIgnoreTypeIDs() { ignoreTypeIDs_.clear(); }
private:
	///************************* メンバ関数 *************************///

	bool isEnabled_ = true;							// 衝突判定を有効にするか
	float cameraRadius_ = 0.5f;						// カメラの球体判定の半径（壁からどれくらい離すか）
	float avoidSpeed_ = 0.3f;						// 壁が迫った時に手前に寄る素早さ (旧 60fps Lerp 係数。内部で /sec に換算)
	float returnSpeed_ = 0.05f;						// 壁がなくなった時に元の距離に戻る素早さ (同上)
	float minDistanceRatio_ = 0.15f;				// カメラ最小距離率。これ以下にはピボットに寄せない (ターゲットが画面いっぱいになるのを防ぐ)

	// --- ハイアングル演出パラメータ ---
	// rotation 補正なしで Y だけ持ち上げると look-at が pivot を外し、
	// 追従ターゲットが画面外に出る。基本的に off にしておくこと。
	bool enableHighAngle_ = false;					// 接近時のハイアングル化を有効にするか
	float highAngleThreshold_ = 0.5f;				// 距離が本来の何割未満になったら持ち上げ始めるか
	float maxPushUpHeight_ = 3.0f;					// 最大でどれくらい上に持ち上げるか

	// --- 内部状態 ---
	float currentDistanceRatio_ = 1.0f;				// 現在の距離の割合 (0.0 ~ 1.0、補間用)
	float minGroundHeight_ = 0.5f;					// カメラが地面にめり込むのを防止するための最低高さ
	std::vector<uint32_t> ignoreTypeIDs_;			// 衝突判定で無視するコライダーのタイプID（例: プレイヤー自身のコライダー）
};
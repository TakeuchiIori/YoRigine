#pragma once

// App
#include "AreaEffectBase.h"
#include "IAreaEffectTarget.h"

// C++
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

class Line;

// ============================================================
// 複数の AreaEffect と効果対象を束ねて一括駆動するマネージャ（シーンが所有）。
//   ・対象(敵/プレイヤー)を登録しておく
//   ・エリアを名前付きで登録する
//   ・Update(dt) 1回で「対象位置を集める → 全エリアへ配る」まで回る
//   これによりシーン側の記述は生成＋Update＋Draw の数行に収まる。
// ============================================================
class AreaEffectManager {
public:
	///--- 効果対象の登録 ---///

	void RegisterTarget(IAreaEffectTarget* target);
	void UnregisterTarget(IAreaEffectTarget* target);
	void ClearTargets();

	///--- エリアの登録 ---///

	void AddEffect(const std::string& name, std::shared_ptr<AreaEffectBase> effect);
	void RemoveEffect(const std::string& name);
	std::shared_ptr<AreaEffectBase> GetEffect(const std::string& name);
	void ClearEffects();

	// 対象・エリアの両方をクリア（シーン終了時に呼ぶ）
	void Clear();

	// 生存対象の位置を1回だけ集め、全エリアへ配って判定・効果適用を回す。
	void Update(float deltaTime);

	// 全エリアのデバッグ描画
	void Draw(Line* line);

private:
	std::vector<IAreaEffectTarget*> targets_;
	std::unordered_map<std::string, std::shared_ptr<AreaEffectBase>> effects_;

	// 毎フレーム再利用する対象バッファ（再確保を避ける）
	std::vector<AreaEffectBase::Target> scratch_;
};

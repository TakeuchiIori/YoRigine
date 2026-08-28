#include "EnemyAttackLibrary.h"

#include "../../AI/EnemyAIContext.h"
#include "../../BaseEnemy.h"

#include <algorithm>
#include <random>

bool EnemyAttackLibrary::enabled_ = false;

EnemyAttackLibrary &EnemyAttackLibrary::GetInstance() {
	static EnemyAttackLibrary instance;
	return instance;
}

bool EnemyAttackLibrary::Load(const std::string &path) {
	return EnemyAttackIO::Load(path, attacks_);
}

bool EnemyAttackLibrary::Save(const std::string &path) const {
	return EnemyAttackIO::Save(path, attacks_);
}

const EnemyAttack *EnemyAttackLibrary::Find(const std::string &id) const {
	for (const auto &attack : attacks_) {
		if (attack.id == id) return &attack;
	}
	return nullptr;
}

// ============================================================
// 追加・複製・削除
// ============================================================
int EnemyAttackLibrary::Add(const std::string &id) {
	EnemyAttack attack;
	attack.id = MakeUniqueId(id);
	attack.displayName = attack.id;

	// 何もカーブが無いと動かないので、前へ出るだけの最小の形を入れておく
	attack.tracks.GetChannel(AttackChannel::PositionZ).AddKey(0.0f, 0.0f);
	attack.tracks.GetChannel(AttackChannel::PositionZ).AddKey(1.0f, 4.0f);

	// 攻撃判定が無いと当たらないので既定で1つ置く
	AttackModifier hitbox;
	hitbox.type = AttackModifierType::Hitbox;
	hitbox.startTime = attack.duration * 0.4f;
	hitbox.endTime = attack.duration * 0.7f;
	hitbox.damageWindow = 0;
	attack.modifiers.push_back(hitbox);

	attacks_.push_back(std::move(attack));
	return static_cast<int>(attacks_.size()) - 1;
}

int EnemyAttackLibrary::Duplicate(int index) {
	if (index < 0 || index >= static_cast<int>(attacks_.size())) return -1;

	EnemyAttack copy = attacks_[index];
	copy.id = MakeUniqueId(copy.id);
	copy.displayName = copy.id;

	attacks_.push_back(std::move(copy));
	return static_cast<int>(attacks_.size()) - 1;
}

void EnemyAttackLibrary::Remove(int index) {
	if (index < 0 || index >= static_cast<int>(attacks_.size())) return;
	attacks_.erase(attacks_.begin() + index);
}

std::string EnemyAttackLibrary::MakeUniqueId(const std::string &base) const {
	const std::string root = base.empty() ? "attack" : base;
	std::string candidate = root;

	int suffix = 1;
	bool duplicated = true;
	while (duplicated) {
		duplicated = false;
		for (const auto &attack : attacks_) {
			if (attack.id != candidate) continue;
			duplicated = true;
			candidate = root + "_" + std::to_string(suffix++);
			break;
		}
	}
	return candidate;
}

namespace {

float RandomRange(float min, float max) {
	if (max <= min) return min;
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(min, max);
	return dist(gen);
}

// この敵が使える攻撃か
bool IsUsableByEnemy(const std::string &attackId, const std::vector<std::string> &attackIds) {
	// リスト未設定の敵は全攻撃を候補にする（移行中の互換動作）
	if (attackIds.empty()) return true;
	return std::find(attackIds.begin(), attackIds.end(), attackId) != attackIds.end();
}

} // namespace

// ============================================================
// 攻撃の抽選
// ============================================================
const EnemyAttack *EnemyAttackPicker::Pick(const BaseEnemy &enemy,
                                           const std::vector<std::string> &attackIds,
                                           const EnemyAIContext &ctx,
                                           const PerceptionParams &perception) {
	(void)enemy;

	const auto &attacks = EnemyAttackLibrary::GetInstance().GetAll();
	if (attacks.empty()) return nullptr;

	std::vector<const EnemyAttack *> candidates;
	std::vector<float> weights;
	float total = 0.0f;

	const bool hasOpening = ctx.HasOpening(perception);

	for (const auto &attack : attacks) {
		// 重み0は抽選対象外（カウンターのように専用経路で出すもの）
		if (attack.weight <= 0.0f) continue;

		if (!IsUsableByEnemy(attack.id, attackIds)) continue;
		if (ctx.distance < attack.minRange) continue;
		if (ctx.distance > attack.maxRange) continue;
		if (ctx.selfHpRatio > attack.selfHpBelow) continue;
		if (ctx.playerHpRatio > attack.targetHpBelow) continue;

		float weight = attack.weight;

		// プレイヤーの状態による補正
		if (perception.enabled && ctx.hasPlayer) {
			if (hasOpening) {
				// 隙は短いので、溜め技を選ぶと終わる頃には消えている
				weight *= attack.fast ? perception.openingFastAttackWeight
				                      : perception.openingSlowAttackWeight;
			} else if (ctx.playerIsDodging && !attack.fast) {
				// 回避を空振りさせるため、あえて溜め技で釣る
				weight *= perception.baitSlowAttackWeight;
			}
		}

		if (weight <= 0.0f) continue;

		candidates.push_back(&attack);
		weights.push_back(weight);
		total += weight;
	}

	if (candidates.empty() || total <= 0.0f) return nullptr;

	float roll = RandomRange(0.0f, total);
	for (size_t i = 0; i < candidates.size(); ++i) {
		roll -= weights[i];
		if (roll <= 0.0f) return candidates[i];
	}
	return candidates.back();
}

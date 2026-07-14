#include "CollisionManager.h"
#include <assert.h>
#include <algorithm>
#include <unordered_map>
#include "CollisionTypeIdDef.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif // _DEBUG

namespace YoRigine {

	CollisionManager* CollisionManager::GetInstance() {
		static CollisionManager instance;
		return &instance;
	}

	CollisionManager::~CollisionManager() {
		Reset();
	}

	void CollisionManager::Initialize() {
		isDrawCollider_ = false;
	}

	void CollisionManager::Update() {
		CheckAllCollisions();
	}

	void CollisionManager::Reset() {
		colliders_.clear();
		collidingPairs_.clear();
	}

	// ============================================================
	// 形状ペアディスパッチ
	//  - shape の組み合わせで分岐し、Intersection の対応関数を呼ぶ
	//  - 戻り値の normal は「A を B から押し戻す向き」に統一する
	// ============================================================
	bool CollisionManager::DispatchShapePair(BaseCollider* a, BaseCollider* b, CollisionResult* outResult) {
		const ColliderShape sa = a->GetShape();
		const ColliderShape sb = b->GetShape();

		// 形状ペアを 16 通りで分岐 (実質 10 通り、対称分はスワップ)
		auto* spA = (sa == ColliderShape::Sphere)  ? static_cast<SphereCollider*>(a)  : nullptr;
		auto* aaA = (sa == ColliderShape::AABB)    ? static_cast<AABBCollider*>(a)    : nullptr;
		auto* oaA = (sa == ColliderShape::OBB)     ? static_cast<OBBCollider*>(a)     : nullptr;
		auto* caA = (sa == ColliderShape::Capsule) ? static_cast<CapsuleCollider*>(a) : nullptr;

		auto* spB = (sb == ColliderShape::Sphere)  ? static_cast<SphereCollider*>(b)  : nullptr;
		auto* aaB = (sb == ColliderShape::AABB)    ? static_cast<AABBCollider*>(b)    : nullptr;
		auto* oaB = (sb == ColliderShape::OBB)     ? static_cast<OBBCollider*>(b)     : nullptr;
		auto* caB = (sb == ColliderShape::Capsule) ? static_cast<CapsuleCollider*>(b) : nullptr;

		bool hit = false;

		if (spA && spB) {
			Sphere A{ spA->GetCenterPosition(), spA->GetRadius() };
			Sphere B{ spB->GetCenterPosition(), spB->GetRadius() };
			hit = Intersection::IsCollision(A, B, outResult);
		}
		else if (spA && aaB) {
			Sphere A{ spA->GetCenterPosition(), spA->GetRadius() };
			hit = Intersection::IsCollision(A, aaB->GetAABB(), outResult);
		}
		else if (spA && oaB) {
			Sphere A{ spA->GetCenterPosition(), spA->GetRadius() };
			hit = Intersection::IsCollision(A, oaB->GetOBB(), outResult);
		}
		else if (spA && caB) {
			Sphere A{ spA->GetCenterPosition(), spA->GetRadius() };
			// IsCollision(Capsule, Sphere) の法線は「Sphere → Capsule」方向。
			// ここでは A=Sphere=自分、B=Capsule=相手 なので、押し戻し向きは A→B = Sphere→Capsule = そのまま。
			hit = Intersection::IsCollision(caB->GetCapsule(), A, outResult);
		}
		else if (aaA && spB) {
			Sphere B{ spB->GetCenterPosition(), spB->GetRadius() };
			hit = Intersection::IsCollision(B, aaA->GetAABB(), outResult);
			if (hit && outResult) outResult->normal = outResult->normal * -1.0f;
		}
		else if (aaA && aaB) {
			hit = Intersection::IsCollision(aaA->GetAABB(), aaB->GetAABB(), outResult);
		}
		else if (aaA && oaB) {
			hit = Intersection::IsCollision(aaA->GetAABB(), oaB->GetOBB(), outResult);
		}
		else if (aaA && caB) {
			hit = Intersection::IsCollision(caB->GetCapsule(), aaA->GetAABB(), outResult);
			if (hit && outResult) outResult->normal = outResult->normal * -1.0f;
		}
		else if (oaA && spB) {
			Sphere B{ spB->GetCenterPosition(), spB->GetRadius() };
			hit = Intersection::IsCollision(B, oaA->GetOBB(), outResult);
			if (hit && outResult) outResult->normal = outResult->normal * -1.0f;
		}
		else if (oaA && aaB) {
			hit = Intersection::IsCollision(aaB->GetAABB(), oaA->GetOBB(), outResult);
			if (hit && outResult) outResult->normal = outResult->normal * -1.0f;
		}
		else if (oaA && oaB) {
			hit = Intersection::IsCollision(oaA->GetOBB(), oaB->GetOBB(), outResult);
		}
		else if (oaA && caB) {
			hit = Intersection::IsCollision(caB->GetCapsule(), oaA->GetOBB(), outResult);
			if (hit && outResult) outResult->normal = outResult->normal * -1.0f;
		}
		else if (caA && spB) {
			Sphere B{ spB->GetCenterPosition(), spB->GetRadius() };
			// 法線が「Sphere→Capsule」になるので、A=Capsule に揃えるため反転
			hit = Intersection::IsCollision(caA->GetCapsule(), B, outResult);
			if (hit && outResult) outResult->normal = outResult->normal * -1.0f;
		}
		else if (caA && aaB) {
			hit = Intersection::IsCollision(caA->GetCapsule(), aaB->GetAABB(), outResult);
		}
		else if (caA && oaB) {
			hit = Intersection::IsCollision(caA->GetCapsule(), oaB->GetOBB(), outResult);
		}
		else if (caA && caB) {
			hit = Intersection::IsCollision(caA->GetCapsule(), caB->GetCapsule(), outResult);
		}

		return hit;
	}

	// ============================================================
	// Ray vs 任意コライダー
	// ============================================================
	bool CollisionManager::DispatchRay(const Ray& ray, BaseCollider* c, RaycastHit* outHit) {
		switch (c->GetShape()) {
		case ColliderShape::Sphere: {
			auto* sc = static_cast<SphereCollider*>(c);
			Sphere s{ sc->GetCenterPosition(), sc->GetRadius() };
			return Intersection::IsCollision(ray, s, outHit);
		}
		case ColliderShape::AABB: {
			auto* ac = static_cast<AABBCollider*>(c);
			return Intersection::IsCollision(ray, ac->GetAABB(), outHit);
		}
		case ColliderShape::OBB: {
			auto* oc = static_cast<OBBCollider*>(c);
			return Intersection::IsCollision(ray, oc->GetOBB(), outHit);
		}
		case ColliderShape::Capsule: {
			auto* cc = static_cast<CapsuleCollider*>(c);
			return Intersection::IsCollision(ray, cc->GetCapsule(), outHit);
		}
		default:
			return false;
		}
	}

	// ============================================================
	// 2つのコライダーの当たり判定とイベント呼び出しのみ。
	// 押し戻しは別関数 ResolveContacts で反復処理する。
	// ============================================================
	void CollisionManager::CheckCollisionPair(BaseCollider* a, BaseCollider* b) {
		if (!a || !b) return;

		// レイヤーマスクで early reject
		if ((a->GetLayerBits() & b->GetCollisionMask()) == 0u) return;
		if ((b->GetLayerBits() & a->GetCollisionMask()) == 0u) return;

		auto key = std::minmax(a, b);
		auto it = collidingPairs_.find(key);
		bool wasColliding = (it != collidingPairs_.end());

		CollisionResult res;
		bool hit = DispatchShapePair(a, b, &res);

		if (hit) {
			if (!wasColliding) {
				a->CallOnEnterCollision(b);
				b->CallOnEnterCollision(a);
				collidingPairs_.emplace(key, 0);
			}
			else {
				it->second = 0; // 接触継続: missStreak をリセット
			}
			a->CallOnCollision(b);
			b->CallOnCollision(a);
		}
		else {
			if (wasColliding) {
				// 押し戻し等で 1 フレームだけ離れた揺らぎでは即 Exit させない。
				// 連続 no-hit が猶予を超えて初めて Exit を発火する。
				if (++it->second > contactExitGraceFrames_) {
					a->CallOnExitCollision(b);
					b->CallOnExitCollision(a);
					collidingPairs_.erase(it);
				}
				// 猶予中は接触継続扱い (Enter も Exit も発火しない)
			}
		}
	}

	// ============================================================
	// 反復押し戻し (質量比ベース、2段階累積)
	//   各反復で:
	//     1) 全ペアの貫通量を検出し、accum[c] += disp で累積
	//     2) 全コライダーに対して accum[c] を一括適用
	//   3-4 反復で 3 体スタックなども安定する。
	// ============================================================
	void CollisionManager::ResolveContacts(const std::vector<std::pair<BaseCollider*, BaseCollider*>>& pairs,
		int iterations)
	{
		if (iterations <= 0) return;

		std::unordered_map<BaseCollider*, Vector3> accum;

		for (int iter = 0; iter < iterations; ++iter) {
			accum.clear();
			bool anyResolved = false;

			for (const auto& pr : pairs) {
				BaseCollider* a = pr.first;
				BaseCollider* b = pr.second;
				if (!a || !b) continue;
				if (!a->GetIsActive() || !a->IsCollisionEnabled()) continue;
				if (!b->GetIsActive() || !b->IsCollisionEnabled()) continue;
				if (!a->GetEnablePenetration() || !b->GetEnablePenetration()) continue;

				// レイヤーマスクで early reject
				if ((a->GetLayerBits() & b->GetCollisionMask()) == 0u) continue;
				if ((b->GetLayerBits() & a->GetCollisionMask()) == 0u) continue;

				WorldTransform* wtA = a->GetWT();
				WorldTransform* wtB = b->GetWT();
				if (!wtA || !wtB) continue;

				CollisionResult res;
				if (!DispatchShapePair(a, b, &res)) continue;
				if (res.penetrationDepth <= 0.0f) continue;

				bool sa = a->GetIsStatic();
				bool sb = b->GetIsStatic();
				if (sa && sb) continue; // 両方静的: 何もしない

				// 質量比で配分。静的側は受け持ちゼロ。
				float ma = std::max(a->GetMass(), 0.0001f);
				float mb = std::max(b->GetMass(), 0.0001f);
				float ratioA, ratioB;
				if (sa) {
					ratioA = 0.0f;
					ratioB = 1.0f;
				} else if (sb) {
					ratioA = 1.0f;
					ratioB = 0.0f;
				} else {
					// 重い方ほど動きにくい → ratioA = mb / (ma + mb)
					float total = ma + mb;
					ratioA = mb / total;
					ratioB = ma / total;
				}

				// res.normal は「A を B から離す方向」になるよう DispatchShapePair で揃えている。
				Vector3 dispA = res.normal * (res.penetrationDepth * ratioA);
				Vector3 dispB = res.normal * (res.penetrationDepth * ratioB) * -1.0f;
				accum[a] += dispA;
				accum[b] += dispB;
				anyResolved = true;
			}

			if (!anyResolved) break;

			// 一括適用
			for (auto& kv : accum) {
				BaseCollider* c = kv.first;
				const Vector3& disp = kv.second;
				WorldTransform* wt = c->GetWT();
				if (!wt) continue;
				wt->translate_ += disp;
				wt->UpdateMatrix();
				c->Update();
			}
		}
	}

	// ============================================================
	// 形状からワールドAABBを計算
	// ============================================================
	bool CollisionManager::IsAABBOutsideCullingFrustum(const AABB& aabb) {
		if (!IsCullingActive()) return false;
		const Frustum fr = FrustumUtil::ExtractFromViewProjection(
			cullingCamera_->GetViewProjectionMatrix());
		return !FrustumUtil::IsAABBVisible(fr, aabb);
	}

	AABB CollisionManager::ComputeWorldAABB(BaseCollider* c) {
		switch (c->GetShape()) {
		case ColliderShape::Sphere: {
			auto* sc = static_cast<SphereCollider*>(c);
			Vector3 center = sc->GetCenterPosition();
			float r = sc->GetRadius();
			return AABB{
				{ center.x - r, center.y - r, center.z - r },
				{ center.x + r, center.y + r, center.z + r }
			};
		}
		case ColliderShape::AABB: {
			return static_cast<AABBCollider*>(c)->GetAABB();
		}
		case ColliderShape::OBB: {
			const OBB obb = static_cast<OBBCollider*>(c)->GetOBB();
			Matrix4x4 rotMat = MakeRotateMatrixXYZ(obb.rotation);
			// ローカル軸ベクトル (列でなく回転行列の行から取る)
			Vector3 ax = { rotMat.m[0][0], rotMat.m[0][1], rotMat.m[0][2] };
			Vector3 ay = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };
			Vector3 az = { rotMat.m[2][0], rotMat.m[2][1], rotMat.m[2][2] };
			float ex = std::fabs(ax.x) * obb.size.x + std::fabs(ay.x) * obb.size.y + std::fabs(az.x) * obb.size.z;
			float ey = std::fabs(ax.y) * obb.size.x + std::fabs(ay.y) * obb.size.y + std::fabs(az.y) * obb.size.z;
			float ez = std::fabs(ax.z) * obb.size.x + std::fabs(ay.z) * obb.size.y + std::fabs(az.z) * obb.size.z;
			return AABB{
				{ obb.center.x - ex, obb.center.y - ey, obb.center.z - ez },
				{ obb.center.x + ex, obb.center.y + ey, obb.center.z + ez }
			};
		}
		case ColliderShape::Capsule: {
			const Capsule cap = static_cast<CapsuleCollider*>(c)->GetCapsule();
			Vector3 mn{
				std::min(cap.start.x, cap.end.x) - cap.radius,
				std::min(cap.start.y, cap.end.y) - cap.radius,
				std::min(cap.start.z, cap.end.z) - cap.radius
			};
			Vector3 mx{
				std::max(cap.start.x, cap.end.x) + cap.radius,
				std::max(cap.start.y, cap.end.y) + cap.radius,
				std::max(cap.start.z, cap.end.z) + cap.radius
			};
			return AABB{ mn, mx };
		}
		default:
			return AABB{};
		}
	}

	// ============================================================
	// 一時オーバーラップクエリ
	//   持続コライダーを登録せず、colliders_ を線形走査して
	//   「今この瞬間、球の範囲に重なっているコライダー」を返すだけの一回限りの検索。
	//   Enter/Exit・接触猶予・CCD 等は一切関与しない。
	// ============================================================
	std::vector<BaseCollider*> CollisionManager::QuerySphere(const Vector3& center, float radius,
		uint32_t layerMask, const std::vector<uint32_t>& ignoreTypeIDs) const {

		std::vector<BaseCollider*> result;
		const Sphere querySphere{ center, radius };

		for (BaseCollider* c : colliders_) {
			if (!c) continue;
			if (!c->GetIsActive() || !c->IsCollisionEnabled()) continue;
			if ((c->GetLayerBits() & layerMask) == 0) continue;

			if (!ignoreTypeIDs.empty()) {
				bool ignored = false;
				for (uint32_t id : ignoreTypeIDs) {
					if (c->GetTypeID() == id) { ignored = true; break; }
				}
				if (ignored) continue;
			}

			// 精密形状ではなく AABB 近似で判定する（QuerySphere は「だいたいこの範囲」の
			// 一括検索用途なので、形状ごとの厳密判定より簡潔さ・安全さを優先する）。
			const AABB worldAABB = ComputeWorldAABB(c);
			if (Intersection::IsCollision(querySphere, worldAABB)) {
				result.push_back(c);
			}
		}

		return result;
	}

	// ============================================================
	// 全コライダーの当たり判定チェック
	//   1) Broad Phase: 有効コライダーの AABB をグリッドに登録、同セル候補ペア列挙
	//   2) ResolveContacts: 反復押し戻しで貫通解消
	//   3) Callback: 各候補ペアに対して Enter/Stay/Exit
	//   4) ExitSweep: 候補に含まれなくなった既往ペアの Exit
	// ============================================================
	// ============================================================
	// CCD: 前フレーム位置から現在位置までを Raycast でスイープし、
	// 衝突を検出した場合は衝突直前の位置で止める。
	// 自身を無視するため typeID を ignore リストに追加する。
	// ============================================================
	void CollisionManager::SweepCCDColliders() {
		for (BaseCollider* c : colliders_) {
			if (!c) continue;
			if (!c->IsCCDEnabled()) continue;
			if (!c->GetIsActive() || !c->IsCollisionEnabled()) continue;
			if (c->GetIsStatic()) continue;

			Vector3 cur = c->GetCenterPosition();
			if (!c->HasPreviousCenter()) {
				c->SetPreviousCenter(cur);
				continue;
			}

			Vector3 prev = c->GetPreviousCenter();
			Vector3 delta = cur - prev;
			float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
			if (dist < 1e-4f) continue;

			Vector3 dir = delta * (1.0f / dist);
			Ray ray{ prev, dir };

			// 自身の typeID を無視
			std::vector<uint32_t> ignore{ c->GetTypeID() };
			RaycastHit hit;
			if (Raycast(ray, dist, &hit, ignore)) {
				// 衝突直前で止める
				WorldTransform* wt = c->GetWT();
				if (wt) {
					Vector3 safePos = hit.hitPoint - dir * 0.01f;
					wt->translate_ = safePos;
					wt->UpdateMatrix();
					c->Update();
				}
			}
		}
	}

	void CollisionManager::CheckAllCollisions() {
		// コールバック内 Add/Remove を保留に誘導
		isIterating_ = true;

		// CCD パス (broad/narrow より前に実行してトンネリング防止)
		SweepCCDColliders();

		grid_.Clear();

		// Frustum culling 用に視錐台を抽出 (毎フレーム)
		Frustum frustum{};
		bool useCulling = (enableFrustumCulling_ && cullingCamera_);
		if (useCulling) {
			frustum = FrustumUtil::ExtractFromViewProjection(
				cullingCamera_->GetViewProjectionMatrix());
		}

		// Broad Phase: 有効コライダーをグリッドに登録
		for (BaseCollider* c : colliders_) {
			if (!c) continue;
			if (c->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kNone)) continue;
			if (!c->GetIsActive() || !c->IsCollisionEnabled()) continue;
			AABB aabb = ComputeWorldAABB(c);

			// Frustum culling: 視錐台外なら BroadPhase 登録をスキップ。
			// IsCheckOutsideCamera() が true のコライダーだけがカリング対象
			// (常時アクティブなプレイヤーなどは false にしておく)。
			if (useCulling && c->IsCheckOutsideCamera()) {
				if (!FrustumUtil::IsAABBVisible(frustum, aabb)) continue;
			}

			grid_.Insert(c, aabb);
		}

		// 候補ペア列挙
		grid_.QueryPairs(broadPhasePairsScratch_);

		// 静的×静的のペアは narrow-phase もコールバックも実質意味がないので
		// この時点で一括除去する。同一場所に多数の static collider を置いた場合の
		// O(N²) コストを劇的に減らす最重要フィルタ。
		broadPhasePairsScratch_.erase(
			std::remove_if(broadPhasePairsScratch_.begin(), broadPhasePairsScratch_.end(),
				[](const std::pair<BaseCollider*, BaseCollider*>& p) {
					return p.first && p.second &&
						p.first->GetIsStatic() && p.second->GetIsStatic();
				}),
			broadPhasePairsScratch_.end());

		// 反復押し戻し
		ResolveContacts(broadPhasePairsScratch_, resolveIterations_);

		// Callback (Enter/Stay/Exit): 各候補ペアを再判定
		std::unordered_set<std::pair<BaseCollider*, BaseCollider*>, PairHash> visited;
		visited.reserve(broadPhasePairsScratch_.size() * 2);
		for (auto& pr : broadPhasePairsScratch_) {
			BaseCollider* a = pr.first;
			BaseCollider* b = pr.second;
			if (!a || !b) continue;
			if (!a->GetIsActive() || !a->IsCollisionEnabled()) continue;
			if (!b->GetIsActive() || !b->IsCollisionEnabled()) continue;
			CheckCollisionPair(a, b);
			visited.insert(std::minmax(a, b));
		}

		// Broad Phase で消えたペアの Exit を発火する。
		// (押し戻しなどで距離が離れてセルが分かれた、片方が無効化された、削除された等)
		// ここでも Exit 猶予を適用し、1 フレームの揺らぎで Enter が多重発火しないようにする。
		for (auto it = collidingPairs_.begin(); it != collidingPairs_.end(); ) {
			if (visited.find(it->first) != visited.end()) {
				++it; continue;
			}
			if (++it->second <= contactExitGraceFrames_) {
				++it; continue; // 猶予中は接触継続扱い
			}
			BaseCollider* a = it->first.first;
			BaseCollider* b = it->first.second;
			if (a && b) {
				a->CallOnExitCollision(b);
				b->CallOnExitCollision(a);
			}
			it = collidingPairs_.erase(it);
		}

		// 次フレームの CCD で使うため、最終位置を previousCenter として保存
		for (BaseCollider* c : colliders_) {
			if (!c) continue;
			if (!c->IsCCDEnabled()) continue;
			c->SetPreviousCenter(c->GetCenterPosition());
		}

		isIterating_ = false;
		FlushPending();
	}

	bool CollisionManager::IsColliderInView(const Vector3& position, const Camera* camera) {
		Vector3 clipPos = Transform(position, camera->GetViewProjectionMatrix());
		return (clipPos.x >= -1.0f && clipPos.x <= 1.0f &&
			clipPos.y >= -1.0f && clipPos.y <= 1.0f &&
			clipPos.z >= 0.0f && clipPos.z <= 1.0f);
	}

	void CollisionManager::AddCollider(BaseCollider* collider) {
		if (!collider) return;
		if (isIterating_) {
			// 走査中の追加は遅延 (走査終了後にflush)
			pendingAdds_.push_back(collider);
			return;
		}
		colliders_.push_back(collider);
	}

	void CollisionManager::RemoveCollider(BaseCollider* collider) {
		if (!collider) return;
		if (isIterating_) {
			// 走査中に実体が破棄される場合があるため、参照はこの場で即座に潰す。
			// pendingRemoves_ に raw pointer を積むと、同フレーム中に ColliderPool が同じ番地を
			// 再利用した時、新しく生成された collider まで後段 Flush で消してしまう。
			// そのため「後で消す」のではなく、走査中の配列上では nullptr 化して無効化だけ行う。
			for (BaseCollider*& c : colliders_) {
				if (c == collider) c = nullptr;
			}
			for (auto& pair : broadPhasePairsScratch_) {
				if (pair.first == collider) pair.first = nullptr;
				if (pair.second == collider) pair.second = nullptr;
			}
			for (auto it = collidingPairs_.begin(); it != collidingPairs_.end(); ) {
				if (it->first.first == collider || it->first.second == collider) {
					it = collidingPairs_.erase(it);
				} else {
					++it;
				}
			}
			return;
		}
		DoRemove(collider);
	}

	void CollisionManager::DoRemove(BaseCollider* collider) {
		if (!collider) return;
		auto it = std::find(colliders_.begin(), colliders_.end(), collider);
		if (it != colliders_.end()) {
			colliders_.erase(it);
		}
		// 削除されたコライダーに関わるペアを掃除
		for (auto it2 = collidingPairs_.begin(); it2 != collidingPairs_.end(); ) {
			if (it2->first.first == collider || it2->first.second == collider) {
				it2 = collidingPairs_.erase(it2);
			} else {
				++it2;
			}
		}
	}

	void CollisionManager::FlushPending() {
		// Remove は Add より先に処理 (削除直後の再追加を許容するため)
		for (BaseCollider* c : pendingRemoves_) {
			DoRemove(c);
		}
		pendingRemoves_.clear();
		for (BaseCollider* c : pendingAdds_) {
			if (!c) continue;
			// すでに居ないか確認
			auto it = std::find(colliders_.begin(), colliders_.end(), c);
			if (it == colliders_.end()) {
				colliders_.push_back(c);
			}
		}
		pendingAdds_.clear();
		colliders_.erase(std::remove(colliders_.begin(), colliders_.end(), nullptr), colliders_.end());
	}

	bool CollisionManager::RaycastMasked(const Ray& ray, float maxDistance, uint32_t layerMask, RaycastHit* outHit)
	{
		bool hitAnything = false;
		float closestDistance = maxDistance;
		RaycastHit tempHit;

		for (BaseCollider* collider : colliders_) {
			if (!collider || !collider->GetIsActive() || !collider->IsCollisionEnabled()) continue;
			if ((collider->GetLayerBits() & layerMask) == 0u) continue;

			if (!DispatchRay(ray, collider, &tempHit)) continue;
			if (tempHit.distance <= 0.001f) continue;
			if (tempHit.distance <= closestDistance) {
				closestDistance = tempHit.distance;
				if (outHit) *outHit = tempHit;
				hitAnything = true;
			}
		}
		return hitAnything;
	}

	bool CollisionManager::Raycast(const Ray& ray, float maxDistance, RaycastHit* outHit, const std::vector<uint32_t>& ignoreTypeIDs)
	{
		bool hitAnything = false;
		float closestDistance = maxDistance;
		RaycastHit tempHit;

		for (BaseCollider* collider : colliders_) {
			if (!collider || !collider->GetIsActive() || !collider->IsCollisionEnabled()) continue;
			if (!ignoreTypeIDs.empty()) {
				auto it = std::find(ignoreTypeIDs.begin(), ignoreTypeIDs.end(), collider->GetTypeID());
				if (it != ignoreTypeIDs.end()) continue;
			}

			bool isHit = DispatchRay(ray, collider, &tempHit);

			if (isHit && tempHit.distance <= 0.001f) continue; // 至近距離無視
			if (isHit && tempHit.distance <= closestDistance) {
				closestDistance = tempHit.distance;
				if (outHit) *outHit = tempHit;
				hitAnything = true;
			}
		}

		return hitAnything;
	}

	// ============================================================
	// ヒット方向判定用ユーティリティ
	// ============================================================
	HitDirection CollisionManager::ConvertVectorToHitDirection(const Vector3& dir) {
		if (fabs(dir.x) > fabs(dir.y) && fabs(dir.x) > fabs(dir.z)) {
			return dir.x > 0 ? HitDirection::Right : HitDirection::Left;
		}
		else if (fabs(dir.y) > fabs(dir.z)) {
			return dir.y > 0 ? HitDirection::Top : HitDirection::Bottom;
		}
		else {
			return dir.z > 0 ? HitDirection::Front : HitDirection::Back;
		}
	}

	HitDirection CollisionManager::InverseHitDirection(HitDirection hitdirection) {
		switch (hitdirection) {
		case HitDirection::Top: return HitDirection::Bottom;
		case HitDirection::Bottom: return HitDirection::Top;
		case HitDirection::Left: return HitDirection::Right;
		case HitDirection::Right: return HitDirection::Left;
		case HitDirection::Front: return HitDirection::Back;
		case HitDirection::Back: return HitDirection::Front;
		default: return HitDirection::None;
		}
	}

	HitDirection CollisionManager::GetSelfLocalHitDirection(BaseCollider* self, BaseCollider* other) {
		Vector3 toOther = Normalize(other->GetCenterPosition() - self->GetCenterPosition());
		const Matrix4x4& mat = self->GetWorldTransform().matWorld_;
		Vector3 up = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 right = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		struct DirDot { HitDirection dir; float dot; };
		std::vector<DirDot> dots = {
			{ HitDirection::Top,    Dot(toOther, up) },
			{ HitDirection::Bottom, Dot(toOther, up * -1.0f) },
			{ HitDirection::Right,  Dot(toOther, right) },
			{ HitDirection::Left,   Dot(toOther, right * -1.0f) },
			{ HitDirection::Front,  Dot(toOther, forward) },
			{ HitDirection::Back,   Dot(toOther, forward * -1.0f) },
		};

		const float threshold = 0.5f;
		for (const auto& d : dots) {
			if (d.dot >= threshold) return d.dir;
		}
		return HitDirection::None;
	}

	HitDirectionBits CollisionManager::GetSelfLocalHitDirectionFlags(BaseCollider* self, BaseCollider* other, float threshold) {
		HitDirectionBits flags = HitDirection_None;
		Vector3 toOther = Normalize(other->GetCenterPosition() - self->GetCenterPosition());
		const Matrix4x4& mat = self->GetWorldTransform().matWorld_;
		Vector3 up = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 right = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		if (Dot(toOther, up) >= threshold)     flags |= HitDirection_Top;
		if (Dot(toOther, up * -1.0f) >= threshold)    flags |= HitDirection_Bottom;
		if (Dot(toOther, right) >= threshold)  flags |= HitDirection_Right;
		if (Dot(toOther, right * -1.0f) >= threshold) flags |= HitDirection_Left;
		if (Dot(toOther, forward) >= threshold) flags |= HitDirection_Front;
		if (Dot(toOther, forward * -1.0f) >= threshold) flags |= HitDirection_Back;

		return flags;
	}

	HitDirectionBits CollisionManager::GetSelfLocalHitDirectionsSimple(BaseCollider* self, BaseCollider* other) {
		HitDirectionBits flags = HitDirection_None;
		Vector3 toOther = Normalize(other->GetCenterPosition() - self->GetCenterPosition());
		const Matrix4x4& mat = self->GetWorldTransform().matWorld_;
		Vector3 up = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 right = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		if (Dot(toOther, up) > 0.0f)         flags |= HitDirection_Top;
		if (Dot(toOther, up * -1.0f) > 0.0f) flags |= HitDirection_Bottom;
		if (Dot(toOther, right) > 0.0f)      flags |= HitDirection_Right;
		if (Dot(toOther, right * -1.0f) > 0.0f) flags |= HitDirection_Left;
		if (Dot(toOther, forward) > 0.0f)    flags |= HitDirection_Front;
		if (Dot(toOther, forward * -1.0f) > 0.0f) flags |= HitDirection_Back;

		return flags;
	}
}

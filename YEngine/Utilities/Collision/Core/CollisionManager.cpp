#include "CollisionManager.h"
// C++
#include <assert.h>
#include <iostream>

// Engine
#include "CollisionTypeIdDef.h"

// C++
#include "MathFunc.h"


#ifdef USE_IMGUI
#include "imgui.h"
#endif // _DEBUG


namespace YoRigine {

	CollisionManager* CollisionManager::GetInstance()
	{
		static CollisionManager instance;
		return &instance;
	}

	CollisionManager::~CollisionManager()
	{
		Reset();
	}


	inline float ProjectOBB(const OBB& obb, const Vector3& axis, const Vector3 axes[3]) {
		return	obb.size.x * fabs(Dot(axes[0], axis)) +
			obb.size.y * fabs(Dot(axes[1], axis)) +
			obb.size.z * fabs(Dot(axes[2], axis));
	}

	bool Collision::Check(const SphereCollider* a, const SphereCollider* b)
	{
		Vector3 diff = b->GetCenterPosition() - a->GetCenterPosition();
		float distSq = Length(diff);
		float radiusSum = a->GetRadius() + b->GetRadius();
		return distSq <= radiusSum;
	}

	bool Collision::Check(const SphereCollider* sphere, const AABBCollider* aabb)
	{
		Vector3 center = sphere->GetCenterPosition();
		float radius = sphere->GetRadius();

		// 値が正常かチェック
		if (std::isnan(center.x) || std::isnan(center.y) || std::isnan(center.z) ||
			std::isnan(radius) || radius < 0.0f || !std::isfinite(radius)) {
			return false;
		}


		Vector3 closest = Clamp(sphere->GetCenterPosition(), aabb->GetAABB().min, aabb->GetAABB().max);
		Vector3 diff = closest - sphere->GetCenterPosition();
		return Length(diff) <= sphere->GetRadius() * sphere->GetRadius();
	}

	bool Collision::Check(const SphereCollider* sphere, const OBBCollider* obb)
	{
		Vector3 center = sphere->GetCenterPosition();
		float radius = sphere->GetRadius();

		// 値が正常かチェック
		if (std::isnan(center.x) || std::isnan(center.y) || std::isnan(center.z) ||
			std::isnan(radius) || radius < 0.0f || !std::isfinite(radius)) {
			return false;
		}

		const OBB& ob = obb->GetOBB();

		// 回転行列を作成（オイラー角 → 回転行列）
		Matrix4x4 rotMat = MakeRotateMatrixXYZ(ob.rotation);

		// ワールド→ローカル変換：回転行列の転置を使う（回転の逆）
		Matrix4x4 invRot = TransPose(rotMat);

		Vector3 localPos = Transform(sphere->GetCenterPosition() - ob.center, invRot);
		Vector3 clamped = Clamp(localPos, -ob.size, ob.size);

		// ローカル→ワールドに戻す
		Vector3 closest = ob.center + Transform(clamped, rotMat);
		Vector3 diff = closest - sphere->GetCenterPosition();

		return Length(diff) <= sphere->GetRadius() * sphere->GetRadius();
	}

	bool Collision::Check(const AABBCollider* a, const AABBCollider* b)
	{
		const AABB& aa = a->GetAABB();
		const AABB& bb = b->GetAABB();
		return (aa.min.x <= bb.max.x && aa.max.x >= bb.min.x) &&
			(aa.min.y <= bb.max.y && aa.max.y >= bb.min.y) &&
			(aa.min.z <= bb.max.z && aa.max.z >= bb.min.z);
	}

	bool Collision::Check(const OBB& obbA, const OBB& obbB)
	{
		// 事前に早期リターンを行う球体近似チェック（オプション）
		float radiusA = Length(obbA.size);
		float radiusB = Length(obbB.size);
		float distance = Length(obbB.center - obbA.center);
		if (distance > radiusA + radiusB) {
			return false; // 明らかに離れている場合は早期リターン
		}

		// 回転行列を取得
		Matrix4x4 matA = MakeRotateMatrixXYZ(obbA.rotation);
		Matrix4x4 matB = MakeRotateMatrixXYZ(obbB.rotation);

		// 各OBBの軸を抽出
		Vector3 axesA[3] = {
			{ matA.m[0][0], matA.m[1][0], matA.m[2][0] },
			{ matA.m[0][1], matA.m[1][1], matA.m[2][1] },
			{ matA.m[0][2], matA.m[1][2], matA.m[2][2] }
		};
		Vector3 axesB[3] = {
			{ matB.m[0][0], matB.m[1][0], matB.m[2][0] },
			{ matB.m[0][1], matB.m[1][1], matB.m[2][1] },
			{ matB.m[0][2], matB.m[1][2], matB.m[2][2] }
		};

		// 中心間の距離ベクトル
		Vector3 distanceVec = obbB.center - obbA.center;

		const float EPSILON = 1e-6f; // 数値的に安定した閾値

		// テスト1: obbAの主軸でのテスト
		for (int i = 0; i < 3; ++i) {
			const Vector3& axis = axesA[i];

			// 軸の長さをチェック（ゼロ除算防止）
			if (LengthSquared(axis) < EPSILON) continue;

			// 各OBBの投影を計算
			float projA = ProjectOBB(obbA, axis, axesA);
			float projB = ProjectOBB(obbB, axis, axesB);

			// 分離軸チェック
			if (fabs(Dot(distanceVec, axis)) > projA + projB) {
				return false; // 分離軸が見つかった
			}
		}

		// テスト2: obbBの主軸でのテスト
		for (int i = 0; i < 3; ++i) {
			const Vector3& axis = axesB[i];

			if (LengthSquared(axis) < EPSILON) continue;

			float projA = ProjectOBB(obbA, axis, axesA);
			float projB = ProjectOBB(obbB, axis, axesB);

			if (fabs(Dot(distanceVec, axis)) > projA + projB) {
				return false;
			}
		}

		// テスト3: 両方のOBBの主軸の外積でのテスト
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				Vector3 axis = Cross(axesA[i], axesB[j]);

				// 外積が十分な大きさかチェック
				float axisLengthSq = LengthSquared(axis);
				if (axisLengthSq < EPSILON) continue;

				// 単位ベクトルに正規化
				axis = axis * (1.0f / sqrt(axisLengthSq));

				float projA = ProjectOBB(obbA, axis, axesA);
				float projB = ProjectOBB(obbB, axis, axesB);

				if (fabs(Dot(distanceVec, axis)) > projA + projB) {
					return false;
				}
			}
		}

		// すべてのテストにパスしたら衝突している
		return true;
	}

	bool Collision::Check(const AABBCollider* aabb, const OBBCollider* obb)
	{
		OBB aabbAsOBB;
		aabbAsOBB.center = (aabb->GetAABB().min + aabb->GetAABB().max) * 0.5f;
		aabbAsOBB.size = (aabb->GetAABB().max - aabb->GetAABB().min) * 0.5f;
		aabbAsOBB.rotation = { 0.0f,0.0f,0.0f };
		return Collision::Check(aabbAsOBB, obb->GetOBB());

	}

	bool Collision::Check(const OBBCollider* a, const OBBCollider* b)
	{
		return Check(a->GetOBB(), b->GetOBB());
	}

	bool Collision::Check(BaseCollider* a, BaseCollider* b) {
		// ここで dynamic_cast してペアを判定
		if (auto sa = dynamic_cast<SphereCollider*>(a)) {
			if (auto sb = dynamic_cast<SphereCollider*>(b)) return Check(sa, sb);
			if (auto ob = dynamic_cast<OBBCollider*>(b))    return Check(sa, ob);
			if (auto ab = dynamic_cast<AABBCollider*>(b))    return Check(sa, ab);
		} else if (auto oa = dynamic_cast<OBBCollider*>(a)) {
			if (auto sb = dynamic_cast<SphereCollider*>(b)) return Check(sb, oa); // 順序逆
			if (auto ob = dynamic_cast<OBBCollider*>(b))    return Check(oa, ob);
			if (auto ab = dynamic_cast<AABBCollider*>(b))    return Check(ab, oa); // 順序逆
		} else if (auto aa = dynamic_cast<AABBCollider*>(a)) {
			if (auto sb = dynamic_cast<SphereCollider*>(b)) return Check(sb, aa); // 順序逆
			if (auto ob = dynamic_cast<OBBCollider*>(b))    return Check(aa, ob);
			if (auto ab = dynamic_cast<AABBCollider*>(b))    return Check(aa, ab);
		}
		return false;
	}

	bool Collision::CheckHitDirection(const AABB& a, const AABB& b, HitDirection* hitDirection)
	{
		bool isHitX = (a.min.x <= b.max.x && a.max.x >= b.min.x);
		bool isHitY = (a.min.y <= b.max.y && a.max.y >= b.min.y);
		bool isHitZ = (a.min.z <= b.max.z && a.max.z >= b.min.z);

		// 衝突していない場合
		if (!(isHitX && isHitY && isHitZ)) {
			if (hitDirection)*hitDirection = HitDirection::None;
			return false;
		}

		if (hitDirection) {
			Vector3 aCenter = (a.min + a.max) * 0.5f;
			Vector3 bCenter = (b.min + b.max) * 0.5f;
			Vector3 diff = bCenter - aCenter;

			Vector3 overlap = {
				std::abs(diff.x) - ((a.max.x - a.min.x) + (b.max.x - b.min.x)) * 0.5f,
				std::abs(diff.y) - ((a.max.y - a.min.y) + (b.max.y - b.min.y)) * 0.5f,
				std::abs(diff.z) - ((a.max.z - a.min.z) + (b.max.z - b.min.z)) * 0.5f,
			};


			if (std::abs(overlap.x) < std::abs(overlap.y) && std::abs(overlap.x) < std::abs(overlap.z)) {
				*hitDirection = (diff.x > 0.0f) ? HitDirection::Left : HitDirection::Right;
			} else if (std::abs(overlap.y) < std::abs(overlap.x) && std::abs(overlap.y) < std::abs(overlap.z)) {
				*hitDirection = (diff.y > 0.0f) ? HitDirection::Top : HitDirection::Bottom;
			} else {
				*hitDirection = (diff.z > 0.0f) ? HitDirection::Front : HitDirection::Back;
			}
		}

		return true;
	}

	bool Collision::CheckHitDirection(const AABB& aabb, const OBB& obb, HitDirection* hitDirection)
	{
		OBB aabbAcObb;
		aabbAcObb.center = (aabb.min + aabb.max) * 0.5f;
		aabbAcObb.size = (aabb.max - aabb.min) * 0.5f;
		aabbAcObb.rotation = { 0.0f,0.0f,0.0f }; // AABBは回転しない

		HitDirection dir;
		bool hit = CheckHitDirection(aabbAcObb, obb, &dir);
		if (hitDirection) {
			*hitDirection = dir;
		}

		return hit;
	}

	bool Collision::CheckHitDirection(const OBB& obbA, const OBB& obbB, HitDirection* hitDirection)
	{
		const float EPSILON = 1e-6f; // 数値的に安定した閾値

		// 回転行列取得
		Matrix4x4 matA = MakeRotateMatrixXYZ(obbA.rotation);
		Matrix4x4 matB = MakeRotateMatrixXYZ(obbB.rotation);

		// 各OBBの軸を抽出
		Vector3 axesA[3] = {
			{ matA.m[0][0], matA.m[1][0], matA.m[2][0] },
			{ matA.m[0][1], matA.m[1][1], matA.m[2][1] },
			{ matA.m[0][2], matA.m[1][2], matA.m[2][2] }
		};
		Vector3 axesB[3] = {
			{ matB.m[0][0], matB.m[1][0], matB.m[2][0] },
			{ matB.m[0][1], matB.m[1][1], matB.m[2][1] },
			{ matB.m[0][2], matB.m[1][2], matB.m[2][2] }
		};
		// 中心間の距離ベクトル
		Vector3 distanceVec = obbB.center - obbA.center;

		float minOverlap = FLT_MAX;
		Vector3 minAxis{};

		// obbAの処理
		for (int i = 0; i < 3; i++) {
			const Vector3& axisA = axesA[i];
			if (LengthSquared(axisA) < EPSILON) continue;

			float projA = ProjectOBB(obbA, axisA, axesA);
			float projB = ProjectOBB(obbB, axisA, axesB);

			float distance = fabs(Dot(distanceVec, axisA));
			float overlap = projA + projB - distance;

			if (overlap < 0) {
				return false; // 分離軸が見つかった
			}

			if (overlap < minOverlap) {
				minOverlap = overlap;
				minAxis = axisA * ((Dot(distanceVec, axisA) > 0) ? -1.0f : 1.0f);
			}
		}


		// obbBの処理
		for (int i = 0; i < 3; i++) {
			const Vector3& axisB = axesB[i];
			if (LengthSquared(axisB) < EPSILON) continue;

			float projA = ProjectOBB(obbA, axisB, axesA);
			float projB = ProjectOBB(obbB, axisB, axesB);

			float distance = fabs(Dot(distanceVec, axisB));
			float overlap = projA + projB - distance;

			if (overlap < 0) {
				return false; // 分離軸が見つかった
			}

			if (overlap < minOverlap) {
				minOverlap = overlap;
				minAxis = axisB * ((Dot(distanceVec, axisB) > 0) ? -1.0f : 1.0f);
			}
		}


		// 両方のOBBの主軸の外積でのテスト
	// 最終的に取得した minAxis をローカル基準で比較して HitDirection を決定
		Vector3 selfUp = { matA.m[1][0], matA.m[1][1], matA.m[1][2] };
		Vector3 selfRight = { matA.m[0][0], matA.m[0][1], matA.m[0][2] };
		Vector3 selfForward = { matA.m[2][0], matA.m[2][1], matA.m[2][2] };

		// ここで、minAxis を「自分の軸基準で比較」
		float dotUp = Dot(Normalize(minAxis), selfUp);
		float dotDown = Dot(Normalize(minAxis), selfUp * -1.0f);
		float dotRight = Dot(Normalize(minAxis), selfRight);
		float dotLeft = Dot(Normalize(minAxis), selfRight * -1.0f);
		float dotForward = Dot(Normalize(minAxis), selfForward);
		float dotBack = Dot(Normalize(minAxis), selfForward * -1.0f);

		// 一番大きい内積の方向が衝突方向
		float maxDot = dotUp;
		HitDirection hitDir = HitDirection::Top;

		if (dotDown > maxDot) { maxDot = dotDown; hitDir = HitDirection::Bottom; }
		if (dotRight > maxDot) { maxDot = dotRight; hitDir = HitDirection::Right; }
		if (dotLeft > maxDot) { maxDot = dotLeft; hitDir = HitDirection::Left; }
		if (dotForward > maxDot) { maxDot = dotForward; hitDir = HitDirection::Front; }
		if (dotBack > maxDot) { maxDot = dotBack; hitDir = HitDirection::Back; }

		*hitDirection = hitDir;

		return true;
	}

	HitDirection Collision::ConvertVectorToHitDirection(const Vector3& dir)
	{
		if (fabs(dir.x) > fabs(dir.y) && fabs(dir.x) > fabs(dir.z)) {
			return dir.x > 0 ? HitDirection::Right : HitDirection::Left;
		} else if (fabs(dir.y) > fabs(dir.z)) {
			return dir.y > 0 ? HitDirection::Top : HitDirection::Bottom;
		} else {
			return dir.z > 0 ? HitDirection::Front : HitDirection::Back;
		}
	}

	HitDirection Collision::InverseHitDirection(HitDirection hitdirection)
	{
		switch (hitdirection)
		{

		case HitDirection::Top:
			return HitDirection::Bottom;
			break;
		case HitDirection::Bottom:
			return HitDirection::Top;
			break;
		case HitDirection::Left:
			return HitDirection::Right;
			break;
		case HitDirection::Right:
			return HitDirection::Left;
			break;
		case HitDirection::Front:
			return HitDirection::Back;
			break;
		case HitDirection::Back:
			return HitDirection::Front;
			break;
		default: return HitDirection::None;
		}
	}

	HitDirection Collision::GetSelfLocalHitDirection(BaseCollider* self, BaseCollider* other)
	{
		Vector3 toOther = Normalize(other->GetCenterPosition() - self->GetCenterPosition());

		const Matrix4x4& mat = self->GetWorldTransform().matWorld_;
		Vector3 up = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 right = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		struct DirDot {
			HitDirection dir;
			float dot;
		};

		std::vector<DirDot> dots = {
			{ HitDirection::Top,    Dot(toOther, up) },
			{ HitDirection::Bottom, Dot(toOther, up * -1.0f) },
			{ HitDirection::Right,  Dot(toOther, right) },
			{ HitDirection::Left,   Dot(toOther, right * -1.0f) },
			{ HitDirection::Front,  Dot(toOther, forward) },
			{ HitDirection::Back,   Dot(toOther, forward * -1.0f) },
		};

		// 閾値付きで方向分類（優しめ）
		const float threshold = 0.5f; // ≒60度以内なら許容

		for (const auto& d : dots) {
			if (d.dot >= threshold) return d.dir;
		}

		// どれも当てはまらないときは None
		return HitDirection::None;
	}

	HitDirectionBits Collision::GetSelfLocalHitDirectionFlags(BaseCollider* self, BaseCollider* other, float threshold)
	{
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

	HitDirectionBits Collision::GetSelfLocalHitDirectionsSimple(BaseCollider* self, BaseCollider* other)
	{
		HitDirectionBits flags = HitDirection_None;

		Vector3 toOther = Normalize(other->GetCenterPosition() - self->GetCenterPosition());

		const Matrix4x4& mat = self->GetWorldTransform().matWorld_;
		Vector3 up = { mat.m[1][0], mat.m[1][1], mat.m[1][2] };
		Vector3 right = { mat.m[0][0], mat.m[0][1], mat.m[0][2] };
		Vector3 forward = { mat.m[2][0], mat.m[2][1], mat.m[2][2] };

		// 全方向に対して判定
		if (Dot(toOther, up) > 0.0f)         flags |= HitDirection_Top;
		if (Dot(toOther, up * -1.0f) > 0.0f) flags |= HitDirection_Bottom;
		if (Dot(toOther, right) > 0.0f)      flags |= HitDirection_Right;
		if (Dot(toOther, right * -1.0f) > 0.0f) flags |= HitDirection_Left;
		if (Dot(toOther, forward) > 0.0f)    flags |= HitDirection_Front;
		if (Dot(toOther, forward * -1.0f) > 0.0f) flags |= HitDirection_Back;

		return flags;
	}




	void CollisionManager::Initialize() {
		isDrawCollider_ = false;
	}

	void CollisionManager::Update()
	{

		/////カメラ外だったら判定をしない(全て)
		//for (BaseCollider* collider : colliders_) {
		//	if (!collider) continue;
		//	if (!collider->GetIsActive()) continue;

		//	const Camera* cam = collider->camera_;
		//	if (!cam) continue;

		//	Vector3 center = collider->GetCenterPosition();
		//	bool isVisible = IsColliderInView(center, cam);

		//	// ここで無効化するのは当たり判定だけ！
		//	collider->SetCollisionEnabled(isVisible);
		//}


		// 有効なものだけで衝突判定を実行
		CheckAllCollisions();

	}



	void CollisionManager::Reset() {
		// リストを空っぽにする
		colliders_.clear();
		collidingPairs_.clear();
	}

	void CollisionManager::CheckCollisionPair(BaseCollider* a, BaseCollider* b) {
		auto key = std::minmax(a, b);
		bool wasColliding = collidingPairs_.contains(key);
		bool isNowColliding = false;
		HitDirection dirA = HitDirection::None;
		HitDirection dirB = HitDirection::None;

		if (auto aa = dynamic_cast<AABBCollider*>(a)) {
			if (auto ab = dynamic_cast<AABBCollider*>(b)) {
				isNowColliding = Collision::CheckHitDirection(aa->GetAABB(), ab->GetAABB(), &dirA);
				dirB = Collision::InverseHitDirection(dirA);
			} else if (auto ob = dynamic_cast<OBBCollider*>(b)) {
				isNowColliding = Collision::CheckHitDirection(aa->GetAABB(), ob->GetOBB(), &dirA);
				dirB = Collision::GetSelfLocalHitDirection(ob, aa); // ★ これに修正！
			} else {
				isNowColliding = Collision::Check(a, b);
			}
		} else if (auto oa = dynamic_cast<OBBCollider*>(a)) {
			if (auto ab = dynamic_cast<AABBCollider*>(b)) {
				isNowColliding = Collision::CheckHitDirection(ab->GetAABB(), oa->GetOBB(), &dirB);
				dirA = Collision::GetSelfLocalHitDirection(oa, ab); // ★ こっちも逆視点で判定！
			} else if (auto ob = dynamic_cast<OBBCollider*>(b)) {
				isNowColliding = Collision::CheckHitDirection(oa->GetOBB(), ob->GetOBB(), &dirA);
				dirB = Collision::GetSelfLocalHitDirection(ob, oa); // ★ 片方はSAT、もう片方はローカル視点
			} else {
				isNowColliding = Collision::Check(a, b);
			}
		} else {
			isNowColliding = Collision::Check(a, b);
		}

		// イベント処理
		if (isNowColliding) {
			if (!wasColliding) {
				a->CallOnEnterCollision(b);
				b->CallOnEnterCollision(a);
				if (dirA != HitDirection::None || dirB != HitDirection::None) {
					a->CallOnEnterDirectionCollision(b, dirA);
					b->CallOnEnterDirectionCollision(a, dirB);
				}
				collidingPairs_.insert(key);
			}
			a->CallOnCollision(b);
			b->CallOnCollision(a);

			if (dirA != HitDirection::None || dirB != HitDirection::None) {
				a->CallOnDirectionCollision(b, dirA);
				b->CallOnDirectionCollision(a, dirB);
			}
		} else {
			if (wasColliding) {
				a->CallOnExitCollision(b);
				b->CallOnExitCollision(a);
				collidingPairs_.erase(key);
			}
		}
	}


	void CollisionManager::CheckAllCollisions() {

		// リスト内のペアを総当たり
		std::list<BaseCollider*>::iterator itrA = colliders_.begin();
		for (; itrA != colliders_.end(); ++itrA) {
			BaseCollider* colliderA = *itrA;
			// 無効なコライダーはスキップ
			if (colliderA->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kNone)) continue;
			if (!colliderA || !colliderA->GetIsActive() || !colliderA->IsCollisionEnabled()) continue;

			std::list<BaseCollider*>::iterator itrB = itrA;
			itrB++;

			for (; itrB != colliders_.end(); ++itrB) {
				BaseCollider* colliderB = *itrB;
				// 無効なコライダーはスキップ
				if (colliderB->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kNone)) continue;
				if (colliderA == colliderB) continue; // 自分自身との当たり判定はしない
				if (!colliderB || !colliderB->GetIsActive() || !colliderB->IsCollisionEnabled()) continue;

				// ペアの当たり判定
				CheckCollisionPair(colliderA, colliderB);
			}
		}
	}



	bool CollisionManager::IsColliderInView(const Vector3& position, const Camera* camera) {

		// ワールド座標をクリップ空間に変換
		Vector3 clipPos = Transform(position, camera->GetViewProjectionMatrix());

		// 正規化デバイス座標系（NDC）での可視範囲は -1 ~ +1
		return (clipPos.x >= -1.0f && clipPos.x <= 1.0f &&
			clipPos.y >= -1.0f && clipPos.y <= 1.0f &&
			clipPos.z >= 0.0f && clipPos.z <= 1.0f);
	}




	void CollisionManager::AddCollider(BaseCollider* collider) {
		if (!collider) return;
		colliders_.push_back(collider);
		std::cout << "BaseCollider added: " << collider->GetTypeID() << std::endl;
	}

	void CollisionManager::RemoveCollider(BaseCollider* collider)
	{
		if (!collider) return;
		colliders_.remove(collider);
		std::cout << "BaseCollider removed: " << collider->GetTypeID() << std::endl;
	}
}
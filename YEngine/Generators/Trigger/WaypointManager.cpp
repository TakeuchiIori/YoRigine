#include "WaypointManager.h"

#include "Actions/WaypointAction.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshSpawner.h"
#include "Debugger/Logger.h"

WaypointManager* WaypointManager::GetInstance() {
	static WaypointManager instance;
	return &instance;
}

void WaypointManager::Reset() {
	StopBeacon();
	registry_.clear();
	current_    = nullptr;
	currentPos_ = { 0.0f, 0.0f, 0.0f };
}

void WaypointManager::Register(const std::string& name, WaypointAction* wp) {
	if (name.empty() || !wp) return;
	registry_[name] = wp;
}

void WaypointManager::Unregister(WaypointAction* wp) {
	for (auto it = registry_.begin(); it != registry_.end(); ) {
		if (it->second == wp) it = registry_.erase(it);
		else                  ++it;
	}
	if (current_ == wp) {
		StopBeacon();
		current_ = nullptr;
	}
}

void WaypointManager::StopBeacon() {
	if (hasBeacon_) {
		VfxMeshSpawner::GetInstance()->Stop(beaconId_);
		hasBeacon_ = false;
	}
}

void WaypointManager::Activate(const std::string& name) {
	// 今の目的地を止める / 非アクティブ化
	StopBeacon();
	if (current_) current_->SetActive(false);
	current_ = nullptr;

	if (name.empty()) {
		Logger("[Waypoint] 次のウェイポイント無し（全クリア）\n");
		return;
	}

	auto it = registry_.find(name);
	if (it == registry_.end() || !it->second) {
		Logger("[Waypoint] 未解決のウェイポイント名: \"" + name + "\"\n");
		return;
	}

	current_    = it->second;
	current_->SetActive(true);
	currentPos_ = current_->GetWorldPosition();

	// ビーコン(VfxMesh)を目的地に常時再生で出す
	const std::string& fx = current_->GetBeaconEffect();
	if (!fx.empty()) {
		beaconId_  = VfxMeshSpawner::GetInstance()->Spawn(fx, currentPos_, current_->GetBeaconScale(), /*loop*/ true);
		hasBeacon_ = true;
	}
	Logger("[Waypoint] アクティブ化: \"" + name + "\"\n");
}

void WaypointManager::SetBeaconVisible(bool visible) {
	if (visible) {
		if (current_ && !hasBeacon_) {
			const std::string& fx = current_->GetBeaconEffect();
			if (!fx.empty()) {
				beaconId_ = VfxMeshSpawner::GetInstance()->Spawn(fx, currentPos_, current_->GetBeaconScale(), true);
				hasBeacon_ = true;
			}
		}
	}
	else {
		StopBeacon();
	}
}
#include "EnemyAlert.h"
#include "Systems/UI/UIManager.h"


void EnemyAlert::Initialize() {
	alertUI_ = YoRigine::UIManager::GetInstance()->GetUI("enemyAlert");
	if (alertUI_) {
		defaultScale_ = alertUI_->GetScale();
	}
}

void EnemyAlert::Update() {

}

void EnemyAlert::Draw() {
	if (alertUI_) {
		alertUI_->Draw();
	}
}
#include "ParticleEmitter.h"
#ifdef _DEBUG
#include "imgui.h"
#endif
ParticleEmitter::ParticleEmitter(const std::string& name, const Vector3& transform, uint32_t count)
    : emitter_{name, Vector3{transform,}, count, 0.0025f, 0.0f} {}

void ParticleEmitter::Initialize() {

	InitJson();
}

void ParticleEmitter::Update()
{
	emitter_.frequencyTime += deltaTime_;
	if (emitter_.frequency <= emitter_.frequencyTime) {
		// パーティクルを生成してグループに追加
		Emit();
		emitter_.frequencyTime -= emitter_.frequency;
	}

}

void ParticleEmitter::UpdateEmit(const std::string& name, const Vector3& transform, uint32_t count)
{
	ParticleManager::GetInstance()->Emit(name,transform,count);
}

void ParticleEmitter::Emit()
{
	ParticleManager::GetInstance()->Emit(emitter_.name, emitter_.transform, emitter_.count);
}

void ParticleEmitter::ShowImGui()
{
#ifdef _DEBUG
	ImGui::Begin("Particle");

	if(ImGui::Button("Add Particle")) {
		Emit();
	}
	ImGui::End();
#endif // _DEBUG

#ifdef _DEBUG
	ImGui::Begin("Emitter");

	// 周波数の調整 (ボタンで増減)
	ImGui::Text("Frequency:");
	if (ImGui::Button("-##Frequency")) {
		emitter_.frequency -= 0.1f; // 減少
		if (emitter_.frequency < 0.1f) emitter_.frequency = 0.1f; // 最小値を保持
	}
	ImGui::SameLine(); // 同じ行に次の要素を表示
	ImGui::Text("%.2f", emitter_.frequency);
	ImGui::SameLine();
	if (ImGui::Button("+##Frequency")) {
		emitter_.frequency += 0.1f; // 増加
		if (emitter_.frequency > 100.0f) emitter_.frequency = 100.0f; // 最大値を保持
	}

	// 周波数時間の調整 (ボタンで増減)
	ImGui::Text("Frequency Time:");
	if (ImGui::Button("-##FrequencyTime")) {
		emitter_.frequencyTime -= 0.1f; // 減少
		if (emitter_.frequencyTime < 0.0f) emitter_.frequencyTime = 0.0f; // 最小値を保持
	}
	ImGui::SameLine();
	ImGui::Text("%.2f", emitter_.frequencyTime);
	ImGui::SameLine();
	if (ImGui::Button("+##FrequencyTime")) {
		emitter_.frequencyTime += 0.1f; // 増加
		if (emitter_.frequencyTime > 100.0f) emitter_.frequencyTime = 100.0f; // 最大値を保持
	}


	ImGui::End();
#endif // _DEBUG

}

void ParticleEmitter::InitJson() { 
	//jsonManager_ = new JsonManager("パーティクル : " + emitter_.name, "Resources/JSON");
	//jsonManager_->Register("Frequency", &emitter_.frequency);
 //   jsonManager_->Register("FrequencyTime", &emitter_.frequencyTime);
 //   jsonManager_->Register("Count", &emitter_.count);
 //   jsonManager_->Register("Position", &emitter_.transform.translate);
}

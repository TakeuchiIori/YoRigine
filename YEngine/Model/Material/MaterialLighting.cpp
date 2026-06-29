#include "MaterialLighting.h"
#include "DirectXCommon.h"
#include "ToonSettings.h"

void MaterialLighting::Initialize()
{
	resource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(MaterialLight));
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&materialLight_));
	materialLight_->enableLighting = true;
	materialLight_->shininess = 70.0f;
	materialLight_->enableSpecular = false;
	materialLight_->isHalfVector = false;
	materialLight_->enableEnvironment = false;
	materialLight_->environmentCoefficient = 1.0f;
	// トゥーン拡張の既定値（既定では無効＝従来の見た目）
	materialLight_->enableToon = 0;
	materialLight_->toonBands = 3;
	materialLight_->enableRim = 0;
	materialLight_->enableToonSpecular = 0;
	materialLight_->rimPower = 4.0f;
	materialLight_->rimIntensity = 1.0f;
	materialLight_->toonSpecularThreshold = 0.5f;
	materialLight_->toonEdgeSoftness = 0.02f;
	materialLight_->shadowStrength = 1.0f;
	materialLight_->shadowColor = { 0.55f, 0.55f, 0.70f }; // 寒色寄りの影
	materialLight_->highlightColor = { 1.0f, 1.0f, 1.0f };
	materialLight_->_pad0 = materialLight_->_pad1 = materialLight_->_pad2 = 0.0f;
}

void MaterialLighting::RecordDrawCommands(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndexCBV)
{
	// トゥーン関連は全オブジェクト共通のグローバル設定で上書きする
	// （個別ではなく1か所「トゥーン」パネルで統一調整するため）
	if (materialLight_) {
		const ToonSettings::Params& t = ToonSettings::GetInstance()->Get();
		materialLight_->enableToon = t.enableToon;
		materialLight_->toonBands = t.toonBands;
		materialLight_->enableRim = t.enableRim;
		materialLight_->enableToonSpecular = t.enableToonSpecular;
		materialLight_->rimPower = t.rimPower;
		materialLight_->rimIntensity = t.rimIntensity;
		materialLight_->toonSpecularThreshold = t.toonSpecularThreshold;
		materialLight_->toonEdgeSoftness = t.toonEdgeSoftness;
		materialLight_->shadowStrength = t.shadowStrength;
		materialLight_->shadowColor = t.shadowColor;
		materialLight_->highlightColor = t.highlightColor;
	}

	commandList->SetGraphicsRootConstantBufferView(rootParameterIndexCBV, resource_->GetGPUVirtualAddress());
}

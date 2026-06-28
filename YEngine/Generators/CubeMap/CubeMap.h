#pragma once

// Engine
#include "Mesh/MeshPrimitive.h"
#include "Material/MaterialColor.h"
#include "WorldTransform/WorldTransform.h"
#include "DirectXCommon.h"
#include "Systems/Camera/Camera.h"

// Math
#include "Vector4.h"

/// <summary>
/// キューブマップ生成クラス
/// </summary>
namespace YoRigine {
	class CubeMap
	{
	public:
		///************************* 基本関数 *************************///
		void Initialize(Camera* camera, std::string textureFilePath);
		void Update();
		void Draw();
		void SetTextureFilePath(const std::string& filePath);
		WorldTransform wt_;

	private:
		///************************* 内部処理 *************************///
		void CreateMesh();
	public:
		///************************* アクセッサ *************************///

		void SetMaterialColor(const Vector4& color) { materialColor_->SetColor(color); }
		void SetMesh(std::shared_ptr<Mesh> mesh) { mesh_ = std::move(mesh); }

	private:
		///************************* メンバ変数 *************************///

		std::shared_ptr<Mesh> mesh_ = nullptr;
		std::unique_ptr<MaterialColor> materialColor_;
		Camera* camera_ = nullptr;


		std::string textureFilePath_;
	};
}

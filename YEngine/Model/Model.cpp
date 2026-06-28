
// Engine
#include "Model.h"
#include "ModelCommon.h"
#include "SrvManager.h"
#include "Loaders./Texture./TextureManager.h"
#include "Drawer./LineManager/Line.h"
#include "ModelUtils.h"
#include "Systems/GameTime/GameTime.h"
#include "Loaders/Texture/EnvironmentMap.h"
#include <Object3D/Object3dCommon.h>
#include <Debugger/Logger.h>
#include <json.hpp>
#include "Debugger/DebugConsole.h"

// C++
#include <fstream>
#include <assert.h>
#include <sstream>
#include <iostream>


// assimp
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // _DEBUG
#include <PipelineManager/YPipelineManager.h>


// 静的メンバ変数の定義
const std::string Model::binPath = "Resources/Binary/";
std::unordered_map<std::string, Motion> Model::animationCache_;
std::list<std::string> Model::cacheOrder_;
std::unordered_map<std::string, std::list<std::string>::iterator> Model::cacheIterators_;


void Model::Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename, const std::string& animationName, bool isMotion)
{

	isMotion_ = isMotion;
	// 引数から受け取ってメンバ変数に記録する
	modelCommon_ = modelCommon;

	srvManager_ = SrvManager::GetInstance();

	// モデル読み込み
	LoadModelIndexFile(directorypath, filename);

	motionSystem_ = std::make_unique<MotionSystem>();

	// アニメーションをするならtrue
	if (isMotion_) {
		LoadMotionFile(directorypath, filename, animationName);


		if (hasBones_) {
			// 骨の作成
			skeleton_ = std::make_unique<Skeleton>();
			skeleton_->Create(*rootNode_);
			size_t totalVertexCount = 0;
			for (const auto& mesh : meshes_) {
				totalVertexCount += mesh->GetVertexCount();
			}

			skinCluster_->CreateResourceCS(skeleton_->GetJoints().size(), totalVertexCount, skeleton_->GetJointMap());
			std::vector<SkinCluster::Vertex> allVertices;
			for (size_t meshIndex = 0; meshIndex < meshes_.size(); ++meshIndex) {
				const auto& meshData = meshes_[meshIndex]->GetMeshData();
				for (const auto& v : meshData.vertices) {
					SkinCluster::Vertex vertex;
					vertex.position = v.position;
					vertex.normal = v.normal;
					vertex.texcoord = v.texcoord;
					allVertices.push_back(vertex);
				}
			}
			skinCluster_->SetInputVertices(allVertices);



			motionSystem_->Initialize(motion_, *skeleton_, *skinCluster_, rootNode_.get());

		} else {
			motionSystem_->Initialize(motion_, rootNode_.get());
		}
	}
}

void Model::UpdateAnimation()
{
	if (!motionSystem_) return;
	motionSystem_->Update(YoRigine::GameTime::GetDeltaTime());


	if (!motionSystem_->IsFinished()) {
		motionSystem_->Apply();
	}
}

void Model::Draw() {

	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Object");
	auto commandList = modelCommon_->GetDxCommon()->GetCommandList().Get();

	if (hasBones_) {

		YoRigine::DirectXCommon::GetInstance()->TransitionBarrier(
			skinCluster_->GetOutputResource(),
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		skinCluster_->ExecuteSkinningCS();
		commandList->SetPipelineState(
			YPipelineManager::GetInstance()->GetPipeLineStateObject("Object"));
		commandList->SetGraphicsRootSignature(
			YPipelineManager::GetInstance()->GetRootSignature("Object"));

		YoRigine::DirectXCommon::GetInstance()->TransitionBarrier(
			skinCluster_->GetOutputResource(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	} else {
		commandList->SetPipelineState(YPipelineManager::GetInstance()->GetPipeLineStateObject("Object"));
		commandList->SetGraphicsRootSignature(YPipelineManager::GetInstance()->GetRootSignature("Object"));
	}




	/*=================================================================

							 オフセットの計算

	=================================================================*/

	std::vector<size_t> vertexOffsets(meshes_.size());
	size_t accumulatedVertexOffset = 0;
	for (size_t i = 0; i < meshes_.size(); ++i) {
		vertexOffsets[i] = accumulatedVertexOffset;
		accumulatedVertexOffset += meshes_[i]->GetVertexCount();
	}


	/*=================================================================

								DrawCall

	=================================================================*/

	auto shadowHandle = YoRigine::DirectXCommon::GetInstance()->GetShadowDepthGPUHandle();
	commandList->SetGraphicsRootDescriptorTable(indices.at("gShadowMap"), shadowHandle);
	for (size_t i = 0; i < meshes_.size(); ++i) {
		auto& mesh = meshes_[i];
		materials_[mesh->GetMaterialIndex()]->RecordDrawCommands(commandList, indices.at("gMaterialConstant"), indices.at("gTexture"));
		// 環境マップが有効な場合のみバインド
		if (EnvironmentMap::GetInstance()->GetSrvIndex() != UINT32_MAX) {
			auto envHandle = EnvironmentMap::GetInstance()->GetSrvHandle();
			commandList->SetGraphicsRootDescriptorTable(indices.at("gEnvironmentTexture"), envHandle);
		}

		if (mesh->HasBones()) {
			mesh->RecordDrawCommands(commandList, *skinCluster_);
			commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, static_cast<INT>(vertexOffsets[i]), 0);
		} else {
			mesh->RecordDrawCommands(commandList);
			commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
		}

#ifdef USE_IMGUI
		DebugConsole::GetInstance()->RecordDrawCall(mesh->GetIndexCount(), 1);
#endif // _DEBUG
	}
}

void Model::DrawShadow()
{

	/*=================================================================

						 オフセットの計算

	=================================================================*/

	std::vector<size_t> vertexOffsets(meshes_.size());
	size_t accumulatedVertexOffset = 0;
	for (size_t i = 0; i < meshes_.size(); ++i) {
		vertexOffsets[i] = accumulatedVertexOffset;
		accumulatedVertexOffset += meshes_[i]->GetVertexCount();
	}
	auto commandList = modelCommon_->GetDxCommon()->GetCommandList().Get();
	// DrawCalls
	for (size_t i = 0; i < meshes_.size(); ++i)
	{
		auto& mesh = meshes_[i];

		// ボーンあり（Skinning）
		if (mesh->HasBones()) {
			mesh->RecordDrawCommands(commandList, *skinCluster_);
			commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, static_cast<INT>(vertexOffsets[i]), 0);
		} else {
			// ボーンなし
			mesh->RecordDrawCommands(commandList);
			commandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
		}
	}


}

void Model::DrawInstanced(uint32_t instanceCount, D3D12_GPU_DESCRIPTOR_HANDLE instanceSRV)
{
	// Skinning付きはインスタンス対応外 (CSスキニングが per-object 前提のため)
	assert(!hasBones_ && "Model::DrawInstanced does not support skinned meshes");

	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("ObjectInstanced");
	auto commandList = modelCommon_->GetDxCommon()->GetCommandList().Get();

	// PSO + RS は呼び出し側 (InstancedObject3d::Flush) でセット済み

	// Per-instance SRV
	commandList->SetGraphicsRootDescriptorTable(indices.at("gInstances"), instanceSRV);

	// シャドウマップ
	auto shadowHandle = YoRigine::DirectXCommon::GetInstance()->GetShadowDepthGPUHandle();
	commandList->SetGraphicsRootDescriptorTable(indices.at("gShadowMap"), shadowHandle);

	// メッシュごとのマテリアル + DrawIndexedInstanced
	for (size_t i = 0; i < meshes_.size(); ++i) {
		auto& mesh = meshes_[i];
		materials_[mesh->GetMaterialIndex()]->RecordDrawCommands(
			commandList, indices.at("gMaterialConstant"), indices.at("gTexture"));

		if (EnvironmentMap::GetInstance()->GetSrvIndex() != UINT32_MAX) {
			auto envHandle = EnvironmentMap::GetInstance()->GetSrvHandle();
			commandList->SetGraphicsRootDescriptorTable(indices.at("gEnvironmentTexture"), envHandle);
		}

		mesh->RecordDrawCommands(commandList);
		commandList->DrawIndexedInstanced(mesh->GetIndexCount(), instanceCount, 0, 0, 0);

#ifdef USE_IMGUI
		DebugConsole::GetInstance()->RecordDrawCall(mesh->GetIndexCount(), instanceCount);
#endif
	}
}

void Model::DrawShadowInstanced(uint32_t instanceCount, D3D12_GPU_DESCRIPTOR_HANDLE instanceSRV)
{
	assert(!hasBones_ && "Model::DrawShadowInstanced does not support skinned meshes");

	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("ShadowMapInstanced");
	auto commandList = modelCommon_->GetDxCommon()->GetCommandList().Get();

	// PSO + RS + gLight は呼び出し側でセット済み
	commandList->SetGraphicsRootDescriptorTable(indices.at("gInstances"), instanceSRV);

	for (size_t i = 0; i < meshes_.size(); ++i) {
		auto& mesh = meshes_[i];
		mesh->RecordDrawCommands(commandList);
		commandList->DrawIndexedInstanced(mesh->GetIndexCount(), instanceCount, 0, 0, 0);
	}
}



void Model::LoadModelIndexFile(const std::string& directoryPath, const std::string& filename)
{
	// ファイル読み込み
	Assimp::Importer importer;
	std::string filePath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);
	assert(scene->HasMeshes());
	LoadNode(scene);
	hasBones_ = HasBones(scene);
	LoadMesh(scene);
	LoadMaterial(scene, directoryPath);
	if (hasBones_) {
		LoadSkinCluster(scene);
	}
}

void Model::LoadMotionFile(const std::string& directoryPath, const std::string& filename, const std::string& animationName) {
	std::string fullPath = directoryPath + "/" + filename;

	// キャッシュキー（GLTF + アニメ名）
	std::string cacheKey = fullPath + "#" + animationName;
	if (animationCache_.contains(cacheKey)) {
		motion_ = animationCache_.at(cacheKey);
		AddToCache(cacheKey, motion_);
		return;
	}

	// バイナリの保存パス（新方式：個別アニメファイル）
	std::filesystem::path fileStem = std::filesystem::path(filename).stem(); // 例: "Player"
	std::string binFile = binPath + fileStem.string() + "_" + animationName + ".anim";

	// バイナリが存在していればそれを読み込む
	if (std::filesystem::exists(binFile)) {
		motion_ = motion_.LoadBinary(binFile);
		animationCache_[cacheKey] = motion_;
		return;
	}

	// ここまで来たらGLTFから生成（初回 or バイナリ無し）
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(fullPath.c_str(), 0);
	if (!scene || scene->mNumAnimations == 0) {
		throw std::runtime_error("アニメーション読み込み失敗: " + fullPath);
	}

	motion_ = Motion::LoadFromScene(scene, fullPath, animationName);

	// 安全なファイル名（バイナリ保存）
	motion_.SaveBinary(motion_, animationName, binPath + fileStem.string());

	animationCache_[cacheKey] = motion_;
}

void Model::AddToCache(const std::string& key, const Motion& motion) {
	// キャッシュサイズが上限に達している場合、古いものを削除
	while (animationCache_.size() >= MAX_CACHE_SIZE) {
		std::string oldestKey = cacheOrder_.back();
		cacheOrder_.pop_back();
		cacheIterators_.erase(oldestKey);
		animationCache_.erase(oldestKey);

		std::cout << "Cache evicted: " << oldestKey << std::endl;
	}

	// 新しいエントリを追加
	animationCache_[key] = motion;
	cacheOrder_.push_front(key);
	cacheIterators_[key] = cacheOrder_.begin();
}

// キャッシュクリア用の静的メソッドも追加
void Model::ClearAnimationCache() {
	animationCache_.clear();
	cacheOrder_.clear();
	cacheIterators_.clear();
	std::cout << "Animation cache cleared" << std::endl;
}

// デバッグ用：現在のキャッシュサイズを取得
size_t Model::GetCacheSize() {
	return animationCache_.size();
}


bool Model::HasBones(const aiScene* scene)
{
	// メッシュがなければボーンもない
	if (!scene->HasMeshes()) {
		return false;
	}

	// 各メッシュをチェック
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		aiMesh* mesh = scene->mMeshes[meshIndex];

		// ボーン数が0でなければボーンがあると判断
		if (mesh->mNumBones > 0) {
			return true;
		}
	}

	// どのメッシュにもボーンが含まれていない場合
	return false;
}

void Model::SetChangeMotion(const std::string& directoryPath, const std::string& filename, MotionPlayMode playMode, const std::string& animationName)
{
	// 既存のアニメーションと同じ場合はスキップ
	std::string newCacheKey = directoryPath + "/" + filename + "#" + animationName;
	static std::string currentCacheKey;

	if (currentCacheKey == newCacheKey) {
		// 同じアニメーションの場合は再生モードだけ変更
		if (motionSystem_) {
			motionSystem_->SetPlayMode(playMode);
		}
		return;
	}

	currentCacheKey = newCacheKey;

	// 新しいアニメーションファイルを読み込む
	LoadMotionFile(directoryPath, filename, animationName);

	if (hasBones_) {
		// 再生状態はブレンド後にセット
		motionSystem_->StartBlend(motion_, 0.25f);
		motionSystem_->SetPlayMode(playMode);
	} else {
		motionSystem_->Initialize(motion_, rootNode_.get());
		motionSystem_->SetPlayMode(playMode);
	}
}

void Model::PlayUpperMotion(const std::string& directoryPath, const std::string& filename, MotionPlayMode playMode, const std::string& animationName)
{
	if (!motionSystem_) return;

	// アニメーションデータをロード（すでにキャッシュにあればそれを取得）
	LoadMotionFile(directoryPath, filename, animationName);

	std::string cacheKey = directoryPath + "/" + filename + "#" + animationName;
	auto it = animationCache_.find(cacheKey);
	if (it != animationCache_.end()) {
		motionSystem_->PlayUpperAnimation(&(it->second), playMode);
	}
}

void Model::DrawBone(Line& line, const Matrix4x4& worldMatrix)
{
	if (skeleton_) {
		skeleton_->Draw(line, worldMatrix);
	}
}


void Model::PlayOnce()
{
	if (motionSystem_)
		motionSystem_->PlayOnce();
}

void Model::PlayLoop()
{
	if (motionSystem_)
		motionSystem_->PlayLoop();
}

void Model::Stop()
{
	if (motionSystem_)
		motionSystem_->Stop();
}

void Model::Resume()
{
	if (motionSystem_)
		motionSystem_->Resume();
}

void Model::DebugInfo()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader((name_ + "のデバッグ表示").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {

		ImGui::Text("メッシュ数: %d", static_cast<int>(meshes_.size()));
		ImGui::Text("マテリアル数: %d", static_cast<int>(materials_.size()));

		if (skeleton_) {
			if (ImGui::TreeNode("骨")) {
				ImGui::Text("関節数: %d", static_cast<int>(skeleton_->GetJoints().size()));

				if (ImGui::TreeNode("関節名")) {
					for (const auto& name : skeleton_->GetAllJointNames()) {
						ImGui::BulletText("%s", name.c_str());
					}
					ImGui::TreePop();
				}

				ImGui::TreePop(); // Skeleton
			}
		}
	}
#endif // _DEBUG


}

void Model::LoadMesh(const aiScene* scene)
{
	meshes_.resize(scene->mNumMeshes); // メッシュ数分のメモリを確保
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {

		aiMesh* mesh = scene->mMeshes[meshIndex];
		assert(mesh->HasNormals());
		std::unique_ptr<Mesh> meshs = std::make_unique<Mesh>();
		// 初期化（リソース確保、転送など）
		meshs->Initialize();

		if (mesh->mNumBones > 0) {
			meshs->SetHasBones(true);
		}

		// メッシュデータの取得と頂点バッファのサイズ設定
		auto& meshData = meshs->GetMeshData();
		meshData.vertices.resize(mesh->mNumVertices);

		// 頂点データの設定
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];

			// 右手系->左手系への変換を考慮して頂点データを設定
			meshData.vertices[vertexIndex].position = { -position.x, position.y, position.z, 1.0f };
			meshData.vertices[vertexIndex].normal = { -normal.x, normal.y, normal.z };
			// テクスチャ座標があるか確認
			if (mesh->HasTextureCoords(0)) {
				aiVector3D& texcoords = mesh->mTextureCoords[0][vertexIndex];
				meshData.vertices[vertexIndex].texcoord = { texcoords.x, texcoords.y };
			} else {
				// 無い場合は(0,0)で初期化
				meshData.vertices[vertexIndex].texcoord = { 0.0f, 0.0f };
			}
		}

		// インデックスデータの設定
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			if (face.mNumIndices == 4) {
				// 四角形の場合、2つの三角形に分割
				uint32_t i0 = face.mIndices[0];
				uint32_t i1 = face.mIndices[1];
				uint32_t i2 = face.mIndices[2];
				uint32_t i3 = face.mIndices[3];

				// 1つ目の三角形
				meshData.indices.push_back(i0);
				meshData.indices.push_back(i1);
				meshData.indices.push_back(i2);

				// 2つ目の三角形
				meshData.indices.push_back(i0);
				meshData.indices.push_back(i2);
				meshData.indices.push_back(i3);
			} else if (face.mNumIndices == 3) {
				// 三角形の場合はそのまま追加
				for (uint32_t element = 0; element < face.mNumIndices; ++element) {
					meshData.indices.push_back(face.mIndices[element]);
				}
			}
		}

		// マテリアルインデックスを設定
		meshData.materialIndex = mesh->mMaterialIndex;

		meshs->SetMaterialIndex(mesh->mMaterialIndex);
		meshs->TransferData();

		meshes_[meshIndex] = std::move(meshs); // メッシュを格納
	}
}

void Model::LoadMaterial(const aiScene* scene, std::string directoryPath)
{
	materials_.resize(scene->mNumMaterials);

	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		aiMaterial* materialSrc = scene->mMaterials[materialIndex];

		aiString textureFilePath;
		std::string fullPath;

		bool hasTexture = false;
		if (materialSrc->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			materialSrc->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);
			fullPath = directoryPath + "/" + textureFilePath.C_Str(); // スペース消して正しいパス連結
			hasTexture = true;

		}

		materials_[materialIndex] = std::make_unique<Material>();
		Material& material = *materials_[materialIndex];
		aiColor3D color;

		if (materialSrc->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
			material.SetKd({ color.r, color.g, color.b });
		}

		if (materialSrc->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
			material.SetKa({ color.r, color.g, color.b });
		}

		if (materialSrc->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
			material.SetKs({ color.r, color.g, color.b });
		}

		float refractionIndex = 1.0f;
		if (materialSrc->Get(AI_MATKEY_REFRACTI, refractionIndex) == AI_SUCCESS) {
			material.SetNi(refractionIndex);
		}

		unsigned int illumModel = 0;
		if (materialSrc->Get(AI_MATKEY_SHADING_MODEL, illumModel) == AI_SUCCESS) {
			material.SetIllum(illumModel);
		}

		if (hasTexture) {
			material.Initialize(fullPath);
		} else {
			fullPath = "Resources/images/white.png"; // テクスチャが無い場合は白を指定
			material.Initialize(fullPath);
		}
	}
}

void Model::LoadSkinCluster(const aiScene* scene)
{
	skinCluster_ = std::make_unique<SkinCluster>();
	skinCluster_->LoadFromScene(scene);
}

void Model::LoadNode(const aiScene* scene)
{
	rootNode_ = std::make_unique<Node>(Node::ReadNode(scene->mRootNode));
}
void Model::ApplyNodeTransform(const aiScene* scene, const aiNode* node, const Matrix4x4& parentMatrix) {
	Matrix4x4 local = ConvertMatrix(node->mTransformation);
	Matrix4x4 world = MultiplyMatrix(parentMatrix, local);

	for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
		uint32_t meshIndex = node->mMeshes[i];
		if (meshIndex < meshes_.size()) {
			meshes_[meshIndex]->SetWorldMatrix(world);
		}
	}

	for (uint32_t i = 0; i < node->mNumChildren; ++i) {
		ApplyNodeTransform(scene, node->mChildren[i], world);
	}
}



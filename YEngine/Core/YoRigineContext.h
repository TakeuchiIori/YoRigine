#pragma once

// Engine
namespace YoRigine
{
	class DirectXCommon;
	class Input;
	class Audio;
	class CollisionManager;
	class LightManager;
	class ParticleManager;
}

class TextureManager;
class ModelManager;
class LineManager;
class ObjectManager;
class SceneManager;
class PostEffectManager;


struct YoRigineContext
{
	//	エンジン側の各種システム
	YoRigine::DirectXCommon* dxCommon = nullptr;
	YoRigine::Input* input = nullptr;
	YoRigine::Audio* audio = nullptr;
	TextureManager* textureManager = nullptr;
	ModelManager* modelManager = nullptr;
	YoRigine::CollisionManager* collisionManager = nullptr;
	YoRigine::LightManager* lightManager = nullptr;
	LineManager* lineManager = nullptr;
	ObjectManager* objectManager = nullptr;


	// ゲーム側の各種システム
	SceneManager* sceneManager = nullptr;
	PostEffectManager* postEffectManager = nullptr;
	YoRigine::ParticleManager* particleManager = nullptr;

};


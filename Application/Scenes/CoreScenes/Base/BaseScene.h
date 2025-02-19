#pragma once

// 前方宣言
class SceneManager;
// 基底クラス
class BaseScene
{
public: // 純粋仮想関数
	/// <summary>
	/// 初期化
	/// </summary>
	virtual void Initialize() = 0;

	/// <summary>
	/// 終了
	/// </summary>
	virtual void Finalize() = 0;

	/// <summary>
	/// 更新
	/// </summary>
	virtual void Update() = 0;

	/// <summary>
	/// 描画
	/// </summary>
	virtual void Draw() = 0;
	
	/// <summary>
	/// デストラクタ
	/// </summary>
	virtual ~BaseScene() = default;

public: // アクセッサ

	virtual void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

protected: // メンバ関数
	SceneManager* sceneManager_ = nullptr;
};


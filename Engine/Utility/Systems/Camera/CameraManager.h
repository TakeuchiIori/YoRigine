#pragma once
#include "Camera.h"

// C++
#include <vector>
#include <memory>

class CameraManager
{
public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    CameraManager() = default;

    /// <summary>
    /// 新しいカメラを追加
    /// </summary>
    /// <returns>追加されたカメラのポインタ</returns>
    std::shared_ptr<Camera> AddCamera();

    /// <summary>
    /// カメラを削除
    /// </summary>
    /// <param name="camera">削除するカメラのポインタ</param>
    void RemoveCamera(std::shared_ptr<Camera> camera);

    /// <summary>
    /// 現在使用中のカメラを設定
    /// </summary>
    /// <param name="camera">設定するカメラのポインタ</param>
    void SetCurrentCamera(std::shared_ptr<Camera> camera);

    /// <summary>
    /// 現在のカメラを取得
    /// </summary>
    /// <returns>現在のカメラのポインタ</returns>
    std::shared_ptr<Camera> GetCurrentCamera() const;

    /// <summary>
    /// すべてのカメラを更新
    /// </summary>
    void UpdateAllCameras();

private:
    std::vector<std::shared_ptr<Camera>> cameras_;  // カメラのリスト
    std::shared_ptr<Camera> currentCamera_;         // 現在のカメラ
};



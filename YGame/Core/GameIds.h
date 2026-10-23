#pragma once

// ============================================================
// ゲーム内で文字列キーとして使う ID の一覧
// 綴りを間違えても黙って失敗する種類の文字列なので、ここ以外に直書きしない。
// （JSON ファイル名など、保存データのキーになっているものは対象外）
// ============================================================

/// メインシーン名（SceneFactory / SceneManager::ChangeScene / Editor の表示先）
namespace SceneId {
inline constexpr const char *Title = "Title";
inline constexpr const char *Game = "Game";
inline constexpr const char *Clear = "Clear";
inline constexpr const char *Develop = "Develop";
} // namespace SceneId

/// GameScene 内のサブシーン名（SubSceneManager の登録キー）
namespace SubSceneId {
inline constexpr const char *Field = "Field";
inline constexpr const char *Battle = "Battle";
} // namespace SubSceneId

/// CameraDirector に登録される VirtualCamera 名
/// （デバッグカメラは全シーン共通なので SceneCameraRig::kDebugCameraId を使う）
namespace CameraId {
inline constexpr const char *PlayerFollow = "PlayerFollow";
inline constexpr const char *ClearCinematic = "ClearCinematic";
inline constexpr const char *TitleCamera = "TitleCamera";
inline constexpr const char *ClearCamera = "ClearCamera";
} // namespace CameraId

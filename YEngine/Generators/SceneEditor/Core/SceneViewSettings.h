#pragma once

namespace YoRigine {

/// <summary>
/// シーンエディタのビューポート表示設定。
///
/// 以前は SceneEditor がフラグを個別に持ち、UI 側へ bool*
/// を1つずつ渡していた。 設定が増えるたびに Set〜Flag()
/// が増える構造だったので、1 つの構造体に集約して 参照で渡すだけにしている。
/// </summary>
struct SceneViewSettings {
  ///--- デバッグ描画 -------------------------------------------------------
  bool showCollider = true;               // コライダー形状を線で表示する
  bool showColliderSelectedOnly = false;  // 選択中のオブジェクトだけ表示する
  bool showBroadPhaseGrid = false;        // BroadPhase グリッドを可視化する
  float broadPhaseGridDrawRadius = 30.0f; // カメラからの可視化半径

  ///--- カリング -----------------------------------------------------------
  bool enableDrawFrustumCulling = true; // 描画時に視錐台カリングする
  // コライダーが無いオブジェクトの外接サイズを scale から推定する係数
  float drawBoundsScaleFactor = 2.0f;

  ///--- 選択表示 -----------------------------------------------------------
  bool showSelectionOutline = true; // 選択中を橙色の外接ボックスで囲む
};

} // namespace YoRigine

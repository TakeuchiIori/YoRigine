#pragma once

#ifdef USE_IMGUI

// C++
#include <vector>
#include <stack>
#include <string>
#include <functional>

// Math
#include "Vector3.h"
#include "Matrix4x4.h"
#include "Ray/Raycast.h"

// ImGui
#include "imgui.h"
#include "ImGuizmo.h"

// Interface
#include "IGizmable.h"

// 前方宣言
namespace YoRigine { class Camera; }

/// <summary>
/// 汎用ギズモコントローラー
///
/// ■ 汎用化の要点
///   - 操作対象は IGizmable* だけ。YParticleEmitter / YEmitterGroup /
///     任意のゲームオブジェクトを一切 #include せずに操作できる。
///   - 複数選択（multi-select）は IGizmable* のリストで受け取る。
///   - Undo/Redo は GizmoSnapshot（位置・回転・スケールの値コピー）で管理。
///
/// ■ クリック選択
///   - TryPickObject() にビューポートのマウス座標とカメラを渡すと、
///     登録済みオブジェクトの中から最も近い IGizmable を返す。
///   - ヒット判定は IGizmable::GetGizmoPickRadius() の球で行う。
/// </summary>
class GizmoController
{
public:
    //=================================================================
    // 列挙型
    //=================================================================

    enum class Operation { Translate, Rotate, Scale };
    enum class Mode { World, Local };

    //=================================================================
    // スナップショット（Undo/Redo 用）
    //=================================================================

    struct GizmoSnapshot
    {
        IGizmable* target = nullptr;
        Vector3    position = { 0,0,0 };
        Vector3    rotation = { 0,0,0 };
        Vector3    scale = { 1,1,1 };
    };

    //=================================================================
    // 設定
    //=================================================================

    struct Settings
    {
        Operation operation = Operation::Translate;
        Mode      mode = Mode::World;
        bool      useSnap = false;
        Vector3   snapValues = { 1.0f, 1.0f, 1.0f };
        float     rotationSnapDegrees = 15.0f;
    };

public:
    //=================================================================
    // 基本
    //=================================================================

    GizmoController() = default;
    ~GizmoController() = default;

    void Initialize();

    //=================================================================
    // ギズモ描画
    //=================================================================

    /// <summary>
    /// 【メインエントリポイント】呼び出し側はこれだけ呼べばよい。
    /// targets を受け取り、以下を一括処理する。
    ///   - 単一 / 複数の自動振り分け
    ///   - 操作開始時の Undo スナップショット記録
    ///   - Ctrl+Z / Ctrl+Y ショートカット, T/R/S 操作切替
    /// @return 操作が行われた場合 true
    /// </summary>
    bool Draw(
        YoRigine::Camera* camera,
        std::vector<IGizmable*>& targets,
        const ImVec2& viewportPos,
        const ImVec2& viewportSize);

    //=================================================================
    // 設定 UI
    //=================================================================

    void DrawSettings();
    void HandleShortcuts();

    //=================================================================
    // ピッキング（クリック選択）
    //=================================================================

    /// <summary>
    /// ビューポート上のマウス位置からレイキャストし、
    /// 登録オブジェクト（pickables_）の中で最も近いものを返す。
    /// ヒットしなければ nullptr を返す。
    /// </summary>
    /// <param name="mouseScreenPos">ImGui座標系でのマウス位置</param>
    /// <param name="viewportPos">ビューポートの左上座標</param>
    /// <param name="viewportSize">ビューポートのサイズ</param>
    /// <param name="camera">レイ生成に使うカメラ</param>
    IGizmable* TryPickObject(
        const ImVec2& mouseScreenPos,
        const ImVec2& viewportPos,
        const ImVec2& viewportSize,
        YoRigine::Camera* camera);

    /// ピッキング対象として登録
    void RegisterPickable(IGizmable* obj);
    void UnregisterPickable(IGizmable* obj);
    void ClearPickables();

    //=================================================================
    // Undo / Redo
    //=================================================================

    /// 操作前に呼ぶ（単一）
    void PushSnapshot(IGizmable* target);
    bool Undo();
    bool Redo();
    void ClearHistory();

    /// 複数選択用
    void PushSnapshotMulti(const std::vector<IGizmable*>& targets);
    bool UndoMulti(std::vector<IGizmable*>& targets);
    bool RedoMulti(std::vector<IGizmable*>& targets);

    //=================================================================
    // アクセッサ
    //=================================================================

    void SetOperation(Operation op) { settings_.operation = op; }
    void SetMode(Mode m) { settings_.mode = m; }
    void SetUseSnap(bool use) { settings_.useSnap = use; }
    void SetSnapValues(const Vector3& snap) { settings_.snapValues = snap; }
    void SetRotationSnap(float deg) { settings_.rotationSnapDegrees = deg; }
	void SetSelectedTarget(IGizmable* target) { selectedTarget_ = target; }

    Operation GetOperation()  const { return settings_.operation; }
    Mode      GetMode()       const { return settings_.mode; }
    bool      IsUsingSnap()   const { return settings_.useSnap; }
    Vector3   GetSnapValues() const { return settings_.snapValues; }
    float     GetRotationSnap()const { return settings_.rotationSnapDegrees; }

    bool IsUsing() const;
    bool IsOver()  const;

    bool CanUndo()      const { return !undoStack_.empty(); }
    bool CanRedo()      const { return !redoStack_.empty(); }
    bool CanUndoMulti() const { return !undoStackMulti_.empty(); }
    bool CanRedoMulti() const { return !redoStackMulti_.empty(); }

private:
    //=================================================================
    // 内部処理
    //=================================================================

    // IGizmable から Matrix4x4 を組み立て・分解
    Matrix4x4 BuildWorldMatrix(const IGizmable* target) const;
    void      ApplyWorldMatrix(const Matrix4x4& mat, IGizmable* target) const;

    // ギズモ描画（内部）
    bool DrawSingle(YoRigine::Camera* camera, IGizmable* target,
        const ImVec2& viewportPos, const ImVec2& viewportSize);
    bool DrawMulti(YoRigine::Camera* camera, std::vector<IGizmable*>& targets,
        const ImVec2& viewportPos, const ImVec2& viewportSize);

    // ImGuizmo ↔ Matrix4x4 変換
    void      MatrixToImGuizmo(const Matrix4x4& m, float* out) const;
    Matrix4x4 ImGuizmoToMatrix(const float* in) const;

    // 行列分解（ImGuizmo::DecomposeMatrixToComponents を利用）
    void DecomposeMatrix(const Matrix4x4& mat,
        Vector3& outPos, Vector3& outRot, Vector3& outScale) const;

    // 単位変換
    float DegToRad(float d) const { return d * (3.14159265359f / 180.0f); }
    float RadToDeg(float r) const { return r * (180.0f / 3.14159265359f); }

    // レイ生成

    Ray BuildPickRay(
        const ImVec2& mouseScreenPos,
        const ImVec2& viewportPos,
        const ImVec2& viewportSize,
        YoRigine::Camera* camera) const;

    // 球とレイの交差判定
    bool IntersectRaySphere(
        const Ray& ray,
        const Vector3& center,
        float          radius,
        float& outT) const;

    //=================================================================
    // メンバ変数
    //=================================================================

    Settings settings_;
    bool     isInitialized_ = false;

    // Undo スナップショットのタイミング管理（操作開始の立ち上がりを検出）
    bool wasUsing_ = false;
    bool wasUsingMulti_ = false;

    // ピッキング対象リスト
    std::vector<IGizmable*> pickables_;

    // Undo/Redo（単一）
    static constexpr size_t MAX_HISTORY = 100;
    std::stack<GizmoSnapshot>              undoStack_;
    std::stack<GizmoSnapshot>              redoStack_;

    // Undo/Redo（複数選択）
    std::stack<std::vector<GizmoSnapshot>> undoStackMulti_;
    std::stack<std::vector<GizmoSnapshot>> redoStackMulti_;

    // どのオブジェクトを選択中か保持する
    IGizmable* selectedTarget_ = nullptr;
};

#endif // USE_IMGUI
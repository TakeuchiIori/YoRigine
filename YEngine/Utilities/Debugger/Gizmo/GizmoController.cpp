#ifdef USE_IMGUI

#include "GizmoController.h"
#include "Systems/Camera/Camera.h"
#include <cmath>
#include <algorithm>

//=================================================================
// 初期化
//=================================================================

void GizmoController::Initialize()
{
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    isInitialized_ = true;
}

//=================================================================
// DrawSingle（単一オブジェクト・内部処理用）
//=================================================================

bool GizmoController::DrawSingle(
    YoRigine::Camera* camera,
    IGizmable* target,
    const ImVec2& viewportPos,
    const ImVec2& viewportSize)
{
    if (!isInitialized_ || !camera || !target) return false;

    // ---- 行列準備 ------------------------------------------------
    Matrix4x4 worldMatrix = BuildWorldMatrix(target);

    float view[16], proj[16], model[16];
    MatrixToImGuizmo(camera->GetViewMatrix(), view);
    MatrixToImGuizmo(camera->GetProjectionMatrix(), proj);
    MatrixToImGuizmo(worldMatrix, model);

    // ---- ImGuizmo 設定 -------------------------------------------
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);
    ImGuizmo::SetDrawlist();

    // 操作モード変換
    ImGuizmo::OPERATION op;
    switch (settings_.operation)
    {
    case Operation::Translate: op = ImGuizmo::TRANSLATE; break;
    case Operation::Rotate:    op = ImGuizmo::ROTATE;    break;
    case Operation::Scale:     op = ImGuizmo::SCALE;     break;
    default:                   op = ImGuizmo::TRANSLATE; break;
    }

    ImGuizmo::MODE mode =
        (settings_.mode == Mode::World) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

    // スナップ
    float  snapRad = DegToRad(settings_.rotationSnapDegrees);
    float  snapArr[3] = { settings_.snapValues.x,
                          settings_.snapValues.y,
                          settings_.snapValues.z };
    float* snap = nullptr;
    if (settings_.useSnap)
        snap = (settings_.operation == Operation::Rotate) ? &snapRad : snapArr;

    // ---- 描画 ----------------------------------------------------
    ImGuizmo::PushID((int)(intptr_t)target);
    bool manipulated = ImGuizmo::Manipulate(view, proj, op, mode, model,
        nullptr, snap);
    ImGuizmo::PopID();

    // ---- 操作結果を即時反映 --------------------------------------
    if (manipulated)
    {
        Matrix4x4 result = ImGuizmoToMatrix(model);
        ApplyWorldMatrix(result, target);
        return true;
    }

    return false;
}

//=================================================================
// DrawMulti（複数選択・内部処理用）
//=================================================================

bool GizmoController::DrawMulti(
    YoRigine::Camera* camera,
    std::vector<IGizmable*>& targets,
    const ImVec2& viewportPos,
    const ImVec2& viewportSize)
{
    if (!isInitialized_ || !camera || targets.empty()) return false;

    // 全オブジェクトの中心位置を計算
    Vector3 center = { 0,0,0 };
    for (auto* t : targets)
    {
        auto p = t->GetGizmoPosition();
        center.x += p.x;
        center.y += p.y;
        center.z += p.z;
    }
    float n = static_cast<float>(targets.size());
    center.x /= n;  center.y /= n;  center.z /= n;

    // 中心にダミーの IGizmable を作って DrawGizmo に渡す
    // 位置・回転・スケールはすべて中心値で初期化
    struct CenterProxy : IGizmable
    {
        Vector3 pos, rot, scl;
        std::string GetGizmoLabel()      const override { return "__center__"; }
        Vector3 GetGizmoPosition()       const override { return pos; }
        void    SetGizmoPosition(const Vector3& p) override { pos = p; }
        Vector3 GetGizmoRotation()       const override { return rot; }
        void    SetGizmoRotation(const Vector3& r) override { rot = r; }
        Vector3 GetGizmoScale()          const override { return scl; }
        void    SetGizmoScale(const Vector3& s)    override { scl = s; }
    };

    CenterProxy proxy;
    proxy.pos = center;
    proxy.rot = targets[0]->GetGizmoRotation();
    proxy.scl = targets[0]->GetGizmoScale();

    Vector3 oldCenter = center;
    bool manipulated = DrawSingle(camera, &proxy, viewportPos, viewportSize);

    if (manipulated)
    {
        // 移動差分を全オブジェクトに適用
        Vector3 delta = {
            proxy.pos.x - oldCenter.x,
            proxy.pos.y - oldCenter.y,
            proxy.pos.z - oldCenter.z
        };

        for (auto* t : targets)
        {
            auto p = t->GetGizmoPosition();
            t->SetGizmoPosition({ p.x + delta.x,
                                   p.y + delta.y,
                                   p.z + delta.z });

            if (settings_.operation == Operation::Rotate)
                t->SetGizmoRotation(proxy.rot);
            else if (settings_.operation == Operation::Scale)
                t->SetGizmoScale(proxy.scl);

            // 操作終了通知（JSON同期など）
            t->OnGizmoManipulationEnd();
        }
        return true;
    }

    return false;
}

//=================================================================
// Draw（統合エントリポイント）
//=================================================================

bool GizmoController::Draw(
    YoRigine::Camera* camera,
    std::vector<IGizmable*>& targets,
    const ImVec2& viewportPos,
    const ImVec2& viewportSize)
{
    if (!isInitialized_ || !camera || targets.empty()) return false;

    HandleShortcuts();

    const bool nowUsing = IsUsing();
    bool manipulated = false;

    if (targets.size() == 1)
    {
        // 操作開始の立ち上がり（false→true）でのみスナップショットを積む
        if (!wasUsing_ && nowUsing)
            PushSnapshot(targets[0]);

        manipulated = DrawSingle(camera, targets[0], viewportPos, viewportSize);
        wasUsing_ = nowUsing;
    }
    else
    {
        if (!wasUsingMulti_ && nowUsing)
            PushSnapshotMulti(targets);

        manipulated = DrawMulti(camera, targets, viewportPos, viewportSize);
        wasUsingMulti_ = nowUsing;
    }

    // Undo / Redo ショートカット
    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))
        {
            if (targets.size() == 1) Undo();
            else                     UndoMulti(targets);
        }
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))
        {
            if (targets.size() == 1) Redo();
            else                     RedoMulti(targets);
        }
    }

    return manipulated;
}

//=================================================================
// 設定 UI
//=================================================================

void GizmoController::DrawSettings()
{
    if (ImGui::CollapsingHeader("Gizmo Settings"))
    {
        // 操作モード
        ImGui::Text("Operation");
        if (ImGui::RadioButton("Translate##Op",
            settings_.operation == Operation::Translate))
            settings_.operation = Operation::Translate;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate##Op",
            settings_.operation == Operation::Rotate))
            settings_.operation = Operation::Rotate;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale##Op",
            settings_.operation == Operation::Scale))
            settings_.operation = Operation::Scale;

        ImGui::Separator();

        // 座標系
        ImGui::Text("Space");
        if (ImGui::RadioButton("World##Sp", settings_.mode == Mode::World))
            settings_.mode = Mode::World;
        ImGui::SameLine();
        if (ImGui::RadioButton("Local##Sp", settings_.mode == Mode::Local))
            settings_.mode = Mode::Local;

        ImGui::Separator();

        // スナップ
        ImGui::Checkbox("Snap", &settings_.useSnap);
        if (settings_.useSnap)
        {
            if (settings_.operation == Operation::Rotate)
                ImGui::DragFloat("Snap Angle", &settings_.rotationSnapDegrees,
                    0.1f, 0.1f, 45.0f, "%.1f deg");
            else
                ImGui::DragFloat3("Snap XYZ", &settings_.snapValues.x,
                    0.1f, 0.1f, 10.0f);
        }

        ImGui::Separator();
        ImGui::TextDisabled("Shortcuts: T / R / S");
    }
}

void GizmoController::HandleShortcuts()
{
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (ImGui::IsKeyPressed(ImGuiKey_T)) settings_.operation = Operation::Translate;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) settings_.operation = Operation::Rotate;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) settings_.operation = Operation::Scale;
}

//=================================================================
// ピッキング（レイキャスト）
//=================================================================

void GizmoController::RegisterPickable(IGizmable* obj)
{
    if (obj &&
        std::find(pickables_.begin(), pickables_.end(), obj) == pickables_.end())
        pickables_.push_back(obj);
}

void GizmoController::UnregisterPickable(IGizmable* obj)
{
    pickables_.erase(std::remove(pickables_.begin(), pickables_.end(), obj),
        pickables_.end());
}

void GizmoController::ClearPickables()
{
    pickables_.clear();
}

IGizmable* GizmoController::TryPickObject(
    const ImVec2& mouseScreenPos,
    const ImVec2& viewportPos,
    const ImVec2& viewportSize,
    YoRigine::Camera* camera)
{
    if (!camera || pickables_.empty()) return nullptr;

    Ray ray = BuildPickRay(mouseScreenPos, viewportPos, viewportSize, camera);

    IGizmable* best = nullptr;
    float      bestT = FLT_MAX;

    for (auto* obj : pickables_)
    {
        if (!obj) continue;
        float t = 0.0f;
        if (IntersectRaySphere(ray,
            obj->GetGizmoPosition(),
            obj->GetGizmoPickRadius(),
            t))
        {
            if (t < bestT)
            {
                bestT = t;
                best = obj;
            }
        }
    }

    return best;
}

//=================================================================
// Undo / Redo（単一）
//=================================================================

void GizmoController::PushSnapshot(IGizmable* target)
{
    if (!target) return;
    if (undoStack_.size() >= MAX_HISTORY)
    {
        // 最古の1件を消す（stack なので一度全て退避）
        std::stack<GizmoSnapshot> tmp;
        while (undoStack_.size() > 1)
        {
            tmp.push(undoStack_.top()); undoStack_.pop();
        }
        undoStack_.pop();                       // 最古を捨てる
        while (!tmp.empty())
        {
            undoStack_.push(tmp.top()); tmp.pop();
        }
    }

    GizmoSnapshot snap;
    snap.target = target;
    snap.position = target->GetGizmoPosition();
    snap.rotation = target->GetGizmoRotation();
    snap.scale = target->GetGizmoScale();
    undoStack_.push(snap);

    // 新操作があった → Redo 破棄
    while (!redoStack_.empty()) redoStack_.pop();
}

bool GizmoController::Undo()
{
    if (undoStack_.empty()) return false;

    GizmoSnapshot snap = undoStack_.top();
    undoStack_.pop();

    if (!snap.target) return false;

    // 現在状態を Redo へ
    GizmoSnapshot cur;
    cur.target = snap.target;
    cur.position = snap.target->GetGizmoPosition();
    cur.rotation = snap.target->GetGizmoRotation();
    cur.scale = snap.target->GetGizmoScale();
    redoStack_.push(cur);

    // 復元
    snap.target->SetGizmoPosition(snap.position);
    snap.target->SetGizmoRotation(snap.rotation);
    snap.target->SetGizmoScale(snap.scale);
    snap.target->OnGizmoManipulationEnd();

    return true;
}

bool GizmoController::Redo()
{
    if (redoStack_.empty()) return false;

    GizmoSnapshot snap = redoStack_.top();
    redoStack_.pop();

    if (!snap.target) return false;

    // 現在状態を Undo へ
    GizmoSnapshot cur;
    cur.target = snap.target;
    cur.position = snap.target->GetGizmoPosition();
    cur.rotation = snap.target->GetGizmoRotation();
    cur.scale = snap.target->GetGizmoScale();
    undoStack_.push(cur);

    // 復元
    snap.target->SetGizmoPosition(snap.position);
    snap.target->SetGizmoRotation(snap.rotation);
    snap.target->SetGizmoScale(snap.scale);
    snap.target->OnGizmoManipulationEnd();

    return true;
}

void GizmoController::ClearHistory()
{
    while (!undoStack_.empty())      undoStack_.pop();
    while (!redoStack_.empty())      redoStack_.pop();
    while (!undoStackMulti_.empty()) undoStackMulti_.pop();
    while (!redoStackMulti_.empty()) redoStackMulti_.pop();
}

//=================================================================
// Undo / Redo（複数選択）
//=================================================================

void GizmoController::PushSnapshotMulti(const std::vector<IGizmable*>& targets)
{
    std::vector<GizmoSnapshot> snaps;
    snaps.reserve(targets.size());
    for (auto* t : targets)
    {
        if (!t) continue;
        GizmoSnapshot s;
        s.target = t;
        s.position = t->GetGizmoPosition();
        s.rotation = t->GetGizmoRotation();
        s.scale = t->GetGizmoScale();
        snaps.push_back(s);
    }
    undoStackMulti_.push(std::move(snaps));
    while (!redoStackMulti_.empty()) redoStackMulti_.pop();
}

bool GizmoController::UndoMulti(std::vector<IGizmable*>& targets)
{
    if (undoStackMulti_.empty()) return false;

    auto saved = undoStackMulti_.top();
    undoStackMulti_.pop();

    // 現在状態を Redo へ
    std::vector<GizmoSnapshot> cur;
    cur.reserve(targets.size());
    for (auto* t : targets)
    {
        if (!t) continue;
        GizmoSnapshot s;
        s.target = t;
        s.position = t->GetGizmoPosition();
        s.rotation = t->GetGizmoRotation();
        s.scale = t->GetGizmoScale();
        cur.push_back(s);
    }
    redoStackMulti_.push(std::move(cur));

    // 復元
    for (auto& snap : saved)
    {
        if (!snap.target) continue;
        snap.target->SetGizmoPosition(snap.position);
        snap.target->SetGizmoRotation(snap.rotation);
        snap.target->SetGizmoScale(snap.scale);
        snap.target->OnGizmoManipulationEnd();
    }
    return true;
}

bool GizmoController::RedoMulti(std::vector<IGizmable*>& targets)
{
    if (redoStackMulti_.empty()) return false;

    auto saved = redoStackMulti_.top();
    redoStackMulti_.pop();

    // 現在状態を Undo へ
    std::vector<GizmoSnapshot> cur;
    cur.reserve(targets.size());
    for (auto* t : targets)
    {
        if (!t) continue;
        GizmoSnapshot s;
        s.target = t;
        s.position = t->GetGizmoPosition();
        s.rotation = t->GetGizmoRotation();
        s.scale = t->GetGizmoScale();
        cur.push_back(s);
    }
    undoStackMulti_.push(std::move(cur));

    // 復元
    for (auto& snap : saved)
    {
        if (!snap.target) continue;
        snap.target->SetGizmoPosition(snap.position);
        snap.target->SetGizmoRotation(snap.rotation);
        snap.target->SetGizmoScale(snap.scale);
        snap.target->OnGizmoManipulationEnd();
    }
    return true;
}

//=================================================================
// 状態取得
//=================================================================

bool GizmoController::IsUsing() const { return ImGuizmo::IsUsing(); }
bool GizmoController::IsOver()  const { return ImGuizmo::IsOver(); }

//=================================================================
// 内部処理
//=================================================================

Matrix4x4 GizmoController::BuildWorldMatrix(const IGizmable* target) const
{
    return MakeAffineMatrix(
        target->GetGizmoScale(),
        target->GetGizmoRotation(),
        target->GetGizmoPosition()
    );
}

void GizmoController::ApplyWorldMatrix(const Matrix4x4& mat, IGizmable* target) const
{
    Vector3 pos, rot, scl;
    DecomposeMatrix(mat, pos, rot, scl);
    target->SetGizmoPosition(pos);
    target->SetGizmoRotation(rot);
    target->SetGizmoScale(scl);
    target->OnGizmoManipulationEnd();
}

void GizmoController::MatrixToImGuizmo(const Matrix4x4& m, float* out) const
{
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            out[i * 4 + j] = m.m[i][j];
}

Matrix4x4 GizmoController::ImGuizmoToMatrix(const float* in) const
{
    Matrix4x4 m{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            m.m[i][j] = in[i * 4 + j];
    return m;
}

void GizmoController::DecomposeMatrix(
    const Matrix4x4& mat,
    Vector3& outPos, Vector3& outRot, Vector3& outScale) const
{
    const float* raw = reinterpret_cast<const float*>(&mat.m[0][0]);
    float t[3]{}, rDeg[3]{}, s[3]{};
    ImGuizmo::DecomposeMatrixToComponents(const_cast<float*>(raw), t, rDeg, s);

    outPos = { t[0],              t[1],              t[2] };
    outRot = { DegToRad(rDeg[0]), DegToRad(rDeg[1]), DegToRad(rDeg[2]) };
    outScale = { s[0],              s[1],              s[2] };
}

//=================================================================
// レイキャスト
//=================================================================

Ray GizmoController::BuildPickRay(
    const ImVec2& mouseScreenPos,
    const ImVec2& viewportPos,
    const ImVec2& viewportSize,
    YoRigine::Camera* camera) const
{
    // NDC に変換（-1〜1）
    float ndcX = ((mouseScreenPos.x - viewportPos.x) / viewportSize.x) * 2.0f - 1.0f;
    float ndcY = 1.0f - ((mouseScreenPos.y - viewportPos.y) / viewportSize.y) * 2.0f;

    // 逆プロジェクション行列でビュー空間へ
    Matrix4x4 invProj = Inverse(camera->GetProjectionMatrix());
    Matrix4x4 invView = Inverse(camera->GetViewMatrix());

    // ニアプレーン上の点（z = -1）
    float clipNear[4] = { ndcX, ndcY, -1.0f, 1.0f };
    float clipFar[4] = { ndcX, ndcY,  1.0f, 1.0f };

    // クリップ → ビュー
    auto MultiplyVec4 = [](const Matrix4x4& m, const float* v, float* out) {
        for (int i = 0; i < 4; ++i) {
            out[i] = 0;
            for (int j = 0; j < 4; ++j)
                out[i] += m.m[i][j] * v[j];
        }
        };

    float viewNear[4]{}, viewFar[4]{};
    MultiplyVec4(invProj, clipNear, viewNear);
    MultiplyVec4(invProj, clipFar, viewFar);

    // w除算
    for (int i = 0; i < 3; ++i) viewNear[i] /= viewNear[3];
    for (int i = 0; i < 3; ++i) viewFar[i] /= viewFar[3];

    // ビュー → ワールド
    float worldNear[4]{}, worldFar[4]{};
    float vn4[4] = { viewNear[0], viewNear[1], viewNear[2], 1.0f };
    float vf4[4] = { viewFar[0],  viewFar[1],  viewFar[2],  1.0f };
    MultiplyVec4(invView, vn4, worldNear);
    MultiplyVec4(invView, vf4, worldFar);

    Ray ray;
    ray.origin = { worldNear[0], worldNear[1], worldNear[2] };
    Vector3 dir = {
        worldFar[0] - worldNear[0],
        worldFar[1] - worldNear[1],
        worldFar[2] - worldNear[2]
    };

    // 正規化
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-6f) { dir.x /= len; dir.y /= len; dir.z /= len; }
    ray.direction = dir;

    return ray;
}

bool GizmoController::IntersectRaySphere(
    const Ray& ray,
    const Vector3& center,
    float          radius,
    float& outT) const
{
    Vector3 oc = {
        ray.origin.x - center.x,
        ray.origin.y - center.y,
        ray.origin.z - center.z
    };
    float a = ray.direction.x * ray.direction.x
        + ray.direction.y * ray.direction.y
        + ray.direction.z * ray.direction.z;
    float b = 2.0f * (oc.x * ray.direction.x
        + oc.y * ray.direction.y
        + oc.z * ray.direction.z);
    float c = oc.x * oc.x + oc.y * oc.y + oc.z * oc.z - radius * radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) return false;

    outT = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (outT < 0.0f) outT = (-b + std::sqrt(discriminant)) / (2.0f * a);
    return outT >= 0.0f;
}

#endif // USE_IMGUI
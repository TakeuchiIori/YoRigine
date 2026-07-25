// ===========================================================
// VfxParamsJson.cpp
//
// VfxParamsJson.h のシリアライズが AutoJson / nlohmann の両経路で
// 正しくコンパイル・往復できることを保証する自己テスト。
// （テストフレームワークが無いため、テンプレート実体化を強制して
//   ビルド時に破綻を検出する目的も兼ねる）
// ===========================================================
#include "VfxParamsJson.h"
#include "Loaders/Json/Use/AutoJson.h"

namespace YoRigine {

// nlohmann 直接経路: variant → json → variant の往復が一致するか
static bool CheckDirectRoundTrip()
{
    VfxGeometryParams geom = ConeGeomParams{ 1.5f, 4.0f, 20 };
    nlohmann::json j = geom;                       // to_json（variant）
    VfxGeometryParams back = j.get<VfxGeometryParams>(); // from_json（variant）

    if (!std::holds_alternative<ConeGeomParams>(back)) return false;
    const auto& c = std::get<ConeGeomParams>(back);
    return c.radius == 1.5f && c.height == 4.0f && c.segments == 20;
}

// AutoJson 経路: 変数を Add して Save/Load が通るか
static bool CheckAutoJsonRoundTrip()
{
    VfxGeometryParams geom = SphereGeomParams{ 2.0f, 20, 30 };
    VfxMaterialParams mat  = ShockwaveMatParams{};

    AutoJson aj;
    aj.Add("geom", &geom).Add("mat", &mat);

    nlohmann::json j;
    aj.Save(j);   // VariableJson<T>::SaveToJson → j = *ptr_（variant を含む）
    aj.Load(j);   // VariableJson<T>::LoadFromJson → *ptr_ = j.get<T>()

    return std::holds_alternative<SphereGeomParams>(geom) &&
           std::holds_alternative<ShockwaveMatParams>(mat);
}

// 呼び出せば両経路を検証して返す（デバッグメニュー等から任意に利用可）
bool VfxParamsJson_SelfTest()
{
    return CheckDirectRoundTrip() && CheckAutoJsonRoundTrip();
}

} // namespace YoRigine

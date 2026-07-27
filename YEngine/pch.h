// =============================================================================
// pch.h — YEngine プリコンパイルヘッダ
//
// ここには「変更頻度が低く・多数の .cpp から使われ・パースが重い」ヘッダだけを置く。
// （STL / Windows / DirectX12 / nlohmann-json / ImGui など外部・システムヘッダ）
//
// プロジェクト自作ヘッダ (MathFunc.h / Logger.h 等) はここに入れないこと。
// それらを入れると、編集のたびに PCH が再生成され YEngine 全体が再コンパイルされ、
// PCH の目的（再コンパイル削減）が逆効果になる。
// =============================================================================
#pragma once

// ---- C ランタイム / 標準ライブラリ -----------------------------------------
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cassert>

#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <algorithm>
#include <utility>
#include <optional>
#include <mutex>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

// ---- Windows / DirectX 12 ---------------------------------------------------
// NOMINMAX は premake5.lua で全構成に定義済み。
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// ---- YMath コア (全TUがほぼ必ず使い、かつ変更頻度が低い安定ヘッダ) ----------
// 注意: YMath 側を編集すると YEngine 全体が再コンパイルされる。頻繁に触る場合は外すこと。
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "MathFunc.h"

// ---- サードパーティ (angle include = /external:W3 扱いで /WX に掛からない) ----
#include <json.hpp>

// ---- ImGui は Debug/Develop (USE_IMGUI) のみ --------------------------------
#ifdef USE_IMGUI
#include <imgui.h>
#include <IconsFontAwesome5.h>
#endif

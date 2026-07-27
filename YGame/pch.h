// =============================================================================
// pch.h — YGame プリコンパイルヘッダ
//
// 方針は YEngine/pch.h と同じ:
// 「変更頻度が低く・多数の .cpp から使われ・パースが重い」外部/システムヘッダだけを置く。
// ゲーム自作ヘッダはここに入れない (編集のたびに全再コンパイルになるため)。
// =============================================================================
#pragma once

// ---- 標準ライブラリ ---------------------------------------------------------
#include <cstdint>
#include <cstddef>
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
// 並行処理 / 時間系（C++20 の <chrono> は特に重いためここで集約）
#include <mutex>
#include <thread>
#include <atomic>
#include <future>
#include <chrono>
#include <random>
#include <limits>
#include <filesystem>
#include <fstream>
#include <sstream>

// ---- Windows / DirectX 12 (エンジンヘッダ経由でも多用される) ----------------
#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>

// ---- サードパーティ ---------------------------------------------------------
#include <json.hpp>

// ---- ImGui は Debug/Develop (USE_IMGUI) のみ --------------------------------
#ifdef USE_IMGUI
#include <imgui.h>
#include <IconsFontAwesome5.h>
#endif

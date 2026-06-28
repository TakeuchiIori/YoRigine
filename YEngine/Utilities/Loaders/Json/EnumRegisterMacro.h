#pragma once
#include "EnumRegistry.h"

/// <summary>
///  Enum の表示名を登録するマクロ
/// </summary>
#define REGISTER_ENUM(EnumType, ...) \
    namespace { \
        struct EnumRegister_##EnumType { \
            /* <summary>Enum をレジストリへ登録</summary>*/ \
            EnumRegister_##EnumType() { \
                EnumRegistry::GetInstance().RegistryEnum(typeid(EnumType), { __VA_ARGS__ }); \
            } \
        } enumRegisterInstance_##EnumType; \
    }

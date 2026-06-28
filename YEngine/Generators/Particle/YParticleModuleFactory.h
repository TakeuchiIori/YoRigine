#pragma once
#include "Modules/IParticleModule.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <functional>

/// <summary>
/// パーティクルモジュールのファクトリークラス
/// タイプ名からモジュールの実体を生成
/// </summary>
class YParticleModuleFactory {
public:
    using SpawnCreator = std::function<std::shared_ptr<ISpawnModule>()>;
    using UpdateCreator = std::function<std::shared_ptr<IUpdateModule>()>;

    //=================================================================
    // シングルトン
    //=================================================================

    static YParticleModuleFactory& GetInstance() {
        static YParticleModuleFactory instance;
        return instance;
    }

    YParticleModuleFactory(const YParticleModuleFactory&) = delete;
    YParticleModuleFactory& operator=(const YParticleModuleFactory&) = delete;

    //=================================================================
    // モジュール登録
    //=================================================================

    /// <summary>
    /// Spawnモジュールを登録
    /// </summary>
    /// <param name="typeName">タイプ名（例: "SpawnVelocity"）</param>
    /// <param name="creator">生成関数</param>
    void RegisterSpawnModule(const std::string& typeName, SpawnCreator creator) {
        spawnCreators_[typeName] = creator;
    }

    /// <summary>
    /// Updateモジュールを登録
    /// </summary>
    /// <param name="typeName">タイプ名（例: "UpdateGravity"）</param>
    /// <param name="creator">生成関数</param>
    void RegisterUpdateModule(const std::string& typeName, UpdateCreator creator) {
        updateCreators_[typeName] = creator;
    }

    //=================================================================
    // モジュール生成
    //=================================================================

    /// <summary>
    /// タイプ名からSpawnモジュールを生成
    /// </summary>
    /// <param name="typeName">タイプ名</param>
    /// <returns>生成されたモジュール（存在しない場合nullptr）</returns>
    std::shared_ptr<ISpawnModule> CreateSpawnModule(const std::string& typeName) const {
        auto it = spawnCreators_.find(typeName);
        if (it != spawnCreators_.end()) {
            return it->second();
        }
        return nullptr;
    }

    /// <summary>
    /// タイプ名からUpdateモジュールを生成
    /// </summary>
    /// <param name="typeName">タイプ名</param>
    /// <returns>生成されたモジュール（存在しない場合nullptr）</returns>
    std::shared_ptr<IUpdateModule> CreateUpdateModule(const std::string& typeName) const {
        auto it = updateCreators_.find(typeName);
        if (it != updateCreators_.end()) {
            return it->second();
        }
        return nullptr;
    }

    //=================================================================
    // 登録済みモジュール一覧
    //=================================================================

    /// <summary>
    /// 登録されているSpawnモジュールのタイプ名一覧を取得
    /// </summary>
    std::vector<std::string> GetSpawnModuleTypes() const {
        std::vector<std::string> types;
        types.reserve(spawnCreators_.size());
        for (const auto& [type, _] : spawnCreators_) {
            types.push_back(type);
        }
        return types;
    }

    /// <summary>
    /// 登録されているUpdateモジュールのタイプ名一覧を取得
    /// </summary>
    std::vector<std::string> GetUpdateModuleTypes() const {
        std::vector<std::string> types;
        types.reserve(updateCreators_.size());
        for (const auto& [type, _] : updateCreators_) {
            types.push_back(type);
        }
        return types;
    }

private:
    YParticleModuleFactory() = default;
    ~YParticleModuleFactory() = default;

    //=================================================================
    // メンバ変数
    //=================================================================

    std::unordered_map<std::string, SpawnCreator> spawnCreators_;
    std::unordered_map<std::string, UpdateCreator> updateCreators_;
};

//=================================================================
// モジュール登録用マクロ
//=================================================================

/// <summary>
/// Spawnモジュールを自動登録するマクロ
/// 使用例: REGISTER_SPAWN_MODULE(SpawnVelocity)
/// </summary>
#define REGISTER_SPAWN_MODULE(ModuleClass) \
    namespace { \
        struct ModuleClass##Registrar { \
            ModuleClass##Registrar() { \
                YParticleModuleFactory::GetInstance().RegisterSpawnModule( \
                    #ModuleClass, \
                    []() { return std::make_shared<ModuleClass>(); } \
                ); \
            } \
        }; \
        static ModuleClass##Registrar g_##ModuleClass##Registrar; \
    }

/// <summary>
/// Updateモジュールを自動登録するマクロ
/// 使用例: REGISTER_UPDATE_MODULE(UpdateGravity)
/// </summary>
#define REGISTER_UPDATE_MODULE(ModuleClass) \
    namespace { \
        struct ModuleClass##Registrar { \
            ModuleClass##Registrar() { \
                YParticleModuleFactory::GetInstance().RegisterUpdateModule( \
                    #ModuleClass, \
                    []() { return std::make_shared<ModuleClass>(); } \
                ); \
            } \
        }; \
        static ModuleClass##Registrar g_##ModuleClass##Registrar; \
    }
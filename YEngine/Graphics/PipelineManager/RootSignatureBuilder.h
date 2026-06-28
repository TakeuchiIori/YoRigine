#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <vector>
#include <string>
#include <memory>
#include <functional>

namespace YoRigine {

    /// <summary>
    /// ルートシグネチャを簡単に構築するためのビルダークラス
    /// ルートパラメータの番号を自動管理し、コマンドリスト設定時のインデックスを提供
    /// </summary>
    class RootSignatureBuilder {
    public:
        /// <summary>
        /// ルートパラメータのタイプと用途を表す列挙型
        /// </summary>
        enum class ParamType {
            CBV,
            SRV_TABLE,
            UAV_TABLE,
            CONSTANT_32BIT
        };

        /// <summary>
        /// パラメータ情報を保持する構造体
        /// </summary>
        struct ParameterInfo {
            std::string name;              // パラメータ名（例: "Material", "Transform"）
            ParamType type;                 // パラメータタイプ
            UINT rootIndex;                 // 自動割り当てされるルートインデックス
            D3D12_SHADER_VISIBILITY visibility;  // シェーダー可視性
            UINT shaderRegister;            // シェーダーレジスタ番号
            UINT registerSpace = 0;         // レジスタスペース（通常は0）
        };

        RootSignatureBuilder();
        ~RootSignatureBuilder() = default;

        /// <summary>
        /// CBVパラメータを追加（自動的にインデックスが採番される）
        /// </summary>
        /// <param name="name">パラメータ名（例: "Material"）</param>
        /// <param name="shaderRegister">シェーダーレジスタ番号（例: b0なら0）</param>
        /// <param name="visibility">シェーダー可視性</param>
        /// <returns>自動採番されたルートパラメータインデックス</returns>
        UINT AddCBV(
            const std::string& name,
            UINT shaderRegister,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL,
            UINT registerSpace = 0
        );

        /// <summary>
        /// ディスクリプタテーブル（SRV）を追加
        /// </summary>
        /// <param name="name">パラメータ名</param>
        /// <param name="baseRegister">開始レジスタ番号（t0なら0）</param>
        /// <param name="numDescriptors">ディスクリプタ数</param>
        /// <param name="visibility">シェーダー可視性</param>
        /// <returns>自動採番されたルートパラメータインデックス</returns>
        UINT AddDescriptorTable(
            const std::string& name,
            UINT baseRegister,
            UINT numDescriptors = 1,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL,
            UINT registerSpace = 0
        );

        /// <summary>
        /// UAVディスクリプタテーブルを追加（コンピュートシェーダー用）
        /// </summary>
        UINT AddUAVTable(
            const std::string& name,
            UINT baseRegister,
            UINT numDescriptors = 1,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL,
            UINT registerSpace = 0
        );

        /// <summary>
        /// 32bitルート定数を追加
        /// </summary>
        UINT Add32BitConstants(
            const std::string& name,
            UINT num32BitValues,
            UINT shaderRegister,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL,
            UINT registerSpace = 0
        );

        /// <summary>
        /// スタティックサンプラーを追加
        /// </summary>
        void AddStaticSampler(
            UINT shaderRegister,
            D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_PIXEL
        );

        /// <summary>
        /// ルートシグネチャをビルド
        /// </summary>
        Microsoft::WRL::ComPtr<ID3D12RootSignature> Build(
            ID3D12Device* device,
            D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        );

        /// <summary>
        /// パラメータ名からルートインデックスを取得
        /// 使用例: SetGraphicsRootConstantBufferView(builder.GetIndex("Material"), address);
        /// </summary>
        UINT GetIndex(const std::string& name) const;

        /// <summary>
        /// すべてのパラメータ情報を取得（デバッグ用）
        /// </summary>
        const std::vector<ParameterInfo>& GetParameterInfos() const { return parameterInfos_; }

        /// <summary>
        /// リセット（新しいルートシグネチャを作り直す場合）
        /// </summary>
        void Reset();

    private:
        std::vector<D3D12_ROOT_PARAMETER> rootParameters_;
        std::vector<ParameterInfo> parameterInfos_;
        std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers_;

        // ディスクリプタレンジの保持（ポインタの寿命管理のため）
        std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> descriptorRanges_;

        UINT currentRootIndex_ = 0;
    };

    /// <summary>
    /// ルートパラメータのインデックスを名前で管理するヘルパークラス
    /// 描画時に使用することで、マジックナンバーを排除
    /// </summary>
    class RootParameterIndices {
    public:
        RootParameterIndices() = default;

        /// <summary>
        /// ビルダーから情報をコピー
        /// </summary>
        void InitializeFrom(const RootSignatureBuilder& builder);

        /// <summary>
        /// 名前からインデックスを取得
        /// </summary>
        UINT operator[](const std::string& name) const;

        /// <summary>
        /// パラメータが存在するかチェック
        /// </summary>
        bool HasParameter(const std::string& name) const;

    private:
        std::unordered_map<std::string, UINT> indices_;
    };

} // namespace YoRigine
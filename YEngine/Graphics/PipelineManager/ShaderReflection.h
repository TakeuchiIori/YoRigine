#pragma once
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include <wrl.h>
#include <d3d12.h>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <deque>

namespace YoRigine {

    /// <summary>
    /// シェーダーリフレクション情報
    /// シェーダーから自動的にルートシグネチャやインプットレイアウトを抽出
    /// </summary>
    class ShaderReflection {
    public:
        /// <summary>
        /// リソースバインディング情報
        /// </summary>
        struct ResourceBinding {
            std::string name;                    // リソース名（例: "g_Material"）
            D3D_SHADER_INPUT_TYPE type;          // CBV, SRV, UAV, SAMPLERなど
            UINT bindPoint;                      // レジスタ番号（例: b0のb部分の0）
            UINT bindCount;                      // 配列の場合の要素数
            UINT space;                          // レジスタスペース
            D3D12_SHADER_VISIBILITY visibility;  // どのシェーダーステージで使用されるか
        };

        /// <summary>
        /// 入力要素情報（頂点シェーダーの入力）
        /// </summary>
        struct InputElement {
            std::string semanticName;
            UINT semanticIndex;
            DXGI_FORMAT format;
            UINT inputSlot;
            UINT alignedByteOffset;
            D3D12_INPUT_CLASSIFICATION slotClass;
            UINT instanceDataStepRate;
        };

        /// <summary>
        /// 定数バッファの変数情報
        /// </summary>
        struct ConstantBufferVariable {
            std::string name;
            UINT offset;
            UINT size;
            std::string typeName;
        };

        /// <summary>
        /// 定数バッファ情報
        /// </summary>
        struct ConstantBuffer {
            std::string name;
            UINT size;
            UINT bindPoint;
            UINT space;
            std::vector<ConstantBufferVariable> variables;
        };

        ShaderReflection() = default;
        ~ShaderReflection() = default;

        /// <summary>
        /// DXCコンパイル済みシェーダーからリフレクション情報を取得
        /// </summary>
        bool Reflect(IDxcBlob* shaderBlob, const std::wstring& shaderProfile);

        /// <summary>
        /// D3D12ShaderReflectionを使用してリフレクション（従来のfxc用）
        /// </summary>
        bool ReflectLegacy(ID3DBlob* shaderBlob);

        /// <summary>
        /// リソースバインディング情報を取得
        /// </summary>
        const std::vector<ResourceBinding>& GetResourceBindings() const { return resourceBindings_; }

        /// <summary>
        /// 入力レイアウト情報を取得（頂点シェーダーのみ）
        /// </summary>
        const std::vector<InputElement>& GetInputElements() const { return inputElements_; }

        /// <summary>
        /// 定数バッファ情報を取得
        /// </summary>
        const std::vector<ConstantBuffer>& GetConstantBuffers() const { return constantBuffers_; }

        /// <summary>
        /// シェーダーの可視性を取得
        /// </summary>
        D3D12_SHADER_VISIBILITY GetShaderVisibility() const { return shaderVisibility_; }

        /// <summary>
        /// デバッグ情報を出力
        /// </summary>
        void PrintDebugInfo() const;

    private:
        std::vector<ResourceBinding> resourceBindings_;
        std::vector<InputElement> inputElements_;
        std::vector<ConstantBuffer> constantBuffers_;
        D3D12_SHADER_VISIBILITY shaderVisibility_ = D3D12_SHADER_VISIBILITY_ALL;

        /// <summary>
        /// シェーダープロファイルからシェーダービジビリティを判定
        /// </summary>
        D3D12_SHADER_VISIBILITY DetermineVisibilityFromProfile(const std::wstring& profile);

        /// <summary>
        /// D3D_SHADER_INPUT_TYPEをD3D12_ROOT_PARAMETER_TYPEに変換
        /// </summary>
        D3D12_ROOT_PARAMETER_TYPE ConvertToRootParameterType(D3D_SHADER_INPUT_TYPE inputType);
    };

    /// <summary>
    /// 複数のシェーダーのリフレクション情報を統合してルートシグネチャを自動生成
    /// </summary>
    class AutoRootSignatureGenerator {
    public:
        AutoRootSignatureGenerator() = default;
        ~AutoRootSignatureGenerator() = default;

        /// <summary>
        /// シェーダーリフレクション情報を追加
        /// </summary>
        void AddShaderReflection(const ShaderReflection& reflection);

        /// <summary>
        /// 頂点シェーダー、ピクセルシェーダーなどから一度に追加
        /// </summary>
        void AddShaders(
            IDxcBlob* vertexShader,
			IDxcBlob* pixelShader = nullptr,
			IDxcBlob* geometryShader = nullptr,
			IDxcBlob * hullShader = nullptr,
			IDxcBlob* domainShader = nullptr
        );

        /// <summary>
		/// 計算シェーダーから追加
        /// </summary>
        /// <param name="computeShader"></param>
        void AddComputeShader(IDxcBlob* computeShader);


        /// <summary>
        /// 収集した情報からルートシグネチャを自動生成
        /// </summary>
        Microsoft::WRL::ComPtr<ID3D12RootSignature> Generate(
            ID3D12Device* device,
            D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        );

        /// <summary>
        /// ルートパラメータのインデックスマップを取得
        /// </summary>
        const std::unordered_map<std::string, UINT>& GetParameterIndices() const {
            return parameterIndices_;
        }

        /// <summary>
        /// 自動生成されたインプットレイアウトを取得（頂点シェーダーから）
        /// </summary>
        const std::vector<D3D12_INPUT_ELEMENT_DESC>& GetInputLayout() const {
            return inputLayout_;
        }

        /// <summary>
        /// デバッグ情報を出力
        /// </summary>
        void PrintDebugInfo() const;

    private:
        struct MergedResourceBinding {
            std::string name;
            D3D_SHADER_INPUT_TYPE type;
            UINT bindPoint;
            UINT bindCount;
            UINT space;
            D3D12_SHADER_VISIBILITY visibility;  // 複数シェーダーで使われる場合はALL
        };

        std::vector<MergedResourceBinding> mergedBindings_;
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout_;
        std::deque<std::string> semanticNames_; // vectorからdequeに変更するとポインタが安定します // メモリ寿命管理用
        std::unordered_map<std::string, UINT> parameterIndices_;

        /// <summary>
        /// リソースバインディングをマージ（同じリソースが複数シェーダーで使われる場合）
        /// </summary>
        void MergeResourceBindings(const std::vector<ShaderReflection::ResourceBinding>& bindings);

        /// <summary>
        /// マージされたバインディングをルートパラメータに変換
        /// </summary>
        std::vector<D3D12_ROOT_PARAMETER> CreateRootParameters();

        /// <summary>
        /// サンプラーを自動検出して追加
        /// </summary>
        std::vector<D3D12_STATIC_SAMPLER_DESC> CreateStaticSamplers();
    };

    /// <summary>
    /// シェーダーリフレクションベースの完全自動パイプライン生成
    /// </summary>
    class ReflectionBasedPipelineBuilder {
    public:
        ReflectionBasedPipelineBuilder();

        /// <summary>
        /// シェーダーファイルから完全自動でパイプラインを生成
        /// </summary>
        struct BuildResult {
            Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
            std::unordered_map<std::string, UINT> parameterIndices;
            std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
        };

        ///// <summary>
        ///// シェーダーファイルパスから完全自動生成
        ///// </summary>
        //BuildResult BuildFromShaders(
        //    ID3D12Device* device,
        //    const std::wstring& vsPath,
        //    const std::wstring& psPath,
        //    const std::wstring& gsPath = L"",
        //    std::function<Microsoft::WRL::ComPtr<IDxcBlob>(const std::wstring&, const std::wstring&)> compileFunc = nullptr
        //);

        /// <summary>
        /// コンパイル済みシェーダーから生成
        /// </summary>
        BuildResult BuildFromCompiledShaders(
            ID3D12Device* device,
            IDxcBlob* vertexShader,
            IDxcBlob* pixelShader = nullptr,
            IDxcBlob* geometryShader = nullptr
        );

        BuildResult BuildFromCompiledComputeShader(
            ID3D12Device* device,
            IDxcBlob* computeShader
		);

        /// <summary>
        /// レンダーステート設定（オプション、デフォルト値を上書き）
        /// </summary>
        ReflectionBasedPipelineBuilder& SetBlendState(const D3D12_BLEND_DESC& blend) {
            blendState_ = blend;
            return *this;
        }

        ReflectionBasedPipelineBuilder& SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizer) {
            rasterizerState_ = rasterizer;
            return *this;
        }

        ReflectionBasedPipelineBuilder& SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depthStencil) {
            depthStencilState_ = depthStencil;
            return *this;
        }

        ReflectionBasedPipelineBuilder& SetRenderTargetFormat(DXGI_FORMAT format) {
            rtvFormat_ = format;
            return *this;
        }

        ReflectionBasedPipelineBuilder& SetDepthStencilFormat(DXGI_FORMAT format) {
            dsvFormat_ = format;
            return *this;
        }

        ReflectionBasedPipelineBuilder& SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE type) {
			primitiveTopology_ = type;
			return *this;
        }

    private:
        D3D12_BLEND_DESC blendState_;
        D3D12_RASTERIZER_DESC rasterizerState_;
        D3D12_DEPTH_STENCIL_DESC depthStencilState_;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        DXGI_FORMAT dsvFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT;

        //void InitializeDefaultStates();
    };

    /// <summary>
    /// シェーダーリフレクション統合型パイプラインマネージャー
    /// シェーダーファイルを指定するだけで自動的にパイプラインを生成
    /// </summary>
    class ReflectionPipelineManager {
    public:
        //static ReflectionPipelineManager* GetInstance();

        //void Initialize(ID3D12Device* device);

        ///// <summary>
        ///// シェーダーファイルから自動生成してキャッシュ
        ///// </summary>
        //void CreatePipelineFromShaders(
        //    const std::string& name,
        //    const std::wstring& vsPath,
        //    const std::wstring& psPath,
        //    const std::wstring& gsPath = L"",
        //    std::function<void(ReflectionBasedPipelineBuilder&)> stateOverride = nullptr
        //);

        ///// <summary>
        ///// パイプラインを取得
        ///// </summary>
        //ID3D12PipelineState* GetPipelineState(const std::string& name);
        //ID3D12RootSignature* GetRootSignature(const std::string& name);

        ///// <summary>
        ///// パラメータインデックスを取得（名前ベースアクセス用）
        ///// </summary>
        //UINT GetParameterIndex(const std::string& pipelineName, const std::string& parameterName);

        ///// <summary>
        ///// リフレクション情報を出力（デバッグ用）
        ///// </summary>
        //void PrintPipelineInfo(const std::string& name) const;

    private:
        ReflectionPipelineManager() = default;
        ~ReflectionPipelineManager() = default;

        struct PipelineData {
            Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
            std::unordered_map<std::string, UINT> parameterIndices;
        };

        ID3D12Device* device_ = nullptr;
        std::unordered_map<std::string, PipelineData> pipelines_;
        std::function<Microsoft::WRL::ComPtr<IDxcBlob>(const std::wstring&, const std::wstring&)> compileShader_;
    };



} // namespace YoRigine

DXGI_FORMAT GetFormat(const D3D12_SIGNATURE_PARAMETER_DESC& desc);

// =============================================================================
// インバートハル輪郭線 PS
//   シェルをフラットな線色で塗るだけ。
//   オブジェクト本体パス(2RTV: color + 法線 G-buffer)と同じ出力構成に合わせる。
//   輪郭シェルは法線 G-buffer に書かない(.w=0)。ポストの法線エッジ輪郭線と
//   二重にならないようにするため。
// =============================================================================

struct Outline
{
    float4 color;
    float  width;
    int    enable;
    float2 _pad;
};

ConstantBuffer<Outline> gOutline : register(b1);

struct PixelShaderOutput
{
    float4 color  : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

PixelShaderOutput main()
{
    PixelShaderOutput output;
    output.color  = gOutline.color;
    output.normal = float4(0.0f, 0.0f, 0.0f, 0.0f);
    return output;
}

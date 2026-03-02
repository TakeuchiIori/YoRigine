// PickPS.hlsl
// GPU Pick Buffer 用 Pixel Shader
//
// RenderTarget フォーマット: DXGI_FORMAT_R32_UINT
// SV_Target に uint を返すことで ObjectID をピクセルに焼き込む。
//
// エンコードルール:
//   書き込み値 = objectID + 1
//   0          = 空選択（RT クリア値）
//   1〜        = objectID 0〜
//
// CPU 側での復元:
//   encodedID == 0  → 選択なし（-1 を返す）
//   encodedID >  0  → objectID = encodedID - 1

cbuffer PickID : register(b1)
{
    uint gObjectID; // = objectID + 1（呼び出し側で +1 してから渡す）
};

uint main() : SV_Target
{
    return gObjectID;
}

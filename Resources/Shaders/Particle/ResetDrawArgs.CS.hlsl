// インダイレクト描画の引数バッファ（D3D12_DRAW_INDEXED_ARGUMENTS 相当・5 uint）を毎フレーム初期化する。
//   [0]=IndexCountPerInstance（板ポリの索引数。root定数で受け取る）
//   [1]=InstanceCount（0にリセット。この後 Update CS が生存粒子を InterlockedAdd で数える）
//   [2]=StartIndexLocation, [3]=BaseVertexLocation, [4]=StartInstanceLocation（全て0）
// 1スレッドで実行する。

RWStructuredBuffer<uint> g_DrawArgs : register(u0);

cbuffer ResetParams : register(b0)
{
    uint g_IndexCount; // root 32bit 定数
};

[numthreads(1, 1, 1)]
void main()
{
    g_DrawArgs[0] = g_IndexCount;
    g_DrawArgs[1] = 0;
    g_DrawArgs[2] = 0;
    g_DrawArgs[3] = 0;
    g_DrawArgs[4] = 0;
}

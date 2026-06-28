#pragma once

// DirectXリソースリーク検出クラス
// 終了時にDirect3Dのリソースが解放されているかを確認する
class D3DResourceLeakChecker
{
public:
	///************************* デストラクタ *************************///

	// 終了時に未解放リソースを検出
	~D3DResourceLeakChecker();
};

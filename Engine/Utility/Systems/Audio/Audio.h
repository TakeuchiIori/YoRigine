#pragma once
#include <xaudio2.h>
#include <wrl/client.h>
#include <fstream>
#include <vector>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cassert>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib,"xaudio2.lib")
class Audio {
public:
	// シングルトンの取得と破棄
	static Audio* GetInstance();
	static void Finalize();

	// 初期化と終了処理
	void Initialize();
	void FinalizeAudio();

	struct ChunkHeader
	{
		char id[4];		 // チャンク毎のID
		int32_t size;	 // チャンクサイズ
	};
	// RIFFヘッダチャンク
	struct RiffHeader
	{
		ChunkHeader chunk;		// "RIFF"
		char type[4];			// "WAVE"
	};
	// FMTチャンク
	struct FormatChunk
	{
		ChunkHeader chunk;		// "fmt"
		WAVEFORMATEX fmt;			// 波形フォーマット
	};
	// 音声データ
	struct SoundData
	{
		// 波形フォーマット
		WAVEFORMATEX wfex;
		// バッファの先頭アドレス
		BYTE* pBuffer;
		// バッファサイズ
		unsigned int bufferSize;
	};


	SoundData LoadWave(const char* filename);
	SoundData LoadAudio(const wchar_t* filename); // .mp3, .mp4 用
	void SoundUnload(SoundData* soundData);

	IXAudio2SourceVoice* SoundPlayWave(const SoundData& soundData);
	IXAudio2SourceVoice* SoundPlayAudio(const SoundData& soundData); // .mp3, .mp4 用

	// 音量設定
	void SetVolume(IXAudio2SourceVoice* pSourceVoice, float volume);

private:
	// シングルトンパターン
	static Audio* instance;
	Audio();
	~Audio();

	// XAudio2 関連
	IXAudio2* xAudio2_;
	IXAudio2MasteringVoice* masterVoice_;
	HRESULT hr_;

	// Media Foundation 関連
	bool mediaFoundationInitialized_;
};

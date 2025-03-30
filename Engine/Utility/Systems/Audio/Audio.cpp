// Audio.cpp

#include "Audio.h"
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cassert>



// シングルトンの初期化
Audio* Audio::instance = nullptr;

Audio* Audio::GetInstance()
{
    if (instance == nullptr) {
        instance = new Audio;
    }
    return instance;
}

void Audio::Finalize()
{
    if (instance) {
        instance->FinalizeAudio();
        delete instance;
        instance = nullptr;
    }
}

Audio::Audio()
    : xAudio2_(nullptr),
    masterVoice_(nullptr),
    hr_(S_OK),
    mediaFoundationInitialized_(false)
{
}

Audio::~Audio()
{
    FinalizeAudio();
}

void Audio::Initialize()
{
    // COM の初期化
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    assert(SUCCEEDED(hr) && "Failed to initialize COM");


    // Media Foundation の初期化
    if (!mediaFoundationInitialized_) {
        hr_ = MFStartup(MF_VERSION);
        assert(SUCCEEDED(hr_) && "Failed to initialize Media Foundation");
        mediaFoundationInitialized_ = true;
    }

    // XAudio2 の初期化
    hr_ = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr_) && "Failed to initialize XAudio2");

    // マスターボイスの作成
    hr_ = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(hr_) && "Failed to create mastering voice");
}

void Audio::FinalizeAudio()
{
    // マスターボイスの破棄
    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }

    // XAudio2 のシャットダウン
    if (xAudio2_) {
        xAudio2_->StopEngine();
        xAudio2_->Release();
        xAudio2_ = nullptr;
    }

    // Media Foundation のシャットダウン
    if (mediaFoundationInitialized_) {
        MFShutdown();
        mediaFoundationInitialized_ = false;
    }

    // COM のクリーンアップ
    CoUninitialize();
}

Audio::SoundData Audio::LoadAudio(const wchar_t* filename)
{
    SoundData soundData = {};
    IMFSourceReader* pReader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(filename, nullptr, &pReader);
    assert(SUCCEEDED(hr) && "Failed to create source reader");

    // PCM フォーマットを設定
    IMFMediaType* pAudioType = nullptr;
    hr = MFCreateMediaType(&pAudioType);
    assert(SUCCEEDED(hr) && "Failed to create media type");

    hr = pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(hr));
    hr = pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    assert(SUCCEEDED(hr));

    hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pAudioType);
    pAudioType->Release();
    assert(SUCCEEDED(hr) && "Failed to set media type to PCM");

    // メディアタイプから WAVEFORMATEX を取得
    IMFMediaType* pActualType = nullptr;
    hr = pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pActualType);
    assert(SUCCEEDED(hr) && "Failed to get current media type");

    // GetWaveFormatEx を MFCreateWaveFormatExFromMFMediaType に置き換え
    WAVEFORMATEX* pWfx = nullptr;
    hr = MFCreateWaveFormatExFromMFMediaType(pActualType, &pWfx, nullptr);
    assert(SUCCEEDED(hr) && "Failed to create WaveFormatEx from IMFMediaType");

    // コピーして SoundData に設定
    memcpy(&soundData.wfex, pWfx, sizeof(WAVEFORMATEX));
    CoTaskMemFree(pWfx); // MFCreateWaveFormatExFromMFMediaType で確保されたメモリを解放

    pActualType->Release();

    // バッファ用のベクターを準備
    std::vector<BYTE> bufferData;

    // データの読み取り
    while (true)
    {
        DWORD dwFlags = 0;
        IMFSample* pSample = nullptr;
        hr = pReader->ReadSample(
            static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),
            0,
            nullptr,
            &dwFlags,
            nullptr,
            &pSample
        );

        if (FAILED(hr)) {
            pReader->Release();
            assert(false && "Failed to read sample");
            break;
        }

        if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
            break;
        }

        if (pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);
            assert(SUCCEEDED(hr) && "Failed to convert sample to contiguous buffer");

            BYTE* pData = nullptr;
            DWORD maxLength = 0, currentLength = 0;
            hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
            assert(SUCCEEDED(hr) && "Failed to lock buffer");

            // バッファにデータを追加
            bufferData.insert(bufferData.end(), pData, pData + currentLength);

            pBuffer->Unlock();
            pBuffer->Release();
            pSample->Release();
        }
    }

    pReader->Release();

    // バッファのサイズとデータを SoundData に設定
    if (!bufferData.empty()) {
        soundData.bufferSize = static_cast<DWORD>(bufferData.size());
        soundData.pBuffer = new BYTE[soundData.bufferSize];
        memcpy(soundData.pBuffer, bufferData.data(), soundData.bufferSize);
    }

    return soundData;
}


void Audio::SoundUnload(SoundData* soundData)
{
    if (soundData->pBuffer) {
        delete[] soundData->pBuffer;
        soundData->pBuffer = nullptr;
    }
    soundData->bufferSize = 0;
    ZeroMemory(&soundData->wfex, sizeof(WAVEFORMATEX));
}

IXAudio2SourceVoice* Audio::SoundPlayAudio(const SoundData& soundData)
{
    HRESULT hr;

    // SourceVoice の作成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(hr) && "Failed to create source voice");

    // XAUDIO2_BUFFER の設定
    XAUDIO2_BUFFER buf = {};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // バッファの送信
    hr = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(hr) && "Failed to submit source buffer");

    // 再生開始
    hr = pSourceVoice->Start();
    assert(SUCCEEDED(hr) && "Failed to start source voice");

    return pSourceVoice;
}

IXAudio2SourceVoice* Audio::SoundPlayWave(const SoundData& soundData)
{
    HRESULT hr;

    // SourceVoice の作成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(hr) && "Failed to create source voice");

    // XAUDIO2_BUFFER の設定
    XAUDIO2_BUFFER buf = {};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // バッファの送信
    hr = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(hr) && "Failed to submit source buffer");

    // 再生開始
    hr = pSourceVoice->Start();
    assert(SUCCEEDED(hr) && "Failed to start source voice");

    return pSourceVoice;
}
void Audio::SetVolume(IXAudio2SourceVoice* pSourceVoice, float volume)
{
    // 0.0f ～ 1.0f の範囲で音量を設定
    HRESULT hr = pSourceVoice->SetVolume(volume);
    assert(SUCCEEDED(hr) && "Failed to set volume");
}

Audio::SoundData Audio::LoadWave(const char* filename)
{
    // ファイル入力ストリームのインスタンス
    std::ifstream file;
    // .wavファイルをバイナリモードで開く
    file.open(filename, std::ios_base::binary);
    // ファイルオープン失敗を検出
    assert(file.is_open());


    // RIFFヘッダーの読み込み
    RiffHeader riff;
    file.read((char*)&riff, sizeof(riff));
    // ファイルがRIFFかチェック
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
        assert(0);
    }
    // タイプがWAVEかチェック
    if (strncmp(riff.type, "WAVE", 4) != 0) {
        assert(0);
    }

    // チャンクのループを開始
    ChunkHeader chunkHeader;
    FormatChunk format = {};

    while (file.read((char*)&chunkHeader, sizeof(chunkHeader))) {
        // チャンクIDが "fmt " かを確認
        if (strncmp(chunkHeader.id, "fmt ", 4) == 0) {
            // Formatチャンクのサイズを確認し、データを読み込む
            assert(chunkHeader.size <= sizeof(format.fmt));
            format.chunk = chunkHeader; // チャンクヘッダーをコピー
            file.read((char*)&format.fmt, chunkHeader.size); // fmtのデータを読み込み
            break;
        }
        else {
            // 次のチャンクに移動
            file.seekg(chunkHeader.size, std::ios_base::cur);
        }
    }

    // "fmt "チャンクが見つからなかった場合のエラー処理
    if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
        //std::cerr << "Error: 'fmt ' chunk not found!" << std::endl;
        assert(0);
    }



    // Dataチャンクの読み込み
    ChunkHeader data;
    file.read((char*)&data, sizeof(data));
    // JUNKチャンクを検出した場合
    if (strncmp(data.id, "JUNK", 4) == 0) {
        // 読み取り位置をJUNKチャンクの終わりまで進める
        file.seekg(data.size, std::ios_base::cur);
        // 再読み込み
        file.read((char*)&data, sizeof(data));
    }

    if (strncmp(data.id, "data", 4) != 0) {
        assert(0);
    }

    // Dataチャンクのデータ部（波形データの読み込み）
    char* pBuffer = new char[data.size];
    file.read(pBuffer, data.size);

    // Waveファイルを閉じる
    file.close();


    // returnする為のデータ
    SoundData soundData = {};

    soundData.wfex = format.fmt;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
    soundData.bufferSize = data.size;

    return soundData;
}

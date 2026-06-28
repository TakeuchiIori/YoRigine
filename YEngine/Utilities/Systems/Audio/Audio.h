#pragma once

#include <xaudio2.h>
#include <wrl/client.h>
#include <fstream>
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "xaudio2.lib")

namespace YoRigine {

	///************************* サウンドカテゴリ *************************///
	/// 
	/// サウンドの種類を分類
	/// カテゴリごとに音量管理が可能
	/// 
	enum class SoundCategory {
		BGM,      // 背景音楽
		SE,       // 効果音
		VOICE,    // ボイス・会話
		AMBIENT   // 環境音
	};

	// カテゴリ名を文字列に変換
	inline const char* GetCategoryName(SoundCategory category) {
		switch (category) {
		case SoundCategory::BGM: return "BGM";
		case SoundCategory::SE: return "SE";
		case SoundCategory::VOICE: return "VOICE";
		case SoundCategory::AMBIENT: return "AMBIENT";
		default: return "Unknown";
		}
	}

	///************************* サウンドハンドル *************************///
	/// 
	/// 再生中の音声を管理するハンドル
	/// 自動的にリソースを解放します
	/// 
	class SoundHandle {
	public:
		SoundHandle() = default;
		~SoundHandle();

		SoundHandle(const SoundHandle&) = delete;
		SoundHandle& operator=(const SoundHandle&) = delete;
		SoundHandle(SoundHandle&& other) noexcept;
		SoundHandle& operator=(SoundHandle&& other) noexcept;

		// 再生制御
		void Stop();
		void Pause();
		void Resume();
		void SetVolume(float volume); // 0.0f ~ 1.0f
		void SetPitch(float pitch);   // 0.5f ~ 2.0f (1.0fが標準)
		bool IsPlaying() const;
		float GetVolume() const;
		float GetPitch() const;

	private:
		friend class Audio;
		IXAudio2SourceVoice* sourceVoice_ = nullptr;
		SoundCategory category_ = SoundCategory::SE;
		Audio* audioInstance_ = nullptr;
		float baseVolume_ = 1.0f; // ユーザーが設定した音量
		float pitch_ = 1.0f;      // ピッチ（周波数比率）

		void SetSourceVoice(IXAudio2SourceVoice* voice);
		void SetCategory(SoundCategory category);
		void SetAudioInstance(Audio* audio);
		void UpdateVolume(); // カテゴリ音量を反映
	};

	///************************* サウンド管理クラス *************************///
	///
	/// XAudio2 と Media Foundation を使用して  
	/// wav / mp3 / mp4 形式の音声を再生・制御する
	///
	/// 使用例:
	/// ```cpp
	/// // 初期化
	/// Audio::GetInstance()->Initialize();
	/// 
	/// // BGMをループ再生
	/// auto bgm = Audio::GetInstance()->Play("bgm.mp3", true, 0.7f);
	/// 
	/// // 効果音を再生
	/// Audio::GetInstance()->PlayOneShot("explosion.wav");
	/// 
	/// // 音量調整
	/// bgm.SetVolume(0.5f);
	/// 
	/// // 停止
	/// bgm.Stop();
	/// ```
	///
	class Audio {
	public:
		///************************* シングルトン *************************///
		static Audio* GetInstance();
		static void Finalize();

	public:
		///************************* 初期化と終了 *************************///
		void Initialize();
		void FinalizeAudio();

	public:
		///************************* 簡単再生API *************************///

		// 音声を再生してハンドルを返す
		SoundHandle Play(const std::string& filepath, bool loop = false, float volume = 1.0f, SoundCategory category = SoundCategory::SE);
		SoundHandle Play(const std::wstring& filepath, bool loop = false, float volume = 1.0f, SoundCategory category = SoundCategory::SE);

		// ワンショット再生(ハンドル不要な効果音向け)
		void PlayOneShot(const std::string& filepath, float volume = 1.0f, SoundCategory category = SoundCategory::SE);
		void PlayOneShot(const std::wstring& filepath, float volume = 1.0f, SoundCategory category = SoundCategory::SE);

		// 事前読み込み(起動時の読み込み用)
		void Preload(const std::string& filepath);
		void Preload(const std::wstring& filepath);
		// 指定フォルダ内の全音声を事前読み込み
		void PreloadAllInPath(const std::string& path);

		// キャッシュクリア
		void ClearCache();
		void RemoveFromCache(const std::string& filepath);
		void RemoveFromCache(const std::wstring& filepath);

		// ハンドルの登録・解除
		void RegisterHandle(SoundHandle* handle);
		void UnregisterHandle(SoundHandle* handle);

		///************************* カテゴリ別音量制御 *************************///

		// カテゴリ全体の音量を設定 (0.0f ~ 1.0f)
		void SetCategoryVolume(SoundCategory category, float volume);

		// カテゴリ全体の音量を取得
		float GetCategoryVolume(SoundCategory category) const;

		// マスター音量の設定・取得（全体の音量）
		void SetMasterVolume(float volume);
		float GetMasterVolume() const;

		// BGM音量の設定・取得
		void SetBGMVolume(float volume);
		float GetBGMVolume() const;

		// SE音量の設定・取得
		void SetSEVolume(float volume);
		float GetSEVolume() const;

		// VOICE音量の設定・取得
		void SetVoiceVolume(float volume);
		float GetVoiceVolume() const;

		// AMBIENT音量の設定・取得
		void SetAmbientVolume(float volume);
		float GetAmbientVolume() const;

		///************************* 拡張機能 *************************///

		// ダッキング機能
		void EnableDucking(bool enable);
		void SetDuckingLevel(float level); // 減衰レベル 0.0f ~ 1.0f
		bool IsDuckingEnabled() const;
		float GetDuckingLevel() const;

		// フェード機能
		void FadeIn(SoundHandle& handle, float duration);
		void FadeOut(SoundHandle& handle, float duration);

		// ピッチのグローバル設定
		void SetGlobalPitch(float pitch);
		float GetGlobalPitch() const;

		///************************* 設定の保存・読み込み *************************///

		// JSON形式で設定を保存・読み込み
		void SaveSettings(const std::string& filepath);
		void LoadSettings(const std::string& filepath);

		// ImGui用デバッグウィンドウ
		void ShowDebugWindow();
		void ShowSettingsWindow();

	public:
		///************************* 従来のAPI(互換性維持) *************************///

		struct ChunkHeader {
			char id[4];
			int32_t size;
		};

		struct RiffHeader {
			ChunkHeader chunk;
			char type[4];
		};

		struct FormatChunk {
			ChunkHeader chunk;
			WAVEFORMATEX fmt;
		};

		struct SoundData {
			WAVEFORMATEX wfex;
			BYTE* pBuffer;
			unsigned int bufferSize;

			SoundData() : wfex{}, pBuffer(nullptr), bufferSize(0) {}
			SoundData(const SoundData&) = delete;
			SoundData& operator=(const SoundData&) = delete;

			SoundData(SoundData&& other) noexcept
				: wfex(other.wfex), pBuffer(other.pBuffer), bufferSize(other.bufferSize) {
				other.pBuffer = nullptr;
				other.bufferSize = 0;
				ZeroMemory(&other.wfex, sizeof(WAVEFORMATEX));
			}

			SoundData& operator=(SoundData&& other) noexcept {
				if (this != &other) {
					if (pBuffer) {
						delete[] pBuffer;
					}
					wfex = other.wfex;
					pBuffer = other.pBuffer;
					bufferSize = other.bufferSize;
					other.pBuffer = nullptr;
					other.bufferSize = 0;
					ZeroMemory(&other.wfex, sizeof(WAVEFORMATEX));
				}
				return *this;
			}

			~SoundData() {
				if (pBuffer) {
					delete[] pBuffer;
					pBuffer = nullptr;
				}
			}
		};

		SoundData LoadWave(const char* filename);
		SoundData LoadAudio(const wchar_t* filename);
		void SoundUnload(SoundData* soundData);
		IXAudio2SourceVoice* SoundPlayWave(const SoundData& soundData, bool loop = false);
		IXAudio2SourceVoice* SoundPlayAudio(const SoundData& soundData, bool loop = false);
		void SetVolume(IXAudio2SourceVoice* pSourceVoice, float volume);
		void StopAndDestroyVoice(IXAudio2SourceVoice* pSourceVoice);

	private:
		///************************* 内部ヘルパー *************************///

		// ファイル拡張子の判定
		bool IsWaveFile(const std::wstring& filepath) const;
		std::wstring ToWideString(const std::string& str) const;

		// キャッシュされたサウンドデータの取得/作成
		std::shared_ptr<SoundData> GetOrLoadSound(const std::wstring& filepath);

		// 再生処理の共通化
		IXAudio2SourceVoice* PlayInternal(const SoundData& soundData, bool loop, float volume, SoundCategory category);

	private:
		///************************* シングルトン内部 *************************///
		static Audio* instance;
		Audio();
		~Audio();

	private:
		///************************* メンバ変数 *************************///
		IXAudio2* xAudio2_;
		IXAudio2MasteringVoice* masterVoice_;
		HRESULT hr_;
		bool mediaFoundationInitialized_;

		// サウンドデータのキャッシュ
		std::unordered_map<std::wstring, std::shared_ptr<SoundData>> soundCache_;

		// 音量設定
		float masterVolume_ = 1.0f;
		float bgmVolume_ = 1.0f;
		float seVolume_ = 1.0f;
		float voiceVolume_ = 1.0f;
		float ambientVolume_ = 1.0f;

		// ピッチ設定
		float globalPitch_ = 1.0f;

		// ダッキング設定
		bool duckingEnabled_ = false;
		float duckingLevel_ = 0.3f; // BGMを30%まで減衰
		bool isDucking_ = false;

		// 再生中のハンドルリスト（カテゴリ音量変更時の更新用）
		std::vector<SoundHandle*> activeSoundHandles_;

		const std::string settingsDirectory_ = "Resources/Json/Audio/";
		const std::string savePath_ = "audio_settings.json";

		// 内部ヘルパー
		void ApplyDucking(bool duck);
		// 内部ヘルパー関数: ディレクトリ内のファイル一覧を取得
		std::vector<std::string> GetAllAudioFilesInPath(const std::string& path);
	};
}
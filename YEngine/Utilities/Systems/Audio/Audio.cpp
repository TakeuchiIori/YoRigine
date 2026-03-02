#include "Audio.h"
#include <Windows.h>
#include <cassert>
#include <algorithm>
#include <locale>
#include <codecvt>
#include <filesystem>
#include <thread>


#include <Debugger/Logger.h>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include <future>

namespace YoRigine {

	///************************* SoundHandle 実装 *************************///

	SoundHandle::~SoundHandle() {
		if (sourceVoice_) {
			sourceVoice_->Stop();
			sourceVoice_->DestroyVoice();
			sourceVoice_ = nullptr;
		}
		if (audioInstance_) {
			audioInstance_->UnregisterHandle(this);
		}
	}

	SoundHandle::SoundHandle(SoundHandle&& other) noexcept
		: sourceVoice_(other.sourceVoice_),
		category_(other.category_),
		audioInstance_(other.audioInstance_),
		baseVolume_(other.baseVolume_) {
		other.sourceVoice_ = nullptr;
		other.audioInstance_ = nullptr;

		// ハンドル管理の更新
		if (audioInstance_) {
			audioInstance_->UnregisterHandle(&other);
			audioInstance_->RegisterHandle(this);
		}
	}

	SoundHandle& SoundHandle::operator=(SoundHandle&& other) noexcept {
		if (this != &other) {
			if (sourceVoice_) {
				sourceVoice_->Stop();
				sourceVoice_->DestroyVoice();
			}
			if (audioInstance_) {
				audioInstance_->UnregisterHandle(this);
			}

			sourceVoice_ = other.sourceVoice_;
			category_ = other.category_;
			audioInstance_ = other.audioInstance_;
			baseVolume_ = other.baseVolume_;

			other.sourceVoice_ = nullptr;
			other.audioInstance_ = nullptr;

			if (audioInstance_) {
				audioInstance_->UnregisterHandle(&other);
				audioInstance_->RegisterHandle(this);
			}
		}
		return *this;
	}

	void SoundHandle::SetSourceVoice(IXAudio2SourceVoice* voice) {
		sourceVoice_ = voice;
	}

	void SoundHandle::SetCategory(SoundCategory category) {
		category_ = category;
	}

	void SoundHandle::SetAudioInstance(Audio* audio) {
		audioInstance_ = audio;
		if (audio) {
			audio->RegisterHandle(this);
		}
	}

	void SoundHandle::UpdateVolume() {
		if (!sourceVoice_ || !audioInstance_) {
			return;
		}

		float categoryVolume = audioInstance_->GetCategoryVolume(category_);
		float masterVolume = audioInstance_->GetMasterVolume();
		float finalVolume = baseVolume_ * categoryVolume * masterVolume;
		finalVolume = std::max(0.0f, std::min(1.0f, finalVolume));

		sourceVoice_->SetVolume(finalVolume);
	}

	void SoundHandle::Stop() {
		if (sourceVoice_) {
			sourceVoice_->Stop();
			sourceVoice_->DestroyVoice();
			sourceVoice_ = nullptr;
		}
		if (audioInstance_) {
			audioInstance_->UnregisterHandle(this);
			audioInstance_ = nullptr;
		}
	}

	void SoundHandle::Pause() {
		if (sourceVoice_) {
			sourceVoice_->Stop();
		}
	}

	void SoundHandle::Resume() {
		if (sourceVoice_) {
			sourceVoice_->Start();
		}
	}

	void SoundHandle::SetVolume(float volume) {
		baseVolume_ = std::max(0.0f, std::min(1.0f, volume));
		UpdateVolume();
	}

	void SoundHandle::SetPitch(float pitch) {
		pitch_ = std::max(0.5f, std::min(2.0f, pitch));
		if (sourceVoice_) {
			sourceVoice_->SetFrequencyRatio(pitch_);
		}
	}

	float SoundHandle::GetVolume() const {
		return baseVolume_;
	}

	float SoundHandle::GetPitch() const {
		return pitch_;
	}

	bool SoundHandle::IsPlaying() const {
		if (!sourceVoice_) {
			return false;
		}

		XAUDIO2_VOICE_STATE state;
		sourceVoice_->GetState(&state);
		return state.BuffersQueued > 0;
	}

	///************************* Audio シングルトン *************************///

	Audio* Audio::instance = nullptr;

	Audio* Audio::GetInstance() {
		if (instance == nullptr) {
			instance = new Audio;
		}
		return instance;
	}

	void Audio::Finalize() {
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

	Audio::~Audio() {
		FinalizeAudio();
	}

	///************************* 初期化と終了 *************************///

	void Audio::Initialize() {
		// COM の初期化
		HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		if (FAILED(hr)) {
			assert(SUCCEEDED(hr) && "Failed to initialize COM");
			return;
		}

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

	void Audio::FinalizeAudio() {
		// キャッシュクリア
		soundCache_.clear();

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

	///************************* 簡単再生API *************************///

	SoundHandle Audio::Play(const std::string& filepath, bool loop, float volume, SoundCategory category) {
		return Play(ToWideString(filepath), loop, volume, category);
	}

	SoundHandle Audio::Play(const std::wstring& filepath, bool loop, float volume, SoundCategory category) {
		SoundHandle handle;

		auto soundData = GetOrLoadSound(filepath);
		if (!soundData || !soundData->pBuffer) {
			return handle;
		}

		IXAudio2SourceVoice* voice = PlayInternal(*soundData, loop, volume, category);
		if (voice) {
			handle.SetSourceVoice(voice);
			handle.SetCategory(category);
			handle.SetAudioInstance(this);
			handle.baseVolume_ = volume;
			handle.UpdateVolume();
		}

		return handle;
	}

	void Audio::PlayOneShot(const std::string& filepath, float volume, SoundCategory category) {
		PlayOneShot(ToWideString(filepath), volume, category);
	}

	void Audio::PlayOneShot(const std::wstring& filepath, float volume, SoundCategory category) {
		auto soundData = GetOrLoadSound(filepath);
		if (!soundData || !soundData->pBuffer) {
			return;
		}

		PlayInternal(*soundData, false, volume, category);
		// SourceVoiceは自動的に解放されます(XAUDIO2_END_OF_STREAM後)
	}

	void Audio::Preload(const std::string& filepath) {
		Preload(ToWideString(filepath));
	}

	void Audio::Preload(const std::wstring& filepath) {
		GetOrLoadSound(filepath);
	}

	void Audio::PreloadAllInPath(const std::string& path)
	{
		// 指定フォルダ内の全音声ファイルを取得
		auto audioFiles = GetAllAudioFilesInPath(path);

		// ロード処理をマルチスレッドで実行
		std::vector<std::future<void>> futures;
		for (const auto& file : audioFiles) {
			futures.push_back(std::async(std::launch::async, [this, file]() {
				this->Preload(file);  // Preloadメソッドで個別読み込み
				}));
			Logger("読み込み完了の音声ファイル: " + file);
		}

		// 全てのスレッドの完了を待機
		for (auto& fut : futures) {
			fut.wait();
		}
		Logger("-------------全ての音声ファイルを読み込みました。-------------");
	}

	void Audio::ClearCache() {
		soundCache_.clear();
	}

	void Audio::RemoveFromCache(const std::string& filepath) {
		RemoveFromCache(ToWideString(filepath));
	}

	void Audio::RemoveFromCache(const std::wstring& filepath) {
		soundCache_.erase(filepath);
	}

	///************************* カテゴリ別音量制御 *************************///

	void Audio::SetCategoryVolume(SoundCategory category, float volume) {
		float clampedVolume = std::max(0.0f, std::min(1.0f, volume));

		switch (category) {
		case SoundCategory::BGM:
			bgmVolume_ = clampedVolume;
			break;
		case SoundCategory::SE:
			seVolume_ = clampedVolume;
			break;
		case SoundCategory::VOICE:
			voiceVolume_ = clampedVolume;
			break;
		case SoundCategory::AMBIENT:
			ambientVolume_ = clampedVolume;
			break;
		}

		// 再生中の全サウンドの音量を更新
		for (auto* handle : activeSoundHandles_) {
			if (handle && handle->category_ == category) {
				handle->UpdateVolume();
			}
		}
	}

	float Audio::GetCategoryVolume(SoundCategory category) const {
		switch (category) {
		case SoundCategory::BGM: return bgmVolume_;
		case SoundCategory::SE: return seVolume_;
		case SoundCategory::VOICE: return voiceVolume_;
		case SoundCategory::AMBIENT: return ambientVolume_;
		default: return 1.0f;
		}
	}

	void Audio::SetMasterVolume(float volume) {
		masterVolume_ = std::max(0.0f, std::min(1.0f, volume));

		// 再生中の全サウンドの音量を更新
		for (auto* handle : activeSoundHandles_) {
			if (handle) {
				handle->UpdateVolume();
			}
		}
	}

	float Audio::GetMasterVolume() const {
		return masterVolume_;
	}

	void Audio::SetBGMVolume(float volume) {
		SetCategoryVolume(SoundCategory::BGM, volume);
	}

	float Audio::GetBGMVolume() const {
		return bgmVolume_;
	}

	void Audio::SetSEVolume(float volume) {
		SetCategoryVolume(SoundCategory::SE, volume);
	}

	float Audio::GetSEVolume() const {
		return seVolume_;
	}

	void Audio::SetVoiceVolume(float volume) {
		SetCategoryVolume(SoundCategory::VOICE, volume);
	}

	float Audio::GetVoiceVolume() const {
		return voiceVolume_;
	}

	void Audio::SetAmbientVolume(float volume) {
		SetCategoryVolume(SoundCategory::AMBIENT, volume);
	}

	float Audio::GetAmbientVolume() const {
		return ambientVolume_;
	}

	///************************* 拡張機能 *************************///

	void Audio::EnableDucking(bool enable) {
		duckingEnabled_ = enable;
		if (!enable && isDucking_) {
			ApplyDucking(false);
		}
	}

	void Audio::SetDuckingLevel(float level) {
		duckingLevel_ = std::max(0.0f, std::min(1.0f, level));
	}

	bool Audio::IsDuckingEnabled() const {
		return duckingEnabled_;
	}

	float Audio::GetDuckingLevel() const {
		return duckingLevel_;
	}

	void Audio::ApplyDucking(bool duck) {
		isDucking_ = duck;
		// BGMの音量を調整
		for (auto* handle : activeSoundHandles_) {
			if (handle && handle->category_ == SoundCategory::BGM) {
				handle->UpdateVolume();
			}
		}
	}

	std::vector<std::string> Audio::GetAllAudioFilesInPath(const std::string& path)
	{
		std::vector<std::string> audioFiles;
		try {
			for (const auto& entry : std::filesystem::directory_iterator(path)) {
				if (entry.is_regular_file()) {
					// ファイル拡張子の判定
					std::string ext = entry.path().extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);  // 小文字化
					if (ext == ".wav" || ext == ".mp3") {
						audioFiles.push_back(entry.path().string());
					}
				}
			}
		}
		catch (const std::filesystem::filesystem_error& e) {
			(void)e;
			// エラーハンドリング（例: ディレクトリが存在しない場合）
			assert(false && "Specified path does not exist or is inaccessible.");
		}
		return audioFiles;
	}

	void Audio::FadeIn(SoundHandle& handle, float duration) {
		(void)duration;
		// 簡易実装: 段階的に音量を上げる
		// 実際のゲームループで呼び出す想定
		float targetVolume = handle.GetVolume();
		handle.SetVolume(0.0f);
		(void)targetVolume;
		// TODO: フレームごとに音量を増加させる処理を追加
	}

	void Audio::FadeOut(SoundHandle& handle, float duration) {
		(void)duration;
		(void)handle;
		// 簡易実装: 段階的に音量を下げる
		// 実際のゲームループで呼び出す想定
		// TODO: フレームごとに音量を減少させる処理を追加
	}

	void Audio::SetGlobalPitch(float pitch) {
		globalPitch_ = std::max(0.5f, std::min(2.0f, pitch));
		// 再生中のサウンド全てにピッチを適用
		for (auto* handle : activeSoundHandles_) {
			if (handle && handle->sourceVoice_) {
				handle->sourceVoice_->SetFrequencyRatio(handle->pitch_ * globalPitch_);
			}
		}
	}

	float Audio::GetGlobalPitch() const {
		return globalPitch_;
	}

	///************************* 設定の保存・読み込み *************************///

	void Audio::SaveSettings(const std::string& filepath) {
		std::ofstream file(settingsDirectory_ + filepath);
		if (!file.is_open()) {
			return;
		}

		// 簡易JSON形式で保存
		file << "{\n";
		file << "  \"masterVolume\": " << masterVolume_ << ",\n";
		file << "  \"bgmVolume\": " << bgmVolume_ << ",\n";
		file << "  \"seVolume\": " << seVolume_ << ",\n";
		file << "  \"voiceVolume\": " << voiceVolume_ << ",\n";
		file << "  \"ambientVolume\": " << ambientVolume_ << ",\n";
		file << "  \"globalPitch\": " << globalPitch_ << ",\n";
		file << "  \"duckingEnabled\": " << (duckingEnabled_ ? "true" : "false") << ",\n";
		file << "  \"duckingLevel\": " << duckingLevel_ << "\n";
		file << "}\n";

		file.close();
	}

	void Audio::LoadSettings(const std::string& filepath) {
		std::ifstream file(settingsDirectory_ + filepath);
		if (!file.is_open()) {
			return;
		}

		std::string line;
		while (std::getline(file, line)) {
			// 簡易パーサー
			size_t colonPos = line.find(':');
			if (colonPos == std::string::npos) continue;

			std::string key = line.substr(0, colonPos);
			std::string value = line.substr(colonPos + 1);

			// 前後の空白と引用符を削除
			key.erase(0, key.find_first_not_of(" \t\""));
			key.erase(key.find_last_not_of(" \t\"") + 1);
			value.erase(0, value.find_first_not_of(" \t"));
			value.erase(value.find_last_not_of(" \t,") + 1);

			// 値を設定
			if (key == "masterVolume") {
				masterVolume_ = std::stof(value);
			} else if (key == "bgmVolume") {
				bgmVolume_ = std::stof(value);
			} else if (key == "seVolume") {
				seVolume_ = std::stof(value);
			} else if (key == "voiceVolume") {
				voiceVolume_ = std::stof(value);
			} else if (key == "ambientVolume") {
				ambientVolume_ = std::stof(value);
			} else if (key == "globalPitch") {
				globalPitch_ = std::stof(value);
			} else if (key == "duckingEnabled") {
				duckingEnabled_ = (value == "true");
			} else if (key == "duckingLevel") {
				duckingLevel_ = std::stof(value);
			}
		}

		file.close();

		// 全サウンドの音量を更新
		for (auto* handle : activeSoundHandles_) {
			if (handle) {
				handle->UpdateVolume();
			}
		}
	}

	///************************* ImGui用ウィンドウ *************************///

	void Audio::ShowDebugWindow() {
#ifdef USE_IMGUI
		ImGui::Text("アクティブなサウンド数: %d", (int)activeSoundHandles_.size());
		ImGui::Separator();

		// 再生中のサウンド一覧
		int index = 0;
		for (auto* handle : activeSoundHandles_) {
			if (handle && handle->IsPlaying()) {
				ImGui::PushID(index++);
				ImGui::Text("サウンド %d - カテゴリ: %s", index, GetCategoryName(handle->category_));
				ImGui::Text("  音量: %.2f, ピッチ: %.2f", handle->GetVolume(), handle->GetPitch());
				if (ImGui::Button("停止")) {
					handle->Stop();
				}
				ImGui::PopID();
				ImGui::Separator();
			}
		}
#endif // USE_IMGUI
	}

	void Audio::ShowSettingsWindow() {
#ifdef USE_IMGUI
		// マスター音量
		float masterVol = GetMasterVolume();
		if (ImGui::SliderFloat("マスター音量", &masterVol, 0.0f, 1.0f)) {
			SetMasterVolume(masterVol);
		}

		ImGui::Separator();

		// カテゴリ別音量
		float bgmVol = GetBGMVolume();
		if (ImGui::SliderFloat("BGM 音量", &bgmVol, 0.0f, 1.0f)) {
			SetBGMVolume(bgmVol);
		}

		float seVol = GetSEVolume();
		if (ImGui::SliderFloat("SE 音量", &seVol, 0.0f, 1.0f)) {
			SetSEVolume(seVol);
		}

		float voiceVol = GetVoiceVolume();
		if (ImGui::SliderFloat("ボイス音量", &voiceVol, 0.0f, 1.0f)) {
			SetVoiceVolume(voiceVol);
		}

		float ambientVol = GetAmbientVolume();
		if (ImGui::SliderFloat("環境音量", &ambientVol, 0.0f, 1.0f)) {
			SetAmbientVolume(ambientVol);
		}

		ImGui::Separator();

		// ピッチ
		float pitch = GetGlobalPitch();
		if (ImGui::SliderFloat("全体ピッチ", &pitch, 0.5f, 2.0f)) {
			SetGlobalPitch(pitch);
		}

		ImGui::Separator();

		// ダッキング
		bool ducking = IsDuckingEnabled();
		if (ImGui::Checkbox("ダッキングを有効にする", &ducking)) {
			EnableDucking(ducking);
		}

		if (ducking) {
			float duckLevel = GetDuckingLevel();
			if (ImGui::SliderFloat("ダッキングレベル", &duckLevel, 0.0f, 1.0f)) {
				SetDuckingLevel(duckLevel);
			}
		}

		ImGui::Separator();

		// 保存・読み込み
		if (ImGui::Button("設定を保存")) {
			SaveSettings(savePath_);
		}
		ImGui::SameLine();
		if (ImGui::Button("設定を読み込み")) {
			LoadSettings(savePath_);
		}
#endif // USE_IMGUI
	}

	///************************* ハンドル管理 *************************///

	void Audio::RegisterHandle(SoundHandle* handle) {
		if (handle) {
			activeSoundHandles_.push_back(handle);
		}
	}

	void Audio::UnregisterHandle(SoundHandle* handle) {
		if (!handle) {
			return;
		}

		auto it = std::find(activeSoundHandles_.begin(), activeSoundHandles_.end(), handle);
		if (it != activeSoundHandles_.end()) {
			activeSoundHandles_.erase(it);
		}
	}

	///************************* 内部ヘルパー *************************///

	bool Audio::IsWaveFile(const std::wstring& filepath) const {
		if (filepath.length() < 4) {
			return false;
		}

		std::wstring ext = filepath.substr(filepath.length() - 4);
		std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

		return ext == L".wav";
	}

	std::wstring Audio::ToWideString(const std::string& str) const {
		if (str.empty()) {
			return std::wstring();
		}

		int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
		std::wstring wstr(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
		wstr.resize(size - 1); // null文字を削除

		return wstr;
	}

	std::shared_ptr<Audio::SoundData> Audio::GetOrLoadSound(const std::wstring& filepath) {
		// キャッシュに存在するか確認
		auto it = soundCache_.find(filepath);
		if (it != soundCache_.end()) {
			return it->second;
		}

		// 新規読み込み
		SoundData loadedData;

		if (IsWaveFile(filepath)) {
			// UTF-8文字列に変換
			int size = WideCharToMultiByte(CP_UTF8, 0, filepath.c_str(), -1, nullptr, 0, nullptr, nullptr);
			std::string narrowPath(size, 0);
			WideCharToMultiByte(CP_UTF8, 0, filepath.c_str(), -1, &narrowPath[0], size, nullptr, nullptr);
			narrowPath.resize(size - 1);

			loadedData = LoadWave(narrowPath.c_str());
		} else {
			loadedData = LoadAudio(filepath.c_str());
		}

		if (!loadedData.pBuffer) {
			return nullptr;
		}

		// shared_ptrに変換してキャッシュに追加
		auto sharedData = std::make_shared<SoundData>(std::move(loadedData));
		soundCache_[filepath] = sharedData;

		return sharedData;
	}

	IXAudio2SourceVoice* Audio::PlayInternal(const SoundData& soundData, bool loop, float volume, SoundCategory category) {
		if (!soundData.pBuffer || soundData.bufferSize == 0) {
			return nullptr;
		}

		HRESULT hr;

		// SourceVoice の作成
		IXAudio2SourceVoice* pSourceVoice = nullptr;
		hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
		if (FAILED(hr)) {
			assert(false && "Failed to create source voice");
			return nullptr;
		}

		// 音量設定（個別音量 × カテゴリ音量）
		float categoryVolume = GetCategoryVolume(category);
		float finalVolume = volume * categoryVolume;
		float clampedVolume = std::max(0.0f, std::min(1.0f, finalVolume));
		pSourceVoice->SetVolume(clampedVolume);

		// XAUDIO2_BUFFER の設定
		XAUDIO2_BUFFER buf = {};
		buf.pAudioData = soundData.pBuffer;
		buf.AudioBytes = soundData.bufferSize;
		buf.Flags = XAUDIO2_END_OF_STREAM;

		if (loop) {
			buf.LoopCount = XAUDIO2_LOOP_INFINITE;
		}

		// バッファの送信
		hr = pSourceVoice->SubmitSourceBuffer(&buf);
		if (FAILED(hr)) {
			pSourceVoice->DestroyVoice();
			assert(false && "Failed to submit source buffer");
			return nullptr;
		}

		// 再生開始
		hr = pSourceVoice->Start();
		if (FAILED(hr)) {
			pSourceVoice->DestroyVoice();
			Logger("再生できません");
			assert(false && "Failed to start source voice");
			return nullptr;
		}

		return pSourceVoice;
	}

	///************************* 従来のAPI(互換性維持) *************************///

	Audio::SoundData Audio::LoadAudio(const wchar_t* filename) {
		SoundData soundData = {};
		IMFSourceReader* pReader = nullptr;
		IMFMediaType* pAudioType = nullptr;
		IMFMediaType* pActualType = nullptr;

		HRESULT hr = MFCreateSourceReaderFromURL(filename, nullptr, &pReader);
		if (FAILED(hr)) {
			assert(false && "Failed to create source reader");
			return soundData;
		}

		// PCM フォーマットを設定
		hr = MFCreateMediaType(&pAudioType);
		if (FAILED(hr)) {
			pReader->Release();
			assert(false && "Failed to create media type");
			return soundData;
		}

		hr = pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
		if (FAILED(hr)) {
			pAudioType->Release();
			pReader->Release();
			assert(false);
			return soundData;
		}

		hr = pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
		if (FAILED(hr)) {
			pAudioType->Release();
			pReader->Release();
			assert(false);
			return soundData;
		}

		hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pAudioType);
		pAudioType->Release();
		pAudioType = nullptr;

		if (FAILED(hr)) {
			pReader->Release();
			assert(false && "Failed to set media type to PCM");
			return soundData;
		}

		// メディアタイプから WAVEFORMATEX を取得
		hr = pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pActualType);
		if (FAILED(hr)) {
			pReader->Release();
			assert(false && "Failed to get current media type");
			return soundData;
		}

		WAVEFORMATEX* pWfx = nullptr;
		hr = MFCreateWaveFormatExFromMFMediaType(pActualType, &pWfx, nullptr);
		pActualType->Release();
		pActualType = nullptr;

		if (FAILED(hr)) {
			pReader->Release();
			assert(false && "Failed to create WaveFormatEx from IMFMediaType");
			return soundData;
		}

		// コピーして SoundData に設定
		memcpy(&soundData.wfex, pWfx, sizeof(WAVEFORMATEX));
		CoTaskMemFree(pWfx);
		pWfx = nullptr;

		// バッファ用のベクターを準備
		std::vector<BYTE> bufferData;

		// データの読み取り
		while (true) {
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
				if (!bufferData.empty() && soundData.pBuffer) {
					delete[] soundData.pBuffer;
					soundData.pBuffer = nullptr;
					soundData.bufferSize = 0;
				}
				assert(false && "Failed to read sample");
				return soundData;
			}

			if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
				break;
			}

			if (pSample) {
				IMFMediaBuffer* pBuffer = nullptr;
				hr = pSample->ConvertToContiguousBuffer(&pBuffer);
				if (FAILED(hr)) {
					pSample->Release();
					pReader->Release();
					assert(false && "Failed to convert sample to contiguous buffer");
					return soundData;
				}

				BYTE* pData = nullptr;
				DWORD maxLength = 0, currentLength = 0;
				hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
				if (FAILED(hr)) {
					pBuffer->Release();
					pSample->Release();
					pReader->Release();
					assert(false && "Failed to lock buffer");
					return soundData;
				}

				bufferData.insert(bufferData.end(), pData, pData + currentLength);

				pBuffer->Unlock();
				pBuffer->Release();
				pSample->Release();
			}
		}

		pReader->Release();
		pReader = nullptr;

		// バッファのサイズとデータを SoundData に設定
		if (!bufferData.empty()) {
			soundData.bufferSize = static_cast<DWORD>(bufferData.size());
			soundData.pBuffer = new BYTE[soundData.bufferSize];
			memcpy(soundData.pBuffer, bufferData.data(), soundData.bufferSize);
		}

		return soundData;
	}

	void Audio::StopAndDestroyVoice(IXAudio2SourceVoice* pSourceVoice) {
		if (pSourceVoice) {
			pSourceVoice->Stop();
			pSourceVoice->DestroyVoice();
		}
	}

	void Audio::SoundUnload(SoundData* soundData) {
		if (soundData && soundData->pBuffer) {
			delete[] soundData->pBuffer;
			soundData->pBuffer = nullptr;
			soundData->bufferSize = 0;
			ZeroMemory(&soundData->wfex, sizeof(WAVEFORMATEX));
		}
	}

	IXAudio2SourceVoice* Audio::SoundPlayAudio(const SoundData& soundData, bool loop) {
		return PlayInternal(soundData, loop, 1.0f, SoundCategory::SE);
	}

	IXAudio2SourceVoice* Audio::SoundPlayWave(const SoundData& soundData, bool loop) {
		return PlayInternal(soundData, loop, 1.0f, SoundCategory::SE);
	}

	void Audio::SetVolume(IXAudio2SourceVoice* pSourceVoice, float volume) {
		if (pSourceVoice) {
			float clampedVolume = std::max(0.0f, std::min(1.0f, volume));
			HRESULT hr = pSourceVoice->SetVolume(clampedVolume);
			if (FAILED(hr)) {
				assert(SUCCEEDED(hr) && "Failed to set volume");
			}
		}
	}

	Audio::SoundData Audio::LoadWave(const char* filename) {
		SoundData soundData = {};

		std::ifstream file;
		file.open(filename, std::ios_base::binary);
		if (!file.is_open()) {
			assert(false && "Failed to open wave file");
			return soundData;
		}

		// RIFFヘッダーの読み込み
		RiffHeader riff;
		file.read((char*)&riff, sizeof(riff));
		if (!file || file.gcount() != sizeof(riff)) {
			file.close();
			assert(false && "Failed to read RIFF header");
			return soundData;
		}

		if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
			file.close();
			assert(false && "Not a RIFF file");
			return soundData;
		}

		if (strncmp(riff.type, "WAVE", 4) != 0) {
			file.close();
			assert(false && "Not a WAVE file");
			return soundData;
		}

		// チャンクのループを開始
		ChunkHeader chunkHeader;
		FormatChunk format = {};
		bool formatFound = false;

		while (file.read((char*)&chunkHeader, sizeof(chunkHeader))) {
			if (!file || file.gcount() != sizeof(chunkHeader)) {
				break;
			}

			if (strncmp(chunkHeader.id, "fmt ", 4) == 0) {
				if (chunkHeader.size > sizeof(format.fmt)) {
					file.close();
					assert(false && "Format chunk size too large");
					return soundData;
				}
				format.chunk = chunkHeader;
				file.read((char*)&format.fmt, chunkHeader.size);
				if (!file || file.gcount() != chunkHeader.size) {
					file.close();
					assert(false && "Failed to read format chunk");
					return soundData;
				}
				formatFound = true;
				break;
			} else {
				file.seekg(chunkHeader.size, std::ios_base::cur);
				if (!file) {
					break;
				}
			}
		}

		if (!formatFound) {
			file.close();
			assert(false && "'fmt ' chunk not found");
			return soundData;
		}

		// Dataチャンクの読み込み
		ChunkHeader data;
		while (file.read((char*)&data, sizeof(data))) {
			if (!file || file.gcount() != sizeof(data)) {
				file.close();
				assert(false && "Failed to read chunk header");
				return soundData;
			}

			if (strncmp(data.id, "JUNK", 4) == 0) {
				file.seekg(data.size, std::ios_base::cur);
				if (!file) {
					file.close();
					assert(false && "Failed to skip JUNK chunk");
					return soundData;
				}
				continue;
			}

			if (strncmp(data.id, "data", 4) == 0) {
				break;
			}

			file.seekg(data.size, std::ios_base::cur);
			if (!file) {
				file.close();
				assert(false && "Failed to skip chunk");
				return soundData;
			}
		}

		if (strncmp(data.id, "data", 4) != 0) {
			file.close();
			assert(false && "Data chunk not found");
			return soundData;
		}

		if (data.size == 0 || data.size > 100 * 1024 * 1024) {
			file.close();
			assert(false && "Invalid data size");
			return soundData;
		}

		char* pBuffer = new(std::nothrow) char[data.size];
		if (!pBuffer) {
			file.close();
			assert(false && "Memory allocation failed");
			return soundData;
		}

		file.read(pBuffer, data.size);
		if (!file || file.gcount() != data.size) {
			delete[] pBuffer;
			file.close();
			assert(false && "Failed to read wave data");
			return soundData;
		}

		file.close();

		soundData.wfex = format.fmt;
		soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
		soundData.bufferSize = data.size;

		return soundData;
	}
}
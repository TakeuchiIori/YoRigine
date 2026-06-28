#pragma once

// C++
#include <Windows.h>
#include <string>
#include <stdexcept>

// LogSystem.h
#pragma once
#include <vector>
#include <string>

class LogSystem {
public:
	static LogSystem& Get() {
		static LogSystem instance;
		return instance;
	}

	void Add(const std::string& msg) {
		logs_.push_back(msg);
	}

	const std::vector<std::string>& GetLogs() const {
		return logs_;
	}

	void Clear() {
		logs_.clear();
	}

private:
	std::vector<std::string> logs_;
};

///************************* ログ出力ユーティリティ *************************///
///
/// デバッグメッセージをVisual Studioの出力ウィンドウに表示する
///
inline void Logger(const std::string& message)
{
	LogSystem::Get().Add(message);
	OutputDebugStringA(message.c_str());
}
// エラーも投げれるように
inline void ThrowError(const std::string& message)
{
	OutputDebugStringA(("ERROR: " + message + "\n").c_str());
	throw std::runtime_error(message);
}

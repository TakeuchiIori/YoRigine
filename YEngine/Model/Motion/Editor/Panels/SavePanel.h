#pragma once
#include "IMotionEditorPanel.h"
#include <filesystem>

// 元々MotionEditor.hにあった構造体をこちらに移動
struct FileBrowserState
{
	std::string currentDirectory = "Resources/Models";
	std::string selectedFilePath = "";
	std::vector<std::string> directoryHistory;
	bool isOpen = false;
	std::string filterExtension = "";
};

class SavePanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override;
	void DrawImGui() override;

private:
	void DrawSaveLoadPopup();
	void DrawFileBrowser(FileBrowserState& state, const char* title);

	// ユーティリティ
	static std::string AnimDisplayName(const std::string& key);
	std::vector<std::filesystem::directory_entry> GetDirectoryEntries(const std::string& dir, const std::string& ext) const;

	MotionEditorContext* context_ = nullptr;

	std::string savePath_ = "Resources/TestBinary/edited_motion.anim";
	std::string saveMsg_ = "";
	FileBrowserState binaryBrowser_;
};

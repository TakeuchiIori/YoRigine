#pragma once
#include "IMotionEditorPanel.h"
#include <filesystem>
#include <vector>

#ifdef USE_IMGUI
#include <FileOperations/FileBrowser.h>
#endif

class SavePanel : public IMotionEditorPanel
{
public:
	void Initialize(MotionEditorContext* context) override;
	void DrawImGui() override;

private:
	void DrawSaveLoadPopup();
	void DrawSourceAnimationPopup();

#ifdef USE_IMGUI
	void DrawBrowserWindow();
	void DrawSourceBrowserWindow();
#endif

	// ユーティリティ
	static std::string AnimDisplayName(const std::string& key);
	static std::string ToObjectModelPath(const std::string& fullPath);
	static std::string ToModelFilePathFromObjectPath(const std::string& objectPath);

	void LoadBinary(const std::string& path);
	void SelectSourceModel(const std::string& path);
	void LoadSourceAnimation();

	MotionEditorContext* context_ = nullptr;

	std::string savePath_ = "Resources/TestBinary/edited_motion.anim";
	std::string saveMsg_ = "";
	std::string sourceModelPath_ = "";
	std::string sourceObjectPath_ = "";
	std::string sourceAnimationName_ = "";
	std::vector<std::string> sourceAnimationNames_;
	std::string sourceMsg_ = "";

#ifdef USE_IMGUI
	YoRigine::FileBrowser fileBrowser_;
	YoRigine::FileBrowser sourceFileBrowser_;
	bool browserOpen_ = false;
	bool sourceBrowserOpen_ = false;
#endif
};

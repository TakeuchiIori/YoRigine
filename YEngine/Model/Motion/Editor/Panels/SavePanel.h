#pragma once
#include "IMotionEditorPanel.h"
#include <filesystem>

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

#ifdef USE_IMGUI
	void DrawBrowserWindow();
#endif

	// ユーティリティ
	static std::string AnimDisplayName(const std::string& key);

	void LoadBinary(const std::string& path);

	MotionEditorContext* context_ = nullptr;

	std::string savePath_ = "Resources/TestBinary/edited_motion.anim";
	std::string saveMsg_ = "";

#ifdef USE_IMGUI
	YoRigine::FileBrowser fileBrowser_;
	bool browserOpen_ = false;
#endif
};
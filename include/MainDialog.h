#pragma once
#include "framework.h"
#include <optional>

#include "BlitterUtils.h"

class MainDialog
{
public:
	MainDialog();

	void Show(HINSTANCE i_hInstance, HWND i_parrent);

private:
	static INT_PTR CALLBACK StaticDialogProc(HWND i_hwnd, UINT i_msg, WPARAM i_wParam, LPARAM i_lParam);
	INT_PTR DialogProc(HWND i_hwnd, UINT i_msg, WPARAM i_wParam, LPARAM i_lParam);

	void Initialize(HWND i_hwnd);

	void OnOpenMainImagePressed(HWND i_hwnd);
	void OnSelectDirectoryPressed(HWND i_hwnd);
	void OnRemoveLockPressed(HWND i_hwnd);
	void OnDecompressPressed(HWND i_hwnd);
	void OnStatusChanged(blitter::StatusInfo& i_status);

	bool ValidateAndShowError(HWND i_hwnd, const std::filesystem::path& i_input, const std::filesystem::path& i_output);

	void RefreshImageInformation();

	std::optional<std::filesystem::path> OpenImageFile(HWND i_hwnd, uint8_t i_idx) const;

	void OnProgressChanged(uint8_t i_percent);

	blitter::BlitterUtils m_utils;
	HWND m_hwnd = nullptr;
	HWND m_decompressButton = nullptr;
	HWND m_progressBar = nullptr;
	HWND m_imagePathBox = nullptr;
	HWND m_outputDirectoryPathBox = nullptr;
	HWND m_shatusLabel = nullptr;
	HWND m_imageDateTimeLabel = nullptr;
	HWND m_imageVersionLabel = nullptr;
	HWND m_modelLocksList = nullptr;
	HWND m_removeLocksButton = nullptr;

	bool m_hasError = false;
};


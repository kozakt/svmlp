#include "MainDialog.h"

#include "Resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <filesystem>

#include "BlitterUtils.h"

MainDialog::MainDialog()
    : m_utils(2u, [this](blitter::StatusInfo& i_status) { OnStatusChanged(i_status); }, [this](uint8_t i_percent) { OnProgressChanged(i_percent); })
{
}

void MainDialog::Show(HINSTANCE i_hInstance, HWND i_parrent)
{
	DialogBoxParamW(i_hInstance, MAKEINTRESOURCE(IDD_MAINDIALOG), i_parrent, MainDialog::StaticDialogProc, (LPARAM)this);
}

INT_PTR MainDialog::StaticDialogProc(HWND i_hwnd, UINT i_msg, WPARAM i_wParam, LPARAM i_lParam)
{
    MainDialog* dialog_pointer = nullptr;

    if (i_msg == WM_INITDIALOG)
    {
        // 2. Retrieve 'this' pointer from DialogBoxParam's LPARAM
        dialog_pointer = reinterpret_cast<MainDialog*>(i_lParam);
        // 3. Store it in the dialog's reserved user data slot
        SetWindowLongPtr(i_hwnd, DWLP_USER, reinterpret_cast<LONG_PTR>(dialog_pointer));

    }
    else
    {
        // 4. Retrieve stored pointer for all other messages
        dialog_pointer = reinterpret_cast<MainDialog*>(GetWindowLongPtr(i_hwnd, DWLP_USER));
    }

    if (dialog_pointer)
    {
        return dialog_pointer->DialogProc(i_hwnd, i_msg, i_wParam, i_lParam);
    }

    return (INT_PTR)FALSE;
}

INT_PTR MainDialog::DialogProc(HWND i_hwnd, UINT i_msg, WPARAM i_wParam, LPARAM i_lParam)
{
    switch (i_msg)
    {
    case WM_INITDIALOG:
        Initialize(i_hwnd);

        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(i_wParam) == IDOK || LOWORD(i_wParam) == IDCANCEL)
        {
            EndDialog(i_hwnd, LOWORD(i_wParam));
            return (INT_PTR)TRUE;
        }

        if (LOWORD(i_wParam) == IDC_DECOMPRESS_BUTTON)
        {
            OnDecompressPressed(i_hwnd);

            return (INT_PTR)TRUE;
        }
        
        if (LOWORD(i_wParam) == IDC_REMOVE_MODEL_LOCKS_BUTTON)
        {
            OnRemoveLockPressed(i_hwnd);

            return (INT_PTR)TRUE;
        }

        if (LOWORD(i_wParam) == IDC_OPEN_IMAGE_BUTTON)
        {
            OnOpenMainImagePressed(i_hwnd);
            return (INT_PTR)TRUE;
        }
        
        if (LOWORD(i_wParam) == IDC_SELECT_OUTPUT_DIRECTORY_BUTTON)
        {
            OnSelectDirectoryPressed(i_hwnd);
            return (INT_PTR)TRUE;
        }

        if (HIWORD(i_wParam) == EN_CHANGE)
        {
            WORD controlID = LOWORD(i_wParam);
            if (controlID == IDC_MAIN_IMAGE_FILE_PATH_BOX || controlID == IDC_OUTPUT_DIRECTORY_PATH_BOX)
            {
                RefreshImageInformation();
                return (INT_PTR)TRUE;
            }
        }

        break;
    case WM_CTLCOLORSTATIC:
        if ((HWND)i_lParam == m_shatusLabel)
        {
            HDC hdcCtrl = (HDC)i_wParam;
            COLORREF color = m_hasError ? RGB(255, 0, 0) : GetSysColor(COLOR_WINDOWTEXT);
            SetTextColor(hdcCtrl, color);
            SetBkMode(hdcCtrl, TRANSPARENT);

            return (INT_PTR)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;

    case WM_CLOSE:
        EndDialog(i_hwnd, IDCANCEL);
        return TRUE;

    default:
        return (INT_PTR)FALSE;
    }

    return (INT_PTR)FALSE;
}

void MainDialog::Initialize(HWND i_hwnd)
{
    m_hwnd = i_hwnd;
    m_removeLocksButton = GetDlgItem(i_hwnd, IDC_REMOVE_MODEL_LOCKS_BUTTON);
    m_decompressButton = GetDlgItem(i_hwnd, IDC_DECOMPRESS_BUTTON);
    m_progressBar = GetDlgItem(i_hwnd, IDC_TASK_PROGRESS_BAR);
    m_imagePathBox = GetDlgItem(i_hwnd, IDC_MAIN_IMAGE_FILE_PATH_BOX);
    m_outputDirectoryPathBox = GetDlgItem(i_hwnd, IDC_OUTPUT_DIRECTORY_PATH_BOX);
    m_shatusLabel = GetDlgItem(i_hwnd, IDC_STATUS_LABEL);
    m_imageDateTimeLabel = GetDlgItem(i_hwnd, IDC_DATE_TIME_LABEL);
    m_imageVersionLabel = GetDlgItem(i_hwnd, IDC_IMAGE_VERSION_LABEL);
    m_modelLocksList = GetDlgItem(i_hwnd, IDC_MODEL_LOCKS_LIST);

    //ShowWindow(m_progressBar, SW_HIDE);

    EnableWindow(m_removeLocksButton, FALSE);
    EnableWindow(m_decompressButton, FALSE);
    
    SetWindowText(m_shatusLabel, L"Open main image file (*.IMG)");
}

void MainDialog::OnOpenMainImagePressed(HWND i_hwnd)
{
    auto optPath = OpenImageFile(i_hwnd, 0u);

    if (!optPath.has_value())
    {
        return;
    }

    std::wstring filepath = optPath->wstring();

    SetWindowText(m_imagePathBox, filepath.c_str());
}

void MainDialog::OnSelectDirectoryPressed(HWND i_hwnd)
{
    IFileOpenDialog* fileOpen = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL,
        IID_IFileOpenDialog, reinterpret_cast<void**>(&fileOpen));

    if (SUCCEEDED(hr))
    {
        FILEOPENDIALOGOPTIONS options{};
        hr = fileOpen->GetOptions(&options);
        if (SUCCEEDED(hr))
        {
            hr = fileOpen->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
        }

        if (SUCCEEDED(hr))
        {
            hr = fileOpen->Show(i_hwnd);
        }

        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            hr = fileOpen->GetResult(&item);
            if (SUCCEEDED(hr))
            {
                LPWSTR path = nullptr;
                hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
                item->Release();
                SetWindowText(m_outputDirectoryPathBox, path);
            }
        }
        fileOpen->Release();
    }
}

void MainDialog::OnRemoveLockPressed(HWND i_hwnd)
{
    TCHAR imagePathStr[MAX_PATH];
    GetWindowText(m_imagePathBox, imagePathStr, MAX_PATH);

    TCHAR outputPathStr[MAX_PATH];
    GetWindowText(m_outputDirectoryPathBox, outputPathStr, MAX_PATH);

    std::filesystem::path imagePath(imagePathStr);
    std::filesystem::path outputPath(outputPathStr);
    outputPath.append(imagePath.filename().string());

    if (!ValidateAndShowError(i_hwnd, imagePath, outputPath))
    {
        return;
    }

    if (m_utils.PrepareForDecompression())
    {
        EnableWindow(m_removeLocksButton, FALSE);
        EnableWindow(m_decompressButton, FALSE);

        std::ofstream outputFile(outputPath, std::ios::out | std::ios::binary);

        m_utils.RemoveLocksAsync(imagePath, std::move(outputFile));
    }
}

void MainDialog::OnDecompressPressed(HWND i_hwnd)
{
    TCHAR imagePathStr[MAX_PATH];
    GetWindowText(m_imagePathBox, imagePathStr, MAX_PATH);

    TCHAR outputPathStr[MAX_PATH];
    GetWindowText(m_outputDirectoryPathBox, outputPathStr, MAX_PATH);

    std::filesystem::path imagePath(imagePathStr);
    std::filesystem::path outputPath(outputPathStr);
    outputPath.append("decompressed.bin");

    if (!ValidateAndShowError(i_hwnd, imagePath, outputPath))
    {
        return;
    }

    if (m_utils.PrepareForDecompression())
    {
        EnableWindow(m_removeLocksButton, FALSE);
        EnableWindow(m_decompressButton, FALSE);

        std::ofstream outputFile(outputPath, std::ios::out | std::ios::binary);

        m_utils.UncompressAsync(imagePath, std::move(outputFile));
    }
}

void MainDialog::OnStatusChanged(blitter::StatusInfo& i_status)
{
    if (!i_status.errorMessage.empty())
    {
        MessageBox(m_hwnd, i_status.errorMessage.c_str(), L"Error", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (i_status.requireNextDecompress)
    {
        MessageBox(m_hwnd, L"Next image part is required!\n\nClick ok and select next part.", L"Next part required", MB_OK | MB_ICONINFORMATION);

        auto optPath = OpenImageFile(m_hwnd, i_status.nextPartIdx);

        if (!optPath.has_value())
        {
            return;
        }

        m_utils.UncompressAsync(optPath.value(), std::move(i_status.outputStream));

        return;
    }
    
    if (i_status.requireNextRemoveLocks)
    {
        MessageBox(m_hwnd, L"Next image part is required!\n\nClick ok and select next part.", L"Next part required", MB_OK | MB_ICONINFORMATION);

        auto optPath = OpenImageFile(m_hwnd, i_status.nextPartIdx);

        if (!optPath.has_value())
        {
            return;
        }

        TCHAR outputPathStr[MAX_PATH];
        GetWindowText(m_outputDirectoryPathBox, outputPathStr, MAX_PATH);

        std::filesystem::path outputPath(outputPathStr);
        outputPath.append(optPath->filename().string());

        if (!ValidateAndShowError(m_hwnd, optPath.value(), outputPath))
        {
            return;
        }

        std::ofstream outputFile(outputPath, std::ios::out | std::ios::binary);

        m_utils.RemoveLocksAsync(optPath.value(), std::move(outputFile));

        return;
    }

    MessageBox(m_hwnd, L"Operation successfully completed.", L"Completed", MB_OK | MB_ICONINFORMATION);
}

bool MainDialog::ValidateAndShowError(HWND i_hwnd, const std::filesystem::path& i_input, const std::filesystem::path& i_output)
{
    if (!std::filesystem::is_regular_file(i_input))
    {
        m_hasError = true;
        SetWindowText(m_shatusLabel, L"Incorrect image file path!");
        return false;
    }

    if (!std::filesystem::is_directory(i_output.parent_path()))
    {
        SetWindowText(m_shatusLabel, L"Select output folder for patched files.");
        return false;
    }

    if (i_input == i_output)
    {
        m_hasError = true;
        SetWindowText(m_shatusLabel, L"Input and output file names are the same!");
        return false;
    }

    if (std::filesystem::exists(i_output))
    {
        std::wstring message = std::format(L"File: {}\n\nAlready exists!\n\nDo you want to override it?", i_output.wstring());

        int result = MessageBox(i_hwnd, message.c_str(), L"File already exists.", MB_YESNO | MB_ICONQUESTION);

        if (result == IDNO)
        {
            return false;
        }
    }
    
    return true;
}

void MainDialog::RefreshImageInformation()
{
    m_hasError = false;
    EnableWindow(m_removeLocksButton, FALSE);
    EnableWindow(m_decompressButton, FALSE);

    TCHAR imagePathStr[MAX_PATH];
    GetWindowText(m_imagePathBox, imagePathStr, MAX_PATH);
    
    TCHAR outputPathStr[MAX_PATH];
    GetWindowText(m_outputDirectoryPathBox, outputPathStr, MAX_PATH);

    std::filesystem::path imagePath(imagePathStr);
    std::filesystem::path outputPath(outputPathStr);

    if (!std::filesystem::is_regular_file(imagePath))
    {
        m_hasError = true;
        SetWindowText(m_shatusLabel, L"Incorrect image file path!");
        return;
    }

    blitter::BlitterUtils util{0u, nullptr, nullptr};
    auto optInfo = util.GetImageInfo(imagePath);
    if (!optInfo.has_value())
    {
        m_hasError = true;
        SetWindowText(m_shatusLabel, L"Can't get image info!");
        return;
    }

    SendMessage(m_modelLocksList, LB_RESETCONTENT, 0, 0);

    for (const std::wstring& modelLock : optInfo->modelLocks)
    {
        SendMessage(m_modelLocksList, LB_ADDSTRING, 0, (LPARAM)modelLock.c_str());
    }
    
    std::wstring versionStr = std::format(L"Image version: {:X}", optInfo->version);
    SetWindowText(m_imageVersionLabel, versionStr.c_str());

    auto time_point = std::chrono::system_clock::from_time_t(optInfo->timestamp);
    // Format to a human-readable string (e.g., "YYYY-MM-DD HH:MM:SS")
    std::wstring datetimeStr = std::format(L"Date time: {:%d-%m-%Y %H:%M}", time_point);

    SetWindowText(m_imageDateTimeLabel, datetimeStr.c_str());

    if (!std::filesystem::is_directory(outputPath))
    {
        SetWindowText(m_shatusLabel, L"Select output folder for patched files.");
        return;
    }

    std::wstring readyStr = std::format(L"Ready");
    SetWindowText(m_shatusLabel, readyStr.c_str());

    EnableWindow(m_removeLocksButton, TRUE);
    EnableWindow(m_decompressButton, TRUE);
}

std::optional<std::filesystem::path> MainDialog::OpenImageFile(HWND i_hwnd, uint8_t i_idx) const
{
    std::vector<std::pair<std::wstring, std::wstring>> filters{};

    std::wstring filterStr1 = i_idx == 0u ? L"Sony Image (*.IMG)" : std::format(L"Sony Image Part (*.I{:02})", i_idx);
    std::wstring filterStr2 = i_idx == 0u ? L"*.IMG" : std::format(L"*.I{:02}", i_idx);
    filters.emplace_back(filterStr1, filterStr2);
    filters.emplace_back(L"All Files", L"*.*");

    std::vector<wchar_t> filtersBuffer{};
    for (const auto& [desc, mask] : filters)
    {
        filtersBuffer.insert(filtersBuffer.end(), desc.begin(), desc.end());
        filtersBuffer.push_back(L'\0');

        filtersBuffer.insert(filtersBuffer.end(), mask.begin(), mask.end());
        filtersBuffer.push_back(L'\0');
    }
    filtersBuffer.push_back(L'\0'); // Final terminator for the whole list


    OPENFILENAME ofn{}; // Common dialog box structure
    std::wstring path{};
    path.resize(MAX_PATH);
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = i_hwnd; // Handle to owner window
    ofn.lpstrFile = path.data();
    ofn.nMaxFile = static_cast<DWORD>(path.size());
    ofn.lpstrFilter = filtersBuffer.data();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE)
    {
        return std::filesystem::path(ofn.lpstrFile);
    }

    return std::nullopt;
}

void MainDialog::OnProgressChanged(uint8_t i_percent)
{
    SendMessage(m_progressBar, PBM_SETPOS, (WPARAM)i_percent, 0);
}

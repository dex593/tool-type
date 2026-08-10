#define wWinMain ToolTypeDisabledWinMain
#include "../main.cpp"
#undef wWinMain

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

struct TestArchiveEntry {
    PtsEntryKind kind;
    std::wstring rootToken;
    std::wstring relativePath;
    std::wstring displayName;
    std::vector<uint8_t> data;
};

struct BackgroundUiProbe {
    bool heartbeatReceived = false;
    bool progressReceived = false;
    bool completionReceived = false;
    bool operationSucceeded = false;
    bool operationCancelled = false;
    DWORD workerThreadId = 0;
};

struct BackgroundTestResources {
    HWND window = nullptr;
    HANDLE worker = nullptr;
    HANDLE workerStarted = nullptr;
    HANDLE releaseWorker = nullptr;
    PtsCancellationHandle cancellation;

    ~BackgroundTestResources() {
        RequestPtsCancellation(cancellation);
        if (releaseWorker) SetEvent(releaseWorker);
        if (worker) {
            WaitForSingleObject(worker, 3000);
            CloseHandle(worker);
        }
        if (window) {
            MSG pending{};
            while (PeekMessageW(&pending, window, WM_APP_PTS_PROGRESS,
                                WM_APP_PTS_OPERATION_DONE, PM_REMOVE)) {
                if (pending.message == WM_APP_PTS_PROGRESS) {
                    delete reinterpret_cast<PtsProgressMessage*>(pending.lParam);
                } else if (pending.message == WM_APP_PTS_OPERATION_DONE) {
                    delete reinterpret_cast<PtsBackgroundResult*>(pending.lParam);
                }
            }
            DestroyWindow(window);
        }
        if (workerStarted) CloseHandle(workerStarted);
        if (releaseWorker) CloseHandle(releaseWorker);
    }
};

constexpr UINT kBackgroundUiHeartbeat = WM_APP + 120;

LRESULT CALLBACK BackgroundUiProbeWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* probe = reinterpret_cast<BackgroundUiProbe*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        probe = reinterpret_cast<BackgroundUiProbe*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(probe));
    }
    if (!probe) return DefWindowProcW(hwnd, message, wParam, lParam);

    if (message == kBackgroundUiHeartbeat) {
        probe->heartbeatReceived = true;
        return 0;
    }
    if (message == WM_APP_PTS_PROGRESS) {
        auto* progress = reinterpret_cast<PtsProgressMessage*>(lParam);
        probe->progressReceived = progress && progress->percent == 42;
        delete progress;
        return 0;
    }
    if (message == WM_APP_PTS_OPERATION_DONE) {
        auto* result = reinterpret_cast<PtsBackgroundResult*>(lParam);
        if (result) {
            probe->completionReceived = true;
            probe->operationSucceeded = result->ok;
            probe->operationCancelled = result->cancelled;
            probe->workerThreadId = result->workerThreadId;
        }
        delete result;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool PumpMessagesUntil(const bool& condition, DWORD timeoutMs) {
    ULONGLONG deadline = GetTickCount64() + timeoutMs;
    while (!condition && GetTickCount64() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(1);
    }
    return condition;
}

class ScopedEnvironmentVariable {
public:
    ScopedEnvironmentVariable(const wchar_t* name, const std::wstring& value) : name_(name) {
        DWORD needed = GetEnvironmentVariableW(name, nullptr, 0);
        existed_ = needed > 0;
        if (existed_) {
            oldValue_.resize(needed);
            DWORD written = GetEnvironmentVariableW(name, oldValue_.data(), needed);
            oldValue_.resize(written);
        }
        SetEnvironmentVariableW(name, value.c_str());
    }

    ~ScopedEnvironmentVariable() {
        SetEnvironmentVariableW(name_.c_str(), existed_ ? oldValue_.c_str() : nullptr);
    }

private:
    std::wstring name_;
    std::wstring oldValue_;
    bool existed_ = false;
};

class ScopedRegistryValues {
public:
    explicit ScopedRegistryValues(HKEY key) : key_(key) {}

    ~ScopedRegistryValues() {
        if (!key_) return;
        for (const auto& name : valueNames_) RegDeleteValueW(key_, name.c_str());
        RegCloseKey(key_);
    }

    void Add(const std::wstring& name) { valueNames_.push_back(name); }
    HKEY Get() const { return key_; }

private:
    HKEY key_ = nullptr;
    std::vector<std::wstring> valueNames_;
};

void Expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void WriteTestArchive(const std::wstring& path,
                      const std::vector<TestArchiveEntry>& entries) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    Expect(file != INVALID_HANDLE_VALUE, "could not create test archive");
    bool ok = WriteAll(file, kAfangMagic, sizeof(kAfangMagic)) &&
              WriteU32File(file, kAfangVersion) &&
              WriteU32File(file, static_cast<uint32_t>(entries.size()));
    for (const auto& entry : entries) {
        ok = ok && WriteU8(file, static_cast<uint8_t>(entry.kind)) &&
             WriteUtf8String(file, entry.rootToken) &&
             WriteUtf8String(file, entry.relativePath) &&
             WriteUtf8String(file, entry.displayName) &&
             WriteU64File(file, entry.data.size()) &&
             WriteU64File(file, entry.data.size()) && WriteU8(file, 0) &&
             (entry.data.empty() ||
              WriteAll(file, entry.data.data(), static_cast<DWORD>(entry.data.size())));
    }
    CloseHandle(file);
    Expect(ok, "could not write test archive");
}

void TestSafeArchivePaths() {
    Expect(IsSafeRelativePath(L"Adobe\\Adobe Photoshop 2025\\Presets\\Actions.atn"),
           "valid Photoshop path rejected");
    Expect(!IsSafeRelativePath(L"..\\Startup\\payload.exe"),
           "parent traversal accepted");
    Expect(!IsSafeRelativePath(L"C:\\Windows\\Fonts\\payload.ttf"),
           "drive path accepted");
    Expect(!IsSafeRelativePath(L"Adobe\\\\payload.psp"),
           "empty segment accepted");
    Expect(!IsSafeRelativePath(L"Adobe\\Adobe Photoshop 2025\\.\\Prefs.psp"),
           "dot path segment accepted");
    Expect(!IsSafeRelativePath(L"Adobe\\CON\\Prefs.psp"),
           "reserved Windows path segment accepted");
    Expect(!IsSafeRelativePath(L"Adobe\\CON.backup.psp"),
           "reserved Windows device prefix with multiple extensions accepted");
    Expect(!IsSafeRelativePath(std::wstring(L"Adobe\\bad") + wchar_t{0x1f} + L"name.psp"),
           "Windows control character accepted in path segment");

    std::wstring error;
    Expect(ValidatePtsArchiveEntryMetadata(
               static_cast<uint8_t>(PtsEntryKind::Font), L"FONT", L"CustomFont.ttf",
               32, 32, 0, error),
           "valid font metadata rejected");
    error.clear();
    Expect(!ValidatePtsArchiveEntryMetadata(
               static_cast<uint8_t>(PtsEntryKind::Font), L"FONT", L"folder\\CustomFont.ttf",
               32, 32, 0, error),
           "font path with directory accepted");
    error.clear();
    Expect(!ValidatePtsArchiveEntryMetadata(
               static_cast<uint8_t>(PtsEntryKind::Font), L"FONT", L"CON.ttf",
               32, 32, 0, error),
           "reserved Windows font filename accepted");
    error.clear();
    Expect(!ValidatePtsArchiveEntryMetadata(
               static_cast<uint8_t>(PtsEntryKind::Font), L"FONT", L"CON.backup.ttf",
               32, 32, 0, error),
           "reserved Windows font device prefix with multiple extensions accepted");
    error.clear();
    Expect(!ValidatePtsArchiveEntryMetadata(99, L"FONT", L"CustomFont.ttf",
                                             32, 32, 0, error),
           "unknown archive entry kind accepted");
}

void TestCompressionRoundTrip() {
    std::vector<uint8_t> original(8192, static_cast<uint8_t>('A'));
    uint8_t method = 0;
    std::vector<uint8_t> stored;
    CompressForArchive(original, method, stored);
    Expect(method == 1, "compressible data was stored raw");
    Expect(stored.size() < original.size(), "compression did not reduce size");

    std::vector<uint8_t> restored;
    Expect(DecompressFromArchive(stored, method, original.size(), restored),
           "compressed payload did not restore");
    Expect(restored == original, "archive payload changed after round trip");
    Expect(!DecompressFromArchive(stored, 7, original.size(), restored),
           "unknown compression method accepted");
}

void TestArchiveInspectionReportsProgress() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-inspection-progress";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root);

    std::vector<TestArchiveEntry> entries;
    for (int i = 0; i < 160; ++i) {
        entries.push_back(TestArchiveEntry{
            PtsEntryKind::Font, L"FONT", L"ProgressFont" + std::to_wstring(i) + L".ttf",
            L"Progress Font " + std::to_wstring(i), {}});
    }
    fs::path archive = root / L"progress.afang";
    WriteTestArchive(archive.wstring(), entries);

    int callbackCount = 0;
    int lastPercent = -1;
    PtsArchiveScanResult scan{};
    std::wstring error;
    auto progress = [&](int percent, const std::wstring&) {
        ++callbackCount;
        Expect(percent >= lastPercent, "archive inspection progress moved backwards");
        lastPercent = percent;
    };
    Expect(InspectPtsArchiveVersions(archive.wstring(), scan, error, progress),
           "archive inspection with progress failed");
    Expect(scan.entryCount == entries.size(), "archive inspection lost entries");
    Expect(callbackCount >= 3, "archive inspection did not report periodic progress");
    fs::remove_all(root, ignored);
}

void TestPtsBackgroundTaskKeepsUiThreadResponsive() {
    const wchar_t* className = L"ToolTypePtsBackgroundUiProbe";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = BackgroundUiProbeWndProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = className;
    ATOM atom = RegisterClassW(&windowClass);
    Expect(atom != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS,
           "could not register background UI probe window");

    BackgroundUiProbe probe{};
    BackgroundTestResources resources{};
    resources.window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                       nullptr, windowClass.hInstance, &probe);
    Expect(resources.window != nullptr, "could not create background UI probe window");

    resources.workerStarted = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    resources.releaseWorker = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Expect(resources.workerStarted && resources.releaseWorker,
           "could not create background task test events");

    DWORD operationThreadId = 0;
    PtsBackgroundOperation operation = [&](const PtsProgressCallback& progress,
                                            const PtsCancellationCallback&,
                                            std::wstring& summary,
                                            std::wstring&) {
        operationThreadId = GetCurrentThreadId();
        SetEvent(resources.workerStarted);
        WaitForSingleObject(resources.releaseWorker, INFINITE);
        progress(42, L"Đang restore font trong worker...");
        summary = L"Hoàn tất background test.";
        return true;
    };

    resources.cancellation = CreatePtsCancellationHandle();
    std::wstring error;
    Expect(StartPtsBackgroundTask(resources.window, operation, resources.cancellation,
                                  resources.worker, error),
           "could not start Pts background task");
    Expect(WaitForSingleObject(resources.workerStarted, 2000) == WAIT_OBJECT_0,
           "Pts background task did not start");
    Expect(operationThreadId != GetCurrentThreadId(),
           "Pts operation still ran on the UI thread");
    Expect(WaitForSingleObject(resources.worker, 0) == WAIT_TIMEOUT,
           "Pts background task unexpectedly completed before release");

    PostMessageW(resources.window, kBackgroundUiHeartbeat, 0, 0);
    Expect(PumpMessagesUntil(probe.heartbeatReceived, 1000),
           "UI thread could not process messages while Pts task was running");
    Expect(!probe.completionReceived,
           "Pts task completed before the slow operation was released");

    SetEvent(resources.releaseWorker);
    Expect(PumpMessagesUntil(probe.completionReceived, 2000),
           "Pts background completion message was not delivered");
    Expect(probe.progressReceived, "Pts background progress message was not delivered");
    Expect(probe.operationSucceeded, "Pts background operation reported failure");
    Expect(probe.workerThreadId == operationThreadId,
           "Pts completion did not identify the worker thread");
    Expect(WaitForSingleObject(resources.worker, 2000) == WAIT_OBJECT_0,
           "Pts background worker did not exit");
}

void TestPtsBackgroundTaskCanBeCancelled() {
    const wchar_t* className = L"ToolTypePtsBackgroundUiProbe";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = BackgroundUiProbeWndProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = className;
    RegisterClassW(&windowClass);

    BackgroundUiProbe probe{};
    BackgroundTestResources resources{};
    resources.window = CreateWindowExW(0, className, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                       nullptr, windowClass.hInstance, &probe);
    Expect(resources.window != nullptr, "could not create cancellation probe window");

    resources.workerStarted = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Expect(resources.workerStarted != nullptr, "could not create cancellation start event");
    PtsBackgroundOperation operation = [&](const PtsProgressCallback&,
                                            const PtsCancellationCallback& cancelled,
                                            std::wstring&,
                                            std::wstring& error) {
        SetEvent(resources.workerStarted);
        while (!cancelled()) Sleep(1);
        error = L"Đã hủy thao tác Pts.";
        return false;
    };

    resources.cancellation = CreatePtsCancellationHandle();
    std::wstring error;
    Expect(StartPtsBackgroundTask(resources.window, operation, resources.cancellation,
                                  resources.worker, error),
           "could not start cancellable Pts task");
    Expect(WaitForSingleObject(resources.workerStarted, 2000) == WAIT_OBJECT_0,
           "cancellable Pts task did not start");
    RequestPtsCancellation(resources.cancellation);
    Expect(PumpMessagesUntil(probe.completionReceived, 2000),
           "cancelled Pts task did not report completion");
    Expect(probe.operationCancelled, "Pts completion did not report cancellation");
    Expect(!probe.operationSucceeded, "cancelled Pts task incorrectly reported success");

    Expect(WaitForSingleObject(resources.worker, 2000) == WAIT_OBJECT_0,
           "cancelled Pts worker did not exit");
}

void TestCrossVersionMappingAndCs6Aliases() {
    std::wstring mapped;
    Expect(BuildMappedPhotoshopSettingsRelativePath(
               L"Adobe\\Adobe Photoshop 2025\\Adobe Photoshop 2025 Settings\\Adobe Photoshop 2025 Prefs.psp",
               L"Adobe\\Adobe Photoshop 2025", L"Adobe\\Adobe Photoshop CS6",
               L"Adobe Photoshop 2025", L"Adobe Photoshop CS6", mapped),
           "2025 to CS6 path mapping failed");
    Expect(mapped ==
               L"Adobe\\Adobe Photoshop CS6\\Adobe Photoshop CS6 Settings\\Adobe Photoshop CS6 Prefs.psp",
           "2025 labels were not rewritten for CS6");

    auto aliases = BuildPhotoshopSettingsRestoreRelativePaths(
        L"Adobe\\Adobe Photoshop CS6\\Adobe Photoshop CS6 Settings\\Adobe Photoshop CS6 Prefs.psp",
        L"Adobe Photoshop 2025", L"Adobe Photoshop CS6");
    bool hasLegacyName = false;
    for (const auto& alias : aliases) {
        if (alias.find(L"Adobe Photoshop X64 CS6") != std::wstring::npos) {
            hasLegacyName = true;
            break;
        }
    }
    Expect(hasLegacyName, "CS6 legacy filename alias missing");

    auto workspaceAliases = BuildPhotoshopSettingsRestoreRelativePaths(
        L"Adobe\\Adobe Photoshop CS6\\Adobe Photoshop CS6 Settings\\WorkSpaces\\Editing.psw",
        L"Adobe Photoshop 2025", L"Adobe Photoshop CS6");
    bool hasModifiedWorkspace = false;
    bool hasExtensionlessWorkspace = false;
    for (const auto& alias : workspaceAliases) {
        if (alias.find(L"WorkSpaces (Modified)") != std::wstring::npos) {
            hasModifiedWorkspace = true;
        }
        if (alias.size() >= 7 && alias.substr(alias.size() - 7) == L"Editing") {
            hasExtensionlessWorkspace = true;
        }
    }
    Expect(hasModifiedWorkspace, "CS6 modified-workspace alias missing");
    Expect(hasExtensionlessWorkspace, "CS6 extensionless workspace alias missing");
}

void TestFontCollisionPath() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-font-collision";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root);
    fs::path desired = root / L"CustomFont.ttf";
    fs::path firstCollision = root / L"CustomFont-restored-1.ttf";
    {
        std::ofstream file(desired, std::ios::binary);
        file << "existing";
    }
    bool identical = false;
    std::vector<uint8_t> replacement{1, 2, 3};
    Expect(std::filesystem::path(ReusableOrNonConflictingFontPath(
               desired.wstring(), replacement, identical)) == firstCollision,
           "font collision name is not deterministic");
    Expect(!identical, "different font collision was treated as identical");
    fs::remove_all(root, ignored);
}

void TestCrossVersionArchiveRestore() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-cross-version";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root / L"Roaming");
    fs::create_directories(root / L"Local");
    ScopedEnvironmentVariable appData(L"APPDATA", (root / L"Roaming").wstring());
    ScopedEnvironmentVariable localAppData(L"LOCALAPPDATA", (root / L"Local").wstring());

    const std::string workspaceXml =
        "<photoshop-workspace version=\"2.0\"><frame id=\"123\" "
        "active-palette=\"456\"/></photoshop-workspace>";
    TestArchiveEntry setting{
        PtsEntryKind::PhotoshopSetting,
        L"APPDATA",
        L"Adobe\\Adobe Photoshop 2025\\Adobe Photoshop 2025 Settings\\WorkSpaces\\Editing.psw",
        L"Adobe Photoshop 2025",
        std::vector<uint8_t>(workspaceXml.begin(), workspaceXml.end())};
    fs::path archive = root / L"cross-version.afang";
    WriteTestArchive(archive.wstring(), {setting});

    PtsRestoreOptions options{};
    options.sourceVersionLabel = L"Adobe Photoshop 2025";
    options.targetVersionLabel = L"Adobe Photoshop CS6";
    options.targetAppDataRelativeRoot = L"Adobe\\Adobe Photoshop CS6";
    options.targetLocalAppDataRelativeRoot = L"Adobe\\Adobe Photoshop CS6";
    options.archiveHasSettings = true;
    std::wstring summary;
    std::wstring error;
    auto progress = [](int, const std::wstring&) {};
    Expect(RestorePtsBackupArchive(archive.wstring(), options, progress, summary, error),
           "cross-version archive restore failed");

    fs::path settingsRoot = root / L"Roaming" / L"Adobe" / L"Adobe Photoshop CS6" /
                            L"Adobe Photoshop CS6 Settings";
    fs::path primary = settingsRoot / L"WorkSpaces" / L"Editing.psw";
    fs::path modified = settingsRoot / L"WorkSpaces (Modified)" / L"Editing.psw";
    fs::path extensionless = settingsRoot / L"WorkSpaces" / L"Editing";
    Expect(fs::exists(primary), "primary CS6 workspace was not restored");
    Expect(fs::exists(modified), "modified-workspace alias was not written");
    Expect(fs::exists(extensionless), "extensionless CS6 alias was not written");

    std::ifstream restored(primary, std::ios::binary);
    std::string restoredXml((std::istreambuf_iterator<char>(restored)),
                            std::istreambuf_iterator<char>());
    Expect(restoredXml.find("version=\"1.0\"") != std::string::npos,
           "CS6 workspace version was not normalized");
    Expect(restoredXml.find("version=\"2.0\"") == std::string::npos,
           "modern workspace version remained in CS6 output");

    for (const auto& item : fs::recursive_directory_iterator(root)) {
        Expect(item.path().filename().wstring().find(L"before-restore") == std::wstring::npos,
               "restore created a forbidden before-restore directory");
    }
    fs::remove_all(root, ignored);
}

void TestSettingsBackupArchive() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-settings-backup";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::path appDataRoot = root / L"Roaming";
    fs::path localRoot = root / L"Local";
    fs::path programFiles = root / L"ProgramFiles";
    fs::path install = programFiles / L"Adobe" / L"Adobe Photoshop 2025";
    fs::path install2024 = programFiles / L"Adobe" / L"Adobe Photoshop 2024";
    fs::path settings = appDataRoot / L"Adobe" / L"Adobe Photoshop 2025" /
                        L"Adobe Photoshop 2025 Settings";
    fs::path settings2024 = appDataRoot / L"Adobe" / L"Adobe Photoshop 2024" /
                            L"Adobe Photoshop 2024 Settings";
    fs::create_directories(install);
    fs::create_directories(install2024);
    fs::create_directories(settings);
    fs::create_directories(settings2024);
    {
        std::ofstream photoshop(install / L"Photoshop.exe", std::ios::binary);
        photoshop << "test";
        std::ofstream preference(settings / L"Adobe Photoshop 2025 Prefs.psp",
                                 std::ios::binary);
        preference << "preference-data";
        std::ofstream photoshop2024(install2024 / L"Photoshop.exe", std::ios::binary);
        photoshop2024 << "test";
        std::ofstream preference2024(settings2024 / L"Adobe Photoshop 2024 Prefs.psp",
                                     std::ios::binary);
        preference2024 << "older-preference-data";
    }

    ScopedEnvironmentVariable appData(L"APPDATA", appDataRoot.wstring());
    ScopedEnvironmentVariable localAppData(L"LOCALAPPDATA", localRoot.wstring());
    ScopedEnvironmentVariable pf(L"ProgramFiles", programFiles.wstring());
    ScopedEnvironmentVariable pf32(L"ProgramFiles(x86)", programFiles.wstring());
    ScopedEnvironmentVariable pf64(L"ProgramW6432", programFiles.wstring());

    auto versions = DiscoverPhotoshopVersions();
    Expect(versions.size() >= 2,
           "multiple installed Photoshop versions were not offered for selection");
    auto version = std::find_if(versions.begin(), versions.end(), [](const auto& candidate) {
        return LowerWide(candidate.label) == L"adobe photoshop 2025";
    });
    Expect(version != versions.end(), "installed Photoshop version was not discovered");
    PtsBackupOptions options{};
    options.selectedPhotoshopVersionLabel = version->label;
    options.selectedPhotoshopVersionLabels.insert(LowerWide(version->label));
    for (const auto& candidateRoot : version->roots) {
        options.selectedPhotoshopVersionKeys.insert(candidateRoot.versionKey);
    }

    fs::path archive = root / L"settings.afang";
    std::wstring summary;
    std::wstring error;
    auto progress = [](int, const std::wstring&) {};
    Expect(CreatePtsBackupArchive(archive.wstring(), true, false, options, progress,
                                  summary, error),
           "Photoshop settings backup failed");
    PtsArchiveScanResult scan{};
    Expect(InspectPtsArchiveVersions(archive.wstring(), scan, error),
           "settings backup could not be inspected");
    Expect(scan.hasSettings && scan.entryCount > 0,
           "settings backup did not contain a settings entry");
    Expect(scan.versions.size() == 1 &&
               LowerWide(scan.versions.front().label) == L"adobe photoshop 2025",
           "backup selection leaked settings from another installed version");
    fs::remove_all(root, ignored);
}

void TestCancelledBackupDeletesIncompleteArchive() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-cancelled-backup";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::path appDataRoot = root / L"Roaming";
    fs::path localRoot = root / L"Local";
    fs::path programFiles = root / L"ProgramFiles";
    fs::path install = programFiles / L"Adobe" / L"Adobe Photoshop 2025";
    fs::path settings = appDataRoot / L"Adobe" / L"Adobe Photoshop 2025" /
                        L"Adobe Photoshop 2025 Settings";
    fs::create_directories(install);
    fs::create_directories(settings);
    {
        std::ofstream photoshop(install / L"Photoshop.exe", std::ios::binary);
        photoshop << "test";
        std::ofstream preference(settings / L"Adobe Photoshop 2025 Prefs.psp",
                                 std::ios::binary);
        preference << std::string(1024, 'P');
    }

    ScopedEnvironmentVariable appData(L"APPDATA", appDataRoot.wstring());
    ScopedEnvironmentVariable localAppData(L"LOCALAPPDATA", localRoot.wstring());
    ScopedEnvironmentVariable pf(L"ProgramFiles", programFiles.wstring());
    ScopedEnvironmentVariable pf32(L"ProgramFiles(x86)", programFiles.wstring());
    ScopedEnvironmentVariable pf64(L"ProgramW6432", programFiles.wstring());

    auto versions = DiscoverPhotoshopVersions();
    auto version = std::find_if(versions.begin(), versions.end(), [](const auto& candidate) {
        return LowerWide(candidate.label) == L"adobe photoshop 2025";
    });
    Expect(version != versions.end(), "cancelled backup Photoshop version was not discovered");
    PtsBackupOptions options{};
    options.selectedPhotoshopVersionLabel = version->label;
    options.selectedPhotoshopVersionLabels.insert(LowerWide(version->label));
    for (const auto& rootItem : version->roots) {
        options.selectedPhotoshopVersionKeys.insert(rootItem.versionKey);
    }

    bool cancelNow = false;
    auto progress = [&](int, const std::wstring& message) {
        if (message.find(L"Đang nén") != std::wstring::npos) cancelNow = true;
    };
    PtsCancellationCallback cancelled = [&] { return cancelNow; };
    fs::path archive = root / L"cancelled.afang";
    std::wstring summary;
    std::wstring error;
    Expect(!CreatePtsBackupArchive(archive.wstring(), true, false, options, progress,
                                   summary, error, cancelled),
           "cancelled backup incorrectly reported success");
    Expect(!fs::exists(archive), "cancelled backup left an incomplete .afang file");
    Expect(LowerWide(error).find(L"hủy") != std::wstring::npos,
           "cancelled backup did not return a cancellation message");

    {
        std::ofstream existingBackup(archive, std::ios::binary);
        existingBackup << "previous-valid-backup";
    }
    cancelNow = false;
    summary.clear();
    error.clear();
    Expect(!CreatePtsBackupArchive(archive.wstring(), true, false, options, progress,
                                   summary, error, cancelled),
           "cancelled overwrite backup incorrectly reported success");
    std::ifstream preservedBackup(archive, std::ios::binary);
    std::string preservedBytes((std::istreambuf_iterator<char>(preservedBackup)),
                               std::istreambuf_iterator<char>());
    Expect(preservedBytes == "previous-valid-backup",
           "cancelled backup destroyed the previous archive");
    fs::remove_all(root, ignored);
}

void TestCancelledRestoreStopsBeforeNextFile() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-cancelled-restore";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root / L"Roaming");
    fs::create_directories(root / L"Local");
    ScopedEnvironmentVariable appData(L"APPDATA", (root / L"Roaming").wstring());
    ScopedEnvironmentVariable localAppData(L"LOCALAPPDATA", (root / L"Local").wstring());

    const std::wstring settingsRoot =
        L"Adobe\\Adobe Photoshop 2025\\Adobe Photoshop 2025 Settings\\";
    TestArchiveEntry first{PtsEntryKind::PhotoshopSetting, L"APPDATA",
                           settingsRoot + L"First.psp", L"Adobe Photoshop 2025",
                           std::vector<uint8_t>{1, 2, 3}};
    TestArchiveEntry second{PtsEntryKind::PhotoshopSetting, L"APPDATA",
                            settingsRoot + L"Second.psp", L"Adobe Photoshop 2025",
                            std::vector<uint8_t>{4, 5, 6}};
    fs::path archive = root / L"restore.afang";
    WriteTestArchive(archive.wstring(), {first, second});

    bool cancelNow = false;
    auto progress = [&](int, const std::wstring& message) {
        if (message.find(L"Đang restore file 2/2") != std::wstring::npos) cancelNow = true;
    };
    PtsCancellationCallback cancelled = [&] { return cancelNow; };
    PtsRestoreOptions options{};
    std::wstring summary;
    std::wstring error;
    Expect(!RestorePtsBackupArchive(archive.wstring(), options, progress, summary, error,
                                    cancelled),
           "cancelled restore incorrectly reported success");
    fs::path restoredRoot = root / L"Roaming" / L"Adobe" / L"Adobe Photoshop 2025" /
                            L"Adobe Photoshop 2025 Settings";
    Expect(fs::exists(restoredRoot / L"First.psp"),
           "restore cancellation happened before the completed first file");
    Expect(!fs::exists(restoredRoot / L"Second.psp"),
           "restore wrote another file after cancellation was requested");
    Expect(LowerWide(error).find(L"hủy") != std::wstring::npos,
           "cancelled restore did not return a cancellation message");
    fs::remove_all(root, ignored);
}

void TestIdenticalFontRestoreIsSkipped() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-identical-font";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root / L"Roaming");
    fs::create_directories(root / L"Local");
    ScopedEnvironmentVariable appData(L"APPDATA", (root / L"Roaming").wstring());
    ScopedEnvironmentVariable localAppData(L"LOCALAPPDATA", (root / L"Local").wstring());

    std::wstring registryBase = L"ToolType Pts Regression Font " +
                                std::to_wstring(GetCurrentProcessId());
    TestArchiveEntry font{PtsEntryKind::Font, L"FONT", L"ToolTypePtsTest.ttf",
                          registryBase,
                          std::vector<uint8_t>{0, 1, 2, 3, 4, 5}};
    fs::path archive = root / L"font.afang";
    WriteTestArchive(archive.wstring(), {font});
    fs::path fonts = root / L"Local" / L"Microsoft" / L"Windows" / L"Fonts";
    fs::create_directories(fonts);
    {
        std::ofstream existing(fonts / L"ToolTypePtsTest.ttf", std::ios::binary);
        existing << "different-font";
    }
    HKEY key = nullptr;
    Expect(RegCreateKeyExW(HKEY_CURRENT_USER,
                           L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0,
                           nullptr, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key,
                           nullptr) == ERROR_SUCCESS,
           "could not open current-user font registry for test");
    ScopedRegistryValues registry(key);
    const std::wstring unrelatedPath = L"C:\\unrelated-font.ttf";
    std::wstring alternateName = registryBase + L" (ToolType restored 1)";
    std::wstring secondAlternateName = registryBase + L" (ToolType restored 2)";
    registry.Add(registryBase);
    registry.Add(alternateName);
    registry.Add(secondAlternateName);
    Expect(RegSetValueExW(registry.Get(), registryBase.c_str(), 0, REG_SZ,
                          reinterpret_cast<const BYTE*>(unrelatedPath.c_str()),
                          static_cast<DWORD>((unrelatedPath.size() + 1) * sizeof(wchar_t))) ==
               ERROR_SUCCESS,
           "could not create font registry collision for test");
    PtsRestoreOptions options{};
    std::wstring summary;
    std::wstring error;
    auto progress = [](int, const std::wstring&) {};
    Expect(RestorePtsBackupArchive(archive.wstring(), options, progress, summary, error),
           "initial font restore failed");
    Expect(RestorePtsBackupArchive(archive.wstring(), options, progress, summary, error),
           "second identical font restore failed");

    Expect(fs::exists(fonts / L"ToolTypePtsTest.ttf"), "font file was not restored");
    Expect(fs::exists(fonts / L"ToolTypePtsTest-restored-1.ttf"),
           "different existing font did not receive a collision-safe restore path");
    Expect(!fs::exists(fonts / L"ToolTypePtsTest-restored-2.ttf"),
           "second restore did not reuse an identical collision file");

    std::wstring preservedMapping;
    Expect(ReadRegistryStringValue(registry.Get(), registryBase.c_str(), preservedMapping) &&
               preservedMapping == unrelatedPath,
           "font restore overwrote an unrelated Registry value");
    std::wstring restoredMapping;
    Expect(ReadRegistryStringValue(registry.Get(), alternateName.c_str(), restoredMapping),
           "font restore did not create a collision-safe Registry value");
    fs::remove_all(root, ignored);
}

void TestMalformedArchiveDoesNotWriteAnything() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-malformed";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root / L"Roaming");
    fs::create_directories(root / L"Local");
    ScopedEnvironmentVariable appData(L"APPDATA", (root / L"Roaming").wstring());
    ScopedEnvironmentVariable localAppData(L"LOCALAPPDATA", (root / L"Local").wstring());

    TestArchiveEntry hostile{PtsEntryKind::PhotoshopSetting, L"APPDATA",
                             L"..\\Startup\\payload.psp", L"Adobe Photoshop 2025",
                             std::vector<uint8_t>{1, 2, 3}};
    fs::path archive = root / L"malformed.afang";
    WriteTestArchive(archive.wstring(), {hostile});
    PtsRestoreOptions options{};
    std::wstring summary;
    std::wstring error;
    auto progress = [](int, const std::wstring&) {};
    Expect(!RestorePtsBackupArchive(archive.wstring(), options, progress, summary, error),
           "archive traversal was restored");
    Expect(!fs::exists(root / L"Roaming" / L"Startup"),
           "malformed archive created a destination directory");
    fs::remove_all(root, ignored);
}

void TestAtomicRestoreWritePreservesLockedDestination() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-atomic-write";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root);
    fs::path destination = root / L"Prefs.psp";
    {
        std::ofstream original(destination, std::ios::binary);
        original << "original-settings";
    }

    HANDLE lock = CreateFileW(destination.wstring().c_str(), GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    Expect(lock != INVALID_HANDLE_VALUE, "could not lock destination for atomic-write test");
    std::vector<uint8_t> replacement{'n', 'e', 'w'};
    std::wstring error;
    Expect(!WriteBytesToFile(destination.wstring(), replacement, error),
           "locked destination unexpectedly reported a successful replace");
    CloseHandle(lock);

    std::ifstream preserved(destination, std::ios::binary);
    std::string preservedBytes((std::istreambuf_iterator<char>(preserved)),
                               std::istreambuf_iterator<char>());
    Expect(preservedBytes == "original-settings",
           "failed restore truncated the existing settings file");
    for (const auto& item : fs::directory_iterator(root)) {
        Expect(item.path().filename().wstring().find(L".tooltype-tmp-") ==
                   std::wstring::npos,
               "failed atomic restore left a temporary file behind");
    }
    fs::remove_all(root, ignored);
}

void TestIncompatibleCs6WorkspaceFailsInsteadOfClaimingSuccess() {
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / L"tooltype-pts-incompatible-cs6";
    std::error_code ignored;
    fs::remove_all(root, ignored);
    fs::create_directories(root / L"Roaming");
    fs::create_directories(root / L"Local");
    ScopedEnvironmentVariable appData(L"APPDATA", (root / L"Roaming").wstring());
    ScopedEnvironmentVariable localAppData(L"LOCALAPPDATA", (root / L"Local").wstring());

    const std::string unknownWorkspace =
        "<photoshop-workspace version=\"3.0\"><future-panel/></photoshop-workspace>";
    TestArchiveEntry setting{
        PtsEntryKind::PhotoshopSetting, L"APPDATA",
        L"Adobe\\Adobe Photoshop 2025\\Adobe Photoshop 2025 Settings\\WorkSpaces\\Future.psw",
        L"Adobe Photoshop 2025",
        std::vector<uint8_t>(unknownWorkspace.begin(), unknownWorkspace.end())};
    fs::path archive = root / L"incompatible.afang";
    WriteTestArchive(archive.wstring(), {setting});

    PtsRestoreOptions options{};
    options.sourceVersionLabel = L"Adobe Photoshop 2025";
    options.targetVersionLabel = L"Adobe Photoshop CS6";
    options.targetAppDataRelativeRoot = L"Adobe\\Adobe Photoshop CS6";
    options.targetLocalAppDataRelativeRoot = L"Adobe\\Adobe Photoshop CS6";
    options.archiveHasSettings = true;
    std::wstring summary;
    std::wstring error;
    auto progress = [](int, const std::wstring&) {};
    Expect(!RestorePtsBackupArchive(archive.wstring(), options, progress, summary, error),
           "incompatible CS6 workspace incorrectly reported success");
    fs::path target = root / L"Roaming" / L"Adobe" / L"Adobe Photoshop CS6" /
                      L"Adobe Photoshop CS6 Settings" / L"WorkSpaces" / L"Future.psw";
    Expect(!fs::exists(target), "incompatible workspace was written raw to CS6");
    fs::remove_all(root, ignored);
}

}  // namespace

int main() {
    try {
        TestSafeArchivePaths();
        TestCompressionRoundTrip();
        TestArchiveInspectionReportsProgress();
        TestPtsBackgroundTaskKeepsUiThreadResponsive();
        TestPtsBackgroundTaskCanBeCancelled();
        TestCrossVersionMappingAndCs6Aliases();
        TestFontCollisionPath();
        TestSettingsBackupArchive();
        TestCancelledBackupDeletesIncompleteArchive();
        TestCancelledRestoreStopsBeforeNextFile();
        TestCrossVersionArchiveRestore();
        TestIdenticalFontRestoreIsSkipped();
        TestMalformedArchiveDoesNotWriteAnything();
        TestAtomicRestoreWritePreservesLockedDestination();
        TestIncompatibleCs6WorkspaceFailsInsteadOfClaimingSuccess();
        std::cout << "Pts regression tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Pts regression test failed: " << ex.what() << "\n";
        return 1;
    }
}

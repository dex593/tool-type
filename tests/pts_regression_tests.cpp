#define wWinMain ToolTypeDisabledWinMain
#include "../main.cpp"
#undef wWinMain

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void Expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
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
        L"Adobe Photoshop CS6");
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
        L"Adobe Photoshop CS6");
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
    Expect(std::filesystem::path(NonConflictingPath(desired.wstring())) == firstCollision,
           "font collision name is not deterministic");
    fs::remove_all(root, ignored);
}

}  // namespace

int main() {
    try {
        TestSafeArchivePaths();
        TestCompressionRoundTrip();
        TestCrossVersionMappingAndCs6Aliases();
        TestFontCollisionPath();
        std::cout << "Pts regression tests passed\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Pts regression test failed: " << ex.what() << "\n";
        return 1;
    }
}

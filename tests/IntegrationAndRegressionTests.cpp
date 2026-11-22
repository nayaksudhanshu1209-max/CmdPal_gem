#include "tests/TestRunner.hpp"
#include "tests/MockPlatformServices.hpp"

#include "bootstrap/Bootstrap.hpp"
#include "features/apps/AppsCoordinator.hpp"
#include "features/filesearch/FileSearchCoordinator.hpp"
#include "features/notes/NotesCoordinator.hpp"
#include "features/palette/PaletteCoordinator.hpp"
#include "features/quickfolders/QuickFoldersCoordinator.hpp"
#include "features/settings/SettingsCoordinator.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

std::string makeTempEnv(const std::string& name) {
    fs::path p = fs::temp_directory_path() / ("cmdpal_int_" + name);
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p, ec);
    return p.string();
}

void writeFile(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out << content;
}

}  // namespace

TEST_CASE(IntegrationFullBootstrapSearchHideRestoreAndShutdown) {
    std::string root = makeTempEnv("full");
    fs::path dataDir = fs::path(root) / "data";
    fs::path appsDir = fs::path(root) / "StartMenu";
    writeFile(appsDir / "Notepad.lnk", "dummy");
    writeFile(appsDir / "Calculator.lnk", "dummy");
    writeFile(appsDir / "Uninstall Tool.lnk", "noise");

    auto plat = std::make_unique<cmdpal::test::MockPlatformServices>();
    auto* platPtr = plat.get();
    platPtr->knownFolders.desktop = (fs::path(root) / "Desktop").string();
    platPtr->knownFolders.documents = (fs::path(root) / "Documents").string();
    platPtr->knownFolders.downloads = (fs::path(root) / "Downloads").string();
    fs::create_directories(platPtr->knownFolders.desktop);
    fs::create_directories(platPtr->knownFolders.documents);
    fs::create_directories(platPtr->knownFolders.downloads);

    {
        cmdpal::bootstrap::ApplicationContext app(
            std::move(plat), dataDir.string(), {appsDir.string()});

        auto* palette = app.registry().palette();
        palette->openPalette();
        palette->updateQuery("Notepad", false);
        ASSERT_FALSE(palette->state().results.empty());
        ASSERT_EQ(palette->state().results[0].title, std::string("Notepad"));

        // Execute launches app and hides palette
        auto execRes = palette->handleKey(cmdpal::features::palette::NavKey::Enter);
        ASSERT_TRUE(execRes.success);
        ASSERT_FALSE(palette->state().isOpen);
        ASSERT_EQ(platPtr->openedShellItems.size(), size_t(1));

        // Hide app and verify it disappears
        palette->openPalette();
        palette->updateQuery("Calculator", false);
        ASSERT_EQ(palette->state().results[0].title, std::string("Calculator"));
        palette->hideResultAt(0);
        for (const auto& r : palette->state().results) {
            ASSERT_TRUE(r.title != "Calculator");
        }

        // Restore app and verify it returns
        app.registry().apps()->restoreApp("app:calculator");
        palette->updateQuery("Calculator", false);
        ASSERT_EQ(palette->state().results[0].title, std::string("Calculator"));
    }

    // Restart and verify hidden-result persistence
    auto plat2 = std::make_unique<cmdpal::test::MockPlatformServices>();
    cmdpal::bootstrap::ApplicationContext app2(
        std::move(plat2), dataDir.string(), {appsDir.string()});
    auto* p2 = app2.registry().palette();
    p2->openPalette();
    p2->updateQuery("Notepad", false);
    ASSERT_EQ(p2->state().results[0].title, std::string("Notepad"));
    p2->hideResultAt(0);
    app2.shutdown();

    auto plat3 = std::make_unique<cmdpal::test::MockPlatformServices>();
    cmdpal::bootstrap::ApplicationContext app3(
        std::move(plat3), dataDir.string(), {appsDir.string()});
    app3.registry().palette()->openPalette();
    app3.registry().palette()->updateQuery("Notepad", false);
    for (const auto& r : app3.registry().palette()->state().results) {
        ASSERT_TRUE(r.title != "Notepad");
    }

    fs::remove_all(root);
}

TEST_CASE(IntegrationWebBangChipAndExternalLaunchOnlyOnSubmit) {
    std::string root = makeTempEnv("web");
    auto plat = std::make_unique<cmdpal::test::MockPlatformServices>();
    auto* platPtr = plat.get();

    cmdpal::bootstrap::ApplicationContext app(std::move(plat), (fs::path(root) / "data").string());
    auto* palette = app.registry().palette();
    palette->openPalette();

    // Typing "g hello world" sets Google Search chip and preserves query
    palette->updateQuery("g hello world", false);
    ASSERT_TRUE(palette->state().activeChip.has_value());
    ASSERT_EQ(palette->state().activeChip->label, std::string("Google Search"));
    ASSERT_EQ(palette->state().rawQuery, std::string("g hello world"));
    ASSERT_TRUE(platPtr->openedUrls.empty());  // Must NOT open browser while typing!

    // Pressing Enter opens external browser URL
    auto res = palette->handleKey(cmdpal::features::palette::NavKey::Enter);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(platPtr->openedUrls.size(), size_t(1));
    ASSERT_EQ(platPtr->openedUrls[0], std::string("https://www.google.com/search?q=hello+world"));

    // Arbitrary deep URL
    palette->openPalette();
    palette->updateQuery("https://example.com/home", false);
    res = palette->handleKey(cmdpal::features::palette::NavKey::Enter);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(platPtr->openedUrls.back(), std::string("https://example.com/home"));

    fs::remove_all(root);
}

TEST_CASE(IntegrationFileIndexingScopedToConfiguredRootsOnly) {
    std::string root = makeTempEnv("files");
    fs::path indexedDir = fs::path(root) / "IndexedDocs";
    fs::path unindexedDir = fs::path(root) / "SecretUnindexed";
    writeFile(indexedDir / "nested" / "budget_2026.txt", "data");
    writeFile(unindexedDir / "private_unscanned.txt", "secret");

    auto plat = std::make_unique<cmdpal::test::MockPlatformServices>();
    plat->knownFolders.desktop = (fs::path(root) / "Desktop").string();
    plat->knownFolders.documents = (fs::path(root) / "Documents").string();
    plat->knownFolders.downloads = (fs::path(root) / "Downloads").string();

    cmdpal::bootstrap::ApplicationContext app(std::move(plat), (fs::path(root) / "data").string());
    auto* fsearch = app.registry().fileSearch();
    ASSERT_TRUE(fsearch->addRoot("Custom", indexedDir.string(), 6));
    // Independently disable default Desktop root
    ASSERT_TRUE(fsearch->setRootEnabled("default-desktop", false));

    fsearch->indexSynchronouslyNow();

    auto* palette = app.registry().palette();
    palette->openPalette();
    palette->updateQuery("budget_2026", false);
    bool foundBudget = false;
    for (const auto& r : palette->state().results) {
        if (r.title == "budget_2026.txt") foundBudget = true;
    }
    ASSERT_TRUE(foundBudget);

    palette->updateQuery("private_unscanned", false);
    for (const auto& r : palette->state().results) {
        ASSERT_TRUE(r.title != "private_unscanned.txt");
    }

    fs::remove_all(root);
}

TEST_CASE(IntegrationQuickFoldersIndependentFromFileRoots) {
    std::string root = makeTempEnv("qf");
    auto plat = std::make_unique<cmdpal::test::MockPlatformServices>();
    plat->knownFolders.desktop = (fs::path(root) / "Desktop").string();

    cmdpal::bootstrap::ApplicationContext app(std::move(plat), (fs::path(root) / "data").string());
    auto* qf = app.registry().quickFolders();
    size_t initialRoots = app.core().settings().snapshot().fileRoots.size();

    ASSERT_TRUE(qf->addFolder("Projects", (fs::path(root) / "Projects").string(), false));
    ASSERT_TRUE(qf->addFolder("Logs", (fs::path(root) / "Logs").string(), false));
    auto list = qf->listFolders();
    ASSERT_EQ(list.size(), size_t(2));
    ASSERT_TRUE(qf->moveFolder(list[1].id, -1));
    ASSERT_EQ(qf->listFolders()[0].label, std::string("Logs"));

    ASSERT_TRUE(qf->removeFolder(list[0].id));
    ASSERT_EQ(app.core().settings().snapshot().fileRoots.size(), initialRoots);

    fs::remove_all(root);
}

TEST_CASE(IntegrationNotesLifecycleAndSafeguards) {
    std::string root = makeTempEnv("notes");
    auto plat = std::make_unique<cmdpal::test::MockPlatformServices>();
    plat->knownFolders.documents = (fs::path(root) / "Documents").string();

    cmdpal::bootstrap::ApplicationContext app(std::move(plat), (fs::path(root) / "data").string());
    auto* notes = app.registry().notes();
    auto created = notes->createNote("Meeting Notes");
    ASSERT_TRUE(created.has_value());
    ASSERT_TRUE(notes->saveNoteContent(created->id, "# Meeting\n- [ ] Action item"));
    ASSERT_EQ(notes->loadNoteContent(created->id), std::string("# Meeting\n- [ ] Action item"));
    ASSERT_TRUE(notes->renameNote(created->id, "Renamed Meeting"));
    ASSERT_FALSE(notes->deleteNote("Renamed Meeting", false));  // Safeguard blocks unconfirmed delete
    ASSERT_TRUE(notes->deleteNote("Renamed Meeting", true));

    fs::remove_all(root);
}

TEST_CASE(IntegrationSettingsSearchUserCommandAndKeyboardNav) {
    std::string root = makeTempEnv("settings_cmd");
    auto plat = std::make_unique<cmdpal::test::MockPlatformServices>();
    auto* platPtr = plat.get();

    cmdpal::bootstrap::ApplicationContext app(std::move(plat), (fs::path(root) / "data").string());
    std::string openedSection;
    app.registry().settings()->setOpenWindowCallback([&](const std::string& sec) { openedSection = sec; });

    // Settings search navigates directly to section
    auto* palette = app.registry().palette();
    palette->openPalette();
    palette->updateQuery("startup", false);
    ASSERT_FALSE(palette->state().results.empty());
    bool foundSetting = false;
    for (int i = 0; i < static_cast<int>(palette->state().results.size()); ++i) {
        if (palette->state().results[static_cast<size_t>(i)].category == cmdpal::core::SearchCategory::Settings) {
            palette->activateIndex(i);
            foundSetting = true;
            break;
        }
    }
    ASSERT_TRUE(foundSetting);
    ASSERT_EQ(openedSection, std::string("general"));

    // User-defined command executes with structured args
    auto s = app.core().settings().snapshot();
    s.userCommands.push_back({"uc1", "Ping Local", "Run test tool", "pingloc", "Custom",
                              "C:\\Tools\\ping.exe", {"-n", "1", "127.0.0.1"}, "", false, true});
    app.registry().settings()->applySettings(s);

    palette->openPalette();
    palette->updateQuery("pingloc", false);
    ASSERT_FALSE(palette->state().results.empty());
    ASSERT_EQ(palette->state().results[0].title, std::string("Ping Local"));
    auto res = palette->handleKey(cmdpal::features::palette::NavKey::Enter);
    ASSERT_TRUE(res.success);
    ASSERT_EQ(platPtr->launchedProcesses.size(), size_t(1));
    ASSERT_EQ(platPtr->launchedProcesses[0].exe, std::string("C:\\Tools\\ping.exe"));
    ASSERT_EQ(platPtr->launchedProcesses[0].args.size(), size_t(3));

    // Keyboard navigation checks
    palette->openPalette();
    palette->updateQuery("", false);
    int initialSel = palette->state().selectedIndex;
    palette->handleKey(cmdpal::features::palette::NavKey::Down);
    if (palette->state().results.size() > 1) {
        ASSERT_EQ(palette->state().selectedIndex, initialSel + 1);
    }
    palette->handleKey(cmdpal::features::palette::NavKey::ShiftTab);
    ASSERT_EQ(palette->state().selectedIndex, initialSel);
    palette->handleKey(cmdpal::features::palette::NavKey::Escape);
    ASSERT_FALSE(palette->state().isOpen);

    fs::remove_all(root);
}

TEST_CASE(RegressionExcludedFeaturesAbsent) {
    std::string root = makeTempEnv("reg");
    auto plat = std::make_unique<cmdpal::test::MockPlatformServices>();
    cmdpal::bootstrap::ApplicationContext app(std::move(plat), (fs::path(root) / "data").string());

    auto ids = app.registry().registeredFeatureIds();
    std::vector<std::string> forbidden = {
        "windowswitcher", "windows-switcher", "unitconverter", "unit-converter",
        "numberbase", "colorconverter", "colourconverter", "texttools",
        "browser", "webview2", "cef", "chromium", "electron"
    };
    for (const auto& id : ids) {
        for (const auto& bad : forbidden) {
            ASSERT_TRUE(id.find(bad) == std::string::npos);
        }
    }
    ASSERT_EQ(ids.size(), size_t(8));
    fs::remove_all(root);
}

int main() {
    return cmdpal::test::runAllTests();
}

#include "tests/TestRunner.hpp"
#include "tests/MockPlatformServices.hpp"

#include "core/CoreServices.hpp"
#include "core/Json.hpp"
#include "core/SearchMatch.hpp"
#include "core/SettingsStore.hpp"
#include "core/UsageTracker.hpp"
#include "features/commands/CommandsCoordinator.hpp"
#include "features/filesearch/FileSearchCoordinator.hpp"
#include "features/notes/NotesCoordinator.hpp"
#include "features/notes/NotesMarkdown.hpp"
#include "features/palette/PaletteCoordinator.hpp"
#include "features/quickfolders/QuickFoldersCoordinator.hpp"
#include "features/web/WebCoordinator.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

std::string makeTempDir(const std::string& suffix) {
    fs::path p = fs::temp_directory_path() / ("cmdpal_test_" + suffix);
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p, ec);
    return p.string();
}

}  // namespace

TEST_CASE(JsonParseAndDumpRoundTrip) {
    auto parsed = cmdpal::core::JsonValue::parse(R"({"name":"CmdPal","count":42,"ok":true,"arr":[1,2]})");
    ASSERT_TRUE(parsed.has_value());
    ASSERT_TRUE(parsed->isObject());
    ASSERT_EQ(parsed->get("name")->asString(), std::string("CmdPal"));
    ASSERT_EQ(parsed->get("count")->asInt(), 42);
    ASSERT_TRUE(parsed->get("ok")->asBool());
    ASSERT_EQ(parsed->get("arr")->asArray().size(), size_t(2));
}

TEST_CASE(SettingsValidationAndMalformedRecovery) {
    std::string dir = makeTempDir("settings");
    std::string cfgPath = (fs::path(dir) / "settings.json").string();

    {
        std::ofstream bad(cfgPath);
        bad << "{ invalid json !!!";
    }

    cmdpal::test::MockPlatformServices plat;
    plat.knownFolders.desktop = (fs::path(dir) / "Desktop").string();
    plat.knownFolders.documents = (fs::path(dir) / "Documents").string();
    plat.knownFolders.downloads = (fs::path(dir) / "Downloads").string();

    cmdpal::core::SettingsStore store(cfgPath, plat.knownFolders);
    ASSERT_FALSE(store.load());
    ASSERT_TRUE(store.lastLoadRecoveredFromMalformed());
    ASSERT_TRUE(fs::exists(cfgPath + ".bak"));
    ASSERT_EQ(store.snapshot().fileRoots.size(), size_t(3));

    auto s = store.snapshot();
    s.globalShortcut = {false, false, false, false, ""};  // invalid, should be sanitized
    s.webBangs.push_back({"bad", "js", "Bad", "javascript:alert(1)", true});
    store.update(s);

    auto sanitized = store.snapshot();
    ASSERT_TRUE(sanitized.globalShortcut.ctrl && sanitized.globalShortcut.alt);
    ASSERT_EQ(sanitized.globalShortcut.key, std::string("Space"));
    for (const auto& b : sanitized.webBangs) {
        ASSERT_TRUE(b.alias != "js");
    }
    fs::remove_all(dir);
}

TEST_CASE(WebUrlValidationAndBangParsing) {
    using cmdpal::features::web::WebCoordinator;

    auto validDeep = WebCoordinator::validateUrlStatic("https://example.com/home?tab=1#top");
    ASSERT_TRUE(validDeep.isUrl);
    ASSERT_FALSE(validDeep.rejectedDangerousScheme);
    ASSERT_EQ(validDeep.normalizedUrl, std::string("https://example.com/home?tab=1#top"));

    auto schemeless = WebCoordinator::validateUrlStatic("example.com/home");
    ASSERT_TRUE(schemeless.isUrl);
    ASSERT_EQ(schemeless.normalizedUrl, std::string("https://example.com/home"));

    auto normalText = WebCoordinator::validateUrlStatic("how to bake bread");
    ASSERT_FALSE(normalText.isUrl);
    ASSERT_FALSE(normalText.rejectedDangerousScheme);

    auto jsScheme = WebCoordinator::validateUrlStatic("javascript:alert(1)");
    ASSERT_FALSE(jsScheme.isUrl);
    ASSERT_TRUE(jsScheme.rejectedDangerousScheme);

    auto dataScheme = WebCoordinator::validateUrlStatic("data:text/html,<script>alert(1)</script>");
    ASSERT_FALSE(dataScheme.isUrl);
    ASSERT_TRUE(dataScheme.rejectedDangerousScheme);
}

TEST_CASE(PaletteBoundsClampingMultiMonitorAndEdge) {
    using cmdpal::features::palette::PaletteCoordinator;

    cmdpal::core::MonitorWorkArea mon{{-1920, 0, 0, 1040}, 1.0};
    // Cursor near bottom-right corner of secondary left monitor
    cmdpal::core::Point cursor{-20, 1030};
    auto r = PaletteCoordinator::computeBounds(
        cmdpal::core::PalettePlacement::NearCursor, mon, cursor, 680, 400);
    ASSERT_TRUE(r.left >= mon.workArea.left);
    ASSERT_TRUE(r.top >= mon.workArea.top);
    ASSERT_TRUE(r.right <= mon.workArea.right);
    ASSERT_TRUE(r.bottom <= mon.workArea.bottom);
    ASSERT_EQ(r.width(), 680);
    ASSERT_EQ(r.height(), 400);
}

TEST_CASE(MarkdownCaretSensitiveSyntaxAndToolbar) {
    using namespace cmdpal::features::notes;

    std::string doc = "Hello **bold** world";
    // Caret outside "**bold**" (at index 0) -> markers hidden
    auto outside = NotesMarkdown::renderDocument(doc, 0);
    ASSERT_EQ(outside.size(), size_t(1));
    std::string visibleOutside;
    for (const auto& sp : outside[0].spans) visibleOutside += sp.text;
    ASSERT_EQ(visibleOutside, std::string("Hello bold world"));

    // Caret inside "**bold**" (at index 9) -> markers shown as dimmed
    auto inside = NotesMarkdown::renderDocument(doc, 9);
    std::string visibleInside;
    bool hasDimmedMarker = false;
    for (const auto& sp : inside[0].spans) {
        visibleInside += sp.text;
        if (sp.dimmedMarker) hasDimmedMarker = true;
    }
    ASSERT_EQ(visibleInside, std::string("Hello **bold** world"));
    ASSERT_TRUE(hasDimmedMarker);

    // Toolbar formatting & per-note ordering
    auto actions = NotesMarkdown::defaultToolbarActions();
    auto edited = NotesMarkdown::applyToolbarAction("hello", 0, 5, actions[0]);  // bold
    ASSERT_EQ(edited.newContent, std::string("**hello**"));

    auto cont = NotesMarkdown::handleEnterKey("- [ ] Buy milk", 14);
    ASSERT_EQ(cont.newContent, std::string("- [ ] Buy milk\n- [ ] "));

    auto ordered = NotesMarkdown::orderToolbarActions(actions, {{"link", 5}, {"bold", 2}});
    ASSERT_EQ(ordered[0].id, std::string("link"));
    ASSERT_EQ(ordered[1].id, std::string("bold"));
}

TEST_CASE(CommandValidationRejectsUnsafeInputs) {
    cmdpal::features::commands::CommandsCoordinator cmds;
    cmdpal::core::UserCommandDef bad1{"1", "", "desc", "", "Custom", "notepad.exe", {}, "", false, true};
    ASSERT_FALSE(cmds.validateUserCommand(bad1).valid);

    cmdpal::core::UserCommandDef bad2{"2", "Test", "desc", "", "Custom", "bad\nexe", {}, "", false, true};
    ASSERT_FALSE(cmds.validateUserCommand(bad2).valid);

    cmdpal::core::UserCommandDef good{"3", "List", "desc", "ls", "Custom", "C:\\Tools\\tool.exe", {"--flag"}, "", false, true};
    ASSERT_TRUE(cmds.validateUserCommand(good).valid);
}

TEST_CASE(FileRootBoundaryCheck) {
    using cmdpal::features::filesearch::FileSearchCoordinator;
    ASSERT_TRUE(FileSearchCoordinator::isPathInsideRoot("/tmp/root", "/tmp/root/sub/file.txt"));
    ASSERT_FALSE(FileSearchCoordinator::isPathInsideRoot("/tmp/root", "/tmp/other/file.txt"));
}

#pragma once

#include "core/Contracts.hpp"
#include "core/CoreServices.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cmdpal::features::palette { class PaletteCoordinator; }
namespace cmdpal::features::apps { class AppsCoordinator; }
namespace cmdpal::features::commands { class CommandsCoordinator; }
namespace cmdpal::features::web { class WebCoordinator; }
namespace cmdpal::features::filesearch { class FileSearchCoordinator; }
namespace cmdpal::features::quickfolders { class QuickFoldersCoordinator; }
namespace cmdpal::features::notes { class NotesCoordinator; }
namespace cmdpal::features::settings { class SettingsCoordinator; }

namespace cmdpal::bootstrap {

// Central feature registry: the single documented integration point for adding or removing features.
class FeatureRegistry {
public:
    void registerAllFeatures(core::CoreServices& core,
                             std::function<void()> onQuitRequested,
                             std::vector<std::string> customAppScanDirs = {});
    void shutdownAll();

    const std::vector<std::unique_ptr<core::IFeature>>& features() const { return features_; }
    std::vector<std::string> registeredFeatureIds() const;

    features::palette::PaletteCoordinator* palette() const { return palette_; }
    features::apps::AppsCoordinator* apps() const { return apps_; }
    features::commands::CommandsCoordinator* commands() const { return commands_; }
    features::web::WebCoordinator* web() const { return web_; }
    features::filesearch::FileSearchCoordinator* fileSearch() const { return fileSearch_; }
    features::quickfolders::QuickFoldersCoordinator* quickFolders() const { return quickFolders_; }
    features::notes::NotesCoordinator* notes() const { return notes_; }
    features::settings::SettingsCoordinator* settings() const { return settings_; }

private:
    template <typename T>
    T* addFeature(std::unique_ptr<T> feat, core::CoreServices& core) {
        T* raw = feat.get();
        feat->initialize(core);
        core.registerSettingDescriptors(feat->settingDescriptors());
        features_.push_back(std::move(feat));
        return raw;
    }

    std::vector<std::unique_ptr<core::IFeature>> features_;
    features::palette::PaletteCoordinator* palette_ = nullptr;
    features::apps::AppsCoordinator* apps_ = nullptr;
    features::commands::CommandsCoordinator* commands_ = nullptr;
    features::web::WebCoordinator* web_ = nullptr;
    features::filesearch::FileSearchCoordinator* fileSearch_ = nullptr;
    features::quickfolders::QuickFoldersCoordinator* quickFolders_ = nullptr;
    features::notes::NotesCoordinator* notes_ = nullptr;
    features::settings::SettingsCoordinator* settings_ = nullptr;
};

}  // namespace cmdpal::bootstrap

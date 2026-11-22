#include "bootstrap/FeatureRegistry.hpp"

#include "features/apps/AppsCoordinator.hpp"
#include "features/commands/CommandsCoordinator.hpp"
#include "features/filesearch/FileSearchCoordinator.hpp"
#include "features/notes/NotesCoordinator.hpp"
#include "features/palette/PaletteCoordinator.hpp"
#include "features/quickfolders/QuickFoldersCoordinator.hpp"
#include "features/settings/SettingsCoordinator.hpp"
#include "features/web/WebCoordinator.hpp"

namespace cmdpal::bootstrap {

void FeatureRegistry::registerAllFeatures(core::CoreServices& core,
                                          std::function<void()> onQuitRequested,
                                          std::vector<std::string> customAppScanDirs) {
    palette_ = addFeature(std::make_unique<features::palette::PaletteCoordinator>(), core);
    apps_ = addFeature(std::make_unique<features::apps::AppsCoordinator>(std::move(customAppScanDirs)), core);
    commands_ = addFeature(std::make_unique<features::commands::CommandsCoordinator>(std::move(onQuitRequested)), core);
    web_ = addFeature(std::make_unique<features::web::WebCoordinator>(), core);
    fileSearch_ = addFeature(std::make_unique<features::filesearch::FileSearchCoordinator>(), core);
    quickFolders_ = addFeature(std::make_unique<features::quickfolders::QuickFoldersCoordinator>(), core);
    notes_ = addFeature(std::make_unique<features::notes::NotesCoordinator>(), core);
    settings_ = addFeature(std::make_unique<features::settings::SettingsCoordinator>(), core);

    // Wire ISearchProvider implementations into PaletteCoordinator via public contracts
    for (const auto& feat : features_) {
        if (auto* provider = feat->searchProvider()) {
            palette_->registerProvider(provider);
        }
    }

    // Connect web bang parser through IWebService contract
    features::web::IWebService* webSvc = web_;
    palette_->setBangParser([webSvc](const std::string& raw) {
        auto parsed = webSvc->parseInput(raw);
        return std::make_pair(parsed.chip, parsed.remainingQuery);
    });
}

void FeatureRegistry::shutdownAll() {
    for (auto it = features_.rbegin(); it != features_.rend(); ++it) {
        (*it)->shutdown();
    }
    features_.clear();
    palette_ = nullptr;
    apps_ = nullptr;
    commands_ = nullptr;
    web_ = nullptr;
    fileSearch_ = nullptr;
    quickFolders_ = nullptr;
    notes_ = nullptr;
    settings_ = nullptr;
}

std::vector<std::string> FeatureRegistry::registeredFeatureIds() const {
    std::vector<std::string> ids;
    ids.reserve(features_.size());
    for (const auto& f : features_) {
        ids.push_back(f->id());
    }
    return ids;
}

}  // namespace cmdpal::bootstrap

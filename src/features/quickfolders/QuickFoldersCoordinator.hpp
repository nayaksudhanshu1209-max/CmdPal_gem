#pragma once

#include "core/CoreServices.hpp"
#include "features/quickfolders/QuickFoldersContract.hpp"

#include <string>
#include <vector>

namespace cmdpal::features::quickfolders {

class QuickFoldersCoordinator : public core::IFeature,
                                public core::ISearchProvider,
                                public IQuickFoldersService {
public:
    // IFeature
    std::string id() const override { return "quickfolders"; }
    std::string displayName() const override { return "Quick Folders"; }
    void initialize(core::CoreServices& core) override;
    void shutdown() override;
    core::ISearchProvider* searchProvider() override { return this; }
    std::vector<core::SettingDescriptor> settingDescriptors() const override;

    // ISearchProvider
    std::string providerId() const override { return "quickfolders"; }
    core::SearchCategory category() const override { return core::SearchCategory::QuickFolders; }
    std::vector<core::SearchResult> query(const core::QueryContext& ctx) override;
    core::ActionResult execute(const core::SearchResult& item) override;

    // IQuickFoldersService
    bool addFolder(const std::string& label, const std::string& path, bool indexAsRoot = false) override;
    bool editFolder(const std::string& id, const std::string& label, const std::string& path, bool indexAsRoot) override;
    bool removeFolder(const std::string& id) override;
    bool setEnabled(const std::string& id, bool enabled) override;
    bool moveFolder(const std::string& id, int delta) override;
    std::vector<core::QuickFolderItem> listFolders() const override;

private:
    core::CoreServices* core_ = nullptr;
};

}  // namespace cmdpal::features::quickfolders

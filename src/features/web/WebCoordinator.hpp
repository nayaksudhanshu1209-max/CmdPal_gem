#pragma once

#include "core/CoreServices.hpp"
#include "features/web/WebContract.hpp"

#include <string>
#include <vector>

namespace cmdpal::features::web {

class WebCoordinator : public core::IFeature,
                       public core::ISearchProvider,
                       public IWebService {
public:
    // IFeature
    std::string id() const override { return "web"; }
    std::string displayName() const override { return "Web Search, Bangs & Bookmarks"; }
    void initialize(core::CoreServices& core) override;
    void shutdown() override;
    core::ISearchProvider* searchProvider() override { return this; }
    std::vector<core::SettingDescriptor> settingDescriptors() const override;

    // ISearchProvider
    std::string providerId() const override { return "web"; }
    core::SearchCategory category() const override { return core::SearchCategory::Web; }
    std::vector<core::SearchResult> query(const core::QueryContext& ctx) override;
    core::ActionResult execute(const core::SearchResult& item) override;

    // IWebService
    ParsedWebInput parseInput(const std::string& rawQuery) const override;
    UrlValidationResult validateAndNormalizeUrl(const std::string& input) const override;

    // Helpers
    static std::string urlEncode(const std::string& value);
    static std::string buildSearchUrl(const std::string& urlTemplate, const std::string& query);
    static UrlValidationResult validateUrlStatic(const std::string& input);

private:
    core::CoreServices* core_ = nullptr;
};

}  // namespace cmdpal::features::web

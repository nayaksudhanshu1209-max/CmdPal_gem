#pragma once

#include "core/IPlatformServices.hpp"
#include "core/SettingsModels.hpp"

#include <mutex>
#include <string>

namespace cmdpal::core {

class SettingsStore {
public:
    explicit SettingsStore(std::string filePath, const KnownFolders& defaults);

    bool load();
    bool save() const;

    AppSettings snapshot() const;
    void update(const AppSettings& next);
    void resetToDefaults();

    static AppSettings makeDefaults(const KnownFolders& folders);
    static AppSettings validateAndSanitize(AppSettings s);

    const std::string& filePath() const { return filePath_; }
    bool lastLoadRecoveredFromMalformed() const { return recoveredFromMalformed_; }

private:
    std::string filePath_;
    KnownFolders defaults_;
    mutable std::mutex mu_;
    AppSettings settings_;
    bool recoveredFromMalformed_ = false;
};

}  // namespace cmdpal::core

#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace cmdpal::core {

class UsageTracker {
public:
    explicit UsageTracker(std::string filePath);

    bool load();
    bool save() const;

    void recordExecution(const std::string& resultId, int64_t timestampMs);
    double frequencyBonus(const std::string& resultId) const;

    void hideApp(const std::string& appId);
    void restoreApp(const std::string& appId);
    bool isAppHidden(const std::string& appId) const;
    std::vector<std::string> hiddenApps() const;

    void hideResult(const std::string& resultId);
    void restoreResult(const std::string& resultId);
    bool isResultHidden(const std::string& resultId) const;
    std::vector<std::string> hiddenResults() const;

private:
    struct UsageEntry {
        int count = 0;
        int64_t lastUsedMs = 0;
    };

    std::string filePath_;
    mutable std::mutex mu_;
    std::map<std::string, UsageEntry> usage_;
    std::set<std::string> hiddenApps_;
    std::set<std::string> hiddenResults_;
};

}  // namespace cmdpal::core

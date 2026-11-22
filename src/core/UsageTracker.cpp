#include "core/UsageTracker.hpp"

#include "core/Json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace cmdpal::core {

UsageTracker::UsageTracker(std::string filePath) : filePath_(std::move(filePath)) {}

bool UsageTracker::load() {
    std::lock_guard<std::mutex> lock(mu_);
    std::error_code ec;
    if (!fs::exists(filePath_, ec)) return true;

    std::ifstream in(filePath_, std::ios::binary);
    if (!in) return false;
    std::ostringstream oss;
    oss << in.rdbuf();

    auto parsed = JsonValue::parse(oss.str());
    if (!parsed || !parsed->isObject()) return false;

    usage_.clear();
    hiddenApps_.clear();
    hiddenResults_.clear();

    if (auto* u = parsed->get("usage"); u && u->isObject()) {
        for (const auto& [id, val] : u->asObject()) {
            if (!val.isObject()) continue;
            UsageEntry e;
            if (auto* c = val.get("count")) e.count = c->asInt(0);
            if (auto* t = val.get("lastUsedMs")) e.lastUsedMs = t->asInt64(0);
            usage_[id] = e;
        }
    }
    if (auto* ha = parsed->get("hiddenApps"); ha && ha->isArray()) {
        for (const auto& item : ha->asArray()) {
            if (item.isString() && !item.asString().empty()) hiddenApps_.insert(item.asString());
        }
    }
    if (auto* hr = parsed->get("hiddenResults"); hr && hr->isArray()) {
        for (const auto& item : hr->asArray()) {
            if (item.isString() && !item.asString().empty()) hiddenResults_.insert(item.asString());
        }
    }
    return true;
}

bool UsageTracker::save() const {
    JsonObject root;
    {
        std::lock_guard<std::mutex> lock(mu_);
        JsonObject u;
        for (const auto& [id, e] : usage_) {
            JsonObject item;
            item["count"] = JsonValue(e.count);
            item["lastUsedMs"] = JsonValue(e.lastUsedMs);
            u[id] = JsonValue(std::move(item));
        }
        root["usage"] = JsonValue(std::move(u));

        JsonArray ha;
        for (const auto& id : hiddenApps_) ha.push_back(JsonValue(id));
        root["hiddenApps"] = JsonValue(std::move(ha));

        JsonArray hr;
        for (const auto& id : hiddenResults_) hr.push_back(JsonValue(id));
        root["hiddenResults"] = JsonValue(std::move(hr));
    }

    std::error_code ec;
    fs::path p(filePath_);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);

    std::ofstream out(filePath_, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << JsonValue(std::move(root)).dump(2) << "\n";
    return out.good();
}

void UsageTracker::recordExecution(const std::string& resultId, int64_t timestampMs) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto& e = usage_[resultId];
        e.count += 1;
        e.lastUsedMs = timestampMs;
    }
    save();
}

double UsageTracker::frequencyBonus(const std::string& resultId) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = usage_.find(resultId);
    if (it == usage_.end()) return 0.0;
    return std::min(25.0, static_cast<double>(it->second.count) * 2.5);
}

void UsageTracker::hideApp(const std::string& appId) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        hiddenApps_.insert(appId);
    }
    save();
}

void UsageTracker::restoreApp(const std::string& appId) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        hiddenApps_.erase(appId);
    }
    save();
}

bool UsageTracker::isAppHidden(const std::string& appId) const {
    std::lock_guard<std::mutex> lock(mu_);
    return hiddenApps_.count(appId) > 0;
}

std::vector<std::string> UsageTracker::hiddenApps() const {
    std::lock_guard<std::mutex> lock(mu_);
    return {hiddenApps_.begin(), hiddenApps_.end()};
}

void UsageTracker::hideResult(const std::string& resultId) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        hiddenResults_.insert(resultId);
    }
    save();
}

void UsageTracker::restoreResult(const std::string& resultId) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        hiddenResults_.erase(resultId);
    }
    save();
}

bool UsageTracker::isResultHidden(const std::string& resultId) const {
    std::lock_guard<std::mutex> lock(mu_);
    return hiddenResults_.count(resultId) > 0;
}

std::vector<std::string> UsageTracker::hiddenResults() const {
    std::lock_guard<std::mutex> lock(mu_);
    return {hiddenResults_.begin(), hiddenResults_.end()};
}

}  // namespace cmdpal::core

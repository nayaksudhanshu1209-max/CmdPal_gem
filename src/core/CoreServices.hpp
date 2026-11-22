#pragma once

#include "core/Contracts.hpp"
#include "core/EventBus.hpp"
#include "core/IPlatformServices.hpp"
#include "core/SettingsStore.hpp"
#include "core/UsageTracker.hpp"

#include <string>
#include <vector>

namespace cmdpal::core {

class CoreServices {
public:
    CoreServices(IPlatformServices& platform,
                 SettingsStore& settings,
                 UsageTracker& usage,
                 EventBus& events,
                 std::string dataDir)
        : platform_(platform),
          settings_(settings),
          usage_(usage),
          events_(events),
          dataDir_(std::move(dataDir)) {}

    IPlatformServices& platform() { return platform_; }
    SettingsStore& settings() { return settings_; }
    UsageTracker& usage() { return usage_; }
    EventBus& events() { return events_; }
    const std::string& dataDir() const { return dataDir_; }

    void registerSettingDescriptors(std::vector<SettingDescriptor> descs) {
        for (auto& d : descs) {
            settingDescriptors_.push_back(std::move(d));
        }
    }

    const std::vector<SettingDescriptor>& settingDescriptors() const {
        return settingDescriptors_;
    }

private:
    IPlatformServices& platform_;
    SettingsStore& settings_;
    UsageTracker& usage_;
    EventBus& events_;
    std::string dataDir_;
    std::vector<SettingDescriptor> settingDescriptors_;
};

}  // namespace cmdpal::core

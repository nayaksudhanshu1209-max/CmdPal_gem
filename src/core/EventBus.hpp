#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <typeindex>
#include <vector>

namespace cmdpal::core {

struct SettingsChangedEvent {};
struct OpenSettingsEvent {
    std::string section;
};
struct OpenNoteEvent {
    std::string noteId;
};
struct PaletteHideRequestedEvent {};
struct IndexStatusChangedEvent {
    std::string status;
    size_t indexedCount = 0;
    bool paused = false;
};
struct AppsReloadRequestedEvent {};

class EventBus {
public:
    template <typename EventT>
    void subscribe(std::function<void(const EventT&)> handler) {
        std::lock_guard<std::mutex> lock(mu_);
        auto& list = handlers_[std::type_index(typeid(EventT))];
        list.push_back([h = std::move(handler)](const void* ptr) {
            h(*static_cast<const EventT*>(ptr));
        });
    }

    template <typename EventT>
    void publish(const EventT& event) const {
        std::vector<std::function<void(const void*)>> copy;
        {
            std::lock_guard<std::mutex> lock(mu_);
            auto it = handlers_.find(std::type_index(typeid(EventT)));
            if (it != handlers_.end()) {
                copy = it->second;
            }
        }
        for (const auto& fn : copy) {
            fn(&event);
        }
    }

private:
    mutable std::mutex mu_;
    std::map<std::type_index, std::vector<std::function<void(const void*)>>> handlers_;
};

}  // namespace cmdpal::core

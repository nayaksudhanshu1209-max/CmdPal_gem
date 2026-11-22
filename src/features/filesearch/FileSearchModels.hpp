#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cmdpal::features::filesearch {

struct IndexedFileRecord {
    std::string rootId;
    std::string name;
    std::string fullPath;
    std::string extension;
    uint64_t sizeBytes = 0;
    int64_t modifiedSec = 0;
};

struct IndexingStatus {
    bool running = false;
    bool paused = false;
    size_t indexedCount = 0;
    std::string stateText = "Idle";
};

}  // namespace cmdpal::features::filesearch

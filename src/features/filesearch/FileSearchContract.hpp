#pragma once

#include "features/filesearch/FileSearchModels.hpp"

#include <string>

namespace cmdpal::features::filesearch {

class IFileSearchService {
public:
    virtual ~IFileSearchService() = default;
    virtual void startBackgroundIndexing() = 0;
    virtual void rebuildIndex() = 0;
    virtual void pauseIndexing() = 0;
    virtual void resumeIndexing() = 0;
    virtual IndexingStatus status() const = 0;
    virtual bool addRoot(const std::string& label, const std::string& path, int maxDepth = 8) = 0;
    virtual bool removeRoot(const std::string& rootId) = 0;
    virtual bool setRootEnabled(const std::string& rootId, bool enabled) = 0;
};

}  // namespace cmdpal::features::filesearch

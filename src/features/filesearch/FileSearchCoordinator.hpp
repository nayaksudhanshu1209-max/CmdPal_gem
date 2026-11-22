#pragma once

#include "core/CoreServices.hpp"
#include "features/filesearch/FileSearchContract.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cmdpal::features::filesearch {

class FileSearchCoordinator : public core::IFeature,
                              public core::ISearchProvider,
                              public IFileSearchService {
public:
    ~FileSearchCoordinator() override;

    // IFeature
    std::string id() const override { return "filesearch"; }
    std::string displayName() const override { return "File Search & Indexing"; }
    void initialize(core::CoreServices& core) override;
    void shutdown() override;
    core::ISearchProvider* searchProvider() override { return this; }
    std::vector<core::SettingDescriptor> settingDescriptors() const override;

    // ISearchProvider
    std::string providerId() const override { return "filesearch"; }
    core::SearchCategory category() const override { return core::SearchCategory::Files; }
    std::vector<core::SearchResult> query(const core::QueryContext& ctx) override;
    core::ActionResult execute(const core::SearchResult& item) override;

    // IFileSearchService
    void startBackgroundIndexing() override;
    void rebuildIndex() override;
    void pauseIndexing() override;
    void resumeIndexing() override;
    IndexingStatus status() const override;
    bool addRoot(const std::string& label, const std::string& path, int maxDepth = 8) override;
    bool removeRoot(const std::string& rootId) override;
    bool setRootEnabled(const std::string& rootId, bool enabled) override;

    // Synchronous scan helper for deterministic unit/integration tests
    void indexSynchronouslyNow();

    static bool isPathInsideRoot(const std::string& rootPath, const std::string& candidatePath);

private:
    std::string indexFilePath() const;
    void loadPersistedIndex();
    void savePersistedIndex(const std::vector<IndexedFileRecord>& records) const;
    std::vector<IndexedFileRecord> scanConfiguredRoots(std::atomic<bool>* cancelFlag);
    void workerLoop();

    core::CoreServices* core_ = nullptr;
    mutable std::mutex recordsMu_;
    std::vector<IndexedFileRecord> records_;
    bool persistedLoaded_ = false;

    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> paused_{false};
    std::atomic<bool> rebuildRequested_{false};
    std::atomic<bool> running_{false};
    std::string stateText_ = "Idle";
    mutable std::mutex workerMu_;
    std::condition_variable workerCv_;
    std::thread workerThread_;
};

}  // namespace cmdpal::features::filesearch

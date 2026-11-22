#pragma once

#include "core/SettingsModels.hpp"

#include <string>
#include <vector>

namespace cmdpal::features::quickfolders {

class IQuickFoldersService {
public:
    virtual ~IQuickFoldersService() = default;
    virtual bool addFolder(const std::string& label, const std::string& path, bool indexAsRoot = false) = 0;
    virtual bool editFolder(const std::string& id, const std::string& label, const std::string& path, bool indexAsRoot) = 0;
    virtual bool removeFolder(const std::string& id) = 0;
    virtual bool setEnabled(const std::string& id, bool enabled) = 0;
    virtual bool moveFolder(const std::string& id, int delta) = 0;
    virtual std::vector<core::QuickFolderItem> listFolders() const = 0;
};

}  // namespace cmdpal::features::quickfolders

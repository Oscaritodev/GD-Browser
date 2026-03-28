#pragma once

#include "BrowserModels.hpp"

#include <Geode/Geode.hpp>

#include <filesystem>
#include <optional>
#include <vector>

namespace gdbrowser {
    class DownloadUploadManager {
    public:
        void load();
        void save() const;

        std::vector<DownloadRecord> const& downloads() const;
        std::filesystem::path downloadsDirectory() const;
        DownloadRecord const* mostRecent() const;

        DownloadRecord& beginDownload(std::string const& url, std::string const& suggestedName);
        void updateStatus(std::string const& id, DownloadStatus status, std::string const& error = {}, std::string const& targetPath = {});
        void clearFinished();
        bool openDownloadsFolder() const;
        std::optional<std::filesystem::path> pickUploadFile() const;

    private:
        static matjson::Value recordToJson(DownloadRecord const& record);
        static DownloadRecord recordFromJson(matjson::Value const& value);

        std::vector<DownloadRecord> m_downloads;
    };
}

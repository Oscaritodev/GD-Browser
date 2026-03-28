#include "DownloadUploadManager.hpp"

#include <Geode/utils/file.hpp>

#include <algorithm>
#include <array>

#ifdef GEODE_IS_WINDOWS
    #include <commdlg.h>
    #include <windows.h>
#endif

namespace gdbrowser {
    namespace {
        constexpr auto kDownloadsSaveKey = "browser-downloads";
    }

    void DownloadUploadManager::load() {
        m_downloads.clear();

        auto root = geode::Mod::get()->getSavedValue<std::vector<matjson::Value>>(kDownloadsSaveKey, {});
        for (auto const& item : root) {
            m_downloads.push_back(recordFromJson(item));
        }
    }

    void DownloadUploadManager::save() const {
        std::vector<matjson::Value> values;
        values.reserve(m_downloads.size());
        for (auto const& item : m_downloads) {
            values.push_back(recordToJson(item));
        }
        geode::Mod::get()->setSavedValue<std::vector<matjson::Value>>(kDownloadsSaveKey, values);
    }

    std::vector<DownloadRecord> const& DownloadUploadManager::downloads() const {
        return m_downloads;
    }

    std::filesystem::path DownloadUploadManager::downloadsDirectory() const {
        auto configured = geode::Mod::get()->getSettingValue<std::filesystem::path>("downloads-folder");
        if (configured.empty()) {
            configured = geode::Mod::get()->getSaveDir() / "downloads";
        }

        auto created = geode::utils::file::createDirectoryAll(configured);
        if (!created) {
            geode::log::warn("Failed to ensure downloads directory exists: {}", created.unwrapErr());
        }
        return configured;
    }

    DownloadRecord const* DownloadUploadManager::mostRecent() const {
        if (m_downloads.empty()) {
            return nullptr;
        }
        return &m_downloads.front();
    }

    DownloadRecord& DownloadUploadManager::beginDownload(std::string const& url, std::string const& suggestedName) {
        auto finalName = suggestedName.empty() ? "download.bin" : suggestedName;
        m_downloads.insert(m_downloads.begin(), {
            .id = makeOpaqueId("download"),
            .url = url,
            .suggestedName = finalName,
            .targetPath = (this->downloadsDirectory() / finalName).string(),
            .status = DownloadStatus::Pending,
            .createdAt = unixNow(),
            .updatedAt = unixNow(),
            .error = {},
        });
        this->save();
        return m_downloads.front();
    }

    void DownloadUploadManager::updateStatus(std::string const& id, DownloadStatus status, std::string const& error, std::string const& targetPath) {
        auto found = std::find_if(
            m_downloads.begin(),
            m_downloads.end(),
            [&id](DownloadRecord const& record) { return record.id == id; }
        );
        if (found == m_downloads.end()) {
            return;
        }

        found->status = status;
        found->updatedAt = unixNow();
        if (!error.empty()) {
            found->error = error;
        }
        if (!targetPath.empty()) {
            found->targetPath = targetPath;
        }
        this->save();
    }

    void DownloadUploadManager::clearFinished() {
        m_downloads.erase(
            std::remove_if(
                m_downloads.begin(),
                m_downloads.end(),
                [](DownloadRecord const& record) {
                    return record.status == DownloadStatus::Completed
                        || record.status == DownloadStatus::Cancelled;
                }
            ),
            m_downloads.end()
        );
        this->save();
    }

    bool DownloadUploadManager::openDownloadsFolder() const {
        return geode::utils::file::openFolder(this->downloadsDirectory());
    }

    std::optional<std::filesystem::path> DownloadUploadManager::pickUploadFile() const {
#ifdef GEODE_IS_WINDOWS
        std::array<wchar_t, 32768> buffer {};
        auto defaultDir = this->downloadsDirectory().wstring();

        OPENFILENAMEW dialog {};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = nullptr;
        dialog.lpstrFile = buffer.data();
        dialog.nMaxFile = static_cast<DWORD>(buffer.size());
        dialog.lpstrInitialDir = defaultDir.empty() ? nullptr : defaultDir.c_str();
        dialog.lpstrFilter = L"All Files\0*.*\0";
        dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (!GetOpenFileNameW(&dialog)) {
            return std::nullopt;
        }

        return std::filesystem::path(buffer.data());
#else
        geode::log::warn("Upload picker is currently only implemented for Windows in this scaffold build.");
        return std::nullopt;
#endif
    }

    matjson::Value DownloadUploadManager::recordToJson(DownloadRecord const& record) {
        return matjson::makeObject({
            { "id", record.id },
            { "url", record.url },
            { "suggested-name", record.suggestedName },
            { "target-path", record.targetPath },
            { "status", toString(record.status) },
            { "created-at", record.createdAt },
            { "updated-at", record.updatedAt },
            { "error", record.error },
        });
    }

    DownloadRecord DownloadUploadManager::recordFromJson(matjson::Value const& value) {
        return {
            .id = value["id"].asString().unwrapOr(makeOpaqueId("download")),
            .url = value["url"].asString().unwrapOr(""),
            .suggestedName = value["suggested-name"].asString().unwrapOr("download.bin"),
            .targetPath = value["target-path"].asString().unwrapOr(""),
            .status = parseDownloadStatus(value["status"].asString().unwrapOr("pending")),
            .createdAt = static_cast<int>(value["created-at"].asInt().unwrapOr(0)),
            .updatedAt = static_cast<int>(value["updated-at"].asInt().unwrapOr(0)),
            .error = value["error"].asString().unwrapOr(""),
        };
    }
}

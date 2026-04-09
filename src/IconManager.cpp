#include "IconManager.h"

#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace HoardAndSeek {

AddonAPI_t* IconManager::s_API = nullptr;
std::unordered_map<uint32_t, Texture_t*> IconManager::s_IconCache;
std::unordered_map<uint32_t, bool> IconManager::s_LoadingIcons;
std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> IconManager::s_FailedIcons;
std::mutex IconManager::s_Mutex;
std::vector<IconManager::QueuedRequest> IconManager::s_RequestQueue;
std::chrono::steady_clock::time_point IconManager::s_LastRequestTime = std::chrono::steady_clock::now();
std::string IconManager::s_IconsDir;
std::thread IconManager::s_DownloadThread;
std::condition_variable IconManager::s_QueueCV;
std::atomic<bool> IconManager::s_StopWorker{false};
std::vector<uint32_t> IconManager::s_ReadyQueue;
std::vector<uint32_t> IconManager::s_PendingTexture;

void IconManager::Initialize(AddonAPI_t* api) {
    s_API = api;

    // Start background download worker
    s_StopWorker = false;
    s_DownloadThread = std::thread(DownloadWorker);
}

void IconManager::Shutdown() {
    // Stop background worker
    s_StopWorker = true;
    s_QueueCV.notify_all();
    if (s_DownloadThread.joinable()) {
        s_DownloadThread.join();
    }

    std::lock_guard<std::mutex> lock(s_Mutex);
    s_IconCache.clear();
    s_LoadingIcons.clear();
    s_ReadyQueue.clear();
    s_RequestQueue.clear();
    s_PendingTexture.clear();
}

std::string IconManager::GetIconsDir() {
    if (!s_IconsDir.empty()) return s_IconsDir;

    char dllPath[MAX_PATH];
    GetModuleFileNameA(NULL, dllPath, MAX_PATH);
    std::string dllDir(dllPath);
    size_t lastSlash = dllDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        dllDir = dllDir.substr(0, lastSlash);
    }

    std::string addonDir = dllDir + "\\addons\\HoardAndSeek";
    CreateDirectoryA(addonDir.c_str(), NULL);
    s_IconsDir = addonDir + "\\icons";
    CreateDirectoryA(s_IconsDir.c_str(), NULL);
    return s_IconsDir;
}

std::string IconManager::GetIconFilePath(uint32_t itemId) {
    std::stringstream ss;
    ss << GetIconsDir() << "\\" << itemId << ".png";
    return ss.str();
}

bool IconManager::DownloadToFile(const std::string& url, const std::string& filePath) {
    HINTERNET hInternet = InternetOpenA("HoardAndSeek/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    // Read response into buffer
    std::vector<char> buffer;
    char chunk[4096];
    DWORD bytesRead = 0;
    while (InternetReadFile(hUrl, chunk, sizeof(chunk), &bytesRead) && bytesRead > 0) {
        buffer.insert(buffer.end(), chunk, chunk + bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (buffer.empty()) return false;

    // Verify it looks like a PNG (starts with PNG magic bytes)
    if (buffer.size() < 8 || buffer[0] != (char)0x89 || buffer[1] != 'P' || buffer[2] != 'N' || buffer[3] != 'G') {
        return false;
    }

    // Write to file
    std::ofstream file(filePath, std::ios::binary);
    if (!file.is_open()) return false;
    file.write(buffer.data(), buffer.size());
    file.close();
    return true;
}

bool IconManager::LoadIconFromDisk(uint32_t itemId) {
    if (!s_API) return false;

    std::string filePath = GetIconFilePath(itemId);

    // Check if file exists
    DWORD attrs = GetFileAttributesA(filePath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;

    std::stringstream ss;
    ss << "GW2_ICON_" << itemId;
    std::string identifier = ss.str();

    try {
        Texture_t* tex = s_API->Textures_GetOrCreateFromFile(identifier.c_str(), filePath.c_str());
        if (tex && tex->Resource) {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_IconCache[itemId] = tex;
            s_LoadingIcons.erase(itemId);
            s_FailedIcons.erase(itemId);
            return true;
        }
    } catch (...) {}
    return false;
}

void IconManager::Tick() {
    if (!s_API) return;

    // 1. Load newly downloaded icons from disk (batched)
    std::vector<uint32_t> ready;
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        int count = std::min(TICK_BATCH_SIZE, static_cast<int>(s_ReadyQueue.size()));
        if (count > 0) {
            ready.assign(s_ReadyQueue.end() - count, s_ReadyQueue.end());
            s_ReadyQueue.erase(s_ReadyQueue.end() - count, s_ReadyQueue.end());
        }
    }
    for (uint32_t itemId : ready) {
        try {
            if (!LoadIconFromDisk(itemId)) {
                // Resource not ready yet — queue for re-probe next frame
                std::lock_guard<std::mutex> lock(s_Mutex);
                s_PendingTexture.push_back(itemId);
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_LoadingIcons.erase(itemId);
            s_FailedIcons[itemId] = std::chrono::steady_clock::now();
        }
    }

    // 2. Re-probe pending textures that were submitted to Nexus but not yet ready (batched)
    std::vector<uint32_t> pending;
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        int count = std::min(TICK_BATCH_SIZE, static_cast<int>(s_PendingTexture.size()));
        if (count > 0) {
            pending.assign(s_PendingTexture.end() - count, s_PendingTexture.end());
            s_PendingTexture.erase(s_PendingTexture.end() - count, s_PendingTexture.end());
        }
    }
    for (uint32_t itemId : pending) {
        try {
            if (!LoadIconFromDisk(itemId)) {
                // Still not ready — re-queue
                std::lock_guard<std::mutex> lock(s_Mutex);
                s_PendingTexture.push_back(itemId);
            }
        } catch (...) {
            std::lock_guard<std::mutex> lock(s_Mutex);
            s_LoadingIcons.erase(itemId);
            s_FailedIcons[itemId] = std::chrono::steady_clock::now();
        }
    }
}

Texture_t* IconManager::GetIcon(uint32_t itemId) {
    if (!s_API) return nullptr;

    // Cache-only lookup — no Nexus API calls in the render path.
    // Texture probing is handled by Tick() in batches.
    std::lock_guard<std::mutex> lock(s_Mutex);
    auto it = s_IconCache.find(itemId);
    if (it != s_IconCache.end()) {
        if (it->second && it->second->Resource) {
            return it->second;
        }
        // Stale pointer - Nexus may have freed the texture
        s_IconCache.erase(it);
    }
    return nullptr;
}

void IconManager::RequestIcon(uint32_t itemId, const std::string& iconUrl) {
    if (!s_API) return;
    if (iconUrl.empty()) return;

    {
        std::lock_guard<std::mutex> lock(s_Mutex);

        // Check if already loaded or loading
        if (s_IconCache.find(itemId) != s_IconCache.end()) return;
        if (s_LoadingIcons.find(itemId) != s_LoadingIcons.end()) return;

        // Check if recently failed - don't retry until cooldown expires
        auto failIt = s_FailedIcons.find(itemId);
        if (failIt != s_FailedIcons.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - failIt->second).count();
            if (elapsed < RETRY_COOLDOWN_SEC) return;
            s_FailedIcons.erase(failIt);
        }

        // Mark as loading before releasing lock
        s_LoadingIcons[itemId] = true;
    }

    // Try loading from disk cache first (outside lock - no contention)
    if (LoadIconFromDisk(itemId)) return;

    // Not on disk - queue for download
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        QueuedRequest req;
        req.itemId = itemId;
        req.iconUrl = iconUrl;
        s_RequestQueue.push_back(req);
    }
    s_QueueCV.notify_one();
}

void IconManager::ProcessRequestQueue() {
    uint32_t itemId = 0;
    std::string iconUrl;

    {
        std::lock_guard<std::mutex> lock(s_Mutex);

        if (s_RequestQueue.empty()) return;

        // Dequeue one request
        QueuedRequest req = s_RequestQueue.front();
        s_RequestQueue.erase(s_RequestQueue.begin());

        itemId = req.itemId;
        iconUrl = req.iconUrl;
    }

    if (iconUrl.empty()) {
        // No URL - mark as failed
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_LoadingIcons.erase(itemId);
        s_FailedIcons[itemId] = std::chrono::steady_clock::now();
        return;
    }

    // Download to disk (this is the slow part - runs on background thread)
    std::string filePath = GetIconFilePath(itemId);
    if (DownloadToFile(iconUrl, filePath)) {
        // Push to ready queue for render thread to load into Nexus
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_ReadyQueue.push_back(itemId);
        return;
    }

    // Download failed
    std::lock_guard<std::mutex> lock(s_Mutex);
    s_LoadingIcons.erase(itemId);
    s_FailedIcons[itemId] = std::chrono::steady_clock::now();
}

void IconManager::DownloadWorker() {
    while (!s_StopWorker) {
        // Wait for work or shutdown signal
        {
            std::unique_lock<std::mutex> lock(s_Mutex);
            s_QueueCV.wait_for(lock, std::chrono::milliseconds(REQUEST_DELAY_MS), [] {
                return s_StopWorker.load() || !s_RequestQueue.empty();
            });
            if (s_StopWorker) return;
            if (s_RequestQueue.empty()) continue;
        }

        try {
            ProcessRequestQueue();
        } catch (...) {}

        // Rate limit between downloads
        std::this_thread::sleep_for(std::chrono::milliseconds(REQUEST_DELAY_MS));
    }
}

bool IconManager::IsIconLoaded(uint32_t itemId) {
    std::lock_guard<std::mutex> lock(s_Mutex);
    if (s_IconCache.find(itemId) != s_IconCache.end()) return true;
    if (s_LoadingIcons.find(itemId) != s_LoadingIcons.end()) return true;

    auto failIt = s_FailedIcons.find(itemId);
    if (failIt != s_FailedIcons.end()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - failIt->second).count();
        if (elapsed < RETRY_COOLDOWN_SEC) return true;
    }
    return false;
}

} // namespace HoardAndSeek

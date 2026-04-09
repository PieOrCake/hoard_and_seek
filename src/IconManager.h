#ifndef ICONMANAGER_H
#define ICONMANAGER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <chrono>
#include "../include/nexus/Nexus.h"

namespace HoardAndSeek {

class IconManager {
public:
    static void Initialize(AddonAPI_t* api);
    static void Shutdown();

    // Get icon texture for an item ID (returns nullptr if not loaded yet)
    static Texture_t* GetIcon(uint32_t itemId);

    // Request icon to be loaded (async) - uses GW2 API icon URL
    static void RequestIcon(uint32_t itemId, const std::string& iconUrl);

    // Process icon queue (call every frame, even when window is hidden)
    static void Tick();

    // Check if icon is loaded
    static bool IsIconLoaded(uint32_t itemId);

private:
    static void ProcessRequestQueue();
    static void DownloadWorker();

    // Disk cache helpers
    static std::string GetIconsDir();
    static std::string GetIconFilePath(uint32_t itemId);
    static bool DownloadToFile(const std::string& url, const std::string& filePath);
    static bool LoadIconFromDisk(uint32_t itemId);

    static AddonAPI_t* s_API;
    static std::unordered_map<uint32_t, Texture_t*> s_IconCache;
    static std::unordered_map<uint32_t, bool> s_LoadingIcons;
    static std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> s_FailedIcons;
    static const int RETRY_COOLDOWN_SEC = 300;
    static std::mutex s_Mutex;
    static std::string s_IconsDir;

    // Rate limiting
    struct QueuedRequest {
        uint32_t itemId;
        std::string iconUrl;
    };
    static std::vector<QueuedRequest> s_RequestQueue;
    static std::chrono::steady_clock::time_point s_LastRequestTime;
    static const int REQUEST_DELAY_MS = 100;

    // Background download thread
    static std::thread s_DownloadThread;
    static std::condition_variable s_QueueCV;
    static std::atomic<bool> s_StopWorker;

    // Ready queue: items downloaded to disk, waiting for render-thread texture load
    static std::vector<uint32_t> s_ReadyQueue;

    // Items submitted to Nexus but whose Resource isn't ready yet — re-probed in batches
    static std::vector<uint32_t> s_PendingTexture;
    static const int TICK_BATCH_SIZE = 5;
};

} // namespace HoardAndSeek

#endif // ICONMANAGER_H

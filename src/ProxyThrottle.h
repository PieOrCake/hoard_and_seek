#pragma once
#include <string>
#include <functional>
#include <cstdint>

namespace HoardAndSeek {
namespace ProxyThrottle {

    // --- Tunables ---
    constexpr int    RATE_BUCKET_CAPACITY     = 60;
    constexpr double RATE_REFILL_PER_SEC_MAX  = 8.0;
    constexpr double RATE_REFILL_PER_SEC_MIN  = 2.0;
    constexpr int    BACKOFF_DURATION_SECONDS = 60;
    constexpr double BACKOFF_RECOVER_PER_SEC  = 0.1;
    constexpr int    CACHE_TTL_SECONDS        = 30;
    constexpr size_t CACHE_MAX_ENTRIES        = 256;
    constexpr size_t QUEUE_MAX_DEPTH          = 256;
    constexpr size_t PER_REQUESTER_MAX        = 32;

    // --- Lifecycle ---
    void Start();
    void Stop();

    // --- Rate limiter ---
    void AcquireToken();
    void NotifyRateLimited();
    double CurrentRefillRate();

    // --- Cache ---
    bool CacheLookup(const std::string& account_name,
                     const std::string& endpoint,
                     std::string& out);
    void CacheStore(const std::string& account_name,
                    const std::string& endpoint,
                    const std::string& body);

    // --- Job submission ---
    struct SubmitResult {
        bool     accepted;
        uint32_t queue_depth_after;
    };
    SubmitResult Submit(const std::string& requester,
                        const std::string& account_name,
                        const std::string& endpoint,
                        std::function<std::string()> do_fetch,
                        std::function<void(const std::string& body)> on_complete);

    uint32_t CurrentQueueDepth();

} // namespace ProxyThrottle
} // namespace HoardAndSeek

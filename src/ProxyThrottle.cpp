#include "ProxyThrottle.h"
#include "nexus/Nexus.h"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <unordered_map>

extern AddonAPI_t* APIDefs;

namespace HoardAndSeek {
namespace ProxyThrottle {

namespace {
    std::mutex              g_rate_mutex;
    std::condition_variable g_rate_cv;
    double                  g_tokens          = (double)RATE_BUCKET_CAPACITY;
    double                  g_effective_rate  = RATE_REFILL_PER_SEC_MAX;
    std::chrono::steady_clock::time_point g_last_refill   = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point g_backoff_until = std::chrono::steady_clock::time_point{};
    double                  g_last_logged_rate = RATE_REFILL_PER_SEC_MAX;

    void LogRateChange(double new_rate) {
        if (!APIDefs) return;
        char msg[128];
        std::snprintf(msg, sizeof(msg),
                      "Proxy effective rate now %.1f req/sec", new_rate);
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", msg);
    }

    void LogBackoffTriggered() {
        if (!APIDefs) return;
        APIDefs->Log(LOGL_WARNING, "HoardAndSeek",
                     "GW2 API returned 429 - backing off proxy rate for 60s");
    }

    void UpdateEffectiveRateLocked() {
        auto now = std::chrono::steady_clock::now();
        if (now < g_backoff_until) {
            g_effective_rate = RATE_REFILL_PER_SEC_MIN;
        } else if (g_effective_rate < RATE_REFILL_PER_SEC_MAX) {
            double dt = std::chrono::duration<double>(now - g_backoff_until).count();
            g_effective_rate = std::min(RATE_REFILL_PER_SEC_MAX,
                                        RATE_REFILL_PER_SEC_MIN + dt * BACKOFF_RECOVER_PER_SEC);
        } else {
            g_effective_rate = RATE_REFILL_PER_SEC_MAX;
        }
        if (std::abs(g_effective_rate - g_last_logged_rate) >= 0.5) {
            LogRateChange(g_effective_rate);
            g_last_logged_rate = g_effective_rate;
        }
    }

    void RefillLocked() {
        UpdateEffectiveRateLocked();
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - g_last_refill).count();
        if (dt > 0.0) {
            g_tokens = std::min<double>((double)RATE_BUCKET_CAPACITY,
                                        g_tokens + dt * g_effective_rate);
            g_last_refill = now;
        }
    }

    struct CacheEntry {
        std::string body;
        std::chrono::steady_clock::time_point expires_at;
    };
    std::mutex g_cache_mutex;
    std::unordered_map<std::string, CacheEntry> g_cache;

    std::string CacheKey(const std::string& account_name, const std::string& endpoint) {
        return account_name + "|" + endpoint;
    }

    void EvictExpiredLocked() {
        auto now = std::chrono::steady_clock::now();
        for (auto it = g_cache.begin(); it != g_cache.end(); ) {
            if (it->second.expires_at <= now) it = g_cache.erase(it);
            else ++it;
        }
    }
}

void AcquireToken() {
    std::unique_lock<std::mutex> lock(g_rate_mutex);
    for (;;) {
        RefillLocked();
        if (g_tokens >= 1.0) {
            g_tokens -= 1.0;
            return;
        }
        double need = 1.0 - g_tokens;
        auto wait_ms = (int)std::ceil((need / std::max(0.1, g_effective_rate)) * 1000.0);
        if (wait_ms < 10) wait_ms = 10;
        g_rate_cv.wait_for(lock, std::chrono::milliseconds(wait_ms));
    }
}

void NotifyRateLimited() {
    {
        std::lock_guard<std::mutex> lock(g_rate_mutex);
        auto now = std::chrono::steady_clock::now();
        g_backoff_until = now + std::chrono::seconds(BACKOFF_DURATION_SECONDS);
        g_effective_rate = RATE_REFILL_PER_SEC_MIN;
        g_tokens = std::min(g_tokens, (double)RATE_BUCKET_CAPACITY * 0.25);
        g_rate_cv.notify_all();
    }
    LogBackoffTriggered();
}

double CurrentRefillRate() {
    std::lock_guard<std::mutex> lock(g_rate_mutex);
    return g_effective_rate;
}

// --- Stubs (implemented in later tasks) ---
void Start() {}
void Stop() {}

bool CacheLookup(const std::string& account_name,
                 const std::string& endpoint,
                 std::string& out) {
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    auto it = g_cache.find(CacheKey(account_name, endpoint));
    if (it == g_cache.end()) return false;
    if (it->second.expires_at <= std::chrono::steady_clock::now()) {
        g_cache.erase(it);
        return false;
    }
    out = it->second.body;
    return true;
}

void CacheStore(const std::string& account_name,
                const std::string& endpoint,
                const std::string& body) {
    if (body.empty()) return;
    if (body.size() > 0 && body[0] == '<') return; // HTML error page - skip
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    EvictExpiredLocked();
    if (g_cache.size() >= CACHE_MAX_ENTRIES) {
        auto oldest = g_cache.begin();
        for (auto it = g_cache.begin(); it != g_cache.end(); ++it) {
            if (it->second.expires_at < oldest->second.expires_at) oldest = it;
        }
        g_cache.erase(oldest);
    }
    g_cache[CacheKey(account_name, endpoint)] = CacheEntry{
        body,
        std::chrono::steady_clock::now() + std::chrono::seconds(CACHE_TTL_SECONDS)
    };
}
SubmitResult Submit(const std::string&, const std::string&, const std::string&,
                    std::function<std::string()>,
                    std::function<void(const std::string&)>) {
    return {true, 0};
}
uint32_t CurrentQueueDepth() { return 0; }

}} // namespaces

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
#include <deque>
#include <vector>

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

    struct Job {
        std::string requester;
        std::string account_name;
        std::string endpoint;
        std::function<std::string()> do_fetch;
        std::function<void(const std::string&)> on_complete;
    };

    std::mutex              g_queue_mutex;
    std::condition_variable g_queue_cv;
    std::unordered_map<std::string, std::deque<Job>> g_per_requester;
    std::vector<std::string> g_rr_order;
    size_t                   g_rr_index = 0;
    size_t                   g_total_depth = 0;
    bool                     g_worker_run = false;
    std::thread              g_worker_thread;

    bool PopNextJobLocked(Job& out) {
        if (g_total_depth == 0) return false;
        for (size_t tried = 0; tried < g_rr_order.size(); ++tried) {
            const std::string req = g_rr_order[g_rr_index];
            g_rr_index = (g_rr_index + 1) % g_rr_order.size();
            auto it = g_per_requester.find(req);
            if (it != g_per_requester.end() && !it->second.empty()) {
                out = std::move(it->second.front());
                it->second.pop_front();
                g_total_depth--;
                if (it->second.empty()) {
                    g_per_requester.erase(it);
                    auto rit = std::find(g_rr_order.begin(), g_rr_order.end(), req);
                    if (rit != g_rr_order.end()) {
                        size_t pos = (size_t)(rit - g_rr_order.begin());
                        g_rr_order.erase(rit);
                        if (g_rr_order.empty()) {
                            g_rr_index = 0;
                        } else {
                            if (g_rr_index > pos) g_rr_index--;
                            g_rr_index %= g_rr_order.size();
                        }
                    }
                }
                return true;
            }
        }
        return false;
    }

    void WorkerLoop() {
        while (true) {
            Job job;
            {
                std::unique_lock<std::mutex> lock(g_queue_mutex);
                g_queue_cv.wait(lock, [] { return !g_worker_run || g_total_depth > 0; });
                if (!g_worker_run && g_total_depth == 0) return;
                if (!PopNextJobLocked(job)) continue;
            }
            std::string body;
            if (!CacheLookup(job.account_name, job.endpoint, body)) {
                body = job.do_fetch();
                CacheStore(job.account_name, job.endpoint, body);
            }
            try { job.on_complete(body); } catch (...) {}
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

int BackoffSecondsRemaining() {
    std::lock_guard<std::mutex> lock(g_rate_mutex);
    auto now = std::chrono::steady_clock::now();
    if (now >= g_backoff_until) return 0;
    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(g_backoff_until - now).count();
    return (int)remaining + 1; // round up so countdown reads naturally
}

void Start() {
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    if (g_worker_run) return;
    g_worker_run = true;
    g_worker_thread = std::thread(WorkerLoop);
}

void Stop() {
    {
        std::lock_guard<std::mutex> lock(g_queue_mutex);
        g_worker_run = false;
        g_queue_cv.notify_all();
    }
    if (g_worker_thread.joinable()) g_worker_thread.join();
}

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
SubmitResult Submit(const std::string& requester,
                    const std::string& account_name,
                    const std::string& endpoint,
                    std::function<std::string()> do_fetch,
                    std::function<void(const std::string&)> on_complete) {
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    if (g_total_depth >= QUEUE_MAX_DEPTH) return {false, (uint32_t)g_total_depth};
    auto& q = g_per_requester[requester];
    if (q.size() >= PER_REQUESTER_MAX) return {false, (uint32_t)g_total_depth};
    if (q.empty()) {
        g_rr_order.push_back(requester);
    }
    q.push_back(Job{requester, account_name, endpoint, std::move(do_fetch), std::move(on_complete)});
    g_total_depth++;
    g_queue_cv.notify_one();
    return {true, (uint32_t)g_total_depth};
}

uint32_t CurrentQueueDepth() {
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    return (uint32_t)g_total_depth;
}

}} // namespaces

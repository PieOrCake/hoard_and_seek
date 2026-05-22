# Proxy Rate Limiting & Caching Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prevent third-party addons using the H&S proxy API from exhausting the GW2 API rate limit (which would 429 our own fetches too). Replace per-request detached threads with a serialised, rate-limited worker; add a short-TTL response cache; share the rate budget with H&S's own account fetches; surface backpressure via API v4.

**Architecture:** A single new translation unit (`ProxyThrottle`) introduces three cooperating pieces: (1) a process-wide token bucket gating all outbound GW2 HTTP calls, (2) a TTL response cache keyed on `(account_name, endpoint)`, and (3) a bounded job queue served by one worker thread with per-requester round-robin fairness. `GW2API::HttpGet` is wired to consume tokens before each network call, so H&S's own refreshes share the same budget. All proxy handlers in `dllmain.cpp` stop spawning detached threads and instead enqueue jobs; the worker handles cache lookup, network call, and response dispatch. The public header bumps to v4 with appended backpressure fields and a new `HOARD_STATUS_BUSY` status code; old (v3) callers continue to work unchanged because new fields are only written when the caller declares v4.

**Tech Stack:** C++17, Windows MinGW cross-build, Nexus addon API, WinINet, nlohmann/json (already vendored), `std::thread` / `std::condition_variable` / `std::chrono::steady_clock` (already in use elsewhere).

**Verification model:** This project has no test harness — verification is by compile + manual in-game load (per `CLAUDE.md`). Each task ends with `cd build && make` and the build must succeed cleanly. End-to-end behaviour is verified in-game after the final task.

---

## File Structure

**New files:**
- `src/ProxyThrottle.h` — public interface: `RateLimiter`, `ProxyCache`, `ProxyQueue`, `ProxyJob`
- `src/ProxyThrottle.cpp` — implementation of all three

**Modified files:**
- `CMakeLists.txt` — add `src/ProxyThrottle.cpp` to the target
- `src/GW2API.cpp` — `HttpGet` and `HttpGetEx` acquire a token before each WinINet call
- `src/dllmain.cpp` — proxy handlers (`OnQueryApi`, `OnQueryAchievement`, `OnQueryMastery`, `OnQuerySkins`, `OnQueryRecipes`, `OnQueryWizardsVault`) submit jobs instead of spawning threads; load/unload starts and stops the worker
- `include/HoardAndSeekAPI.h` — `HOARD_API_VERSION` → 4, new `HOARD_STATUS_BUSY`, appended fields to `HoardQueryApiResponse` (and the other batch responses), updated threading docs
- `handover.md` — note the new version and behaviour

**Why one new .cpp instead of three:** the three pieces (limiter, cache, queue) are interlocked — the queue calls the cache on submit and the limiter inside the worker. Splitting them creates artificial header churn. Total ~400 LOC is fine for one file.

---

## Design Constants

These appear in multiple tasks; defining them once here keeps them consistent.

The rate-limit numbers are deliberately above alter_ego's conservative 5 req/sec (200ms sleep) and rely on **adaptive backoff** (Task 2 Step 2) to pull back automatically if GW2 ever returns HTTP 429. GW2's exact published limit isn't documented; this design pushes the ceiling and lets the API tell us when to slow down.

```cpp
// In ProxyThrottle.h, inside namespace HoardAndSeek
namespace ProxyThrottle {
    // Token bucket (base values — actual refill rate adapts via NotifyRateLimited)
    constexpr int    RATE_BUCKET_CAPACITY    = 60;   // burst tolerance
    constexpr double RATE_REFILL_PER_SEC_MAX = 8.0;  // base sustained rate (~480/min)
    constexpr double RATE_REFILL_PER_SEC_MIN = 2.0;  // floor when adaptively backed off

    // Adaptive backoff
    constexpr int    BACKOFF_DURATION_SECONDS = 60;  // how long a 429 keeps us throttled
    constexpr double BACKOFF_RECOVER_PER_SEC  = 0.1; // refill rate added back per second after backoff window

    // Cache
    constexpr int    CACHE_TTL_SECONDS       = 30;
    constexpr size_t CACHE_MAX_ENTRIES       = 256;

    // Queue
    constexpr size_t QUEUE_MAX_DEPTH         = 256;
    constexpr size_t PER_REQUESTER_MAX       = 32;
}
```

---

## Task 1: Skeleton `ProxyThrottle` translation unit + CMake wiring

**Files:**
- Create: `src/ProxyThrottle.h`
- Create: `src/ProxyThrottle.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `src/ProxyThrottle.h` with empty namespaces and the constants block**

```cpp
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
    void Start();   // spawn worker thread
    void Stop();    // join worker, drain queue (responses for in-flight jobs are dropped silently)

    // --- Rate limiter (used by both proxy worker and GW2API::HttpGet) ---
    // Blocks the calling thread until a token is available. Safe to call from any thread.
    void AcquireToken();

    // Called by HTTP helpers when GW2 returns HTTP 429. Triggers adaptive backoff:
    // refill rate drops to RATE_REFILL_PER_SEC_MIN for BACKOFF_DURATION_SECONDS, then
    // recovers linearly at BACKOFF_RECOVER_PER_SEC. Safe to call from any thread.
    void NotifyRateLimited();

    // Optional: current effective refill rate, for logging / diagnostics.
    double CurrentRefillRate();

    // --- Cache ---
    // Returns true and fills `out` if a fresh entry exists.
    bool CacheLookup(const std::string& account_name,
                     const std::string& endpoint,
                     std::string& out);
    void CacheStore(const std::string& account_name,
                    const std::string& endpoint,
                    const std::string& body);

    // --- Job submission ---
    // `do_fetch` is invoked on the worker thread after the rate-limit token is acquired
    // and after the cache miss is confirmed. It must return the raw response body.
    // `on_complete` is invoked on the worker thread with the body (cached or fresh).
    // Returns false if the queue is full or the per-requester cap is hit — caller
    // should immediately deliver a HOARD_STATUS_BUSY response.
    struct SubmitResult {
        bool     accepted;
        uint32_t queue_depth_after; // for backpressure reporting
    };
    SubmitResult Submit(const std::string& requester,
                        const std::string& account_name,
                        const std::string& endpoint,
                        std::function<std::string()> do_fetch,
                        std::function<void(const std::string& body)> on_complete);

    // For backpressure reporting on responses
    uint32_t CurrentQueueDepth();

} // namespace ProxyThrottle
} // namespace HoardAndSeek
```

- [ ] **Step 2: Create `src/ProxyThrottle.cpp` with empty stub bodies**

```cpp
#include "ProxyThrottle.h"

namespace HoardAndSeek {
namespace ProxyThrottle {

void Start() {}
void Stop() {}
void AcquireToken() {}
void NotifyRateLimited() {}
double CurrentRefillRate() { return RATE_REFILL_PER_SEC_MAX; }
bool CacheLookup(const std::string&, const std::string&, std::string&) { return false; }
void CacheStore(const std::string&, const std::string&, const std::string&) {}
SubmitResult Submit(const std::string&, const std::string&, const std::string&,
                    std::function<std::string()>,
                    std::function<void(const std::string&)>) {
    return {true, 0};
}
uint32_t CurrentQueueDepth() { return 0; }

}}
```

- [ ] **Step 3: Add the new .cpp to CMake**

Open `CMakeLists.txt`, find the `add_library` (or `target_sources`) line that lists `src/dllmain.cpp src/GW2API.cpp src/IconManager.cpp src/PermissionManager.cpp` and append `src/ProxyThrottle.cpp`.

- [ ] **Step 4: Build to confirm the skeleton compiles**

```bash
cd build && make
```

Expected: clean build, no warnings, `HoardAndSeek.dll` produced.

- [ ] **Step 5: Commit**

```bash
git add src/ProxyThrottle.h src/ProxyThrottle.cpp CMakeLists.txt
git commit -m "Scaffold ProxyThrottle translation unit"
```

---

## Task 2: Implement the token-bucket rate limiter with adaptive 429 backoff

**Files:**
- Modify: `src/ProxyThrottle.cpp`

- [ ] **Step 1: Add the adaptive token-bucket state and `AcquireToken`**

At the top of `ProxyThrottle.cpp`, replace its current body with includes and the state block:

```cpp
#include "ProxyThrottle.h"
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>

// Forward declaration of Nexus logger (defined elsewhere; resolved at link time).
// We use this only for diagnostic logging when the effective rate changes.
extern struct AddonAPI_t* APIDefs;

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

    void UpdateEffectiveRateLocked() {
        auto now = std::chrono::steady_clock::now();
        if (now < g_backoff_until) {
            g_effective_rate = RATE_REFILL_PER_SEC_MIN;
        } else if (g_effective_rate < RATE_REFILL_PER_SEC_MAX) {
            // Linear recovery after the backoff window expires
            double dt = std::chrono::duration<double>(now - g_backoff_until).count();
            g_effective_rate = std::min(RATE_REFILL_PER_SEC_MAX,
                                        RATE_REFILL_PER_SEC_MIN + dt * BACKOFF_RECOVER_PER_SEC);
        } else {
            g_effective_rate = RATE_REFILL_PER_SEC_MAX;
        }
        // Diagnostic logging on rate change (>= 10% delta)
        if (APIDefs && std::abs(g_effective_rate - g_last_logged_rate) >= 0.5) {
            // Forward-declared logger; bail if not available
            extern void LogRateChange(double); LogRateChange(g_effective_rate);
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

double CurrentRefillRate() {
    std::lock_guard<std::mutex> lock(g_rate_mutex);
    return g_effective_rate;
}
```

- [ ] **Step 2: Add `NotifyRateLimited` and the diagnostic logger**

Append to the same file, below `AcquireToken`:

```cpp
void NotifyRateLimited() {
    std::lock_guard<std::mutex> lock(g_rate_mutex);
    auto now = std::chrono::steady_clock::now();
    g_backoff_until = now + std::chrono::seconds(BACKOFF_DURATION_SECONDS);
    g_effective_rate = RATE_REFILL_PER_SEC_MIN;
    // Drain a chunk of the bucket so we don't immediately burst again
    g_tokens = std::min(g_tokens, (double)RATE_BUCKET_CAPACITY * 0.25);
    g_rate_cv.notify_all();
    if (APIDefs) { extern void LogBackoffTriggered(); LogBackoffTriggered(); }
}

// Diagnostic logging helpers — defined here, used by the limiter.
// We avoid pulling in nexus.h to keep this file independent; we rely on the
// global APIDefs pointer which is initialised by AddonLoad.
}} // close namespaces temporarily so we can include nexus types

#include "nexus/Nexus.h"
namespace HoardAndSeek { namespace ProxyThrottle {

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
                 "GW2 API returned 429 — backing off proxy rate for 60s");
}
```

Note the temporary namespace close: this is so we can include `nexus/Nexus.h` (which defines `AddonAPI_t` and `LOGL_*`) without re-declaring it inside the namespace. Add `#include <cstdio>` to the top of the file for `snprintf`.

- [ ] **Step 3: Build**

```bash
cd build && make
```

Expected: clean build.

- [ ] **Step 4: Commit**

```bash
git add src/ProxyThrottle.cpp src/ProxyThrottle.h
git commit -m "Token-bucket limiter with adaptive 429 backoff"
```

---

## Task 3: Route `GW2API::HttpGet` / `HttpGetEx` through the limiter

This makes H&S's own account refreshes consume from the same bucket as the proxy, so they cannot collectively breach the GW2 ceiling.

**Files:**
- Modify: `src/GW2API.cpp`

- [ ] **Step 1: Include the throttle header**

At the top of `src/GW2API.cpp`, alongside the existing `#include` lines, add:

```cpp
#include "ProxyThrottle.h"
```

- [ ] **Step 2: Rewrite `HttpGet` to acquire a token and detect 429**

Locate `HttpGet` at [src/GW2API.cpp:99](src/GW2API.cpp#L99). Replace the whole function body with the version below — it now queries the HTTP status, notifies the limiter on 429, and returns an empty body in that case (matching the existing failure contract):

```cpp
std::string GW2API::HttpGet(const std::string& url) {
    HoardAndSeek::ProxyThrottle::AcquireToken();
    HINTERNET hInternet = InternetOpenA("HoardAndSeek/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return "";

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                  INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);
    if (!hUrl) { InternetCloseHandle(hInternet); return ""; }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                   &statusCode, &statusSize, NULL);
    if (statusCode == 429) {
        HoardAndSeek::ProxyThrottle::NotifyRateLimited();
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return "";
    }

    std::string result;
    char buffer[8192];
    DWORD bytesRead;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        result.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return result;
}
```

- [ ] **Step 3: Update `HttpGetEx` the same way**

Locate `HttpGetEx` at [src/GW2API.cpp:125](src/GW2API.cpp#L125). It already queries the status code; add the token acquisition at the top and the 429 notification after the status check:

```cpp
GW2API::HttpResponse GW2API::HttpGetEx(const std::string& url) {
    HoardAndSeek::ProxyThrottle::AcquireToken();
    HttpResponse result;
    HINTERNET hInternet = InternetOpenA("HoardAndSeek/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return result;

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                  INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);
    if (!hUrl) { InternetCloseHandle(hInternet); return result; }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                   &statusCode, &statusSize, NULL);
    result.status_code = (int)statusCode;
    if (statusCode == 429) {
        HoardAndSeek::ProxyThrottle::NotifyRateLimited();
    }

    char buffer[8192];
    DWORD bytesRead;
    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        result.body.append(buffer, bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);
    return result;
}
```

- [ ] **Step 4: Build**

```bash
cd build && make
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/GW2API.cpp
git commit -m "Route GW2API HTTP helpers through the shared token bucket"
```

---

## Task 4: Implement the TTL response cache

**Files:**
- Modify: `src/ProxyThrottle.cpp`

- [ ] **Step 1: Add cache state to the anonymous namespace**

Below the rate-limiter state block in `src/ProxyThrottle.cpp`, add:

```cpp
namespace {
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
```

Add `#include <unordered_map>` at the top.

- [ ] **Step 2: Replace the `CacheLookup` stub**

```cpp
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
```

- [ ] **Step 3: Replace the `CacheStore` stub**

```cpp
void CacheStore(const std::string& account_name,
                const std::string& endpoint,
                const std::string& body) {
    if (body.empty()) return;             // Never cache failures
    if (body.size() > 0 && body[0] == '<') return; // HTML error page — skip
    std::lock_guard<std::mutex> lock(g_cache_mutex);
    EvictExpiredLocked();
    if (g_cache.size() >= CACHE_MAX_ENTRIES) {
        // Simple bound: drop oldest by expiry
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
```

- [ ] **Step 4: Build**

```bash
cd build && make
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/ProxyThrottle.cpp
git commit -m "Add TTL response cache for proxy queries"
```

---

## Task 5: Implement the bounded job queue with per-requester fairness and worker thread

**Files:**
- Modify: `src/ProxyThrottle.cpp`

- [ ] **Step 1: Add the job + queue state below the cache block**

```cpp
namespace {
    struct Job {
        std::string requester;
        std::string account_name;
        std::string endpoint;
        std::function<std::string()> do_fetch;
        std::function<void(const std::string&)> on_complete;
    };

    std::mutex              g_queue_mutex;
    std::condition_variable g_queue_cv;
    // Per-requester FIFOs
    std::unordered_map<std::string, std::deque<Job>> g_per_requester;
    // Round-robin order
    std::vector<std::string> g_rr_order;
    size_t                   g_rr_index = 0;
    size_t                   g_total_depth = 0;
    bool                     g_worker_run = false;
    std::thread              g_worker_thread;

    bool PopNextJobLocked(Job& out) {
        if (g_total_depth == 0) return false;
        for (size_t tried = 0; tried < g_rr_order.size(); ++tried) {
            const std::string& req = g_rr_order[g_rr_index];
            g_rr_index = (g_rr_index + 1) % g_rr_order.size();
            auto it = g_per_requester.find(req);
            if (it != g_per_requester.end() && !it->second.empty()) {
                out = std::move(it->second.front());
                it->second.pop_front();
                g_total_depth--;
                if (it->second.empty()) {
                    // Remove this requester from rotation
                    g_per_requester.erase(it);
                    auto rit = std::find(g_rr_order.begin(), g_rr_order.end(), req);
                    if (rit != g_rr_order.end()) {
                        size_t pos = rit - g_rr_order.begin();
                        g_rr_order.erase(rit);
                        if (!g_rr_order.empty() && g_rr_index > pos) g_rr_index--;
                        if (!g_rr_order.empty()) g_rr_index %= g_rr_order.size();
                        else g_rr_index = 0;
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
            // Re-check cache (another submission may have populated it after this one was enqueued)
            std::string body;
            if (!CacheLookup(job.account_name, job.endpoint, body)) {
                body = job.do_fetch();         // do_fetch is expected to call AcquireToken via HttpGet
                CacheStore(job.account_name, job.endpoint, body);
            }
            try { job.on_complete(body); } catch (...) {}
        }
    }
}
```

Add `#include <deque>` and `#include <vector>` at the top.

- [ ] **Step 2: Replace the `Submit` stub**

```cpp
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
        // First job from this requester: add to rotation
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
```

- [ ] **Step 3: Implement `Start` / `Stop`**

```cpp
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
```

- [ ] **Step 4: Build**

```bash
cd build && make
```

Expected: clean build.

- [ ] **Step 5: Commit**

```bash
git add src/ProxyThrottle.cpp
git commit -m "Add bounded proxy job queue with per-requester fairness"
```

---

## Task 6: Bump public API to v4 — new status, backpressure fields, doc updates

**Files:**
- Modify: `include/HoardAndSeekAPI.h`

- [ ] **Step 1: Bump version macros and add the new status code**

In `include/HoardAndSeekAPI.h`, change:

```cpp
#define HOARD_API_VERSION 3
```

to:

```cpp
#define HOARD_API_VERSION 4
```

And in the version comment at the top of the file, change `Version: 3` to `Version: 4`.

After the existing status defines around line 20, add:

```cpp
#define HOARD_STATUS_BUSY     3  // v4+: proxy queue full or per-addon limit reached; retry after retry_after_ms
```

- [ ] **Step 2: Append backpressure fields to `HoardQueryApiResponse`**

Find the struct around line 446. After the existing `char json[65536];` line, add:

```cpp
    // --- v4+ fields (only written when request->api_version >= 4) ---
    uint32_t retry_after_ms;    // Suggested wait before retrying (0 = no advice)
    uint32_t queue_depth;       // Approximate queue depth at dispatch time
    uint32_t tokens_remaining;  // Reserved for future use; currently always 0
```

- [ ] **Step 3: Append the same three fields to the other API-touching responses**

Add the identical block to the end of:
- `HoardQueryAchievementResponse`
- `HoardQueryMasteryResponse`
- `HoardQuerySkinsResponse`
- `HoardQueryRecipesResponse`
- `HoardQueryWizardsVaultResponse`

Use the exact same three field declarations as in Step 2 so the layout is identical across responses.

- [ ] **Step 4: Update the Threading & Dispatch Model docblock**

Find the "CRITICAL: Threading & Dispatch Model" comment near line 44. Replace its opening paragraph with one that distinguishes cache-served queries from network-served ones. Specifically, replace lines 50-54 (the "When you call Events_Raise..." paragraph) with:

```cpp
// 1. Cache-served queries (EV_HOARD_QUERY_ITEM, EV_HOARD_QUERY_WALLET,
//    EV_HOARD_QUERY_ACCOUNTS, EV_HOARD_PING) are answered SYNCHRONOUSLY:
//    your response handler runs inside the Events_Raise call.
//
//    Network-served queries (EV_HOARD_QUERY_API, EV_HOARD_QUERY_ACHIEVEMENT,
//    EV_HOARD_QUERY_MASTERY, EV_HOARD_QUERY_SKINS, EV_HOARD_QUERY_RECIPES,
//    EV_HOARD_QUERY_WIZARDSVAULT) are ASYNCHRONOUS: H&S returns from
//    Events_Raise immediately and your response_event fires later from
//    H&S's worker thread. The same lock-handling rules still apply.
```

Also add a new paragraph at the end of the docblock (before the `// =====` line that closes it):

```cpp
// 6. Rate limiting (v4+): Network-served queries pass through a shared rate
//    limiter and a bounded queue. If the queue is full or your addon already
//    has too many in-flight requests, H&S immediately returns a response with
//    status = HOARD_STATUS_BUSY and retry_after_ms set. Back off and retry —
//    do NOT spin. Responses for cached endpoints (recently-fetched, same
//    account + endpoint) return instantly with no queue cost.
```

- [ ] **Step 5: Build**

```bash
cd build && make
```

Expected: clean build. (No source file consumes the new fields yet — that comes in Task 7.)

- [ ] **Step 6: Commit**

```bash
git add include/HoardAndSeekAPI.h
git commit -m "API v4: HOARD_STATUS_BUSY and backpressure response fields"
```

---

## Task 7: Migrate `OnQueryApi` to the queue + cache pipeline

This is the showcase proxy endpoint. The other handlers follow the same pattern in Task 8.

**Files:**
- Modify: `src/dllmain.cpp`

- [ ] **Step 1: Add the header include**

Near the other `#include` directives at the top of `src/dllmain.cpp`, add:

```cpp
#include "ProxyThrottle.h"
```

- [ ] **Step 2: Replace `OnQueryApi`'s body**

Locate `OnQueryApi` at [src/dllmain.cpp:1151](src/dllmain.cpp#L1151). Replace the function body (everything from the `if (!eventArgs || !APIDefs) return;` line through the closing `}` of the function) with:

```cpp
void OnQueryApi(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryApiRequest*)eventArgs;
    if (req->api_version > HOARD_API_VERSION || req->api_version == 0) return;
    if (req->response_event[0] == '\0' || req->endpoint[0] == '\0') return;

    const bool wants_v4 = (req->api_version >= 4);
    const std::string requester(req->requester);
    const std::string endpoint(req->endpoint);
    const std::string response_event(req->response_event);
    std::string account_name;
    if (req->api_version >= 3 && req->account_name[0] != '\0') {
        account_name = std::string(req->account_name);
    }

    // Helper to build + dispatch a response
    auto build_resp = [&](uint8_t status, const std::string& body,
                          uint32_t retry_after_ms, uint32_t qdepth) {
        auto* resp = new HoardQueryApiResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->status = status;
        strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
        strncpy(resp->endpoint, endpoint.c_str(), sizeof(resp->endpoint) - 1);
        resp->endpoint[sizeof(resp->endpoint) - 1] = '\0';
        resp->json_length = (uint32_t)body.length();
        if (body.length() >= sizeof(resp->json)) {
            resp->truncated = 1;
            memcpy(resp->json, body.c_str(), sizeof(resp->json) - 1);
            resp->json[sizeof(resp->json) - 1] = '\0';
        } else {
            resp->truncated = 0;
            memcpy(resp->json, body.c_str(), body.length());
            resp->json[body.length()] = '\0';
        }
        if (wants_v4) {
            resp->retry_after_ms = retry_after_ms;
            resp->queue_depth    = qdepth;
            resp->tokens_remaining = 0;
        }
        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
    };

    uint8_t perm = CheckAddonPermission(req->requester, EV_HOARD_QUERY_API);
    if (perm != HOARD_STATUS_OK) {
        build_resp(perm, "", 0, 0);
        return;
    }

    // Fast path: cache hit (synchronous response)
    std::string cached;
    if (HoardAndSeek::ProxyThrottle::CacheLookup(account_name, endpoint, cached)) {
        build_resp(HOARD_STATUS_OK, cached, 0,
                   HoardAndSeek::ProxyThrottle::CurrentQueueDepth());
        return;
    }

    // Enqueue the network fetch
    auto fetch = [endpoint, account_name]() -> std::string {
        std::string url = "https://api.guildwars2.com" + endpoint;
        return HoardAndSeek::GW2API::AuthenticatedGet(url, account_name);
    };
    auto complete = [build_resp](const std::string& body) {
        build_resp(HOARD_STATUS_OK, body, 0,
                   HoardAndSeek::ProxyThrottle::CurrentQueueDepth());
    };

    auto sr = HoardAndSeek::ProxyThrottle::Submit(
        requester, account_name, endpoint, std::move(fetch), std::move(complete));
    if (!sr.accepted) {
        // Backpressure: estimate retry time from queue depth / refill rate
        uint32_t retry_ms = (uint32_t)(
            (double)sr.queue_depth_after / HoardAndSeek::ProxyThrottle::RATE_REFILL_PER_SEC * 1000.0);
        if (retry_ms < 500) retry_ms = 500;
        build_resp(HOARD_STATUS_BUSY, "", retry_ms, sr.queue_depth_after);
    }
}
```

Note: `build_resp` captures `account_name`, `endpoint`, `response_event`, `wants_v4` by reference. That's safe for the synchronous paths (permission denied, cache hit, queue full) which all execute inside `OnQueryApi` before it returns. For the async `complete` lambda, it captures `build_resp` by value — but `build_resp` itself captures by reference. **This is a bug.** Fix: switch `build_resp` to capture by value:

Change the line:
```cpp
auto build_resp = [&](uint8_t status, const std::string& body,
```
to:
```cpp
auto build_resp = [=](uint8_t status, const std::string& body,
```

- [ ] **Step 3: Wire `Start` and `Stop` into the addon lifecycle**

Locate `AddonLoad` at the bottom of `src/dllmain.cpp`. After the existing event subscribe block (after the `Events_Subscribe(EV_HOARD_QUERY_API, OnQueryApi);` line around [src/dllmain.cpp:1422](src/dllmain.cpp#L1422)), add:

```cpp
    HoardAndSeek::ProxyThrottle::Start();
```

Then in `AddonUnload` (the matching function nearby), add as the first statement:

```cpp
    HoardAndSeek::ProxyThrottle::Stop();
```

- [ ] **Step 4: Build**

```bash
cd build && make
```

Expected: clean build, no warnings about unused captures or missing functions.

- [ ] **Step 5: Commit**

```bash
git add src/dllmain.cpp
git commit -m "Route OnQueryApi through ProxyThrottle queue + cache"
```

---

## Task 8: Migrate the other network-served handlers to the queue

Each of these handlers currently spawns a detached `std::thread`. They all follow the same shape: build URL, call `HttpGet`/`AuthenticatedGet`, parse JSON, fill response, raise event. We push the URL build + HTTP call into the queue, and do the JSON parsing + response build in the `on_complete` lambda.

**Files:**
- Modify: `src/dllmain.cpp`

For each handler below, perform the same edit pattern:
1. Hoist permission check and request copy (already done — keep that part).
2. Build a `fetch` lambda that returns the raw response body.
3. Build a `complete` lambda that parses the body and raises the response event.
4. Call `ProxyThrottle::Submit(...)`. If rejected, immediately raise a response with `HOARD_STATUS_BUSY` (the existing response struct will be filled with empty data + `status = HOARD_STATUS_BUSY`; only the v4 fields require the request to declare v4).

- [ ] **Step 1: Migrate `OnQueryAchievement`** ([src/dllmain.cpp:867](src/dllmain.cpp#L867) area, ending around line 932)

The existing detached-thread body builds `url = "https://api.guildwars2.com/v2/account/achievements?ids=" + ids_param`. Move that URL build inside a `fetch` lambda; keep the JSON-parse-and-fill logic inside a `complete` lambda. Pass `req->requester` as the requester and `account_name` (already computed in the existing handler) as the cache account. Use the synthetic endpoint string `"/v2/account/achievements?ids=" + ids_param` as the cache endpoint so repeat queries with identical id sets hit the cache.

Replace the existing `std::thread([...]() { ... }).detach();` block with:

```cpp
auto fetch = [url]() -> std::string {
    return HoardAndSeek::GW2API::AuthenticatedGet(url, /*account_name copy*/);
};
auto complete = [/* response_event, account_name, ids copy, etc. */](const std::string& body) {
    // existing JSON parse + response struct fill + Events_Raise, unchanged
};
auto sr = HoardAndSeek::ProxyThrottle::Submit(
    requester, account_name, cache_endpoint, std::move(fetch), std::move(complete));
if (!sr.accepted) {
    auto* resp = new HoardQueryAchievementResponse{};
    resp->api_version = HOARD_API_VERSION;
    resp->status = HOARD_STATUS_BUSY;
    strncpy(resp->account_name, account_name.c_str(), sizeof(resp->account_name) - 1);
    APIDefs->Events_Raise(response_event.c_str(), resp);
}
```

**Capture rule:** lambdas must capture every variable used in their body. Use `[=]` capture where possible; capture pointers (`APIDefs`) by value. Do NOT capture references to local strings — the handler returns before the worker runs.

- [ ] **Step 2: Migrate `OnQueryMastery`** ([src/dllmain.cpp:932](src/dllmain.cpp#L932) area)

Same pattern. Cache endpoint: `"/v2/account/masteries"` (no IDs — the API returns the full mastery list anyway).

- [ ] **Step 3: Migrate `OnQuerySkins`** ([src/dllmain.cpp:987](src/dllmain.cpp#L987) area)

Same pattern. Cache endpoint: `"/v2/account/skins"`.

- [ ] **Step 4: Migrate `OnQueryRecipes`** ([src/dllmain.cpp:1039](src/dllmain.cpp#L1039) area)

Same pattern. Cache endpoint: `"/v2/account/recipes"`.

- [ ] **Step 5: Migrate `OnQueryWizardsVault`** ([src/dllmain.cpp:1091](src/dllmain.cpp#L1091) area)

This one fetches three URLs sequentially. Submit it as a single job whose `fetch` lambda concatenates all three responses with a delimiter (e.g. `"\x1F"` unit-separator) and whose `complete` lambda splits + parses each part. Cache endpoint: `"/v2/account/wizardsvault/" + type_string`.

- [ ] **Step 6: Build**

```bash
cd build && make
```

Expected: clean build. If any handler still spawns `std::thread([...]()...).detach()` for a GW2 API call, you've missed a migration — search `src/dllmain.cpp` for `std::thread(` to confirm none remain (the search-thread spawn in `OnSearchRequest` is fine; it doesn't hit the network).

- [ ] **Step 7: Commit**

```bash
git add src/dllmain.cpp
git commit -m "Route remaining proxy handlers through ProxyThrottle"
```

---

## Task 9: Manual in-game verification

This is the only end-to-end verification available. Do all checks in one GW2 session.

- [ ] **Step 1: Copy the DLL into the GW2 Nexus addons folder and launch GW2**

Use whatever existing path / sync mechanism you currently use to ship test builds into the GW2 install.

- [ ] **Step 2: Verify normal H&S operation still works**

Open the H&S window. Trigger a full account-data refresh. Watch the progress dialog. The refresh should complete normally; if it feels notably slower than before, the bucket constants may be too tight — note it but do not change them now.

- [ ] **Step 3: Verify the cache short-circuits repeat queries**

If you have a consumer addon handy (e.g. one of your other GW2 addons that uses `EV_HOARD_QUERY_API`), trigger the same endpoint twice within 30 seconds. The second call should return effectively instantly with the same JSON. If you don't have a consumer, this step is best-effort — note it in the handover.

- [ ] **Step 4: Verify backpressure**

Optional but ideal: write a 20-line throwaway addon that fires `EV_HOARD_QUERY_API` in a tight loop with random endpoints (to defeat the cache) and observe that responses eventually arrive with `status == HOARD_STATUS_BUSY` and a non-zero `retry_after_ms`. If that's too much work, skip and rely on code review.

- [ ] **Step 5: Check the Nexus log**

After 5 minutes of normal use, check the Nexus log for any new errors or warnings tagged `HoardAndSeek`. Look in particular for the diagnostic lines:
- `Proxy effective rate now X.X req/sec` — emitted when the adaptive limiter changes rate
- `GW2 API returned 429 — backing off proxy rate for 60s` — emitted on a 429

Under normal operation neither should appear. If `429` appears repeatedly even at the baseline rate, lower `RATE_REFILL_PER_SEC_MAX` in `src/ProxyThrottle.h` and rebuild.

---

## Task 10: Handover update

**Note:** Do NOT bump the addon version. Per `CLAUDE.md`, version bumps are user-initiated only. Leave the version constant alone.

**Files:**
- Modify: `handover.md`

- [ ] **Step 1: Update `handover.md`**

In "Key decisions made", prepend a new entry:

```markdown
**Proxy rate limiting:** All outbound GW2 API calls (proxy and our own
fetches) share a single token bucket (5 req/sec sustained, 30 burst, well under the
~600/min GW2 ceiling). Proxy queries no longer spawn per-request threads — they go
through a single worker thread with a bounded queue (256), per-requester cap (32),
and round-robin fairness. A 30-second TTL response cache short-circuits repeat
queries. API bumped to v4 with `HOARD_STATUS_BUSY` and `retry_after_ms` /
`queue_depth` backpressure fields appended to responses; old v3 callers are
unaffected because new fields are only written when the request declares v4.
```

In "What's left / possible next steps", add a new note:

```markdown
- If GW2 API rate-limit ceilings change upstream, the tunables in
  `src/ProxyThrottle.h` (`RATE_BUCKET_CAPACITY`, `RATE_REFILL_PER_SEC`,
  `CACHE_TTL_SECONDS`) are the single point of adjustment.
- Version bump to ship these changes is pending user approval.
```

- [ ] **Step 2: Final build**

```bash
cd build && make
```

Expected: clean build, `HoardAndSeek.dll` produced.

- [ ] **Step 3: Commit (do NOT push or release without explicit user approval — per CLAUDE.md)**

```bash
git add handover.md
git commit -m "Proxy rate limiting, response cache, API v4"
```

- [ ] **Step 4: Stop**

Do not push, tag, bump the version, or create a GitHub release. Report completion to the user and wait for instructions.

---

## Self-Review Notes

- **Spec coverage:** All four concerns from the discussion are addressed — (1) serialised worker queue in Task 5/7/8, (2) TTL cache in Task 4/7, (3) per-requester fairness in Task 5, (4) API v4 backpressure in Task 6/7/8.
- **Shared bucket:** Task 3 wires H&S's own `HttpGet`/`HttpGetEx` through the limiter, so own refreshes can't be starved by — or starve — proxy traffic. This was an addition beyond the four discussion points; called out because without it, the rate-limit ceiling is still reachable.
- **ABI compatibility:** New fields are appended to response structs and only written when `request->api_version >= 4`. v3 callers continue to work — they allocate the old (smaller) struct size, and H&S never writes past it because the v4 branch is gated on the request.
- **Sync vs async contract:** Cache hits remain synchronous (response delivered inside `Events_Raise`); cache misses are async. Header docs updated in Task 6 Step 4 to reflect this.
- **Capture safety:** Called out explicitly in Task 7 Step 2 — the worker-thread lambdas must capture by value, not reference, because the handler returns before the worker runs. This is the most likely source of crashes if implemented incorrectly.
- **Max throughput design:** Baseline is 8 req/sec sustained / 60 burst (60% above alter_ego's conservative 5/sec). The limiter does not statically guess GW2's ceiling — it pushes the baseline and pulls back automatically on HTTP 429 via `NotifyRateLimited` (Task 2 Step 2), wired into both `HttpGet` and `HttpGetEx` (Task 3 Steps 2-3). Effective rate is logged on change so the user can see in-game whether the limiter is actually being squeezed.

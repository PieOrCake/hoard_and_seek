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

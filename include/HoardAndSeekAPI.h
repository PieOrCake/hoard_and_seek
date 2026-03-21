/*
 * HoardAndSeekAPI.h - Cross-addon event interface for Hoard & Seek
 *
 * Include this header in your addon to communicate with Hoard & Seek
 * via Nexus events. No link-time dependency is required.
 *
 * Version: 1
 */

#pragma once

#include <cstdint>

#define HOARD_API_VERSION 1

// ============================================================================
// Event Names
// ============================================================================

// Broadcasts (raised by Hoard & Seek) -----------------------------------------

// Raised when account data has finished loading (startup or refresh).
// Payload: HoardDataReadyPayload*
#define EV_HOARD_DATA_UPDATED  "EV_HOARD_DATA_UPDATED"

// Raised periodically during an account data fetch with progress info.
// Payload: HoardFetchProgressPayload*
#define EV_HOARD_FETCH_PROGRESS "EV_HOARD_FETCH_PROGRESS"

// Raised by H&S in response to EV_HOARD_PING.
// Payload: nullptr
#define EV_HOARD_PONG           "EV_HOARD_PONG"

// Requests (subscribe in your addon, raised by your addon) --------------------

// Ping H&S to check if it's loaded. H&S responds with EV_HOARD_PONG.
// Payload: nullptr
#define EV_HOARD_PING          "EV_HOARD_PING"

// Trigger an account data refresh in H&S (same as pressing Refresh Account Data).
// Payload: nullptr
#define EV_HOARD_REFRESH       "EV_HOARD_REFRESH"

// Open H&S window and search for an item by name.
// Payload: const char* (null-terminated item name)
#define EV_HOARD_SEARCH        "EV_HOARD_SEARCH"

// Query total count + locations for a specific item ID.
// Payload: HoardQueryItemRequest*
// Response: Hoard & Seek raises the event named in `response_event`,
//           with payload HoardQueryItemResponse* (caller must free).
#define EV_HOARD_QUERY_ITEM    "EV_HOARD_QUERY_ITEM"

// Query wallet currency balance.
// Payload: HoardQueryWalletRequest*
// Response: Hoard & Seek raises the event named in `response_event`,
//           with payload HoardQueryWalletResponse* (caller must free).
#define EV_HOARD_QUERY_WALLET  "EV_HOARD_QUERY_WALLET"

// Query account achievement progress (batch, up to 200 IDs).
// Payload: HoardQueryAchievementRequest*
// Response: Hoard & Seek raises the event named in `response_event`,
//           with payload HoardQueryAchievementResponse* (caller must free).
#define EV_HOARD_QUERY_ACHIEVEMENT "EV_HOARD_QUERY_ACHIEVEMENT"

// Query account mastery progress (batch, up to 200 IDs).
// Payload: HoardQueryMasteryRequest*
// Response: Hoard & Seek raises the event named in `response_event`,
//           with payload HoardQueryMasteryResponse* (caller must free).
#define EV_HOARD_QUERY_MASTERY "EV_HOARD_QUERY_MASTERY"

// ============================================================================
// Payload Structures
// ============================================================================

#pragma pack(push, 1)

// Broadcast: account data is ready
struct HoardDataReadyPayload {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t item_count;        // Number of unique items tracked
    uint32_t currency_count;    // Number of wallet currencies tracked
};

// Broadcast: fetch progress update
struct HoardFetchProgressPayload {
    uint32_t api_version;       // HOARD_API_VERSION
    char message[128];          // e.g. "Fetching bank...", "Fetching inventory: Character..."
    float progress;             // 0.0-1.0 estimated progress, or -1.0 if indeterminate
};

// Request: query a single item
struct HoardQueryItemRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t item_id;           // GW2 item ID
    char response_event[64];    // Event name H&S will raise with the response
};

// A single location entry in the response
struct HoardItemLocationEntry {
    char location[64];          // e.g. "Bank", "Material Storage", character name
    char sublocation[64];       // e.g. "Bag", "Equipped", "Category 37"
    int32_t count;
};

// Response: item query result
struct HoardQueryItemResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t item_id;
    char name[128];
    char rarity[32];
    char type[32];
    int32_t total_count;
    uint32_t location_count;
    HoardItemLocationEntry locations[32]; // Up to 32 locations
};

// Request: query wallet currency
struct HoardQueryWalletRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t currency_id;       // GW2 currency ID (NOT the synthetic WALLET_ID_BASE | id)
    char response_event[64];    // Event name H&S will raise with the response
};

// Response: wallet query result
struct HoardQueryWalletResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t currency_id;
    char name[128];
    int32_t amount;
    bool found;
};

// Request: query account achievements (batch)
struct HoardQueryAchievementRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t ids[200];          // Achievement IDs to query
    uint32_t id_count;          // Number of IDs (1-200)
    char response_event[64];    // Event name H&S will raise with the response
};

// A single achievement entry in the response
struct HoardAchievementEntry {
    uint32_t id;
    int32_t current;            // Current progress (-1 if not started)
    int32_t max;                // Max progress (-1 if unknown)
    bool done;                  // Whether the achievement is completed
    uint32_t bits[64];          // Completed bit indices
    uint32_t bit_count;         // Number of completed bits
};

// Response: achievement query result
struct HoardQueryAchievementResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t entry_count;       // Number of entries returned
    HoardAchievementEntry entries[200];
};

// Request: query account masteries (batch)
struct HoardQueryMasteryRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t ids[200];          // Mastery IDs to query
    uint32_t id_count;          // Number of IDs (1-200)
    char response_event[64];    // Event name H&S will raise with the response
};

// A single mastery entry in the response
struct HoardMasteryEntry {
    uint32_t id;
    int32_t level;              // Current mastery level (0 if not started)
};

// Response: mastery query result
struct HoardQueryMasteryResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t entry_count;       // Number of entries returned
    HoardMasteryEntry entries[200];
};

#pragma pack(pop)

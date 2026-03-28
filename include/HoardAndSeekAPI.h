/*
 * HoardAndSeekAPI.h - Cross-addon event interface for Hoard & Seek
 *
 * Include this header in your addon to communicate with Hoard & Seek
 * via Nexus events. No link-time dependency is required.
 *
 * Version: 2
 */

#pragma once

#include <cstdint>

#define HOARD_API_VERSION 2
#define HOARD_REFRESH_COOLDOWN 300  // Minimum seconds between refreshes (5 minutes)

// Response status codes
#define HOARD_STATUS_OK       0  // Request succeeded
#define HOARD_STATUS_DENIED   1  // Permission denied by user
#define HOARD_STATUS_PENDING  2  // Permission not yet decided (popup shown)

// ============================================================================
// IMPORTANT: Nexus Event Dispatch Behavior
// ============================================================================
//
// Nexus Events_Raise dispatches SYNCHRONOUSLY. All subscribers are called
// inline on the calling thread before Events_Raise returns. This includes
// YOUR OWN response handler if H&S raises the response event during processing.
//
// WARNING: DO NOT hold any mutex or non-recursive lock when calling
// Events_Raise for request events. If your response handler acquires a lock,
// and you hold that same lock when raising the request, you WILL deadlock.
//
// Recommended pattern:
//   1. Lock mutex, copy out the data you need, unlock mutex
//   2. Call Events_Raise with no lock held
//   3. Your response handler can safely acquire the lock
//
// REQUEST STRUCTS: Use stack-allocated request structs. H&S reads the request
// synchronously before Events_Raise returns, so the struct can safely go out
// of scope afterward. Heap allocation is unnecessary.
//
//   HoardQuerySkinsRequest req{};
//   req.api_version = HOARD_API_VERSION;
//   strncpy(req.requester, "MyAddon", sizeof(req.requester));
//   // ... fill fields ...
//   api->Events_Raise(EV_HOARD_QUERY_SKINS, &req);
//   // req goes out of scope here — safe, H&S has already read it
//
// RESPONSE OWNERSHIP: H&S heap-allocates response payloads and raises your
// response event synchronously. Your handler is called during the original
// Events_Raise call. You MUST delete the response inside your handler.
// Do NOT attempt to free anything after Events_Raise returns — the response
// has already been delivered and freed by that point.
//
// BATCHING & RECURSION: If you send batched requests (e.g. querying skins
// 200 at a time) and your response handler calls Events_Raise to send the
// next batch, this creates recursive dispatch. This is safe as long as no
// locks are held, but be aware of stack depth (e.g. 10000 skins / 200 per
// batch = 50 levels of recursion). Consider queuing the next batch and
// processing it on the next frame tick instead.
//
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

// Raised when an account data fetch fails (API offline, network error, etc.).
// Payload: HoardFetchErrorPayload*
#define EV_HOARD_FETCH_ERROR   "EV_HOARD_FETCH_ERROR"

// Raised by H&S in response to EV_HOARD_PING.
// Payload: HoardPongPayload*
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

// *** See "Nexus Event Dispatch Behavior" above before using request events. ***
// *** Do NOT hold locks when calling Events_Raise. Delete responses in your handler. ***

// Query total count + locations for a specific item ID.
// Payload: HoardQueryItemRequest* (stack-allocated)
// Response: H&S raises `response_event` with HoardQueryItemResponse* (delete in handler).
#define EV_HOARD_QUERY_ITEM    "EV_HOARD_QUERY_ITEM"

// Query wallet currency balance.
// Payload: HoardQueryWalletRequest* (stack-allocated)
// Response: H&S raises `response_event` with HoardQueryWalletResponse* (delete in handler).
#define EV_HOARD_QUERY_WALLET  "EV_HOARD_QUERY_WALLET"

// Query account achievement progress (batch, up to 200 IDs).
// Payload: HoardQueryAchievementRequest* (stack-allocated)
// Response: H&S raises `response_event` with HoardQueryAchievementResponse* (delete in handler).
#define EV_HOARD_QUERY_ACHIEVEMENT "EV_HOARD_QUERY_ACHIEVEMENT"

// Query account mastery progress (batch, up to 200 IDs).
// Payload: HoardQueryMasteryRequest* (stack-allocated)
// Response: H&S raises `response_event` with HoardQueryMasteryResponse* (delete in handler).
#define EV_HOARD_QUERY_MASTERY "EV_HOARD_QUERY_MASTERY"

// Query account skin unlocks (batch, up to 200 IDs).
// Payload: HoardQuerySkinsRequest* (stack-allocated)
// Response: H&S raises `response_event` with HoardQuerySkinsResponse* (delete in handler).
#define EV_HOARD_QUERY_SKINS "EV_HOARD_QUERY_SKINS"

// Query account recipe unlocks (batch, up to 200 IDs).
// Payload: HoardQueryRecipesRequest* (stack-allocated)
// Response: H&S raises `response_event` with HoardQueryRecipesResponse* (delete in handler).
#define EV_HOARD_QUERY_RECIPES "EV_HOARD_QUERY_RECIPES"

// Query Wizard's Vault progress (daily, weekly, or special).
// Payload: HoardQueryWizardsVaultRequest* (stack-allocated)
// Response: H&S raises `response_event` with HoardQueryWizardsVaultResponse* (delete in handler).
#define EV_HOARD_QUERY_WIZARDSVAULT "EV_HOARD_QUERY_WIZARDSVAULT"

// Generic authenticated API proxy query.
// Makes any GW2 API endpoint available via H&S's stored API key.
// Payload: HoardQueryApiRequest* (stack-allocated)
// Response: H&S raises `response_event` with HoardQueryApiResponse* (delete in handler).
#define EV_HOARD_QUERY_API "EV_HOARD_QUERY_API"

// Register a custom right-click context menu item on H&S search results.
// Payload: HoardContextMenuRegister* (stack-allocated)
// Requires permission approval. If pending, the registration is queued and
// applied automatically once the user approves.
#define EV_HOARD_CONTEXT_MENU_REGISTER "EV_HOARD_CONTEXT_MENU_REGISTER"

// Remove a previously registered context menu item.
// Payload: HoardContextMenuRemove*
#define EV_HOARD_CONTEXT_MENU_REMOVE "EV_HOARD_CONTEXT_MENU_REMOVE"

// Context menu item type flags (which result types show this menu item)
#define HOARD_MENU_ITEMS   1  // Show for regular items
#define HOARD_MENU_WALLET  2  // Show for wallet currencies
#define HOARD_MENU_ALL     3  // Show for both

// ============================================================================
// Payload Structures
// ============================================================================

#pragma pack(push, 1)

// Broadcast: account data is ready
struct HoardDataReadyPayload {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t item_count;        // Number of unique items tracked
    uint32_t currency_count;    // Number of wallet currencies tracked
    int64_t  last_updated;      // Unix timestamp of last successful fetch (0 if never)
    int64_t  refresh_available_at; // Unix timestamp when next refresh is allowed
};

// Broadcast: pong response
struct HoardPongPayload {
    uint32_t api_version;          // HOARD_API_VERSION
    int64_t  last_updated;         // Unix timestamp of last successful fetch (0 if never)
    int64_t  refresh_available_at; // Unix timestamp when next refresh is allowed (0 = now)
    uint8_t  has_data;             // 1 if account data is loaded, 0 otherwise
};

// Broadcast: fetch error
struct HoardFetchErrorPayload {
    uint32_t api_version;       // HOARD_API_VERSION
    char message[256];          // Human-readable error message
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
    char requester[64];         // Addon name (used for permission checks)
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
    uint8_t status;             // HOARD_STATUS_OK, HOARD_STATUS_DENIED, or HOARD_STATUS_PENDING
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
    char requester[64];         // Addon name (used for permission checks)
    uint32_t currency_id;       // GW2 currency ID (NOT the synthetic WALLET_ID_BASE | id)
    char response_event[64];    // Event name H&S will raise with the response
};

// Response: wallet query result
struct HoardQueryWalletResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint8_t status;             // HOARD_STATUS_OK, HOARD_STATUS_DENIED, or HOARD_STATUS_PENDING
    uint32_t currency_id;
    char name[128];
    int32_t amount;
    bool found;
};

// Request: query account achievements (batch)
struct HoardQueryAchievementRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    char requester[64];         // Addon name (used for permission checks)
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
    uint8_t status;             // HOARD_STATUS_OK, HOARD_STATUS_DENIED, or HOARD_STATUS_PENDING
    uint32_t entry_count;       // Number of entries returned
    HoardAchievementEntry entries[200];
};

// Request: query account masteries (batch)
struct HoardQueryMasteryRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    char requester[64];         // Addon name (used for permission checks)
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
    uint8_t status;             // HOARD_STATUS_OK, HOARD_STATUS_DENIED, or HOARD_STATUS_PENDING
    uint32_t entry_count;       // Number of entries returned
    HoardMasteryEntry entries[200];
};

// Request: query account skin unlocks (batch)
struct HoardQuerySkinsRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    char requester[64];         // Addon name (used for permission checks)
    uint32_t ids[200];          // Skin IDs to check
    uint32_t id_count;          // Number of IDs (1-200)
    char response_event[64];    // Event name H&S will raise with the response
};

// A single skin entry in the response
struct HoardSkinEntry {
    uint32_t id;
    uint8_t unlocked;           // 1 if unlocked, 0 if not
};

// Response: skin unlock query result
struct HoardQuerySkinsResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint8_t status;             // HOARD_STATUS_OK, HOARD_STATUS_DENIED, or HOARD_STATUS_PENDING
    uint32_t entry_count;       // Number of entries returned
    HoardSkinEntry entries[200];
};

// Request: query account recipe unlocks (batch)
struct HoardQueryRecipesRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    char requester[64];         // Addon name (used for permission checks)
    uint32_t ids[200];          // Recipe IDs to check
    uint32_t id_count;          // Number of IDs (1-200)
    char response_event[64];    // Event name H&S will raise with the response
};

// A single recipe entry in the response
struct HoardRecipeEntry {
    uint32_t id;
    uint8_t unlocked;           // 1 if unlocked, 0 if not
};

// Response: recipe unlock query result
struct HoardQueryRecipesResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint8_t status;             // HOARD_STATUS_OK, HOARD_STATUS_DENIED, or HOARD_STATUS_PENDING
    uint32_t entry_count;       // Number of entries returned
    HoardRecipeEntry entries[200];
};

// Request: query Wizard's Vault progress
struct HoardQueryWizardsVaultRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    char requester[64];         // Addon name (used for permission checks)
    uint8_t type;               // 0 = daily, 1 = weekly, 2 = special
    char response_event[64];    // Event name H&S will raise with the response
};

// A single Wizard's Vault objective
struct HoardWizardsVaultObjective {
    uint32_t id;
    char title[128];
    char track[32];             // e.g. "PvE", "PvP", "WvW"
    int32_t acclaim;            // Astral Acclaim reward
    int32_t progress_current;
    int32_t progress_complete;
    uint8_t claimed;            // 1 if reward claimed, 0 if not
};

// Response: Wizard's Vault query result
struct HoardQueryWizardsVaultResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint8_t status;             // HOARD_STATUS_OK, HOARD_STATUS_DENIED, or HOARD_STATUS_PENDING
    uint8_t type;               // 0 = daily, 1 = weekly, 2 = special
    int32_t meta_progress_current;
    int32_t meta_progress_complete;
    int32_t meta_reward_astral; // Astral Acclaim from meta reward
    uint8_t meta_reward_claimed;
    uint32_t objective_count;   // Number of objectives returned
    HoardWizardsVaultObjective objectives[16]; // Up to 16 objectives
};

// Request: generic authenticated API proxy query
struct HoardQueryApiRequest {
    uint32_t api_version;       // HOARD_API_VERSION
    char requester[64];         // Addon name (used for permission checks)
    char endpoint[256];         // GW2 API path, e.g. "/v2/account/dyes"
    char response_event[64];    // Event name H&S will raise with the response
};

// Response: generic API proxy result (raw JSON)
struct HoardQueryApiResponse {
    uint32_t api_version;       // HOARD_API_VERSION
    uint8_t status;             // HOARD_STATUS_OK, HOARD_STATUS_DENIED, or HOARD_STATUS_PENDING
    char endpoint[256];         // Echo of the requested endpoint
    uint32_t json_length;       // Actual length of the JSON data (may exceed buffer if truncated)
    uint8_t truncated;          // 1 if response was truncated to fit buffer, 0 otherwise
    char json[32768];           // Raw JSON response (up to 32KB, null-terminated)
};

// Register a custom right-click context menu item
struct HoardContextMenuRegister {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t signature;         // Addon's Nexus signature (for auto-cleanup on unload)
    char id[64];                // Unique ID for this menu entry (for later removal)
    char requester[64];         // Addon name
    char label[64];             // Display text in the context menu (e.g. "Add to Watched Items")
    char callback_event[64];    // Event name H&S raises when clicked (payload: HoardContextMenuCallback*)
    uint8_t item_types;         // Bitmask: HOARD_MENU_ITEMS, HOARD_MENU_WALLET, or HOARD_MENU_ALL
};

// Remove a registered context menu item
struct HoardContextMenuRemove {
    uint32_t api_version;       // HOARD_API_VERSION
    char id[64];                // ID of the menu entry to remove (empty = remove all from this requester)
    char requester[64];         // Addon name
};

// Callback payload sent when user clicks a registered context menu item
struct HoardContextMenuCallback {
    uint32_t api_version;       // HOARD_API_VERSION
    uint32_t item_id;           // GW2 item ID (or synthetic wallet ID if WALLET_ID_BASE | currency_id)
    char name[128];             // Item or currency name
    char rarity[32];            // Item rarity (e.g. "Rare", "Exotic") or "Currency"
    char type[32];              // Item type (e.g. "Weapon", "Armor") or empty for currencies
    int32_t total_count;        // Total count across all locations
};

#pragma pack(pop)

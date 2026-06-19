# Hoard & Seek — Cross-Addon API Reference

Hoard & Seek exposes a Nexus event-based API so other addons can query account data without their own GW2 API integration. No link-time dependency is required — just include `HoardAndSeekAPI.h` in your addon.

## Setup

Copy `include/HoardAndSeekAPI.h` into your addon's include path. All communication uses Nexus `Events_Raise` / `Events_Subscribe`.

## Events

### Broadcasts (raised by Hoard & Seek)

| Event | Payload | Description |
|---|---|---|
| `EV_HOARD_DATA_UPDATED` | `HoardDataReadyPayload*` | Raised when account data finishes loading (startup cache or refresh). Includes `account_name` (which account updated) and `account_count`. |
| `EV_HOARD_ACCOUNTS_CHANGED` | `nullptr` | Raised when accounts are added or removed. Re-query `EV_HOARD_QUERY_ACCOUNTS` for the new list. |
| `EV_HOARD_FETCH_PROGRESS` | `HoardFetchProgressPayload*` | Raised during an account data fetch with a status message (e.g. "Fetching bank...", "Fetching inventory: Character...") |
| `EV_HOARD_FETCH_ERROR` | `HoardFetchErrorPayload*` | Raised when a fetch fails (API offline, network error, invalid key, etc.). Includes `account_name` (which account errored). |
| `EV_HOARD_PONG` | `HoardPongPayload*` | Raised in response to `EV_HOARD_PING` — confirms H&S is loaded, includes `last_updated`, `refresh_available_at`, `has_data`, and `account_count` |

Subscribe to `EV_HOARD_DATA_UPDATED` to know when H&S has data available for queries — this fires both when cached data loads from disk at startup and after a fresh API fetch. Subscribe to `EV_HOARD_FETCH_PROGRESS` to show live progress in your addon's UI. Subscribe to `EV_HOARD_FETCH_ERROR` to detect and display fetch failures (e.g. GW2 API maintenance).

**Startup timing:** H&S may load after your addon. If you send `EV_HOARD_PING` during `AddonLoad` and H&S hasn't loaded yet, you'll get no response. Retry pings periodically (every 2–3 seconds) until a pong is received.

### Requests (raised by your addon)

| Event | Request Payload | Response Payload | Description |
|---|---|---|---|
| `EV_HOARD_PING` | `nullptr` | `EV_HOARD_PONG` | Checks if H&S is loaded. If it is, H&S immediately responds with `EV_HOARD_PONG`. |
| `EV_HOARD_REFRESH` | `nullptr` | *(none — triggers refresh)* | Triggers an account data refresh in H&S (same as pressing the button). H&S broadcasts `EV_HOARD_DATA_UPDATED` on completion. |
| `EV_HOARD_SEARCH` | `const char*` | *(none — opens H&S UI)* | Opens the H&S window and searches for the given item name |
| `EV_HOARD_QUERY_ITEM` | `HoardQueryItemRequest*` | `HoardQueryItemResponse*` | Returns total count and up to 64 locations for a specific item ID. Supports `account_filter` to query a single account. Each location includes `account_name`. |
| `EV_HOARD_QUERY_WALLET` | `HoardQueryWalletRequest*` | `HoardQueryWalletResponse*` | Returns wallet currency balance for a specific currency ID. Supports `account_filter`. |
| `EV_HOARD_QUERY_ACHIEVEMENT` | `HoardQueryAchievementRequest*` | `HoardQueryAchievementResponse*` | Proxy query: fetches account achievement progress from the GW2 API (batch, up to 200 IDs). Supports `account_name`. |
| `EV_HOARD_QUERY_MASTERY` | `HoardQueryMasteryRequest*` | `HoardQueryMasteryResponse*` | Proxy query: fetches account mastery levels from the GW2 API (batch, up to 200 IDs). Supports `account_name`. |
| `EV_HOARD_QUERY_SKINS` | `HoardQuerySkinsRequest*` | `HoardQuerySkinsResponse*` | Proxy query: checks if specific skin IDs are unlocked (batch, up to 200 IDs). Supports `account_name`. |
| `EV_HOARD_QUERY_RECIPES` | `HoardQueryRecipesRequest*` | `HoardQueryRecipesResponse*` | Proxy query: checks if specific recipe IDs are unlocked (batch, up to 200 IDs). Supports `account_name`. |
| `EV_HOARD_QUERY_WIZARDSVAULT` | `HoardQueryWizardsVaultRequest*` | `HoardQueryWizardsVaultResponse*` | Proxy query: fetches Wizard's Vault objectives and progress (type: 0=daily, 1=weekly, 2=special). Supports `account_name`. |
| `EV_HOARD_QUERY_API` | `HoardQueryApiRequest*` | `HoardQueryApiResponse*` | Generic proxy: forwards any GW2 API endpoint using H&S's stored API key, returns raw JSON. Supports `account_name`. |
| `EV_HOARD_QUERY_ACCOUNTS` | `HoardQueryAccountsRequest*` | `HoardQueryAccountsResponse*` | Returns the list of configured accounts with names, labels, validation status, and last updated timestamps |
| `EV_HOARD_CONTEXT_MENU_REGISTER` | `HoardContextMenuRegister*` | *(none)* | Registers a custom right-click context menu item on H&S search results |
| `EV_HOARD_CONTEXT_MENU_REMOVE` | `HoardContextMenuRemove*` | *(none)* | Removes a previously registered context menu item |

## Request / Response Pattern

For query events, you provide a `response_event` name in the request struct. H&S processes the query, then raises that named event with a heap-allocated response payload. **You must `delete` the response inside your handler** (see threading notes below).

`EV_HOARD_QUERY_ITEM` and `EV_HOARD_QUERY_WALLET` return cached data and respond synchronously (inline). `EV_HOARD_QUERY_ACHIEVEMENT`, `EV_HOARD_QUERY_MASTERY`, `EV_HOARD_QUERY_SKINS`, `EV_HOARD_QUERY_RECIPES`, `EV_HOARD_QUERY_WIZARDSVAULT`, and `EV_HOARD_QUERY_API` are **proxy queries** that make a live API call using H&S's stored API key, so the response arrives asynchronously on a background thread.

## ⚠️ Critical: Synchronous Event Dispatch

**Nexus `Events_Raise` dispatches synchronously.** All subscribers are called inline on the calling thread before `Events_Raise` returns. This has several important consequences:

**Deadlock risk:** If H&S processes your request and raises the response event immediately (as it does for cached queries like `EV_HOARD_QUERY_ITEM` and `EV_HOARD_QUERY_WALLET`, or for `HOARD_STATUS_DENIED` responses), your response handler runs *during* your original `Events_Raise` call. If you hold a `std::mutex` when calling `Events_Raise`, and your response handler tries to acquire the same mutex, you will **deadlock**.

```cpp
// BAD — will deadlock if H&S responds synchronously
std::lock_guard<std::mutex> lock(myMutex);
APIDefs->Events_Raise(EV_HOARD_QUERY_ITEM, &req);  // Response handler also locks myMutex → deadlock

// GOOD — release lock before raising
{
    std::lock_guard<std::mutex> lock(myMutex);
    // copy data you need from shared state
}
APIDefs->Events_Raise(EV_HOARD_QUERY_ITEM, &req);  // Response handler can safely lock myMutex
```

**Stack-allocated requests:** Use stack-allocated request structs. H&S reads the request synchronously before `Events_Raise` returns, so the struct can safely go out of scope afterward. Heap allocation is unnecessary.

**Response ownership:** H&S heap-allocates the response and raises your response event. Your handler **must** `delete` the response. Do not attempt to free anything after `Events_Raise` returns — for synchronous responses, the handler has already run and freed it by then.

**Batching & recursion:** If you send batched requests (e.g. querying skins 200 at a time) and your response handler calls `Events_Raise` to send the next batch, this creates a recursive call chain. This is safe as long as no locks are held, but be aware of stack depth (e.g. 10,000 skins / 200 per batch = 50 levels of recursion). Consider queuing the next batch and processing it on the next frame tick instead.

`HoardDataReadyPayload` includes a `last_updated` field (Unix timestamp) indicating when the account data was last successfully fetched, and a `refresh_available_at` field indicating when the next refresh is allowed (0 if available now). H&S enforces a 5-minute cooldown (`HOARD_REFRESH_COOLDOWN`) between refreshes since the GW2 API does not update instantly. `EV_HOARD_REFRESH` requests during cooldown are silently ignored.

## Addon Permissions

All query request structs include a `requester` field (64-char addon name). Access is **default-allow** — no permission request or approval step is required:

- **First request from a new addon:** succeeds immediately (`status = HOARD_STATUS_OK`). No popup is shown and no prior approval is needed. The addon is recorded so it appears in the H&S settings panel.
- **Denied:** if the user has explicitly unticked an addon in settings, its requests return `status = HOARD_STATUS_DENIED`.
- **Manage permissions:** The user can view and deny (or re-allow) addons in the H&S settings panel under "Addon Permissions".

All response structs include a `status` field. Check it before using the response data:

| Status | Value | Meaning |
|---|---|---|
| `HOARD_STATUS_OK` | 0 | Request succeeded, response data is valid |
| `HOARD_STATUS_DENIED` | 1 | User has explicitly denied this addon in settings |
| `HOARD_STATUS_PENDING` | 2 | Deprecated — no longer returned (kept for ABI compatibility) |
| `HOARD_STATUS_BUSY` | 3 | Proxy queue full or per-addon limit reached; retry after `retry_after_ms` |

Denials are persisted to `permissions.json` in the H&S data directory.

**Important:** The `requester` field must be non-empty. Requests with an empty requester receive `HOARD_STATUS_DENIED`.

> **Note on `HOARD_STATUS_PENDING`:** Earlier versions showed an approval popup and returned `HOARD_STATUS_PENDING` until the user decided. That step has been removed; the status code remains defined only so existing callers that branch on it still compile and run. It is never returned anymore.

> **Security note:** Addon permissions are a **transparency and consent mechanism**, not a security sandbox. All Nexus addons are DLLs loaded into the same GW2 process and share the same address space and filesystem access. A malicious addon could read H&S's data files directly or access in-process memory regardless. The real security boundary is which addons you choose to install — only use addons from trusted sources.

## Examples

### Query Item Count

```cpp
#include "HoardAndSeekAPI.h"

// 1. Subscribe to the response event
APIDefs->Events_Subscribe("MY_ADDON_ITEM_RESPONSE", [](void* data) {
    auto* resp = (HoardQueryItemResponse*)data;
    if (resp->status != HOARD_STATUS_OK) {
        // HOARD_STATUS_DENIED = user has denied this addon in H&S settings
        delete resp;
        return;
    }
    // Use resp->total_count, resp->name, resp->locations, etc.
    delete resp; // Caller frees
});

// 2. Send the query
HoardQueryItemRequest req{};
req.api_version = HOARD_API_VERSION;
strncpy(req.requester, "My Addon Name", sizeof(req.requester));
req.item_id = 19721; // Glob of Ectoplasm
strncpy(req.response_event, "MY_ADDON_ITEM_RESPONSE", sizeof(req.response_event));
APIDefs->Events_Raise(EV_HOARD_QUERY_ITEM, &req);
```

### Query Wallet Currency

```cpp
HoardQueryWalletRequest req{};
req.api_version = HOARD_API_VERSION;
strncpy(req.requester, "My Addon Name", sizeof(req.requester));
req.currency_id = 1; // Coin
strncpy(req.response_event, "MY_ADDON_WALLET_RESPONSE", sizeof(req.response_event));
APIDefs->Events_Raise(EV_HOARD_QUERY_WALLET, &req);
```

### Query Achievement Progress

```cpp
HoardQueryAchievementRequest req{};
req.api_version = HOARD_API_VERSION;
strncpy(req.requester, "My Addon Name", sizeof(req.requester));
req.ids[0] = 1;
req.ids[1] = 2;
req.id_count = 2;
strncpy(req.response_event, "MY_ADDON_ACH_RESPONSE", sizeof(req.response_event));
APIDefs->Events_Raise(EV_HOARD_QUERY_ACHIEVEMENT, &req);

// In your response handler:
// auto* resp = (HoardQueryAchievementResponse*)data;
// for (uint32_t i = 0; i < resp->entry_count; i++) {
//     resp->entries[i].id, .done, .current, .max, .bits, .bit_count
// }
// delete resp;
```

### Open H&S Search

```cpp
const char* search = "Obsidian";
APIDefs->Events_Raise("EV_HOARD_SEARCH", (void*)search);
```

### Generic API Proxy

The generic proxy (`EV_HOARD_QUERY_API`) makes any authenticated GW2 API endpoint available without needing your own API key. The response contains raw JSON that you parse yourself.

```cpp
// 1. Subscribe to the response event
APIDefs->Events_Subscribe("MY_ADDON_API_RESPONSE", [](void* data) {
    auto* resp = (HoardQueryApiResponse*)data;
    if (resp->status != HOARD_STATUS_OK) {
        delete resp;
        return;
    }
    // resp->json contains the raw JSON string
    // resp->json_length is the original length (check resp->truncated)
    auto j = nlohmann::json::parse(resp->json);
    // Process the data...
    delete resp;
});

// 2. Send the query
HoardQueryApiRequest req{};
req.api_version = HOARD_API_VERSION;
strncpy(req.requester, "My Addon Name", sizeof(req.requester));
strncpy(req.endpoint, "/v2/account/dyes", sizeof(req.endpoint));
strncpy(req.response_event, "MY_ADDON_API_RESPONSE", sizeof(req.response_event));
APIDefs->Events_Raise(EV_HOARD_QUERY_API, &req);
```

<details>
<summary><strong>Available endpoints</strong></summary>

Any authenticated GW2 API endpoint can be used. Common examples:

| Endpoint | Returns |
|---|---|
| `/v2/account` | Basic account info |
| `/v2/account/buildstorage` | Build storage templates |
| `/v2/account/dailycrafting` | Daily time-gated crafting (completed today) |
| `/v2/account/dungeons` | Dungeon paths completed today |
| `/v2/account/dyes` | Unlocked dye IDs |
| `/v2/account/emotes` | Unlocked emote commands |
| `/v2/account/finishers` | Unlocked finishers |
| `/v2/account/gliders` | Unlocked glider IDs |
| `/v2/account/home/cats` | Home instance cats |
| `/v2/account/home/nodes` | Home instance nodes |
| `/v2/account/homestead/decorations` | Homestead decorations |
| `/v2/account/homestead/glyphs` | Homestead glyphs |
| `/v2/account/jadebots` | Unlocked jade bot IDs |
| `/v2/account/luck` | Account luck value |
| `/v2/account/mailcarriers` | Unlocked mail carrier IDs |
| `/v2/account/mapchests` | Map chests opened today |
| `/v2/account/mastery/points` | Mastery point totals per region |
| `/v2/account/minis` | Unlocked mini IDs |
| `/v2/account/mounts/skins` | Unlocked mount skin IDs |
| `/v2/account/mounts/types` | Unlocked mount types |
| `/v2/account/novelties` | Unlocked novelty IDs |
| `/v2/account/outfits` | Unlocked outfit IDs |
| `/v2/account/progression` | Account progression flags |
| `/v2/account/pvp/heroes` | Unlocked PvP hero IDs |
| `/v2/account/raids` | Raid encounters cleared this week |
| `/v2/account/skiffs` | Unlocked skiff skin IDs |
| `/v2/account/titles` | Unlocked title IDs |
| `/v2/account/worldbosses` | World bosses killed today |
| `/v2/account/wizardsvault/listings` | Wizard's Vault shop listings |
| `/v2/account/wvw` | WvW account data |
| `/v2/commerce/delivery` | TP delivery box contents |
| `/v2/commerce/transactions/current/buys` | Current TP buy orders |
| `/v2/commerce/transactions/current/sells` | Current TP sell orders |
| `/v2/commerce/transactions/history/buys` | TP buy history |
| `/v2/commerce/transactions/history/sells` | TP sell history |
| `/v2/pvp/stats` | PvP win/loss statistics |
| `/v2/pvp/games` | Recent PvP match history |
| `/v2/pvp/standings` | PvP league standings |

Endpoints that return large responses (>64KB) will be truncated — check `resp->truncated`. For typed, pre-parsed responses, use the dedicated query events above (item, wallet, achievements, masteries, skins, recipes, Wizard's Vault).

</details>

### Context Menu Hooks

Other addons can register custom right-click context menu items on H&S search results. Registration succeeds immediately — no permission approval step is required (unless the user has explicitly denied the addon in settings). When the user clicks a registered menu item, H&S raises the addon's specified callback event with item details.

```cpp
// 1. Subscribe to the callback event
APIDefs->Events_Subscribe("TP_TRACKER_WATCH_ITEM", [](void* data) {
    auto* cb = (HoardContextMenuCallback*)data;
    // cb->item_id, cb->name, cb->rarity, cb->type, cb->total_count
    AddToWatchList(cb->item_id, cb->name);
    delete cb; // Caller frees
});

// 2. Register the context menu item (use your addon's Nexus signature for auto-cleanup)
HoardContextMenuRegister reg{};
reg.api_version = HOARD_API_VERSION;
reg.signature = 0xBEEF; // Your addon's Nexus signature
strncpy(reg.id, "watch_item", sizeof(reg.id));
strncpy(reg.requester, "TP Price Tracker", sizeof(reg.requester));
strncpy(reg.label, "Add to Watched Items", sizeof(reg.label));
strncpy(reg.callback_event, "TP_TRACKER_WATCH_ITEM", sizeof(reg.callback_event));
reg.item_types = HOARD_MENU_ITEMS; // Only show for items, not wallet currencies
APIDefs->Events_Raise(EV_HOARD_CONTEXT_MENU_REGISTER, &reg);

// 3. Remove on unload (optional — pass empty id to remove all from your addon)
HoardContextMenuRemove rem{};
rem.api_version = HOARD_API_VERSION;
strncpy(rem.requester, "TP Price Tracker", sizeof(rem.requester));
// rem.id left empty = remove all entries from this requester
APIDefs->Events_Raise(EV_HOARD_CONTEXT_MENU_REMOVE, &rem);
```

**`item_types` flags:**

| Flag | Value | Effect |
|---|---|---|
| `HOARD_MENU_ITEMS` | 1 | Show only for regular items |
| `HOARD_MENU_WALLET` | 2 | Show only for wallet currencies |
| `HOARD_MENU_ALL` | 3 | Show for both |

Registered menu items appear below H&S's built-in "Copy Chat Link" option, separated by a divider. Multiple addons can register items simultaneously. If `id` + `requester` match an existing registration, it is updated in place.

**Auto-cleanup:** H&S listens for Nexus `EV_ADDON_UNLOADED` events. When an addon is unloaded or uninstalled, all context menu items registered with that addon's `signature` are automatically removed. Manual removal via `EV_HOARD_CONTEXT_MENU_REMOVE` is still supported but optional.

**Permission note:** Registration is allowed by default and applied right away. It only fails if the user has explicitly denied this addon's context-menu permission in the H&S settings panel.

## Version Compatibility

All payloads include an `api_version` field set to `HOARD_API_VERSION` (currently **3**). H&S will ignore requests with a mismatched version. Always use the `HOARD_API_VERSION` constant from the header rather than hard-coding a number.

## Multi-Account Support (v3)

H&S v3 supports up to 16 configured accounts. Key points for consuming addons:

- **Account list:** Query `EV_HOARD_QUERY_ACCOUNTS` to get all configured accounts. Each `HoardAccountEntry` has `account_name` (e.g. "PieOrCake.7635"), `label` (user-assigned friendly name, e.g. "Main"), and a `characters` list. Display `label` when non-empty, fall back to `account_name`.
- **Character-to-account mapping:** `HoardAccountEntry` includes `character_count` and `characters[80][32]`. Use these to build a character→account lookup table, which lets you resolve the MumbleLink character name to an H&S account name. Character lists are auto-fetched on startup for validated accounts that don't have them yet. If `character_count` is 0, fall back to querying `/v2/characters` via `EV_HOARD_QUERY_API` for that account.
- **Item/wallet queries:** Use `account_filter` to restrict results to a single account, or leave empty for all accounts. Each `HoardItemLocationEntry` includes `account_name` identifying which account owns that location.
- **Proxy queries** (achievements, masteries, skins, recipes, Wizard's Vault): Use `account_name` to select which account's API key is used. Empty defaults to the first configured account.
- **Generic API proxy:** Use `account_name` in `HoardQueryApiRequest` to select the account.
- **Detecting the logged-in account:** H&S does not track which account is currently logged in. Consuming addons should subscribe to `EV_MUMBLE_IDENTITY_UPDATED` (use the `eventArgs` pointer directly as `Mumble::Identity*`, NOT `DL_MUMBLE_LINK_IDENTITY` which may contain garbage before MumbleLink is initialized). The identity provides the **character name** — map it to an account using the character list from `EV_HOARD_QUERY_ACCOUNTS`, then pass that account name in requests. Always validate character names (letters, spaces, hyphens, accented chars only) and check `UITick > 0` on `DL_MUMBLE_LINK` before trusting the data.
- **Account changes:** Subscribe to `EV_HOARD_ACCOUNTS_CHANGED` to detect when accounts are added or removed.
- **Single-account fast path:** `HoardPongPayload` and `HoardDataReadyPayload` include `account_count`. If `account_count == 1`, skip the multi-account flow entirely — leave `account_name`/`account_filter` empty on all requests. No MumbleLink detection or character mapping needed.

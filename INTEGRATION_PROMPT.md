# Hoard & Seek v3 API Integration Guide (LLM Prompt)

Use this prompt when updating a Nexus addon to integrate with **Hoard & Seek v3** multi-account API.

---

## Context

Hoard & Seek (H&S) is a Guild Wars 2 Nexus addon that fetches and caches account data (items, wallet, achievements, skins, recipes, masteries, Wizard's Vault). Other addons communicate with H&S via **Nexus events** — no link-time dependency, just `#include "HoardAndSeekAPI.h"`.

**API version 3** adds multi-account support. H&S can store data for up to 16 GW2 accounts. Each account has a `account_name` (e.g. "PieOrCake.7635") and an optional user-assigned `label` (e.g. "Main", "Alt").

---

## Key Concepts

### 1. Include the header
Copy `HoardAndSeekAPI.h` into your project. It defines all event names, payload structs, and constants. No linking required.

### 2. API version
Set `api_version = HOARD_API_VERSION` (currently 4) in every request struct. H&S rejects requests with `api_version > HOARD_API_VERSION` or `api_version == 0`.

### 3. Permissions (no approval step)
Access is default-allow: your addon's first query succeeds immediately — there is no permission popup or approval to wait for. Just set a non-empty `requester` name and check the status code:
- `HOARD_STATUS_OK (0)` — request succeeded
- `HOARD_STATUS_DENIED (1)` — user has explicitly denied your addon in H&S settings; stop retrying
- `HOARD_STATUS_PENDING (2)` — deprecated, never returned anymore (kept for compatibility)
- `HOARD_STATUS_BUSY (3)` — proxy queue full; retry after `retry_after_ms`

### 4. Threading model (CRITICAL)
`Events_Raise()` is **synchronous**. Your response handler runs inline, on the same thread, inside the `Events_Raise` call.

**Do NOT hold locks when calling `Events_Raise`:**
```cpp
// WRONG — deadlock if response handler also locks myMutex
std::lock_guard<std::mutex> lock(myMutex);
api->Events_Raise(EV_HOARD_QUERY_SKINS, &req);

// CORRECT — release lock before raising
{
    std::lock_guard<std::mutex> lock(myMutex);
    // ... fill req from shared state ...
}
api->Events_Raise(EV_HOARD_QUERY_SKINS, &req);
```

Request structs can be stack-allocated. Response payloads are valid only during your handler — copy what you need, then `delete resp`.

### 5. Detecting H&S availability and startup timing
Subscribe to `EV_HOARD_PONG`. Raise `EV_HOARD_PING` (payload: `nullptr`) to check. Response: `HoardPongPayload*` with:
- `api_version` — H&S API version
- `has_data` — 1 if account data is loaded
- `last_updated` — Unix timestamp of last fetch
- `refresh_available_at` — when next refresh is allowed
- `account_count` — number of configured accounts

**Startup timing:** H&S may load after your addon. If you ping during `AddonLoad` and H&S isn't loaded yet, you'll get no pong. Retry pings periodically (every 2–3 seconds) until a pong is received:

```cpp
// In your render loop or periodic tick:
if (!g_HoardReady && std::chrono::steady_clock::now() >= g_NextPingTime) {
    APIDefs->Events_Raise(EV_HOARD_PING, nullptr);
    g_NextPingTime = std::chrono::steady_clock::now() + std::chrono::seconds(2);
}
```

### 6. Knowing when data is ready
Subscribe to `EV_HOARD_DATA_UPDATED`. Payload: `HoardDataReadyPayload*` with:
- `item_count`, `currency_count` — how many items/currencies are tracked
- `last_updated`, `refresh_available_at` — timing info
- `account_name[64]` — which account was just updated (empty = all accounts)
- `account_count` — total number of configured accounts

H&S fires this event both when cached data finishes loading from disk at startup **and** after a fresh API fetch completes. If your addon loads before H&S, you will receive this broadcast once H&S loads its cache.

### 7. Account list and friendly names
Query `EV_HOARD_QUERY_ACCOUNTS` (payload: `HoardQueryAccountsRequest*`) to get the list of configured accounts. The response (`HoardQueryAccountsResponse*`) contains up to 16 `HoardAccountEntry` structs, each with:
- `account_name[64]` — GW2 account name (e.g. "PieOrCake.7635")
- `label[64]` — user-assigned friendly name (e.g. "Main", "Alt"). **Use this for display in UI** when available; fall back to `account_name` if `label` is empty.
- `last_updated` — Unix timestamp of last data fetch
- `validated` — 1 if API key is validated
- `character_count` — number of characters on this account
- `characters[80][32]` — up to 80 character names (GW2 max is 72, names up to 31 chars)

Store both `account_name` (for lookups/filtering) and `label` (for display) when building your account list.

Additionally, each `HoardItemLocationEntry` in `HoardQueryItemResponse` now includes `account_name[64]`. To resolve the friendly name for a location's account, match `location.account_name` against the `account_name` field from your cached account list and display the corresponding `label`.

Subscribe to `EV_HOARD_ACCOUNTS_CHANGED` (payload: `nullptr`) to detect when accounts are added or removed. When received, re-query `EV_HOARD_QUERY_ACCOUNTS` to refresh your cached account list and labels.

### 8. Querying items (multi-account)
`HoardQueryItemRequest` includes:
- `account_filter[64]` — set to an `account_name` to filter results to one account, or leave empty for all accounts

`HoardQueryItemResponse` includes:
- `locations[64]` — array of `HoardItemLocationEntry`, each with `location`, `sublocation`, `count`, and `account_name`
- `location_count` — number of valid entries
- `total_count` — sum of all location counts

### 9. Querying wallet (multi-account)
`HoardQueryWalletRequest` includes:
- `account_filter[64]` — filter to one account, or empty for sum across all

### 10. Querying achievements, masteries, skins, recipes, Wizard's Vault
All five request structs include `account_name[64]`:
- **Empty** → uses the first configured account's API key (backward compatible)
- **Set** → uses the specified account's API key

All five response structs (and `HoardQueryApiResponse`) echo `account_name[64]` back, so you can correlate responses when querying multiple accounts in parallel.

Request structs:
- `HoardQueryAchievementRequest` — batch up to 200 achievement IDs
- `HoardQueryMasteryRequest` — batch up to 200 mastery IDs
- `HoardQuerySkinsRequest` — batch up to 200 skin IDs
- `HoardQueryRecipesRequest` — batch up to 200 recipe IDs
- `HoardQueryWizardsVaultRequest` — type: 0=daily, 1=weekly, 2=special

### 10a. Determining the currently logged-in account

**Single-account fast path:** `HoardPongPayload` and `HoardDataReadyPayload` both include `account_count`. If `account_count == 1`, skip the entire multi-account flow — leave `account_name` and `account_filter` empty on all requests. All data belongs to the one configured account, and empty defaults to first account.

**Multi-account flow (`account_count > 1`):** Achievements, masteries, skins, recipes, and Wizard's Vault data are per-account. To show data for the currently logged-in GW2 account, your addon must:

1. **Get the current character name** from MumbleLink
2. **Map the character to an account** using the character lists from `EV_HOARD_QUERY_ACCOUNTS`
3. **Pass that account name** in your request structs

#### Step 1: Reading MumbleLink identity

Subscribe to `EV_MUMBLE_IDENTITY_UPDATED`. The `eventArgs` pointer is a `Mumble::Identity*` struct — use it directly (do NOT use `DL_MUMBLE_LINK_IDENTITY` DataLink, which may contain garbage before MumbleLink is initialized).

**⚠️ MumbleLink caveats:**
- `DL_MUMBLE_LINK_IDENTITY` (the DataLink pointer) may contain stale data from adjacent MumbleLink memory before the game populates it (e.g. the GW2 exe path instead of character data)
- `EV_MUMBLE_IDENTITY_UPDATED` fires immediately on subscribe, potentially with invalid data
- The identity contains the **character name**, not the account name
- Always validate character names: GW2 names contain only letters, spaces, hyphens, and accented characters. Reject names containing path separators, digits, or other special characters.
- Check `UITick > 0` on `DL_MUMBLE_LINK` as a reliable indicator that MumbleLink data is valid

```cpp
// Recommended: use eventArgs directly as identity struct
void OnMumbleIdentityUpdated(void* aEventArgs) {
    if (!aEventArgs) return;
    const Mumble::Identity* id = static_cast<const Mumble::Identity*>(aEventArgs);
    char buf[20] = {};
    memcpy(buf, id->Name, 19);
    std::string charName(buf);
    // Validate: reject non-character-name strings
    for (unsigned char c : charName) {
        if (c == ' ' || c == '-' || isalpha(c) || c >= 0x80) continue;
        return; // invalid — likely garbage data
    }
    if (charName.empty()) return;
    g_CurrentCharacterName = charName;
    ResolveCurrentAccount(); // map character → account
}
```

**Render-loop fallback:** If the event fires before identity is valid (e.g. character select screen), poll `Mumble::Data::Identity[256]` (the raw JSON `wchar_t` string from `DL_MUMBLE_LINK`) and parse the `"name"` field:

```cpp
if (g_CurrentCharacterName.empty() && g_MumbleData && g_MumbleData->UITick > 0) {
    std::wstring wIdent(g_MumbleData->Identity);
    std::string ident(wIdent.begin(), wIdent.end());
    if (!ident.empty()) {
        auto j = nlohmann::json::parse(ident, nullptr, false);
        if (!j.is_discarded() && j.contains("name")) {
            g_CurrentCharacterName = j["name"].get<std::string>();
            ResolveCurrentAccount();
        }
    }
}
```

#### Step 2: Character-to-account mapping

The `HoardAccountEntry` response from `EV_HOARD_QUERY_ACCOUNTS` includes `character_count` and `characters[80][32]` — the list of character names on each account. Build a character→account map from this data:

```cpp
// After receiving accounts response:
std::unordered_map<std::string, std::string> g_CharToAccount;
for (uint32_t i = 0; i < resp->account_count; i++) {
    for (uint32_t c = 0; c < resp->accounts[i].character_count; c++) {
        g_CharToAccount[resp->accounts[i].characters[c]] = resp->accounts[i].account_name;
    }
}

// Resolve current account from character name:
void ResolveCurrentAccount() {
    auto it = g_CharToAccount.find(g_CurrentCharacterName);
    if (it != g_CharToAccount.end()) {
        g_CurrentAccountName = it->second;
    }
}
```

H&S does **not** auto-detect the logged-in account — it is a data provider. Your addon is responsible for determining which account to query.

### 11. Generic API proxy
`HoardQueryApiRequest` lets you make any authenticated GW2 API call via H&S:
- `endpoint[256]` — e.g. "/v2/account/dyes"
- `account_name[64]` — which account's API key to use (empty = first account)

Response: `HoardQueryApiResponse` with raw JSON (up to 64KB, `truncated` flag if exceeded).

### 12. Context menus
Register right-click menu items on H&S search results:
- `EV_HOARD_CONTEXT_MENU_REGISTER` — register with `HoardContextMenuRegister*`
- `EV_HOARD_CONTEXT_MENU_REMOVE` — remove with `HoardContextMenuRemove*`
- Callback: `HoardContextMenuCallback*` with item info when clicked
- `item_types` bitmask: `HOARD_MENU_ITEMS`, `HOARD_MENU_WALLET`, or `HOARD_MENU_ALL`

---

## Migration Checklist (v2 → v3)

1. **Update `HoardAndSeekAPI.h`** to the latest version from H&S.
2. **Set `api_version = HOARD_API_VERSION`** (now 3) in all request structs.
3. **`HoardItemLocationEntry` now includes `account_name[64]`** — the struct is larger than v2. If you were reading locations, update your code to account for the new field.
4. **`HoardQueryItemResponse.locations` is now 64 entries** (was 32).
5. **New events to subscribe to:**
   - `EV_HOARD_ACCOUNTS_CHANGED` — accounts added/removed
   - `EV_HOARD_DATA_UPDATED` now includes `account_name` and `account_count`
6. **Query accounts** with `EV_HOARD_QUERY_ACCOUNTS` to get names, labels, and status.
7. **Use `account_filter`** in item/wallet queries if you only want data from a specific account.
8. **Use `account_name`** in API proxy queries to select which account's key to use.
9. **Display `label`** instead of `account_name` in UI when `label` is not empty.
10. **`account_name[64]` added to achievement, mastery, skins, recipes, and Wizard's Vault request structs** — set to query a specific account, or leave empty for first account.
11. **Use Mumble identity** (`EV_MUMBLE_IDENTITY_UPDATED` event, NOT `DL_MUMBLE_LINK_IDENTITY` DataLink) to detect the current character, then map to account via `characters` in `HoardAccountEntry`.
12. **`EV_HOARD_DATA_UPDATED` now fires on cache load** at startup, not just after fresh fetches. No need to retry pings just to know if data is available.
13. **`HoardAccountEntry` now includes `character_count` and `characters[80][32]`** — use these to build a character→account map for detecting the currently logged-in account. Character lists are auto-fetched on startup for any validated account that doesn't have them yet (e.g. after upgrading from an older H&S version). They are also populated during every full data refresh. If `character_count` is 0, the account either has no characters or hasn't been fetched yet — fall back to `EV_HOARD_QUERY_API` with `/v2/characters` for that account.
14. **Single-account fast path:** Check `account_count` from `HoardPongPayload` or `HoardDataReadyPayload`. If `account_count == 1`, skip MumbleLink detection, character mapping, and account queries — just leave `account_name`/`account_filter` empty on all requests.
15. **`account_name[64]` added to all proxy response structs** — achievement, mastery, skins, recipes, Wizard's Vault, and generic API responses now echo back the queried account name for correlation when querying multiple accounts in parallel.
16. **`HoardQueryApiResponse::json` buffer increased to 64KB** (was 32KB). Always check `resp->truncated` before parsing — responses exceeding 64KB are still truncated.

---

## Example: Querying accounts, building character map, and displaying friendly names

```cpp
// Response handler
std::unordered_map<std::string, std::string> g_CharToAccount; // character -> account_name

void OnAccountsResponse(void* eventArgs) {
    auto* resp = static_cast<HoardQueryAccountsResponse*>(eventArgs);
    if (resp->status == HOARD_STATUS_OK) {
        std::lock_guard<std::mutex> lock(myMutex);
        myAccounts.clear();
        g_CharToAccount.clear();
        for (uint32_t i = 0; i < resp->account_count; i++) {
            MyAccount acct;
            acct.name = resp->accounts[i].account_name;
            acct.label = resp->accounts[i].label;
            acct.display = (acct.label[0] != '\0') ? acct.label : acct.name;
            myAccounts.push_back(acct);
            // Build character -> account map
            for (uint32_t c = 0; c < resp->accounts[i].character_count; c++) {
                g_CharToAccount[resp->accounts[i].characters[c]] = acct.name;
            }
        }
    }
    delete resp;
}

// Send query (no lock held!)
void QueryAccounts(AddonAPI_t* api) {
    HoardQueryAccountsRequest req{};
    req.api_version = HOARD_API_VERSION;
    strncpy(req.requester, "MyAddon", sizeof(req.requester));
    strncpy(req.response_event, "EV_MYADDON_ACCOUNTS", sizeof(req.response_event));
    api->Events_Raise(EV_HOARD_QUERY_ACCOUNTS, &req);
}
```

## Example: Querying all accounts for achievement progress

```cpp
// Send one achievement request per account
void QueryAchievementsForAllAccounts(AddonAPI_t* api, const std::vector<uint32_t>& ids) {
    for (const auto& acct : myAccounts) {
        HoardQueryAchievementRequest req{};
        req.api_version = HOARD_API_VERSION;
        strncpy(req.requester, "MyAddon", sizeof(req.requester));
        strncpy(req.response_event, "EV_MYADDON_ACH", sizeof(req.response_event));
        req.id_count = std::min(ids.size(), (size_t)200);
        for (uint32_t i = 0; i < req.id_count; i++) req.ids[i] = ids[i];
        strncpy(req.account_name, acct.name.c_str(), sizeof(req.account_name));
        api->Events_Raise(EV_HOARD_QUERY_ACHIEVEMENT, &req);
    }
}

// Response handler — use resp->account_name to identify which account
void OnAchievementResponse(void* eventArgs) {
    auto* resp = static_cast<HoardQueryAchievementResponse*>(eventArgs);
    if (resp->status == HOARD_STATUS_OK) {
        std::string account(resp->account_name);
        std::lock_guard<std::mutex> lock(myMutex);
        for (uint32_t i = 0; i < resp->entry_count; i++) {
            myAchievements[account][resp->entries[i].id] = resp->entries[i];
        }
    }
    delete resp;
}
```

## Example: Querying items filtered to a specific account

```cpp
void QueryItemForAccount(AddonAPI_t* api, uint32_t itemId, const char* accountName) {
    HoardQueryItemRequest req{};
    req.api_version = HOARD_API_VERSION;
    strncpy(req.requester, "MyAddon", sizeof(req.requester));
    req.item_id = itemId;
    strncpy(req.response_event, "EV_MYADDON_ITEM", sizeof(req.response_event));
    strncpy(req.account_filter, accountName, sizeof(req.account_filter));
    api->Events_Raise(EV_HOARD_QUERY_ITEM, &req);
}
```

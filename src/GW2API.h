#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

namespace HoardAndSeek {

    struct ApiKeyInfo {
        bool valid = false;
        std::string account_name;
        std::string key_name;
        std::vector<std::string> permissions;
        std::string error;
    };

    // Per-account configuration and state
    struct AccountInfo {
        std::string api_key;
        std::string account_name;  // from /v2/account, e.g. "PieOrCake.7635"
        std::string label;         // user-friendly, defaults to account_name
        bool validated = false;
        time_t last_updated = 0;
        std::vector<std::string> permissions;
        std::string error;         // last validation error
        std::vector<std::string> characters; // character names from /v2/characters
    };

    enum class FetchStatus {
        Idle,
        InProgress,
        Success,
        Error
    };

    // Where an item was found
    struct ItemLocation {
        std::string account;      // Account name that owns this item
        std::string location;     // "Bank", "Material Storage", "Shared Inventory", or character name
        std::string sublocation;  // "Bag", "Equipped", "Bank Tab 1", etc.
        int count;
    };

    // Cached info about an item from /v2/items
    struct ItemInfo {
        uint32_t id = 0;
        std::string name;
        std::string icon_url;
        std::string rarity;
        std::string type;
        std::string description;
    };

    // A search result combining item info with all its locations
    struct SearchResult {
        uint32_t item_id = 0;
        std::string name;
        std::string icon_url;
        std::string rarity;
        std::string type;
        std::string description;
        std::vector<ItemLocation> locations;
        int total_count = 0;
    };

    // Base offset for synthetic wallet IDs (avoids collision with real item IDs)
    static const uint32_t WALLET_ID_BASE = 0x80000000;

    class GW2API {
    public:
        // Data path helper
        static std::string GetDataDirectory();

        // --- Account Management ---
        // Returns index of added account, or -1 on failure
        static int AddAccount(const std::string& api_key, const std::string& label = "");
        static bool RemoveAccount(const std::string& account_name);
        static void UpdateAccountKey(const std::string& account_name, const std::string& new_key);
        static void UpdateAccountLabel(const std::string& account_name, const std::string& label);
        static const std::vector<AccountInfo>& GetAccounts();
        static int GetAccountCount();
        static bool SaveAccounts();
        static bool LoadAccounts();

        // --- Validation (async) ---
        static void ValidateAccountAsync(const std::string& account_name);
        static void ValidateAllAccountsAsync();
        static FetchStatus GetValidationStatus();
        // Legacy compat: returns info for first account
        static const ApiKeyInfo& GetApiKeyInfo();

        // --- Account data fetching (async) ---
        // account_names: empty = fetch all accounts
        static void FetchAccountDataAsync(const std::vector<std::string>& account_names = {});
        static FetchStatus GetFetchStatus();
        static const std::string& GetFetchStatusMessage();

        // Query: has data been fetched for any account?
        static bool HasAccountData();

        // Monotonically increasing version of s_item_locations.
        // Bumped on every per-account commit and on LoadAccountData.
        // Callers compare against a stored value to detect changes.
        static uint64_t GetDataVersion();

        // Timestamp of last successful data fetch (earliest across accounts, 0 if never)
        static time_t GetLastUpdated();

        // Per-account cooldown
        static bool IsRefreshOnCooldown(const std::string& account_name);
        static time_t GetRefreshAvailableAt(const std::string& account_name);
        // Legacy: returns true if ALL requested accounts are on cooldown
        static bool IsRefreshOnCooldown();
        static time_t GetRefreshAvailableAt();

        // Search items by name substring (case-insensitive)
        // account_filter: empty = all accounts
        static std::vector<SearchResult> SearchItems(const std::string& query, const std::string& account_filter = "");

        // Get all locations for a specific item ID (optionally filtered by account)
        static std::vector<ItemLocation> GetItemLocations(uint32_t item_id, const std::string& account_filter = "");

        // Get total count of an item across all locations (optionally filtered by account)
        static int GetTotalCount(uint32_t item_id, const std::string& account_filter = "");

        // Get cached item info (name, icon, rarity)
        static const ItemInfo* GetItemInfo(uint32_t item_id);

        // Authenticated API proxy (account_name: empty = first account)
        static std::string AuthenticatedGet(const std::string& url, const std::string& account_name = "");

        // Fetch /v2/characters for validated accounts that have empty character lists
        static void FetchMissingCharacterListsAsync();

        // Persistence
        static bool LoadAccountData();
        static bool SaveAccountData();

        // Tooltip file management (external tooltips.json)
        static bool LoadTooltips();
        static bool SaveTooltips();
        // Fetch description for a single item on-demand (async)
        static void FetchTooltipAsync(uint32_t item_id);

    private:
        friend void TooltipWorker();

        // Account list
        static std::vector<AccountInfo> s_accounts;
        // Legacy compat (for GetApiKeyInfo)
        static ApiKeyInfo s_key_info;
        static FetchStatus s_validation_status;

        static FetchStatus s_fetch_status;
        static std::string s_fetch_message;

        // Merged item locations from all accounts (account tagged on each entry)
        static std::unordered_map<uint32_t, std::vector<ItemLocation>> s_item_locations;

        // Shared item info cache (names, icons, rarity are account-independent)
        static std::unordered_map<uint32_t, ItemInfo> s_item_cache;

        static bool s_has_account_data;
        static std::mutex s_mutex;

        // HTTP helpers
        struct HttpResponse {
            int status_code = 0;   // HTTP status code (0 = network error)
            std::string body;
        };
        static std::string HttpGet(const std::string& url);
        static HttpResponse HttpGetEx(const std::string& url);

        // Returns empty string if response is valid JSON data, or an error message
        static std::string CheckApiResponse(const HttpResponse& resp);

        static bool EnsureDataDirectory();
        static std::string GetAccountDataDir(const std::string& account_name);

        // Fetch item details from /v2/items for a batch of IDs
        static void FetchItemDetails(const std::vector<uint32_t>& item_ids);

        // Fetch a single account's data into the provided locations map
        static bool FetchSingleAccount(const std::string& key, const std::string& account_name,
            std::unordered_map<uint32_t, std::vector<ItemLocation>>& locations,
            std::vector<uint32_t>& all_item_ids);

        // Remove all entries for a given account from s_item_locations
        static void ClearAccountLocations(const std::string& account_name);

        // Merge fetched locations into s_item_locations
        static void MergeLocations(const std::unordered_map<uint32_t, std::vector<ItemLocation>>& new_locations);
    };

}

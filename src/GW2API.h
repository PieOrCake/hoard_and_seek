#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <nlohmann/json.hpp>

namespace HoardAndSeek {

    struct ApiKeyInfo {
        bool valid = false;
        std::string account_name;
        std::string key_name;
        std::vector<std::string> permissions;
        std::string error;
    };

    enum class FetchStatus {
        Idle,
        InProgress,
        Success,
        Error
    };

    // Where an item was found on the account
    struct ItemLocation {
        std::string location;  // "Bank", "Material Storage", "Shared Inventory", or character name
        std::string sublocation; // "Bag", "Equipped", "Bank Tab 1", etc.
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

        // API Key management
        static void SetApiKey(const std::string& key);
        static const std::string& GetApiKey();
        static bool LoadApiKey();
        static bool SaveApiKey();

        // Validation (async)
        static void ValidateApiKeyAsync();
        static FetchStatus GetValidationStatus();
        static const ApiKeyInfo& GetApiKeyInfo();

        // Account data fetching (async) - scans all storage locations
        static void FetchAccountDataAsync();
        static FetchStatus GetFetchStatus();
        static const std::string& GetFetchStatusMessage();

        // Query: has data been fetched?
        static bool HasAccountData();

        // Search items by name substring (case-insensitive)
        static std::vector<SearchResult> SearchItems(const std::string& query);

        // Get all locations for a specific item ID
        static std::vector<ItemLocation> GetItemLocations(uint32_t item_id);

        // Get total count of an item across all locations
        static int GetTotalCount(uint32_t item_id);

        // Get cached item info (name, icon, rarity)
        static const ItemInfo* GetItemInfo(uint32_t item_id);

        // Authenticated API proxy (appends stored API key to URL)
        static std::string AuthenticatedGet(const std::string& url);

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
        static std::string s_api_key;
        static ApiKeyInfo s_key_info;
        static FetchStatus s_validation_status;
        static FetchStatus s_fetch_status;
        static std::string s_fetch_message;

        // item_id -> list of locations where it was found
        static std::unordered_map<uint32_t, std::vector<ItemLocation>> s_item_locations;

        // item_id -> cached item info from /v2/items
        static std::unordered_map<uint32_t, ItemInfo> s_item_cache;

        // Wallet: currency_id -> amount
        static std::unordered_map<int, int> s_wallet;

        static bool s_has_account_data;
        static std::mutex s_mutex;

        // HTTP helper
        static std::string HttpGet(const std::string& url);

        static bool EnsureDataDirectory();

        // Fetch item details from /v2/items for a batch of IDs
        static void FetchItemDetails(const std::vector<uint32_t>& item_ids);
    };

}

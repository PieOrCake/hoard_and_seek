#include "GW2API.h"
#include "HoardAndSeekAPI.h"

#include <windows.h>
#include <wininet.h>
#include <fstream>
#include <sstream>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <unordered_set>
#include <atomic>
#include <condition_variable>

using json = nlohmann::json;

namespace HoardAndSeek {

    // Strip HTML-like tags from GW2 API descriptions
    static std::string StripHtmlTags(const std::string& input) {
        std::string result;
        result.reserve(input.size());
        bool in_tag = false;
        for (size_t i = 0; i < input.size(); i++) {
            if (input[i] == '<') {
                if (i + 3 < input.size() && input[i+1] == 'b' && input[i+2] == 'r' && input[i+3] == '>') {
                    result += '\n';
                    i += 3;
                    continue;
                }
                in_tag = true;
            } else if (input[i] == '>') {
                in_tag = false;
            } else if (!in_tag) {
                result += input[i];
            }
        }
        return result;
    }

    // Static member initialization
    std::string GW2API::s_api_key;
    ApiKeyInfo GW2API::s_key_info;
    FetchStatus GW2API::s_validation_status = FetchStatus::Idle;
    FetchStatus GW2API::s_fetch_status = FetchStatus::Idle;
    std::string GW2API::s_fetch_message;
    std::unordered_map<uint32_t, std::vector<ItemLocation>> GW2API::s_item_locations;
    std::unordered_map<uint32_t, ItemInfo> GW2API::s_item_cache;
    std::unordered_map<int, int> GW2API::s_wallet;
    bool GW2API::s_has_account_data = false;
    time_t GW2API::s_last_updated = 0;
    std::mutex GW2API::s_mutex;

    // Helper: get the DLL directory
    static std::string GetDllDir() {
        char dllPath[MAX_PATH];
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)GetDllDir, &hModule)) {
            if (GetModuleFileNameA(hModule, dllPath, MAX_PATH)) {
                std::string path(dllPath);
                size_t lastSlash = path.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    return path.substr(0, lastSlash);
                }
            }
        }
        return "";
    }

    std::string GW2API::GetDataDirectory() {
        std::string dir = GetDllDir();
        if (!dir.empty()) {
            std::replace(dir.begin(), dir.end(), '\\', '/');
        }
        return dir + "/HoardAndSeek";
    }

    bool GW2API::EnsureDataDirectory() {
        std::string dir = GetDataDirectory();
        try {
            std::filesystem::create_directories(dir);
            return true;
        } catch (...) {
            return false;
        }
    }

    // HTTP GET using WinINet
    std::string GW2API::HttpGet(const std::string& url) {
        HINTERNET hInternet = InternetOpenA("HoardAndSeek/1.0",
            INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return "";

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                      INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);
        if (!hUrl) {
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

    GW2API::HttpResponse GW2API::HttpGetEx(const std::string& url) {
        HttpResponse result;
        HINTERNET hInternet = InternetOpenA("HoardAndSeek/1.0",
            INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return result;

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                      INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);
        if (!hUrl) {
            InternetCloseHandle(hInternet);
            return result;
        }

        // Query HTTP status code
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        HttpQueryInfoA(hUrl, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &statusCode, &statusSize, NULL);
        result.status_code = (int)statusCode;

        char buffer[8192];
        DWORD bytesRead;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            result.body.append(buffer, bytesRead);
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return result;
    }

    std::string GW2API::CheckApiResponse(const HttpResponse& resp) {
        if (resp.status_code == 0)
            return "No response (network error or API offline)";
        if (resp.status_code == 502 || resp.status_code == 503)
            return "GW2 API is temporarily unavailable (maintenance)";
        if (resp.status_code >= 500)
            return "GW2 API server error (HTTP " + std::to_string(resp.status_code) + ")";
        if (resp.body.empty())
            return "Empty response from API";
        // Detect HTML error pages (API gateway failures)
        if (resp.body.size() > 0 && resp.body[0] == '<')
            return "GW2 API returned unexpected response (possible maintenance)";
        // Check for JSON error object
        try {
            json j = json::parse(resp.body);
            if (j.is_object() && j.contains("text"))
                return "GW2 API: " + j["text"].get<std::string>();
        } catch (...) {
            return "GW2 API returned invalid response";
        }
        return ""; // OK
    }

    std::string GW2API::AuthenticatedGet(const std::string& url) {
        std::string key;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            key = s_api_key;
        }
        if (key.empty()) return "";
        std::string sep = (url.find('?') != std::string::npos) ? "&" : "?";
        return HttpGet(url + sep + "access_token=" + key);
    }

    // --- API Key Management ---

    static std::string TrimKey(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    void GW2API::SetApiKey(const std::string& key) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_api_key = TrimKey(key);
    }

    const std::string& GW2API::GetApiKey() {
        return s_api_key;
    }

    bool GW2API::LoadApiKey() {
        std::string path = GetDataDirectory() + "/api_key.json";
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j;
            file >> j;
            if (j.contains("api_key")) {
                std::lock_guard<std::mutex> lock(s_mutex);
                s_api_key = TrimKey(j["api_key"].get<std::string>());
                return true;
            }
        } catch (...) {}
        return false;
    }

    bool GW2API::SaveApiKey() {
        EnsureDataDirectory();
        std::string path = GetDataDirectory() + "/api_key.json";
        json j;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            j["api_key"] = s_api_key;
        }
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(2);
        file.flush();
        return true;
    }

    // --- Validation ---

    void GW2API::ValidateApiKeyAsync() {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_validation_status = FetchStatus::InProgress;
            s_key_info = ApiKeyInfo{};
        }

        std::string key;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            key = s_api_key;
        }

        std::thread([key]() {
            ApiKeyInfo info;
            try {
                // Fetch token info
                std::string url = "https://api.guildwars2.com/v2/tokeninfo?access_token=" + key;
                std::string response = HttpGet(url);
                if (response.empty()) {
                    info.error = "No response from API";
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_key_info = info;
                    s_validation_status = FetchStatus::Error;
                    return;
                }

                json j = json::parse(response);
                if (j.contains("text")) {
                    info.error = j["text"].get<std::string>();
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_key_info = info;
                    s_validation_status = FetchStatus::Error;
                    return;
                }

                info.key_name = j.value("name", "");
                if (j.contains("permissions")) {
                    for (const auto& p : j["permissions"]) {
                        info.permissions.push_back(p.get<std::string>());
                    }
                }

                // Fetch account name
                std::string acc_url = "https://api.guildwars2.com/v2/account?access_token=" + key;
                std::string acc_response = HttpGet(acc_url);
                if (!acc_response.empty()) {
                    json acc_j = json::parse(acc_response);
                    if (acc_j.contains("name")) {
                        info.account_name = acc_j["name"].get<std::string>();
                    }
                }

                info.valid = true;
                std::lock_guard<std::mutex> lock(s_mutex);
                s_key_info = info;
                s_validation_status = FetchStatus::Success;

            } catch (const std::exception& e) {
                info.error = std::string("Exception: ") + e.what();
                std::lock_guard<std::mutex> lock(s_mutex);
                s_key_info = info;
                s_validation_status = FetchStatus::Error;
            }
        }).detach();
    }

    FetchStatus GW2API::GetValidationStatus() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_validation_status;
    }

    const ApiKeyInfo& GW2API::GetApiKeyInfo() {
        return s_key_info;
    }

    // --- URL-encode helper ---
    static std::string UrlEncode(const std::string& name) {
        std::string encoded;
        for (char c : name) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else {
                char hex[4];
                snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
                encoded += hex;
            }
        }
        return encoded;
    }

    // --- Helper: add item to location map ---
    static void AddItemLocation(
        std::unordered_map<uint32_t, std::vector<ItemLocation>>& locations,
        uint32_t item_id, const std::string& location,
        const std::string& sublocation, int count)
    {
        if (item_id == 0 || count <= 0) return;
        // Check if there's already an entry for same location+sublocation, merge counts
        auto& locs = locations[item_id];
        for (auto& loc : locs) {
            if (loc.location == location && loc.sublocation == sublocation) {
                loc.count += count;
                return;
            }
        }
        locs.push_back({location, sublocation, count});
    }

    // --- Fetch item details from /v2/items (batched, max 200 per request) ---
    void GW2API::FetchItemDetails(const std::vector<uint32_t>& item_ids) {
        if (item_ids.empty()) return;

        // Filter out already-cached IDs
        std::vector<uint32_t> to_fetch;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            for (uint32_t id : item_ids) {
                if (s_item_cache.find(id) == s_item_cache.end()) {
                    to_fetch.push_back(id);
                }
            }
        }
        if (to_fetch.empty()) return;

        // Deduplicate
        std::sort(to_fetch.begin(), to_fetch.end());
        to_fetch.erase(std::unique(to_fetch.begin(), to_fetch.end()), to_fetch.end());

        // Batch in groups of 200
        const size_t BATCH_SIZE = 200;
        for (size_t i = 0; i < to_fetch.size(); i += BATCH_SIZE) {
            std::string ids_param;
            size_t end = std::min(i + BATCH_SIZE, to_fetch.size());
            for (size_t j = i; j < end; j++) {
                if (!ids_param.empty()) ids_param += ",";
                ids_param += std::to_string(to_fetch[j]);
            }

            std::string url = "https://api.guildwars2.com/v2/items?ids=" + ids_param;
            std::string response = HttpGet(url);
            if (response.empty()) continue;

            try {
                json j = json::parse(response);
                if (j.is_array()) {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    for (const auto& item : j) {
                        if (!item.contains("id")) continue;
                        ItemInfo info;
                        info.id = item["id"].get<uint32_t>();
                        info.name = item.value("name", "");
                        info.icon_url = item.value("icon", "");
                        info.rarity = item.value("rarity", "");
                        info.type = item.value("type", "");
                        // Preserve existing description from tooltips.json
                        auto existing = s_item_cache.find(info.id);
                        if (existing != s_item_cache.end() && !existing->second.description.empty()) {
                            info.description = existing->second.description;
                        }
                        s_item_cache[info.id] = info;
                    }
                }
            } catch (...) {}
        }
    }

    // --- Account Data Fetching ---

    void GW2API::FetchAccountDataAsync() {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_fetch_status = FetchStatus::InProgress;
            s_fetch_message = "Starting...";
        }

        std::string key;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            key = s_api_key;
        }

        std::thread([key]() {
            std::unordered_map<uint32_t, std::vector<ItemLocation>> locations;
            std::vector<uint32_t> all_item_ids;

            try {
                if (key.empty()) {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_status = FetchStatus::Error;
                    s_fetch_message = "No API key configured";
                    return;
                }

                // 1. Material Storage (also serves as API availability check)
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching material storage...";
                }
                std::string url = "https://api.guildwars2.com/v2/account/materials?access_token=" + key;
                auto firstResp = HttpGetEx(url);
                std::string apiError = CheckApiResponse(firstResp);
                if (!apiError.empty()) {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_status = FetchStatus::Error;
                    s_fetch_message = apiError;
                    return;
                }
                std::string response = firstResp.body;
                {
                    json j = json::parse(response);
                    if (j.is_array()) {
                        for (const auto& slot : j) {
                            if (slot.is_null() || !slot.contains("id")) continue;
                            uint32_t id = slot["id"].get<uint32_t>();
                            int count = slot.value("count", 0);
                            if (count > 0) {
                                // Category info from material storage
                                int category = slot.value("category", 0);
                                std::string sub = "Category " + std::to_string(category);
                                AddItemLocation(locations, id, "Material Storage", sub, count);
                                all_item_ids.push_back(id);
                            }
                        }
                    }
                }

                // 2. Bank
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching bank...";
                }
                url = "https://api.guildwars2.com/v2/account/bank?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    try {
                        json j = json::parse(response);
                        if (j.is_array()) {
                            int slot_index = 0;
                            for (const auto& slot : j) {
                                if (!slot.is_null() && slot.contains("id")) {
                                    uint32_t id = slot["id"].get<uint32_t>();
                                    int count = slot.value("count", 1);
                                    // Bank tabs are 30 slots each
                                    int tab = (slot_index / 30) + 1;
                                    std::string sub = "Tab " + std::to_string(tab);
                                    AddItemLocation(locations, id, "Bank", sub, count);
                                    all_item_ids.push_back(id);
                                }
                                slot_index++;
                            }
                        }
                    } catch (...) {}
                }

                // 3. Shared Inventory
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching shared inventory...";
                }
                url = "https://api.guildwars2.com/v2/account/inventory?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    try {
                        json j = json::parse(response);
                        if (j.is_array()) {
                            for (const auto& slot : j) {
                                if (slot.is_null() || !slot.contains("id")) continue;
                                uint32_t id = slot["id"].get<uint32_t>();
                                int count = slot.value("count", 1);
                                AddItemLocation(locations, id, "Shared Inventory", "", count);
                                all_item_ids.push_back(id);
                            }
                        }
                    } catch (...) {}
                }

                // 4. Character Inventories + Equipment
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching characters...";
                }
                url = "https://api.guildwars2.com/v2/characters?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    try {
                        json chars = json::parse(response);
                        if (chars.is_array()) {
                            for (const auto& char_name_json : chars) {
                                std::string char_name = char_name_json.get<std::string>();
                                std::string encoded_name = UrlEncode(char_name);

                                {
                                    std::lock_guard<std::mutex> lock(s_mutex);
                                    s_fetch_message = "Fetching inventory: " + char_name + "...";
                                }

                                // Character inventory (bags)
                                std::string inv_url = "https://api.guildwars2.com/v2/characters/" +
                                    encoded_name + "/inventory?access_token=" + key;
                                std::string inv_response = HttpGet(inv_url);
                                if (!inv_response.empty()) {
                                    try {
                                        json inv = json::parse(inv_response);
                                        if (inv.contains("bags") && inv["bags"].is_array()) {
                                            int bag_num = 0;
                                            for (const auto& bag : inv["bags"]) {
                                                bag_num++;
                                                if (bag.is_null() || !bag.contains("inventory")) continue;
                                                for (const auto& item : bag["inventory"]) {
                                                    if (item.is_null() || !item.contains("id")) continue;
                                                    uint32_t id = item["id"].get<uint32_t>();
                                                    int count = item.value("count", 1);
                                                    std::string sub = "Bag " + std::to_string(bag_num);
                                                    AddItemLocation(locations, id, char_name, sub, count);
                                                    all_item_ids.push_back(id);
                                                }
                                            }
                                        }
                                    } catch (...) {}
                                }

                                // Character equipment
                                {
                                    std::lock_guard<std::mutex> lock(s_mutex);
                                    s_fetch_message = "Fetching equipment: " + char_name + "...";
                                }
                                std::string equip_url = "https://api.guildwars2.com/v2/characters/" +
                                    encoded_name + "/equipment?access_token=" + key;
                                std::string equip_response = HttpGet(equip_url);
                                if (!equip_response.empty()) {
                                    try {
                                        json equip = json::parse(equip_response);
                                        if (equip.contains("equipment") && equip["equipment"].is_array()) {
                                            for (const auto& piece : equip["equipment"]) {
                                                if (piece.is_null() || !piece.contains("id")) continue;
                                                uint32_t id = piece["id"].get<uint32_t>();
                                                std::string slot_name = piece.value("slot", "Unknown");
                                                AddItemLocation(locations, id, char_name, "Equipped: " + slot_name, 1);
                                                all_item_ids.push_back(id);
                                            }
                                        }
                                    } catch (...) {}
                                }
                            }
                        }
                    } catch (...) {}
                }

                // 5. Legendary Armory
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching legendary armory...";
                }
                url = "https://api.guildwars2.com/v2/account/legendaryarmory?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    try {
                        json j = json::parse(response);
                        if (j.is_array()) {
                            for (const auto& entry : j) {
                                if (!entry.contains("id")) continue;
                                uint32_t id = entry["id"].get<uint32_t>();
                                int count = entry.value("count", 1);
                                AddItemLocation(locations, id, "Legendary Armory", "", count);
                                all_item_ids.push_back(id);
                            }
                        }
                    } catch (...) {}
                }

                // 6. Guild Stash (requires 'guilds' permission — skip gracefully if unavailable)
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching guild stash...";
                }
                {
                    // Get guild IDs from /v2/account
                    std::string acc_url = "https://api.guildwars2.com/v2/account?access_token=" + key;
                    std::string acc_response = HttpGet(acc_url);
                    if (!acc_response.empty()) {
                        try {
                            json acc = json::parse(acc_response);
                            if (acc.contains("guilds") && acc["guilds"].is_array()) {
                                for (const auto& gid : acc["guilds"]) {
                                    std::string guild_id = gid.get<std::string>();

                                    // Get guild name
                                    std::string guild_name = "Guild";
                                    std::string ginfo_url = "https://api.guildwars2.com/v2/guild/" + guild_id + "?access_token=" + key;
                                    std::string ginfo_resp = HttpGet(ginfo_url);
                                    if (!ginfo_resp.empty()) {
                                        try {
                                            json gi = json::parse(ginfo_resp);
                                            if (gi.contains("name")) {
                                                guild_name = gi["name"].get<std::string>();
                                            }
                                        } catch (...) {}
                                    }

                                    {
                                        std::lock_guard<std::mutex> lock(s_mutex);
                                        s_fetch_message = "Fetching guild stash: " + guild_name + "...";
                                    }

                                    // Get stash
                                    std::string stash_url = "https://api.guildwars2.com/v2/guild/" + guild_id + "/stash?access_token=" + key;
                                    std::string stash_resp = HttpGet(stash_url);
                                    if (!stash_resp.empty()) {
                                        try {
                                            json stash = json::parse(stash_resp);
                                            if (stash.is_array()) {
                                                int tab_num = 0;
                                                for (const auto& tab : stash) {
                                                    tab_num++;
                                                    if (!tab.contains("inventory") || !tab["inventory"].is_array()) continue;
                                                    for (const auto& slot : tab["inventory"]) {
                                                        if (slot.is_null() || !slot.contains("id")) continue;
                                                        uint32_t id = slot["id"].get<uint32_t>();
                                                        int count = slot.value("count", 1);
                                                        std::string loc = "Guild: " + guild_name;
                                                        std::string sub = "Tab " + std::to_string(tab_num);
                                                        AddItemLocation(locations, id, loc, sub, count);
                                                        all_item_ids.push_back(id);
                                                    }
                                                }
                                            }
                                        } catch (...) {}
                                    }
                                }
                            }
                        } catch (...) {}
                    }
                }

                // 7. Trading Post Delivery (requires 'tradingpost' permission — skip gracefully)
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching TP delivery...";
                }
                url = "https://api.guildwars2.com/v2/commerce/delivery?access_token=" + key;
                response = HttpGet(url);
                if (!response.empty()) {
                    try {
                        json j = json::parse(response);
                        if (j.contains("items") && j["items"].is_array()) {
                            for (const auto& item : j["items"]) {
                                if (!item.contains("id")) continue;
                                uint32_t id = item["id"].get<uint32_t>();
                                int count = item.value("count", 1);
                                AddItemLocation(locations, id, "TP Delivery", "", count);
                                all_item_ids.push_back(id);
                            }
                        }
                        // TP delivery coins handled separately (not an item)
                    } catch (...) {}
                }

                // 8. Wallet
                std::unordered_map<int, int> wallet;
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching wallet...";
                }
                url = "https://api.guildwars2.com/v2/account/wallet?access_token=" + key;
                response = HttpGet(url);
                std::vector<int> currency_ids;
                if (!response.empty()) {
                    try {
                        json j = json::parse(response);
                        if (j.is_array()) {
                            for (const auto& entry : j) {
                                if (!entry.contains("id") || !entry.contains("value")) continue;
                                int id = entry["id"].get<int>();
                                int value = entry["value"].get<int>();
                                if (value > 0) {
                                    wallet[id] = value;
                                    currency_ids.push_back(id);
                                }
                            }
                        }
                    } catch (...) {}
                }

                // 9. Fetch currency details (names, icons) from /v2/currencies
                if (!currency_ids.empty()) {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching currency details...";
                }
                {
                    // Batch in groups of 200
                    const size_t BATCH = 200;
                    for (size_t i = 0; i < currency_ids.size(); i += BATCH) {
                        std::string ids_param;
                        size_t end = std::min(i + BATCH, currency_ids.size());
                        for (size_t ci = i; ci < end; ci++) {
                            if (!ids_param.empty()) ids_param += ",";
                            ids_param += std::to_string(currency_ids[ci]);
                        }
                        std::string cur_url = "https://api.guildwars2.com/v2/currencies?ids=" + ids_param;
                        std::string cur_response = HttpGet(cur_url);
                        if (cur_response.empty()) continue;
                        try {
                            json cj = json::parse(cur_response);
                            if (cj.is_array()) {
                                for (const auto& cur : cj) {
                                    if (!cur.contains("id")) continue;
                                    int cid = cur["id"].get<int>();
                                    uint32_t synth_id = WALLET_ID_BASE | (uint32_t)cid;
                                    int amount = wallet.count(cid) ? wallet[cid] : 0;

                                    // Add to item cache with synthetic ID
                                    ItemInfo info;
                                    info.id = synth_id;
                                    info.name = cur.value("name", "Currency #" + std::to_string(cid));
                                    info.icon_url = cur.value("icon", "");
                                    info.rarity = "Currency";
                                    info.type = "Currency";

                                    std::lock_guard<std::mutex> lock(s_mutex);
                                    s_item_cache[synth_id] = info;

                                    // Add to locations map
                                    AddItemLocation(locations, synth_id, "Wallet", "", amount);
                                }
                            }
                        } catch (...) {}
                    }
                }

                // 10. Fetch item details (names, icons, rarity) for all found items
                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_message = "Fetching item details...";
                }
                // Deduplicate
                std::sort(all_item_ids.begin(), all_item_ids.end());
                all_item_ids.erase(std::unique(all_item_ids.begin(), all_item_ids.end()), all_item_ids.end());
                FetchItemDetails(all_item_ids);

                // Save and apply
                if (locations.empty()) {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_fetch_status = FetchStatus::Error;
                    s_fetch_message = "API returned no data (key may be invalid)";
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(s_mutex);
                    s_item_locations = locations;
                    s_wallet = wallet;
                    s_has_account_data = true;
                    s_last_updated = std::time(nullptr);
                    s_fetch_status = FetchStatus::Success;
                    int total_entries = (int)all_item_ids.size() + (int)wallet.size();
                    s_fetch_message = "Done (" + std::to_string(all_item_ids.size()) +
                                     " items, " + std::to_string(wallet.size()) + " currencies)";
                }

                SaveAccountData();

            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(s_mutex);
                s_fetch_status = FetchStatus::Error;
                s_fetch_message = std::string("Error: ") + e.what();
            }
        }).detach();
    }

    FetchStatus GW2API::GetFetchStatus() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_fetch_status;
    }

    const std::string& GW2API::GetFetchStatusMessage() {
        return s_fetch_message;
    }

    time_t GW2API::GetLastUpdated() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_last_updated;
    }

    bool GW2API::IsRefreshOnCooldown() {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_last_updated == 0) return false;
        return difftime(std::time(nullptr), s_last_updated) < HOARD_REFRESH_COOLDOWN;
    }

    time_t GW2API::GetRefreshAvailableAt() {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_last_updated == 0) return 0;
        time_t available = s_last_updated + HOARD_REFRESH_COOLDOWN;
        return (std::time(nullptr) >= available) ? 0 : available;
    }

    bool GW2API::HasAccountData() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_has_account_data;
    }

    // --- Search ---

    // Case-insensitive substring match
    static bool ContainsCI(const std::string& haystack, const std::string& needle) {
        if (needle.empty()) return true;
        if (haystack.size() < needle.size()) return false;
        auto it = std::search(haystack.begin(), haystack.end(),
                              needle.begin(), needle.end(),
                              [](char a, char b) { return std::tolower(a) == std::tolower(b); });
        return it != haystack.end();
    }

    std::vector<SearchResult> GW2API::SearchItems(const std::string& query) {
        std::lock_guard<std::mutex> lock(s_mutex);
        std::vector<SearchResult> results;

        if (query.empty() || !s_has_account_data) return results;

        for (const auto& [item_id, locs] : s_item_locations) {
            auto cache_it = s_item_cache.find(item_id);
            std::string name;
            if (cache_it != s_item_cache.end()) {
                name = cache_it->second.name;
            } else {
                name = "Item #" + std::to_string(item_id);
            }

            if (!ContainsCI(name, query)) continue;

            SearchResult result;
            result.item_id = item_id;
            result.name = name;
            result.locations = locs;
            result.total_count = 0;
            for (const auto& loc : locs) {
                result.total_count += loc.count;
            }

            if (cache_it != s_item_cache.end()) {
                result.icon_url = cache_it->second.icon_url;
                result.rarity = cache_it->second.rarity;
                result.type = cache_it->second.type;
                result.description = cache_it->second.description;
            }

            results.push_back(result);
        }

        // Sort alphabetically (case-insensitive)
        std::sort(results.begin(), results.end(), [](const SearchResult& a, const SearchResult& b) {
            std::string a_lower = a.name, b_lower = b.name;
            std::transform(a_lower.begin(), a_lower.end(), a_lower.begin(), ::tolower);
            std::transform(b_lower.begin(), b_lower.end(), b_lower.begin(), ::tolower);
            return a_lower < b_lower;
        });

        return results;
    }

    std::vector<ItemLocation> GW2API::GetItemLocations(uint32_t item_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_item_locations.find(item_id);
        if (it != s_item_locations.end()) return it->second;
        return {};
    }

    int GW2API::GetTotalCount(uint32_t item_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_item_locations.find(item_id);
        if (it == s_item_locations.end()) return 0;
        int total = 0;
        for (const auto& loc : it->second) {
            total += loc.count;
        }
        return total;
    }

    const ItemInfo* GW2API::GetItemInfo(uint32_t item_id) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_item_cache.find(item_id);
        if (it != s_item_cache.end()) return &it->second;
        return nullptr;
    }

    // --- Persistence ---

    bool GW2API::SaveAccountData() {
        EnsureDataDirectory();
        std::string path = GetDataDirectory() + "/account_data.json";

        try {
            json j;

            // Save item locations
            json items_json = json::object();
            for (const auto& [item_id, locs] : s_item_locations) {
                json locs_json = json::array();
                for (const auto& loc : locs) {
                    locs_json.push_back({
                        {"location", loc.location},
                        {"sublocation", loc.sublocation},
                        {"count", loc.count}
                    });
                }
                items_json[std::to_string(item_id)] = locs_json;
            }
            j["item_locations"] = items_json;

            // Save item cache
            json cache_json = json::object();
            for (const auto& [item_id, info] : s_item_cache) {
                cache_json[std::to_string(item_id)] = {
                    {"name", info.name},
                    {"icon", info.icon_url},
                    {"rarity", info.rarity},
                    {"type", info.type}
                };
            }
            j["item_cache"] = cache_json;
            j["last_updated"] = (int64_t)s_last_updated;

            std::ofstream file(path);
            if (!file.is_open()) return false;
            file << j.dump(2);
            file.flush();
            return true;
        } catch (...) {
            return false;
        }
    }

    bool GW2API::LoadAccountData() {
        std::string path = GetDataDirectory() + "/account_data.json";
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j;
            file >> j;

            std::lock_guard<std::mutex> lock(s_mutex);

            // Load item locations
            if (j.contains("item_locations") && j["item_locations"].is_object()) {
                s_item_locations.clear();
                for (auto& [key, locs_json] : j["item_locations"].items()) {
                    uint32_t item_id = std::stoul(key);
                    std::vector<ItemLocation> locs;
                    if (locs_json.is_array()) {
                        for (const auto& loc_json : locs_json) {
                            ItemLocation loc;
                            loc.location = loc_json.value("location", "");
                            loc.sublocation = loc_json.value("sublocation", "");
                            loc.count = loc_json.value("count", 0);
                            locs.push_back(loc);
                        }
                    }
                    s_item_locations[item_id] = locs;
                }
            }

            // Load item cache
            if (j.contains("item_cache") && j["item_cache"].is_object()) {
                s_item_cache.clear();
                for (auto& [key, info_json] : j["item_cache"].items()) {
                    uint32_t item_id = std::stoul(key);
                    ItemInfo info;
                    info.id = item_id;
                    info.name = info_json.value("name", "");
                    info.icon_url = info_json.value("icon", "");
                    info.rarity = info_json.value("rarity", "");
                    info.type = info_json.value("type", "");
                    s_item_cache[item_id] = info;
                }
            }

            if (j.contains("last_updated") && j["last_updated"].is_number()) {
                s_last_updated = (time_t)j["last_updated"].get<int64_t>();
            }

            s_has_account_data = !s_item_locations.empty();
            return s_has_account_data;
        } catch (...) {
            return false;
        }
    }

    // --- Tooltip File Management ---

    bool GW2API::LoadTooltips() {
        std::string path = GetDataDirectory() + "/tooltips.json";
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j;
            file >> j;

            std::lock_guard<std::mutex> lock(s_mutex);
            if (j.is_object()) {
                for (auto& [key, val] : j.items()) {
                    uint32_t item_id = std::stoul(key);
                    std::string desc = val.get<std::string>();
                    // Merge into item cache if entry exists
                    auto it = s_item_cache.find(item_id);
                    if (it != s_item_cache.end()) {
                        it->second.description = desc;
                    } else {
                        // Create a minimal cache entry with just the description
                        ItemInfo info;
                        info.id = item_id;
                        info.description = desc;
                        s_item_cache[item_id] = info;
                    }
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    bool GW2API::SaveTooltips() {
        EnsureDataDirectory();
        std::string path = GetDataDirectory() + "/tooltips.json";

        try {
            json j = json::object();
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                for (const auto& [item_id, info] : s_item_cache) {
                    if (!info.description.empty()) {
                        j[std::to_string(item_id)] = info.description;
                    }
                }
            }

            std::ofstream file(path);
            if (!file.is_open()) return false;
            file << j.dump(2);
            file.flush();
            return true;
        } catch (...) {
            return false;
        }
    }

    // --- Tooltip fetch queue (throttled) ---
    static std::vector<uint32_t> s_tooltip_queue;
    static std::unordered_set<uint32_t> s_tooltip_pending;
    static std::thread s_tooltip_worker;
    static std::condition_variable s_tooltip_cv;
    static std::atomic<bool> s_tooltip_stop{false};
    static const int TOOLTIP_REQUEST_DELAY_MS = 100; // 10 req/sec max

    void TooltipWorker() {
        while (!s_tooltip_stop.load()) {
            uint32_t item_id = 0;
            {
                std::unique_lock<std::mutex> lock(GW2API::s_mutex);
                s_tooltip_cv.wait(lock, [] {
                    return s_tooltip_stop.load() || !s_tooltip_queue.empty();
                });
                if (s_tooltip_stop.load()) break;
                if (s_tooltip_queue.empty()) continue;
                item_id = s_tooltip_queue.front();
                s_tooltip_queue.erase(s_tooltip_queue.begin());
            }

            try {
                std::string url = "https://api.guildwars2.com/v2/items/" + std::to_string(item_id);
                std::string response = GW2API::HttpGet(url);
                if (!response.empty()) {
                    json j = json::parse(response);
                    std::string desc = StripHtmlTags(j.value("description", ""));
                    if (!desc.empty()) {
                        std::lock_guard<std::mutex> lock(GW2API::s_mutex);
                        auto it = GW2API::s_item_cache.find(item_id);
                        if (it != GW2API::s_item_cache.end()) {
                            it->second.description = desc;
                        }
                    }
                }
            } catch (...) {}

            {
                std::lock_guard<std::mutex> lock(GW2API::s_mutex);
                s_tooltip_pending.erase(item_id);
            }

            // Throttle: 100ms between requests
            std::this_thread::sleep_for(std::chrono::milliseconds(TOOLTIP_REQUEST_DELAY_MS));
        }

        // Save all fetched tooltips on worker exit
        GW2API::SaveTooltips();
    }

    void GW2API::FetchTooltipAsync(uint32_t item_id) {
        // Don't fetch for synthetic wallet IDs
        if (item_id >= WALLET_ID_BASE) return;

        std::lock_guard<std::mutex> lock(s_mutex);

        // Already have a description
        auto it = s_item_cache.find(item_id);
        if (it != s_item_cache.end() && !it->second.description.empty()) return;

        // Already queued
        if (s_tooltip_pending.count(item_id)) return;

        s_tooltip_pending.insert(item_id);
        s_tooltip_queue.push_back(item_id);

        // Start worker thread if not running
        if (!s_tooltip_worker.joinable()) {
            s_tooltip_stop.store(false);
            s_tooltip_worker = std::thread(TooltipWorker);
        }

        s_tooltip_cv.notify_one();
    }

}

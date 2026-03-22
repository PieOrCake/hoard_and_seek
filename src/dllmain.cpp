#include <windows.h>
#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <thread>

#include "nexus/Nexus.h"
#include "imgui.h"
#include "GW2API.h"
#include "IconManager.h"
#include "HoardAndSeekAPI.h"
#include <nlohmann/json.hpp>

// Version constants
#define V_MAJOR 0
#define V_MINOR 9
#define V_BUILD 0
#define V_REVISION 0

// Quick Access icon identifiers
#define QA_ID "QA_HOARD_AND_SEEK"
#define TEX_ICON "TEX_HOARD_ICON"
#define TEX_ICON_HOVER "TEX_HOARD_ICON_HOVER"

// Embedded 32x32 magnifying glass icon (normal - light gray with dark border)
static const unsigned char ICON_SEARCH[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a,
    0xf4, 0x00, 0x00, 0x01, 0x98, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0x56, 0x21, 0xae, 0x83,
    0x40, 0x10, 0x1d, 0x2a, 0xd1, 0x48, 0x82, 0xe6, 0x04, 0xa0, 0x7a, 0x01, 0x34, 0xa9, 0xa8, 0x42,
    0x37, 0xc1, 0x71, 0x0c, 0x1c, 0x09, 0x1a, 0xdd, 0x20, 0x2a, 0x08, 0x17, 0xc0, 0xec, 0xec, 0x2d,
    0x4a, 0x90, 0xab, 0x6b, 0xf9, 0x6a, 0x9a, 0xfd, 0x0b, 0x34, 0xbb, 0xb4, 0xe5, 0xe7, 0x27, 0x4c,
    0x42, 0x42, 0xe0, 0xb1, 0xef, 0xcd, 0xec, 0xcc, 0x5b, 0x00, 0xf6, 0xf8, 0xe3, 0xb0, 0x3e, 0xb1,
    0x88, 0xe7, 0x79, 0x23, 0xdd, 0xf7, 0x7d, 0x6f, 0x6d, 0x22, 0x40, 0x26, 0x05, 0x00, 0x88, 0xe3,
    0x18, 0xeb, 0xba, 0x0e, 0xe5, 0x67, 0x3a, 0x62, 0xac, 0x77, 0x88, 0xcb, 0xb2, 0xec, 0x1d, 0xc7,
    0xf1, 0x54, 0x8c, 0x10, 0xa2, 0xbf, 0x5c, 0x2e, 0x9e, 0x8e, 0x10, 0x6b, 0x0d, 0x79, 0x96, 0x65,
    0x18, 0x04, 0xc1, 0x33, 0xdb, 0xb6, 0x6d, 0x71, 0x18, 0x06, 0x70, 0x5d, 0x17, 0xa2, 0x28, 0x7a,
    0x3e, 0xe7, 0x9c, 0x63, 0x9e, 0xe7, 0xe1, 0x2b, 0x11, 0xc6, 0x02, 0x64, 0xf2, 0x24, 0x49, 0xf0,
    0xf1, 0x78, 0xfc, 0x2a, 0xfb, 0xf9, 0x7c, 0xc6, 0xdb, 0xed, 0x06, 0x55, 0x55, 0x85, 0xb2, 0x88,
    0xb7, 0x05, 0x50, 0xf6, 0xd7, 0xeb, 0x15, 0x00, 0x00, 0x4e, 0xa7, 0xd3, 0x64, 0xaf, 0x09, 0xe3,
    0x38, 0x0e, 0x0a, 0x21, 0x42, 0x15, 0x6b, 0xda, 0xa0, 0x13, 0x01, 0x4d, 0xd3, 0xdc, 0x19, 0x63,
    0xa3, 0xef, 0xfb, 0x4c, 0x6d, 0x42, 0x19, 0x47, 0x97, 0xef, 0xfb, 0x8c, 0x31, 0x36, 0x36, 0x4d,
    0x73, 0x5f, 0xc2, 0x1f, 0x4c, 0x44, 0x50, 0xc3, 0xa9, 0x65, 0x57, 0x3b, 0x9f, 0x32, 0x25, 0xdc,
    0x5c, 0xa3, 0x1a, 0x09, 0x90, 0xd5, 0xb7, 0x6d, 0x8b, 0x3a, 0xe5, 0xa4, 0xf7, 0x84, 0x9f, 0x1b,
    0x5d, 0xa3, 0x0a, 0xc4, 0x71, 0x8c, 0x00, 0x00, 0xc3, 0x30, 0x18, 0x6d, 0x1d, 0xe1, 0xe9, 0xfb,
    0xd5, 0x02, 0xc8, 0x64, 0x5c, 0xd7, 0x35, 0x12, 0x40, 0x78, 0xd5, 0xa4, 0x8c, 0x04, 0xc8, 0xe5,
    0xa6, 0x39, 0x5f, 0x6a, 0x2a, 0xb5, 0xdc, 0xb2, 0x2f, 0xcc, 0x6d, 0x9b, 0x51, 0x13, 0x0a, 0x21,
    0x7a, 0x00, 0x00, 0xdb, 0xb6, 0x51, 0x07, 0x4f, 0x38, 0xfa, 0x6e, 0xb5, 0x0f, 0x50, 0x36, 0xc7,
    0xe3, 0x11, 0xd3, 0x34, 0x0d, 0x97, 0x7c, 0x40, 0xad, 0x8c, 0x8e, 0x0f, 0x1c, 0xd6, 0x90, 0xd3,
    0xe2, 0x94, 0xa1, 0x3c, 0xfb, 0x94, 0x39, 0x91, 0x73, 0xce, 0x71, 0xb5, 0x15, 0x2f, 0x91, 0x73,
    0xce, 0xbf, 0x7f, 0x16, 0x2c, 0x91, 0x17, 0x45, 0x81, 0x5d, 0xd7, 0x85, 0x5f, 0x3d, 0x0d, 0x75,
    0xc8, 0xe7, 0x7c, 0xe2, 0x23, 0xff, 0x03, 0x3a, 0xe4, 0xea, 0xc2, 0x1f, 0xfb, 0x23, 0x5a, 0x43,
    0xfe, 0x6e, 0x4c, 0xa6, 0x60, 0x4b, 0xf2, 0xd9, 0x0a, 0xd0, 0xf8, 0x6c, 0x41, 0xfe, 0xd2, 0x07,
    0xb6, 0x20, 0x5f, 0xec, 0x01, 0xd3, 0x4e, 0xde, 0x63, 0x8f, 0x7f, 0x1d, 0x3f, 0x4d, 0xd3, 0x42,
    0x06, 0xae, 0x70, 0xcd, 0xd6, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60,
    0x82,
};
static const unsigned int ICON_SEARCH_size = 465;

// Embedded 32x32 magnifying glass icon (hover - white with dark border)
static const unsigned char ICON_SEARCH_HOVER[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7a, 0x7a,
    0xf4, 0x00, 0x00, 0x01, 0x3f, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xed, 0x56, 0x31, 0xae, 0x84,
    0x20, 0x10, 0x9d, 0xfd, 0xbd, 0x9d, 0x35, 0x56, 0x56, 0x1c, 0x60, 0x8f, 0x85, 0x25, 0xa7, 0x30,
    0x5e, 0x00, 0xcf, 0xc1, 0x11, 0xf0, 0x0c, 0x56, 0x18, 0xec, 0xbc, 0x83, 0xbf, 0x7a, 0x09, 0x9f,
    0xdd, 0xfd, 0x82, 0xb2, 0x6e, 0x36, 0xf1, 0x25, 0x24, 0x06, 0xde, 0xcc, 0x1b, 0xc6, 0x61, 0x80,
    0xe8, 0xc2, 0x87, 0x71, 0xcb, 0xe1, 0x84, 0x31, 0xb6, 0xe2, 0x7b, 0x9a, 0xa6, 0x24, 0x9f, 0x3f,
    0x47, 0x44, 0x31, 0x88, 0x88, 0x9a, 0xa6, 0x19, 0x9e, 0xcd, 0x67, 0x87, 0x2f, 0xe0, 0x9c, 0xb3,
    0xeb, 0x13, 0x38, 0xe7, 0xec, 0x5b, 0x02, 0x81, 0x43, 0xad, 0xb5, 0xf1, 0x05, 0x95, 0x52, 0x46,
    0x4a, 0x69, 0x94, 0x52, 0x7f, 0xe6, 0xb5, 0xd6, 0x26, 0x6b, 0x10, 0xa1, 0x38, 0xe7, 0xdc, 0xf8,
    0x3b, 0x65, 0x8c, 0xad, 0x52, 0x4a, 0xc3, 0x39, 0x37, 0x61, 0x10, 0x87, 0x8b, 0x10, 0x4e, 0xac,
    0xb5, 0x44, 0x44, 0x54, 0x55, 0x15, 0x85, 0x85, 0x07, 0x4e, 0x59, 0x96, 0xc3, 0xb2, 0x2c, 0xf7,
    0x90, 0x9b, 0x5a, 0xa0, 0x0f, 0x01, 0xe0, 0x9f, 0x63, 0xe7, 0x5b, 0x35, 0x82, 0x4c, 0xa0, 0x26,
    0x0e, 0xa7, 0x1f, 0x88, 0xf9, 0xaf, 0xe0, 0xf8, 0x36, 0xbb, 0x8f, 0xa1, 0x6f, 0xdc, 0xf7, 0xfd,
    0x10, 0x93, 0x4e, 0xac, 0x83, 0x1f, 0xfa, 0x49, 0xee, 0x03, 0x38, 0xe7, 0xe3, 0x38, 0x26, 0x65,
    0x0e, 0x7c, 0xd8, 0xef, 0x0e, 0xa0, 0xeb, 0xba, 0x3b, 0x11, 0x51, 0x5d, 0xd7, 0x49, 0x01, 0x80,
    0x0f, 0xfb, 0xef, 0xac, 0x01, 0x60, 0x9e, 0xe7, 0x89, 0x88, 0xa8, 0x28, 0x8a, 0x21, 0x86, 0x0f,
    0x1e, 0xec, 0x0e, 0x77, 0x40, 0x21, 0x84, 0x09, 0xb3, 0x10, 0xee, 0xcc, 0x9f, 0x4f, 0xc9, 0x58,
    0x92, 0xf8, 0x7f, 0x9d, 0xd0, 0x3f, 0xff, 0x31, 0x9d, 0x70, 0x97, 0xf8, 0x29, 0x77, 0xc1, 0x2b,
    0x71, 0x21, 0x84, 0x79, 0xfb, 0x6d, 0x18, 0x23, 0x1e, 0x8e, 0xb6, 0x6d, 0x1f, 0xd6, 0xb2, 0xa6,
    0xdd, 0x17, 0xdf, 0x7a, 0x9c, 0x64, 0xad, 0xf6, 0x2d, 0xf1, 0xec, 0xaf, 0x9d, 0x8f, 0x89, 0x87,
    0xdd, 0xee, 0x74, 0xf1, 0x30, 0x80, 0xb3, 0xc4, 0x6f, 0xaf, 0xae, 0xdd, 0xbd, 0xcf, 0xec, 0x0b,
    0x17, 0xbe, 0x0e, 0xbf, 0xe7, 0x68, 0x3f, 0xe3, 0xf9, 0x40, 0xc6, 0x98, 0x00, 0x00, 0x00, 0x00,
    0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};
static const unsigned int ICON_SEARCH_HOVER_size = 376;

// Global variables
HMODULE hSelf;
AddonDefinition_t AddonDef{};
AddonAPI_t* APIDefs = nullptr;
bool g_WindowVisible = false;

// UI State
static char g_SearchFilter[256] = "";
static char g_ApiKeyBuf[256] = "";
static bool g_ShowApiKey = false;
static std::vector<HoardAndSeek::SearchResult> g_SearchResults;
static uint32_t g_SelectedItemId = 0;
static int g_MinSearchLength = 3;

// GW2 rarity colors
static ImVec4 GetRarityColor(const std::string& rarity) {
    if (rarity == "Legendary")  return ImVec4(0.63f, 0.39f, 0.78f, 1.0f); // Purple
    if (rarity == "Ascended")   return ImVec4(0.90f, 0.39f, 0.55f, 1.0f); // Pink
    if (rarity == "Exotic")     return ImVec4(1.00f, 0.65f, 0.00f, 1.0f); // Orange
    if (rarity == "Rare")       return ImVec4(1.00f, 0.86f, 0.20f, 1.0f); // Yellow
    if (rarity == "Masterwork") return ImVec4(0.12f, 0.71f, 0.12f, 1.0f); // Green
    if (rarity == "Fine")       return ImVec4(0.35f, 0.63f, 0.90f, 1.0f); // Blue
    if (rarity == "Currency")   return ImVec4(1.0f, 0.85f, 0.0f, 1.0f);  // Gold
    return ImVec4(0.80f, 0.80f, 0.80f, 1.0f); // Basic/Junk
}

// Location icon character (purely decorative, using unicode-safe text)
static const char* GetLocationIcon(const std::string& location) {
    if (location == "Bank") return "[Bank]";
    if (location == "Material Storage") return "[Mats]";
    if (location == "Shared Inventory") return "[Shared]";
    return "[Char]"; // Character name
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: hSelf = hModule; break;
    case DLL_PROCESS_DETACH: break;
    case DLL_THREAD_ATTACH: break;
    case DLL_THREAD_DETACH: break;
    }
    return TRUE;
}

// Forward declarations
void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void ProcessKeybind(const char* aIdentifier, bool aIsRelease);
void AddonRender();
void AddonOptions();

// --- Render Helpers ---

// Base64 encoding table
static const char B64_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const unsigned char* data, size_t len) {
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = ((unsigned int)data[i]) << 16;
        if (i + 1 < len) n |= ((unsigned int)data[i + 1]) << 8;
        if (i + 2 < len) n |= (unsigned int)data[i + 2];
        out += B64_TABLE[(n >> 18) & 0x3F];
        out += B64_TABLE[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? B64_TABLE[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? B64_TABLE[n & 0x3F] : '=';
    }
    return out;
}

// Generate a GW2 item chat link: [&Base64Data]
// Binary: 0x02, count(1 byte, max 250), item_id(4 bytes LE), 0x00 0x00 0x00 0x00
static std::string MakeItemChatLink(uint32_t item_id, int count) {
    unsigned char buf[10] = {0};
    buf[0] = 0x02;
    buf[1] = (unsigned char)std::min(count, 250);
    buf[2] = (unsigned char)(item_id & 0xFF);
    buf[3] = (unsigned char)((item_id >> 8) & 0xFF);
    buf[4] = (unsigned char)((item_id >> 16) & 0xFF);
    buf[5] = (unsigned char)((item_id >> 24) & 0xFF);
    // bytes 6-9 are zero (no skin/upgrade)
    return "[&" + Base64Encode(buf, 10) + "]";
}

// Copy text to Windows clipboard
static void CopyToClipboard(const std::string& text) {
    if (!OpenClipboard(NULL)) return;
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hMem) {
        char* p = (char*)GlobalLock(hMem);
        if (p) {
            memcpy(p, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
    }
    CloseClipboard();
}

// Coin currency synthetic ID (currency ID 1)
static const uint32_t COIN_SYNTH_ID = HoardAndSeek::WALLET_ID_BASE | 1;

// Format copper value as gold string (gold only)
static std::string FormatCoin(int copper) {
    int gold = copper / 10000;
    return std::to_string(gold) + "g";
}

// Icon size for item icons in the results table
static const float ICON_SIZE = 20.0f;

// Helper to render an item icon with rarity border and tooltip
static void RenderItemIcon(uint32_t itemId, const std::string& iconUrl,
                           const std::string& name, const std::string& rarity,
                           const std::string& type, const std::string& description) {
    Texture_t* tex = HoardAndSeek::IconManager::GetIcon(itemId);
    if (tex && tex->Resource) {
        ImVec4 borderColor = GetRarityColor(rarity);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRect(
            ImVec2(pos.x - 1, pos.y - 1),
            ImVec2(pos.x + ICON_SIZE + 1, pos.y + ICON_SIZE + 1),
            ImGui::ColorConvertFloat4ToU32(borderColor), 0.0f, 0, 2.0f);
        ImGui::Image(tex->Resource, ImVec2(ICON_SIZE, ICON_SIZE));
    } else {
        // Placeholder colored square while loading
        ImVec4 col = GetRarityColor(rarity);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(
            pos, ImVec2(pos.x + ICON_SIZE, pos.y + ICON_SIZE),
            ImGui::ColorConvertFloat4ToU32(ImVec4(col.x * 0.3f, col.y * 0.3f, col.z * 0.3f, 0.5f)));
        ImGui::Dummy(ImVec2(ICON_SIZE, ICON_SIZE));

        // Request icon download
        if (!iconUrl.empty()) {
            HoardAndSeek::IconManager::RequestIcon(itemId, iconUrl);
        }
    }

    // Tooltip on hover
    if (ImGui::IsItemHovered()) {
        // Read latest description from cache (may have been fetched on-demand)
        std::string liveDesc = description;
        if (liveDesc.empty()) {
            const auto* info = HoardAndSeek::GW2API::GetItemInfo(itemId);
            if (info) liveDesc = info->description;
        }

        ImGui::BeginTooltip();
        // Show larger icon in tooltip if available
        if (tex && tex->Resource) {
            ImGui::Image(tex->Resource, ImVec2(48, 48));
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        ImGui::TextColored(GetRarityColor(rarity), "%s", name.c_str());
        if (!rarity.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", rarity.c_str());
        }
        if (!liveDesc.empty()) {
            ImGui::PushTextWrapPos(300.0f);
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", liveDesc.c_str());
            ImGui::PopTextWrapPos();
        } else if (itemId < HoardAndSeek::WALLET_ID_BASE) {
            // Trigger on-demand fetch for missing tooltip
            HoardAndSeek::GW2API::FetchTooltipAsync(itemId);
        }
        ImGui::EndGroup();
        ImGui::EndTooltip();
    }
}

// Location color helper
static ImVec4 GetLocationColor(const std::string& location) {
    if (location == "Bank")              return ImVec4(0.3f, 0.6f, 0.9f, 1.0f);
    if (location == "Material Storage")  return ImVec4(0.4f, 0.8f, 0.4f, 1.0f);
    if (location == "Shared Inventory")  return ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
    if (location == "Wallet")            return ImVec4(1.0f, 0.85f, 0.0f, 1.0f);
    if (location == "Legendary Armory")  return ImVec4(0.6f, 0.4f, 0.9f, 1.0f);
    return ImVec4(0.8f, 0.5f, 0.8f, 1.0f); // Character
}

static void RenderResultsList() {
    if (g_SearchResults.empty()) {
        if (strlen(g_SearchFilter) > 0 && (int)strlen(g_SearchFilter) >= g_MinSearchLength) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No items found.");
        }
        return;
    }

    // Results table with expandable tree rows
    if (ImGui::BeginTable("##results", 2,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
        ImVec2(0, ImGui::GetContentRegionAvail().y - 4)))
    {
        ImGui::TableSetupColumn("Item", ImGuiTableColumnFlags_WidthStretch, 4.0f);
        ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();

        int max_display = 200;
        int count = 0;
        for (const auto& result : g_SearchResults) {
            if (count++ >= max_display) break;

            ImGui::TableNextRow(0, ICON_SIZE + 4);
            ImGui::TableNextColumn();
            ImGui::PushID((int)result.item_id);

            // Render icon inline, then tree node on same line
            RenderItemIcon(result.item_id, result.icon_url, result.name, result.rarity, result.type, result.description);
            ImGui::SameLine();

            ImVec4 nameColor = GetRarityColor(result.rarity);
            ImGui::PushStyleColor(ImGuiCol_Text, nameColor);
            bool open = ImGui::TreeNodeEx(result.name.c_str(),
                ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                ImGuiTreeNodeFlags_OpenOnArrow);
            ImGui::PopStyleColor();

            // Right-click context menu: copy chat link (not for wallet currencies)
            if (result.item_id < HoardAndSeek::WALLET_ID_BASE) {
                if (ImGui::BeginPopupContextItem("##chatlink_ctx")) {
                    if (ImGui::MenuItem("Copy Chat Link")) {
                        std::string link = MakeItemChatLink(result.item_id, result.total_count);
                        CopyToClipboard(link);
                    }
                    ImGui::EndPopup();
                }
            }

            // Total count column
            ImGui::TableNextColumn();
            if (result.item_id == COIN_SYNTH_ID) {
                ImGui::Text("%s", FormatCoin(result.total_count).c_str());
            } else {
                ImGui::Text("%d", result.total_count);
            }

            // Expanded: show each location as its own row
            if (open) {
                for (const auto& loc : result.locations) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    // Indent the location text under the tree
                    ImGui::TreePush("loc");
                    ImVec4 locColor = GetLocationColor(loc.location);

                    std::string locLabel;
                    if (loc.sublocation.empty()) {
                        locLabel = loc.location;
                    } else {
                        locLabel = loc.location + " (" + loc.sublocation + ")";
                    }
                    ImGui::TextColored(locColor, "%s", locLabel.c_str());
                    ImGui::TreePop();

                    // Count column for this location
                    ImGui::TableNextColumn();
                    if (result.item_id == COIN_SYNTH_ID) {
                        ImGui::Text("%s", FormatCoin(loc.count).c_str());
                    } else {
                        ImGui::Text("x%d", loc.count);
                    }
                }
                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (count >= max_display && (int)g_SearchResults.size() > max_display) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "... and %d more (refine your search)",
                (int)g_SearchResults.size() - max_display);
        }

        ImGui::EndTable();
    }
}

// --- Cross-addon events ---

void OnSearchRequest(void* eventArgs) {
    if (!eventArgs) return;
    const char* name = (const char*)eventArgs;
    if (!name || name[0] == '\0') return;

    // Copy search term into filter buffer
    strncpy(g_SearchFilter, name, sizeof(g_SearchFilter) - 1);
    g_SearchFilter[sizeof(g_SearchFilter) - 1] = '\0';

    // Run the search
    std::string query(g_SearchFilter);
    if ((int)query.length() >= g_MinSearchLength) {
        g_SearchResults = HoardAndSeek::GW2API::SearchItems(query);
    }

    // Show the window
    g_WindowVisible = true;

    if (APIDefs) {
        std::string msg = "Search requested: " + std::string(name);
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", msg.c_str());
    }
}

void OnQueryItem(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryItemRequest*)eventArgs;
    if (req->api_version != HOARD_API_VERSION) return;
    if (req->response_event[0] == '\0') return;

    auto* resp = new HoardQueryItemResponse{};
    resp->api_version = HOARD_API_VERSION;
    resp->item_id = req->item_id;
    resp->total_count = 0;
    resp->location_count = 0;

    // Fill item info
    const auto* info = HoardAndSeek::GW2API::GetItemInfo(req->item_id);
    if (info) {
        strncpy(resp->name, info->name.c_str(), sizeof(resp->name) - 1);
        strncpy(resp->rarity, info->rarity.c_str(), sizeof(resp->rarity) - 1);
        strncpy(resp->type, info->type.c_str(), sizeof(resp->type) - 1);
    }

    // Fill locations
    auto locations = HoardAndSeek::GW2API::GetItemLocations(req->item_id);
    for (const auto& loc : locations) {
        resp->total_count += loc.count;
        if (resp->location_count < 32) {
            auto& entry = resp->locations[resp->location_count];
            strncpy(entry.location, loc.location.c_str(), sizeof(entry.location) - 1);
            strncpy(entry.sublocation, loc.sublocation.c_str(), sizeof(entry.sublocation) - 1);
            entry.count = loc.count;
            resp->location_count++;
        }
    }

    APIDefs->Events_Raise(req->response_event, resp);
    // Caller is responsible for freeing resp via delete
}

void OnQueryWallet(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryWalletRequest*)eventArgs;
    if (req->api_version != HOARD_API_VERSION) return;
    if (req->response_event[0] == '\0') return;

    auto* resp = new HoardQueryWalletResponse{};
    resp->api_version = HOARD_API_VERSION;
    resp->currency_id = req->currency_id;
    resp->found = false;
    resp->amount = 0;

    // Look up using synthetic wallet ID
    uint32_t synth_id = HoardAndSeek::WALLET_ID_BASE | req->currency_id;
    const auto* info = HoardAndSeek::GW2API::GetItemInfo(synth_id);
    if (info) {
        strncpy(resp->name, info->name.c_str(), sizeof(resp->name) - 1);
        resp->found = true;
    }

    resp->amount = HoardAndSeek::GW2API::GetTotalCount(synth_id);

    APIDefs->Events_Raise(req->response_event, resp);
    // Caller is responsible for freeing resp via delete
}

void OnQueryAchievement(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryAchievementRequest*)eventArgs;
    if (req->api_version != HOARD_API_VERSION) return;
    if (req->response_event[0] == '\0' || req->id_count == 0) return;

    // Copy request data for the thread
    std::vector<uint32_t> ids(req->ids, req->ids + std::min(req->id_count, (uint32_t)200));
    std::string response_event(req->response_event);

    std::thread([ids, response_event]() {
        auto* resp = new HoardQueryAchievementResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->entry_count = 0;

        // Build comma-separated ID list
        std::string ids_param;
        for (size_t i = 0; i < ids.size(); i++) {
            if (i > 0) ids_param += ",";
            ids_param += std::to_string(ids[i]);
        }

        std::string url = "https://api.guildwars2.com/v2/account/achievements?ids=" + ids_param;
        std::string raw = HoardAndSeek::GW2API::AuthenticatedGet(url);
        if (!raw.empty()) {
            try {
                auto j = nlohmann::json::parse(raw);
                if (j.is_array()) {
                    for (const auto& entry : j) {
                        if (resp->entry_count >= 200) break;
                        auto& e = resp->entries[resp->entry_count];
                        e.id = entry.value("id", (uint32_t)0);
                        e.current = entry.value("current", -1);
                        e.max = entry.value("max", -1);
                        e.done = entry.value("done", false);
                        e.bit_count = 0;
                        if (entry.contains("bits") && entry["bits"].is_array()) {
                            for (const auto& bit : entry["bits"]) {
                                if (e.bit_count >= 64) break;
                                e.bits[e.bit_count++] = bit.get<uint32_t>();
                            }
                        }
                        resp->entry_count++;
                    }
                }
            } catch (...) {}
        }

        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
        // Caller frees resp
    }).detach();
}

void OnQueryMastery(void* eventArgs) {
    if (!eventArgs || !APIDefs) return;
    auto* req = (HoardQueryMasteryRequest*)eventArgs;
    if (req->api_version != HOARD_API_VERSION) return;
    if (req->response_event[0] == '\0' || req->id_count == 0) return;

    // Copy request data for the thread
    std::vector<uint32_t> ids(req->ids, req->ids + std::min(req->id_count, (uint32_t)200));
    std::string response_event(req->response_event);

    std::thread([ids, response_event]() {
        auto* resp = new HoardQueryMasteryResponse{};
        resp->api_version = HOARD_API_VERSION;
        resp->entry_count = 0;

        // /v2/account/masteries returns all masteries; we filter to requested IDs
        std::string url = "https://api.guildwars2.com/v2/account/masteries";
        std::string raw = HoardAndSeek::GW2API::AuthenticatedGet(url);
        if (!raw.empty()) {
            try {
                auto j = nlohmann::json::parse(raw);
                if (j.is_array()) {
                    // Build lookup set
                    std::unordered_set<uint32_t> wanted(ids.begin(), ids.end());
                    for (const auto& entry : j) {
                        if (resp->entry_count >= 200) break;
                        uint32_t id = entry.value("id", (uint32_t)0);
                        if (wanted.count(id)) {
                            auto& e = resp->entries[resp->entry_count];
                            e.id = id;
                            e.level = entry.value("level", 0);
                            resp->entry_count++;
                        }
                    }
                }
            } catch (...) {}
        }

        if (APIDefs) APIDefs->Events_Raise(response_event.c_str(), resp);
        // Caller frees resp
    }).detach();
}

void OnPing(void* eventArgs) {
    if (!APIDefs) return;
    static HoardPongPayload pong{};
    pong.api_version = HOARD_API_VERSION;
    pong.last_updated = (int64_t)HoardAndSeek::GW2API::GetLastUpdated();
    pong.refresh_available_at = (int64_t)HoardAndSeek::GW2API::GetRefreshAvailableAt();
    pong.has_data = HoardAndSeek::GW2API::HasAccountData() ? 1 : 0;
    APIDefs->Events_Raise(EV_HOARD_PONG, &pong);
}

void OnRefreshRequest(void* eventArgs) {
    if (!APIDefs) return;
    auto status = HoardAndSeek::GW2API::GetFetchStatus();
    if (status == HoardAndSeek::FetchStatus::InProgress) return;
    if (HoardAndSeek::GW2API::IsRefreshOnCooldown()) {
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", "Refresh request ignored (cooldown)");
        return;
    }
    HoardAndSeek::GW2API::FetchAccountDataAsync();
    APIDefs->Log(LOGL_INFO, "HoardAndSeek", "Refresh triggered by external addon");
}

static void BroadcastDataUpdated() {
    if (!APIDefs) return;
    static HoardDataReadyPayload payload{};
    payload.api_version = HOARD_API_VERSION;
    // Count unique items and currencies from search
    // This is a lightweight broadcast, exact counts are informational
    payload.item_count = 0;
    payload.currency_count = 0;
    payload.last_updated = (int64_t)HoardAndSeek::GW2API::GetLastUpdated();
    payload.refresh_available_at = (int64_t)HoardAndSeek::GW2API::GetRefreshAvailableAt();
    APIDefs->Events_Raise(EV_HOARD_DATA_UPDATED, &payload);
}

// --- Addon Lifecycle ---

void AddonLoad(AddonAPI_t* aApi) {
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions((void* (*)(size_t, void*))APIDefs->ImguiMalloc,
                                 (void(*)(void*, void*))APIDefs->ImguiFree);

    // Initialize icon manager
    HoardAndSeek::IconManager::Initialize(APIDefs);

    // Load saved API key and account data
    if (HoardAndSeek::GW2API::LoadApiKey()) {
        const auto& key = HoardAndSeek::GW2API::GetApiKey();
        strncpy(g_ApiKeyBuf, key.c_str(), sizeof(g_ApiKeyBuf) - 1);
        APIDefs->Log(LOGL_INFO, "HoardAndSeek", "API key loaded from config");
        HoardAndSeek::GW2API::ValidateApiKeyAsync();
    }
    HoardAndSeek::GW2API::LoadAccountData();
    HoardAndSeek::GW2API::LoadTooltips();

    // Register render functions
    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);

    // Register keybind
    APIDefs->InputBinds_RegisterWithString("KB_HOARD_TOGGLE", ProcessKeybind, "CTRL+SHIFT+F");

    // Load icon textures from embedded PNG data
    APIDefs->Textures_LoadFromMemory(TEX_ICON, (void*)ICON_SEARCH, ICON_SEARCH_size, nullptr);
    APIDefs->Textures_LoadFromMemory(TEX_ICON_HOVER, (void*)ICON_SEARCH_HOVER, ICON_SEARCH_HOVER_size, nullptr);

    // Register quick access shortcut
    APIDefs->QuickAccess_Add(QA_ID, TEX_ICON, TEX_ICON_HOVER, "KB_HOARD_TOGGLE", "Hoard & Seek");

    // Register close-on-escape
    APIDefs->GUI_RegisterCloseOnEscape("Hoard & Seek", &g_WindowVisible);

    // Subscribe to cross-addon events
    APIDefs->Events_Subscribe(EV_HOARD_SEARCH, OnSearchRequest);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_ITEM, OnQueryItem);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_WALLET, OnQueryWallet);
    APIDefs->Events_Subscribe(EV_HOARD_REFRESH, OnRefreshRequest);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_ACHIEVEMENT, OnQueryAchievement);
    APIDefs->Events_Subscribe(EV_HOARD_QUERY_MASTERY, OnQueryMastery);
    APIDefs->Events_Subscribe(EV_HOARD_PING, OnPing);

    APIDefs->Log(LOGL_INFO, "HoardAndSeek", "Addon loaded successfully");
}

void AddonUnload() {
    HoardAndSeek::IconManager::Shutdown();

    APIDefs->Events_Unsubscribe(EV_HOARD_SEARCH, OnSearchRequest);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_ITEM, OnQueryItem);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_WALLET, OnQueryWallet);
    APIDefs->Events_Unsubscribe(EV_HOARD_REFRESH, OnRefreshRequest);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_ACHIEVEMENT, OnQueryAchievement);
    APIDefs->Events_Unsubscribe(EV_HOARD_QUERY_MASTERY, OnQueryMastery);
    APIDefs->Events_Unsubscribe(EV_HOARD_PING, OnPing);
    APIDefs->GUI_DeregisterCloseOnEscape("Hoard & Seek");
    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->GUI_Deregister(AddonRender);

    APIDefs = nullptr;
}

void ProcessKeybind(const char* aIdentifier, bool aIsRelease) {
    if (aIsRelease) return;

    if (strcmp(aIdentifier, "KB_HOARD_TOGGLE") == 0) {
        g_WindowVisible = !g_WindowVisible;
        if (APIDefs) {
            APIDefs->Log(LOGL_INFO, "HoardAndSeek",
                g_WindowVisible ? "Window shown" : "Window hidden");
        }
    }
}

// --- Main Render ---

void AddonRender() {
    // Process icon download queue every frame (even when window hidden)
    HoardAndSeek::IconManager::Tick();

    // Broadcast fetch progress and data-updated events
    {
        static HoardAndSeek::FetchStatus s_prevFetchStatus = HoardAndSeek::FetchStatus::Idle;
        static std::string s_prevFetchMessage;
        auto curStatus = HoardAndSeek::GW2API::GetFetchStatus();
        std::string curMessage = HoardAndSeek::GW2API::GetFetchStatusMessage();

        // Broadcast progress updates during fetch
        if (curStatus == HoardAndSeek::FetchStatus::InProgress && curMessage != s_prevFetchMessage) {
            static HoardFetchProgressPayload progress{};
            progress.api_version = HOARD_API_VERSION;
            strncpy(progress.message, curMessage.c_str(), sizeof(progress.message) - 1);
            progress.message[sizeof(progress.message) - 1] = '\0';
            progress.progress = -1.0f;
            APIDefs->Events_Raise(EV_HOARD_FETCH_PROGRESS, &progress);
        }
        s_prevFetchMessage = curMessage;

        // Broadcast completion
        if (curStatus == HoardAndSeek::FetchStatus::Success &&
            s_prevFetchStatus != HoardAndSeek::FetchStatus::Success) {
            BroadcastDataUpdated();
        }

        // Broadcast error
        if (curStatus == HoardAndSeek::FetchStatus::Error &&
            s_prevFetchStatus != HoardAndSeek::FetchStatus::Error) {
            static HoardFetchErrorPayload errPayload{};
            errPayload.api_version = HOARD_API_VERSION;
            strncpy(errPayload.message, curMessage.c_str(), sizeof(errPayload.message) - 1);
            errPayload.message[sizeof(errPayload.message) - 1] = '\0';
            APIDefs->Events_Raise(EV_HOARD_FETCH_ERROR, &errPayload);
        }

        s_prevFetchStatus = curStatus;
    }

    if (!g_WindowVisible) return;

    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 150), ImVec2(FLT_MAX, FLT_MAX));
    if (!ImGui::Begin("Hoard & Seek", &g_WindowVisible,
        ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    // Row 1: Refresh button + status message (always on same line)
    auto fetchStatus = HoardAndSeek::GW2API::GetFetchStatus();
    bool scanning = (fetchStatus == HoardAndSeek::FetchStatus::InProgress);
    bool onCooldown = HoardAndSeek::GW2API::IsRefreshOnCooldown();
    bool disabled = scanning || onCooldown;

    if (disabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    if (ImGui::Button("Refresh Account Data") && !disabled) {
        HoardAndSeek::GW2API::FetchAccountDataAsync();
    }
    if (disabled) ImGui::PopStyleVar();

    // Timer flags for transient status messages (must be file-scope statics so InProgress can reset them)
    static bool s_successTimerStarted = false;
    static bool s_successExpired = false;
    static auto s_successTime = std::chrono::steady_clock::now();
    static bool s_errorTimerStarted = false;
    static bool s_errorExpired = false;
    static auto s_errorTime = std::chrono::steady_clock::now();

    ImGui::SameLine();
    if (fetchStatus == HoardAndSeek::FetchStatus::InProgress) {
        // Reset timers so next success/error shows fresh
        s_successTimerStarted = false;
        s_errorTimerStarted = false;
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "%s",
            HoardAndSeek::GW2API::GetFetchStatusMessage().c_str());
    } else if (fetchStatus == HoardAndSeek::FetchStatus::Error) {
        if (!s_errorTimerStarted) {
            s_errorTime = std::chrono::steady_clock::now();
            s_errorTimerStarted = true;
            s_errorExpired = false;
        }
        if (!s_errorExpired) {
            auto errElapsed = std::chrono::steady_clock::now() - s_errorTime;
            if (std::chrono::duration_cast<std::chrono::seconds>(errElapsed).count() >= 5) {
                s_errorExpired = true;
            }
        }
        if (!s_errorExpired) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
                HoardAndSeek::GW2API::GetFetchStatusMessage().c_str());
        } else {
            time_t last = HoardAndSeek::GW2API::GetLastUpdated();
            if (last > 0) {
                time_t now = std::time(nullptr);
                int el = (int)difftime(now, last);
                std::string ago;
                if (el < 60) ago = "just now";
                else if (el < 3600) ago = std::to_string(el / 60) + "m ago";
                else if (el < 86400) ago = std::to_string(el / 3600) + "h ago";
                else ago = std::to_string(el / 86400) + "d ago";
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Last updated %s", ago.c_str());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s",
                    HoardAndSeek::GW2API::GetFetchStatusMessage().c_str());
            }
        }
    } else if (fetchStatus == HoardAndSeek::FetchStatus::Success) {
        // Show green "Done" message for 5s, then show "Last updated"
        if (!s_successTimerStarted) {
            s_successTime = std::chrono::steady_clock::now();
            s_successTimerStarted = true;
            s_successExpired = false;
        }
        if (!s_successExpired) {
            auto elapsed = std::chrono::steady_clock::now() - s_successTime;
            if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() >= 5) {
                s_successExpired = true;
            }
        }
        if (!s_successExpired) {
            ImGui::TextColored(ImVec4(0.35f, 0.82f, 0.35f, 1.0f), "%s",
                HoardAndSeek::GW2API::GetFetchStatusMessage().c_str());
        } else {
            time_t last = HoardAndSeek::GW2API::GetLastUpdated();
            if (last > 0) {
                time_t now = std::time(nullptr);
                int el = (int)difftime(now, last);
                std::string ago;
                if (el < 60) ago = "just now";
                else if (el < 3600) ago = std::to_string(el / 60) + "m ago";
                else if (el < 86400) ago = std::to_string(el / 3600) + "h ago";
                else ago = std::to_string(el / 86400) + "d ago";
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Last updated %s", ago.c_str());
            } else {
                ImGui::TextUnformatted("");
            }
        }
    } else if (!HoardAndSeek::GW2API::HasAccountData()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "No data. Set API key in settings first.");
    } else if (HoardAndSeek::GW2API::HasAccountData()) {
        time_t last = HoardAndSeek::GW2API::GetLastUpdated();
        if (last > 0) {
            time_t now = std::time(nullptr);
            int elapsed = (int)difftime(now, last);
            std::string ago;
            if (elapsed < 60) ago = "just now";
            else if (elapsed < 3600) ago = std::to_string(elapsed / 60) + "m ago";
            else if (elapsed < 86400) ago = std::to_string(elapsed / 3600) + "h ago";
            else ago = std::to_string(elapsed / 86400) + "d ago";
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Last updated %s", ago.c_str());
        } else {
            ImGui::TextUnformatted("");
        }
    } else {
        ImGui::TextUnformatted("");
    }

    // Row 2: Search bar (always in a fixed position)
    ImGui::PushItemWidth(-1);
    bool searchChanged = ImGui::InputTextWithHint("##search", "Search items (min 3 chars)...",
        g_SearchFilter, sizeof(g_SearchFilter));
    ImGui::PopItemWidth();

    if (searchChanged) {
        std::string query(g_SearchFilter);
        if ((int)query.length() >= g_MinSearchLength) {
            g_SearchResults = HoardAndSeek::GW2API::SearchItems(query);
        } else {
            g_SearchResults.clear();
        }
    }

    ImGui::Separator();

    // Results
    RenderResultsList();

    ImGui::End();
}

// --- Options/Settings Render ---

void AddonOptions() {
    ImGui::Text("Hoard & Seek Settings");
    ImGui::Separator();

    // API Key section
    ImGui::Text("GW2 API Key:");
    ImGui::SameLine();
    if (ImGui::SmallButton(g_ShowApiKey ? "Hide" : "Show")) {
        g_ShowApiKey = !g_ShowApiKey;
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
    if (!g_ShowApiKey) flags |= ImGuiInputTextFlags_Password;
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##apikey", "Paste API key here...", g_ApiKeyBuf, sizeof(g_ApiKeyBuf), flags);
    ImGui::PopItemWidth();

    ImGui::Text("Required permissions: account, inventories, characters");

    auto valStatus = HoardAndSeek::GW2API::GetValidationStatus();
    bool validating = (valStatus == HoardAndSeek::FetchStatus::InProgress);
    static bool pendingSave = false;

    if (validating) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    if (ImGui::Button("Save Key") && !validating) {
        HoardAndSeek::GW2API::SetApiKey(std::string(g_ApiKeyBuf));
        HoardAndSeek::GW2API::ValidateApiKeyAsync();
        pendingSave = true;
    }
    if (validating) ImGui::PopStyleVar();

    // Auto-save when validation succeeds
    if (pendingSave && !validating) {
        if (valStatus == HoardAndSeek::FetchStatus::Success) {
            HoardAndSeek::GW2API::SaveApiKey();
            pendingSave = false;
        } else if (valStatus == HoardAndSeek::FetchStatus::Error) {
            pendingSave = false;
        }
    }

    // Validation status
    if (validating) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Validating...");
    } else if (valStatus == HoardAndSeek::FetchStatus::Error) {
        const auto& info = HoardAndSeek::GW2API::GetApiKeyInfo();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid: %s", info.error.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Retry")) {
            HoardAndSeek::GW2API::SetApiKey(std::string(g_ApiKeyBuf));
            HoardAndSeek::GW2API::ValidateApiKeyAsync();
            pendingSave = true;
        }
    }

    // Show key info when valid
    const auto& info = HoardAndSeek::GW2API::GetApiKeyInfo();
    if (info.valid) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.35f, 0.82f, 0.35f, 1.0f), "Valid");
        ImGui::Text("Account: %s", info.account_name.c_str());
        ImGui::Text("Key Name: %s", info.key_name.c_str());

        std::string perms;
        for (size_t i = 0; i < info.permissions.size(); i++) {
            if (i > 0) perms += ", ";
            perms += info.permissions[i];
        }
        ImGui::Text("Scopes: %s", perms.c_str());

        // Check for required permissions
        bool hasAccount = false, hasInventories = false, hasCharacters = false;
        for (const auto& p : info.permissions) {
            if (p == "account") hasAccount = true;
            if (p == "inventories") hasInventories = true;
            if (p == "characters") hasCharacters = true;
        }
        if (!hasAccount || !hasInventories || !hasCharacters) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                "Warning: Missing required permissions. Need: account, inventories, characters");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Search Settings:");
    ImGui::SliderInt("Min search length", &g_MinSearchLength, 1, 5);
}

// --- Export: GetAddonDef ---

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature = 0x5f45d669;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Hoard & Seek";
    AddonDef.Version.Major = V_MAJOR;
    AddonDef.Version.Minor = V_MINOR;
    AddonDef.Version.Build = V_BUILD;
    AddonDef.Version.Revision = V_REVISION;
    AddonDef.Author = "PieOrCake";
    AddonDef.Description = "Find where your items are hiding across your GW2 account";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/PieOrCake/hoard_and_seek";

    return &AddonDef;
}

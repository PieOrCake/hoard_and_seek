#include "PermissionManager.h"
#include "GW2API.h"
#include "../include/HoardAndSeekAPI.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

namespace HoardAndSeek {

    std::unordered_map<std::string, std::unordered_map<std::string, PermissionState>> PermissionManager::s_permissions;
    std::mutex PermissionManager::s_mutex;

    static const struct {
        const char* event_name;
        const char* description;
    } s_event_descriptions[] = {
        { EV_HOARD_QUERY_ITEM,          "Query item locations and counts" },
        { EV_HOARD_QUERY_WALLET,        "Query wallet currency balances" },
        { EV_HOARD_QUERY_ACHIEVEMENT,   "Query achievement progress" },
        { EV_HOARD_QUERY_MASTERY,       "Query mastery levels" },
        { EV_HOARD_QUERY_SKINS,         "Query skin unlock status" },
        { EV_HOARD_QUERY_RECIPES,       "Query recipe unlock status" },
        { EV_HOARD_QUERY_WIZARDSVAULT,  "Query Wizard's Vault progress" },
        { EV_HOARD_QUERY_API,           "Generic API proxy (any endpoint)" },
        { EV_HOARD_CONTEXT_MENU_REGISTER, "Register right-click context menu items" },
        { EV_HOARD_QUERY_ACCOUNTS,       "Query configured account list" },
    };

    void PermissionManager::Init() {
        Load();
    }

    const char* PermissionManager::GetEventDescription(const std::string& event_name) {
        for (const auto& desc : s_event_descriptions) {
            if (event_name == desc.event_name) return desc.description;
        }
        return "Unknown query";
    }

    PermissionState PermissionManager::Check(const std::string& requester, const std::string& event_name) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_permissions.find(requester);
        if (it == s_permissions.end()) return PermissionState::Unknown;
        auto ev_it = it->second.find(event_name);
        if (ev_it == it->second.end()) return PermissionState::Unknown;
        return ev_it->second;
    }

    PermissionState PermissionManager::CheckOrAutoAllow(const std::string& requester, const std::string& event_name) {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            auto it = s_permissions.find(requester);
            if (it != s_permissions.end()) {
                auto ev_it = it->second.find(event_name);
                if (ev_it != it->second.end()) return ev_it->second;
            }
            // Unknown requester — default-allow and record it.
            s_permissions[requester][event_name] = PermissionState::Allowed;
        }
        Save();
        return PermissionState::Allowed;
    }

    void PermissionManager::Grant(const std::string& requester, const std::string& event_name) {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_permissions[requester][event_name] = PermissionState::Allowed;
        }
        Save();
    }

    void PermissionManager::Deny(const std::string& requester, const std::string& event_name) {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_permissions[requester][event_name] = PermissionState::Denied;
        }
        Save();
    }

    void PermissionManager::Revoke(const std::string& requester, const std::string& event_name) {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            auto it = s_permissions.find(requester);
            if (it != s_permissions.end()) {
                it->second.erase(event_name);
                if (it->second.empty()) {
                    s_permissions.erase(it);
                }
            }
        }
        Save();
    }

    void PermissionManager::RenderSettings() {
        bool needs_save = false;

        {
            std::lock_guard<std::mutex> lock(s_mutex);

            if (s_permissions.empty()) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No addons have queried Hoard & Seek yet.");
                return;
            }

            std::string addon_to_remove;

            for (auto& [requester, events] : s_permissions) {
                bool tree_open = ImGui::TreeNode(requester.c_str());

                // Remove button on same line as tree node
                ImGui::SameLine();
                std::string remove_id = "Remove##" + requester;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::SmallButton(remove_id.c_str())) {
                    addon_to_remove = requester;
                }
                ImGui::PopStyleColor(2);

                if (tree_open) {
                    // Show all known events with checkboxes
                    for (const auto& desc : s_event_descriptions) {
                        bool allowed = false;
                        auto ev_it = events.find(desc.event_name);
                        if (ev_it != events.end()) {
                            allowed = (ev_it->second == PermissionState::Allowed);
                        }

                        std::string label = std::string(desc.description) + "##" + requester + desc.event_name;
                        if (ImGui::Checkbox(label.c_str(), &allowed)) {
                            events[desc.event_name] = allowed ? PermissionState::Allowed : PermissionState::Denied;
                            needs_save = true;
                        }
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%s)", desc.event_name);
                    }
                    ImGui::TreePop();
                }
            }

            // Remove addon outside iteration
            if (!addon_to_remove.empty()) {
                s_permissions.erase(addon_to_remove);
                needs_save = true;
            }
        } // lock released

        if (needs_save) {
            Save();
        }
    }

    std::unordered_map<std::string, std::unordered_map<std::string, PermissionState>> PermissionManager::GetAll() {
        std::lock_guard<std::mutex> lock(s_mutex);
        return s_permissions;
    }

    bool PermissionManager::Load() {
        std::string path = GW2API::GetDataDirectory() + "/permissions.json";
        std::ifstream file(path);
        if (!file.is_open()) return false;

        try {
            json j;
            file >> j;

            std::lock_guard<std::mutex> lock(s_mutex);
            s_permissions.clear();

            if (j.is_object()) {
                for (auto& [requester, events_json] : j.items()) {
                    if (!events_json.is_object()) continue;
                    for (auto& [event_name, state_str] : events_json.items()) {
                        std::string s = state_str.get<std::string>();
                        PermissionState state = PermissionState::Unknown;
                        if (s == "allow") state = PermissionState::Allowed;
                        else if (s == "deny") state = PermissionState::Denied;
                        if (state != PermissionState::Unknown) {
                            s_permissions[requester][event_name] = state;
                        }
                    }
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    bool PermissionManager::Save() {
        std::string path = GW2API::GetDataDirectory() + "/permissions.json";

        try {
            json j = json::object();
            {
                std::lock_guard<std::mutex> lock(s_mutex);
                for (const auto& [requester, events] : s_permissions) {
                    json ev = json::object();
                    for (const auto& [event_name, state] : events) {
                        if (state == PermissionState::Allowed) ev[event_name] = "allow";
                        else if (state == PermissionState::Denied) ev[event_name] = "deny";
                    }
                    if (!ev.empty()) j[requester] = ev;
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

}

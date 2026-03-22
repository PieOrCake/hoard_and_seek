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
    std::vector<PendingPermission> PermissionManager::s_pending;
    bool PermissionManager::s_showing_popup = false;
    std::string PermissionManager::s_current_requester;
    std::vector<PermissionManager::PromptEntry> PermissionManager::s_current_entries;
    std::mutex PermissionManager::s_mutex;
    std::chrono::steady_clock::time_point PermissionManager::s_first_pending_time;
    bool PermissionManager::s_collecting = false;

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

    bool PermissionManager::RequestPermission(const std::string& requester, const std::string& event_name) {
        std::lock_guard<std::mutex> lock(s_mutex);

        // Already decided
        auto it = s_permissions.find(requester);
        if (it != s_permissions.end() && it->second.count(event_name)) {
            return false;
        }

        // Already queued
        for (const auto& p : s_pending) {
            if (p.requester == requester && p.event_name == event_name) {
                return false;
            }
        }

        // Also check if it's in the current batch popup
        if (s_showing_popup && s_current_requester == requester) {
            for (const auto& e : s_current_entries) {
                if (e.event_name == event_name) return false;
            }
            // Same requester, new event — add to current popup
            PromptEntry entry;
            entry.event_name = event_name;
            entry.description = GetEventDescription(event_name);
            entry.checked = true;
            s_current_entries.push_back(entry);
            return true;
        }

        PendingPermission pp;
        pp.requester = requester;
        pp.event_name = event_name;
        pp.description = GetEventDescription(event_name);
        s_pending.push_back(pp);

        // Start collection timer on first pending item
        if (!s_collecting) {
            s_first_pending_time = std::chrono::steady_clock::now();
            s_collecting = true;
        }
        return true;
    }

    bool PermissionManager::RenderPopup() {
        // Collect all pending for the first requester if not currently showing
        if (!s_showing_popup) {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (s_pending.empty()) {
                s_collecting = false;
                return false;
            }

            // Wait 500ms after first pending item to let all requests accumulate
            if (s_collecting) {
                auto elapsed = std::chrono::steady_clock::now() - s_first_pending_time;
                if (elapsed < std::chrono::milliseconds(500)) {
                    return false; // Still collecting
                }
                s_collecting = false;
            }

            // Group by first requester found
            s_current_requester = s_pending.front().requester;
            s_current_entries.clear();

            auto it = s_pending.begin();
            while (it != s_pending.end()) {
                if (it->requester == s_current_requester) {
                    PromptEntry entry;
                    entry.event_name = it->event_name;
                    entry.description = it->description;
                    entry.checked = true; // default to allowed
                    s_current_entries.push_back(entry);
                    it = s_pending.erase(it);
                } else {
                    ++it;
                }
            }

            s_showing_popup = true;
            ImGui::OpenPopup("H&S Permission Request");
        }

        bool still_open = true;
        ImVec2 display = ImGui::GetIO().DisplaySize;
        ImVec2 center(display.x * 0.5f, display.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("H&S Permission Request", &still_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Addon requesting access to your account data:");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.0f, 1.0f));
            ImGui::Text("%s", s_current_requester.c_str());
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("Permissions requested:");
            ImGui::Spacing();

            for (size_t i = 0; i < s_current_entries.size(); i++) {
                auto& entry = s_current_entries[i];
                std::string label = entry.description + "##perm" + std::to_string(i);
                ImGui::Checkbox(label.c_str(), &entry.checked);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%s)", entry.event_name.c_str());
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float button_width = 120.0f;
            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - button_width) * 0.5f);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.7f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.8f, 1.0f));
            if (ImGui::Button("Confirm", ImVec2(button_width, 0))) {
                for (const auto& entry : s_current_entries) {
                    if (entry.checked) {
                        Grant(s_current_requester, entry.event_name);
                    } else {
                        Deny(s_current_requester, entry.event_name);
                    }
                }
                s_current_entries.clear();
                s_showing_popup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);

            ImGui::EndPopup();
        }

        // User closed via X button — deny all unchecked
        if (!still_open) {
            for (const auto& entry : s_current_entries) {
                Deny(s_current_requester, entry.event_name);
            }
            s_current_entries.clear();
            s_showing_popup = false;
        }

        return s_showing_popup;
    }

    void PermissionManager::RenderSettings() {
        std::lock_guard<std::mutex> lock(s_mutex);

        if (s_permissions.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No addon permissions have been set yet.");
            return;
        }

        // Collect items to revoke after iteration
        std::vector<std::pair<std::string, std::string>> to_revoke;

        for (auto& [requester, events] : s_permissions) {
            if (ImGui::TreeNode(requester.c_str())) {
                for (auto& [event_name, state] : events) {
                    const char* desc = GetEventDescription(event_name);
                    const char* state_str = (state == PermissionState::Allowed) ? "Allowed" : "Denied";
                    ImVec4 color = (state == PermissionState::Allowed)
                        ? ImVec4(0.35f, 0.82f, 0.35f, 1.0f)
                        : ImVec4(0.82f, 0.35f, 0.35f, 1.0f);

                    ImGui::TextColored(color, "[%s]", state_str);
                    ImGui::SameLine();
                    ImGui::Text("%s", desc);
                    ImGui::SameLine();
                    std::string btn_id = "Revoke##" + requester + event_name;
                    if (ImGui::SmallButton(btn_id.c_str())) {
                        to_revoke.push_back({requester, event_name});
                    }
                }
                ImGui::TreePop();
            }
        }

        // Apply revocations outside iteration
        if (!to_revoke.empty()) {
            for (auto& [req, ev] : to_revoke) {
                auto it = s_permissions.find(req);
                if (it != s_permissions.end()) {
                    it->second.erase(ev);
                    if (it->second.empty()) {
                        s_permissions.erase(it);
                    }
                }
            }
            // Save without lock (we already hold it) — defer to after unlock
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

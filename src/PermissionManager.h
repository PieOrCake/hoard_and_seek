#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <chrono>

namespace HoardAndSeek {

    enum class PermissionState {
        Unknown,    // Not yet decided — will trigger popup
        Allowed,
        Denied
    };

    struct PendingPermission {
        std::string requester;
        std::string event_name;
        std::string description;
    };

    class PermissionManager {
    public:
        static void Init();

        // Check if requester has permission for the given event.
        // Returns Allowed/Denied/Unknown.
        static PermissionState Check(const std::string& requester, const std::string& event_name);

        // Grant or deny permission and persist to disk.
        static void Grant(const std::string& requester, const std::string& event_name);
        static void Deny(const std::string& requester, const std::string& event_name);
        static void Revoke(const std::string& requester, const std::string& event_name);

        // Queue a permission prompt for the render thread.
        // Returns false if already queued/decided.
        static bool RequestPermission(const std::string& requester, const std::string& event_name);

        // Called from the ImGui render loop to show permission popups.
        // Returns true if a popup is currently being displayed.
        static bool RenderPopup();

        // Called from the settings panel to render permission management UI.
        static void RenderSettings();

        // Get human-readable description for an event name.
        static const char* GetEventDescription(const std::string& event_name);

        // Get all stored permissions: map<requester, map<event, state>>
        static std::unordered_map<std::string, std::unordered_map<std::string, PermissionState>> GetAll();

        // Persistence
        static bool Load();
        static bool Save();

        // A single entry in the batch popup, with a checkbox state
        struct PromptEntry {
            std::string event_name;
            std::string description;
            bool checked;               // true = allow, false = deny
        };

    private:
        // requester -> (event_name -> state)
        static std::unordered_map<std::string, std::unordered_map<std::string, PermissionState>> s_permissions;
        static std::vector<PendingPermission> s_pending;
        static bool s_showing_popup;
        static std::string s_current_requester;
        static std::vector<PromptEntry> s_current_entries;
        static std::mutex s_mutex;
        static std::chrono::steady_clock::time_point s_first_pending_time;
        static bool s_collecting;   // true while waiting for collection window
    };

}

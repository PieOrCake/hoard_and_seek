#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

namespace HoardAndSeek {

    enum class PermissionState {
        Unknown,    // Not yet decided
        Allowed,
        Denied
    };

    class PermissionManager {
    public:
        static void Init();

        // Check if requester has permission for the given event.
        // Returns Allowed/Denied/Unknown without mutating state.
        static PermissionState Check(const std::string& requester, const std::string& event_name);

        // Effective permission check used by the cross-addon API.
        // Default-allow: an unknown requester is granted automatically and
        // persisted so it appears in the settings panel, where the user can
        // later deny it. Returns Allowed or Denied.
        static PermissionState CheckOrAutoAllow(const std::string& requester, const std::string& event_name);

        // Grant or deny permission and persist to disk.
        static void Grant(const std::string& requester, const std::string& event_name);
        static void Deny(const std::string& requester, const std::string& event_name);
        static void Revoke(const std::string& requester, const std::string& event_name);

        // Called from the settings panel to render permission management UI.
        static void RenderSettings();

        // Get human-readable description for an event name.
        static const char* GetEventDescription(const std::string& event_name);

        // Get all stored permissions: map<requester, map<event, state>>
        static std::unordered_map<std::string, std::unordered_map<std::string, PermissionState>> GetAll();

        // Persistence
        static bool Load();
        static bool Save();

    private:
        // requester -> (event_name -> state)
        static std::unordered_map<std::string, std::unordered_map<std::string, PermissionState>> s_permissions;
        static std::mutex s_mutex;
    };

}

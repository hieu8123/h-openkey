//
//  ToplevelWatcher.cpp
//  OpenKey cho Linux
//

#include "ToplevelWatcher.h"

#include <wayland-client.h>

#include "cosmic-toplevel-info-unstable-v1-client-protocol.h"
#include "ext-foreign-toplevel-list-v1-client-protocol.h"
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

namespace openkey {
namespace {

ToplevelWatcher* watcherOf(void* data) { return static_cast<ToplevelWatcher*>(data); }

} // namespace

ToplevelWatcher::~ToplevelWatcher() { stop(); }

bool ToplevelWatcher::start(ext_foreign_toplevel_list_v1* list,
                            zcosmic_toplevel_info_v1* info,
                            zwlr_foreign_toplevel_manager_v1* wlr) {
    // Uu tien duong wlr: mot giao thuc la du, va no pho bien hon.
    if (wlr) {
        _wlr = wlr;
        static const zwlr_foreign_toplevel_manager_v1_listener managerListener = {
            onWlrToplevel,
            /* finished */ [](void*, zwlr_foreign_toplevel_manager_v1*) {},
        };
        zwlr_foreign_toplevel_manager_v1_add_listener(_wlr, &managerListener, this);
        _mode = "wlr-foreign-toplevel";
        return true;
    }

    if (list && info) {
        _list = list;
        _info = info;
        static const ext_foreign_toplevel_list_v1_listener listener = {onToplevel,
                                                                      onListFinished};
        ext_foreign_toplevel_list_v1_add_listener(_list, &listener, this);
        _mode = "ext-foreign-toplevel + cosmic-toplevel-info";
        return true;
    }

    return false;
}

void ToplevelWatcher::onWlrToplevel(void* data, zwlr_foreign_toplevel_manager_v1*,
                                    zwlr_foreign_toplevel_handle_v1* handle) {
    auto* self = watcherOf(data);
    static const zwlr_foreign_toplevel_handle_v1_listener listener = {
        /* title */ [](void*, zwlr_foreign_toplevel_handle_v1*, const char*) {},
        onWlrAppId,
        /* output_enter */ [](void*, zwlr_foreign_toplevel_handle_v1*, wl_output*) {},
        /* output_leave */ [](void*, zwlr_foreign_toplevel_handle_v1*, wl_output*) {},
        onWlrState,
        /* done */ [](void*, zwlr_foreign_toplevel_handle_v1*) {},
        onWlrClosed,
        /* parent */
        [](void*, zwlr_foreign_toplevel_handle_v1*, zwlr_foreign_toplevel_handle_v1*) {},
    };
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &listener, data);
    self->_wlrToplevels.emplace(handle, WlrToplevel{});
}

void ToplevelWatcher::onWlrAppId(void* data, zwlr_foreign_toplevel_handle_v1* handle,
                                 const char* appId) {
    auto* self = watcherOf(data);
    auto it = self->_wlrToplevels.find(handle);
    if (it == self->_wlrToplevels.end()) {
        return;
    }
    it->second.appId = appId ? appId : "";
    // app-id thuong toi sau su kien state, nen phai bao lai o day.
    if (it->second.activated) {
        self->publishFocus(it->second.appId);
    }
}

void ToplevelWatcher::onWlrState(void* data, zwlr_foreign_toplevel_handle_v1* handle,
                                 wl_array* state) {
    auto* self = watcherOf(data);
    auto it = self->_wlrToplevels.find(handle);
    if (it == self->_wlrToplevels.end()) {
        return;
    }

    bool activated = false;
    const uint32_t* values = static_cast<const uint32_t*>(state->data);
    for (size_t i = 0; i < state->size / sizeof(uint32_t); i++) {
        if (values[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
            activated = true;
            break;
        }
    }

    it->second.activated = activated;
    if (activated) {
        self->publishFocus(it->second.appId);
    }
}

void ToplevelWatcher::onWlrClosed(void* data, zwlr_foreign_toplevel_handle_v1* handle) {
    auto* self = watcherOf(data);
    auto it = self->_wlrToplevels.find(handle);
    if (it == self->_wlrToplevels.end()) {
        return;
    }
    const bool wasFocused = it->second.activated;
    self->_wlrToplevels.erase(it);
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
    if (wasFocused) {
        self->publishFocus(std::string());
    }
}

void ToplevelWatcher::stop() {
    for (auto& [handle, toplevel] : _toplevels) {
        if (toplevel.cosmic) {
            zcosmic_toplevel_handle_v1_destroy(toplevel.cosmic);
        }
        ext_foreign_toplevel_handle_v1_destroy(handle);
    }
    _toplevels.clear();

    for (auto& [handle, _] : _wlrToplevels) {
        zwlr_foreign_toplevel_handle_v1_destroy(handle);
    }
    _wlrToplevels.clear();
    if (_wlr) {
        zwlr_foreign_toplevel_manager_v1_stop(_wlr);
        _wlr = nullptr;
    }

    if (_list) {
        ext_foreign_toplevel_list_v1_destroy(_list);
        _list = nullptr;
    }
    _info = nullptr;
}

ToplevelWatcher::Toplevel* ToplevelWatcher::findByExt(
    ext_foreign_toplevel_handle_v1* handle) {
    auto it = _toplevels.find(handle);
    return it == _toplevels.end() ? nullptr : &it->second;
}

ToplevelWatcher::Toplevel* ToplevelWatcher::findByCosmic(
    zcosmic_toplevel_handle_v1* handle) {
    for (auto& [_, toplevel] : _toplevels) {
        if (toplevel.cosmic == handle) {
            return &toplevel;
        }
    }
    return nullptr;
}

void ToplevelWatcher::publishFocus(const std::string& appId) {
    if (appId == _focusedAppId) {
        return;
    }
    _focusedAppId = appId;
    if (onFocusChanged) {
        onFocusChanged(appId);
    }
}

void ToplevelWatcher::onToplevel(void* data, ext_foreign_toplevel_list_v1*,
                                 ext_foreign_toplevel_handle_v1* handle) {
    auto* self = watcherOf(data);

    static const ext_foreign_toplevel_handle_v1_listener handleListener = {
        onHandleClosed, onHandleDone, onHandleTitle, onHandleAppId, onHandleIdentifier};
    ext_foreign_toplevel_handle_v1_add_listener(handle, &handleListener, data);

    Toplevel toplevel;
    if (self->_info) {
        toplevel.cosmic = zcosmic_toplevel_info_v1_get_cosmic_toplevel(self->_info, handle);
        if (toplevel.cosmic) {
            static const zcosmic_toplevel_handle_v1_listener cosmicListener = {
                /* closed */ [](void*, zcosmic_toplevel_handle_v1*) {},
                /* done */ [](void*, zcosmic_toplevel_handle_v1*) {},
                /* title */ [](void*, zcosmic_toplevel_handle_v1*, const char*) {},
                /* app_id */ [](void*, zcosmic_toplevel_handle_v1*, const char*) {},
                /* output_enter */ [](void*, zcosmic_toplevel_handle_v1*, wl_output*) {},
                /* output_leave */ [](void*, zcosmic_toplevel_handle_v1*, wl_output*) {},
                /* workspace_enter */
                [](void*, zcosmic_toplevel_handle_v1*, zcosmic_workspace_handle_v1*) {},
                /* workspace_leave */
                [](void*, zcosmic_toplevel_handle_v1*, zcosmic_workspace_handle_v1*) {},
                /* state */ onCosmicState,
                /* geometry */
                [](void*, zcosmic_toplevel_handle_v1*, wl_output*, int32_t, int32_t,
                   int32_t, int32_t) {},
                /* ext_workspace_enter */
                [](void*, zcosmic_toplevel_handle_v1*, ext_workspace_handle_v1*) {},
                /* ext_workspace_leave */
                [](void*, zcosmic_toplevel_handle_v1*, ext_workspace_handle_v1*) {},
            };
            zcosmic_toplevel_handle_v1_add_listener(toplevel.cosmic, &cosmicListener, data);
        }
    }
    self->_toplevels.emplace(handle, toplevel);
}

void ToplevelWatcher::onHandleClosed(void* data,
                                     ext_foreign_toplevel_handle_v1* handle) {
    auto* self = watcherOf(data);
    auto it = self->_toplevels.find(handle);
    if (it == self->_toplevels.end()) {
        return;
    }
    const bool wasFocused = it->second.activated;
    if (it->second.cosmic) {
        zcosmic_toplevel_handle_v1_destroy(it->second.cosmic);
    }
    self->_toplevels.erase(it);
    ext_foreign_toplevel_handle_v1_destroy(handle);

    if (wasFocused) {
        self->publishFocus(std::string());
    }
}

void ToplevelWatcher::onHandleAppId(void* data, ext_foreign_toplevel_handle_v1* handle,
                                    const char* appId) {
    auto* self = watcherOf(data);
    if (Toplevel* toplevel = self->findByExt(handle)) {
        toplevel->appId = appId ? appId : "";
        // app-id thuong toi sau su kien state, nen phai bao lai o day.
        if (toplevel->activated) {
            self->publishFocus(toplevel->appId);
        }
    }
}

void ToplevelWatcher::onCosmicState(void* data, zcosmic_toplevel_handle_v1* handle,
                                    wl_array* state) {
    auto* self = watcherOf(data);
    Toplevel* toplevel = self->findByCosmic(handle);
    if (!toplevel) {
        return;
    }

    bool activated = false;
    const uint32_t* values = static_cast<const uint32_t*>(state->data);
    for (size_t i = 0; i < state->size / sizeof(uint32_t); i++) {
        if (values[i] == ZCOSMIC_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED) {
            activated = true;
            break;
        }
    }

    toplevel->activated = activated;
    if (activated) {
        self->publishFocus(toplevel->appId);
    }
}

} // namespace openkey

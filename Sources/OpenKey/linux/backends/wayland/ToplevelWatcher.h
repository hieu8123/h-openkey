//
//  ToplevelWatcher.h
//  OpenKey cho Linux
//
//  Theo doi cua so nao dang duoc kich hoat, de biet app-id cua no. Day la thu
//  Smart Switch Key can: nho terminal dung tieng Anh con trinh duyet dung
//  tieng Viet, thay cho bundleID ma ban macOS dung.
//
//  Co hai duong lay thong tin, tuy compositor:
//
//   - zwlr_foreign_toplevel_manager_v1: mot giao thuc la du, co ca app_id lan
//     trang thai activated. Co tren wlroots (Sway, Hyprland, river) va KWin.
//
//   - ext_foreign_toplevel_list_v1 + zcosmic_toplevel_info_v1: giao thuc chuan
//     cho app_id, mo rong cua COSMIC cho trang thai activated. COSMIC khong
//     expose duong wlr nen phai giu ca hai.
//
//  Khong co duong nao thi tinh nang tat, phan con lai cua bo go van chay.
//

#ifndef OPENKEY_LINUX_TOPLEVELWATCHER_H
#define OPENKEY_LINUX_TOPLEVELWATCHER_H

#include <functional>
#include <map>
#include <string>

struct wl_array;
struct ext_foreign_toplevel_handle_v1;
struct ext_foreign_toplevel_list_v1;
struct zcosmic_toplevel_handle_v1;
struct zcosmic_toplevel_info_v1;
struct zwlr_foreign_toplevel_handle_v1;
struct zwlr_foreign_toplevel_manager_v1;

namespace openkey {

class ToplevelWatcher {
public:
    ~ToplevelWatcher();

    // Tra ve false neu compositor khong du giao thuc; khi do cu bo qua.
    bool start(ext_foreign_toplevel_list_v1* list, zcosmic_toplevel_info_v1* info,
               zwlr_foreign_toplevel_manager_v1* wlr);

    // Ten duong dang dung, de ghi vao nhat ky.
    const char* mode() const { return _mode; }
    void stop();

    // Goi khi cua so dang kich hoat doi sang ung dung khac.
    std::function<void(const std::string&)> onFocusChanged;

    const std::string& focusedAppId() const { return _focusedAppId; }

private:
    struct Toplevel {
        std::string appId;
        zcosmic_toplevel_handle_v1* cosmic = nullptr;
        bool activated = false;
    };

    static void onToplevel(void* data, ext_foreign_toplevel_list_v1*,
                           ext_foreign_toplevel_handle_v1* handle);
    static void onListFinished(void*, ext_foreign_toplevel_list_v1*) {}

    static void onHandleClosed(void* data, ext_foreign_toplevel_handle_v1* handle);
    static void onHandleDone(void*, ext_foreign_toplevel_handle_v1*) {}
    static void onHandleTitle(void*, ext_foreign_toplevel_handle_v1*, const char*) {}
    static void onHandleAppId(void* data, ext_foreign_toplevel_handle_v1* handle,
                              const char* appId);
    static void onHandleIdentifier(void*, ext_foreign_toplevel_handle_v1*, const char*) {}

    static void onCosmicState(void* data, zcosmic_toplevel_handle_v1* handle,
                              wl_array* state);

    static void onWlrToplevel(void* data, zwlr_foreign_toplevel_manager_v1*,
                              zwlr_foreign_toplevel_handle_v1* handle);
    static void onWlrAppId(void* data, zwlr_foreign_toplevel_handle_v1* handle,
                           const char* appId);
    static void onWlrState(void* data, zwlr_foreign_toplevel_handle_v1* handle,
                           wl_array* state);
    static void onWlrClosed(void* data, zwlr_foreign_toplevel_handle_v1* handle);

    // Tra ve nullptr neu handle da bi go bo.
    Toplevel* findByExt(ext_foreign_toplevel_handle_v1* handle);
    Toplevel* findByCosmic(zcosmic_toplevel_handle_v1* handle);
    void publishFocus(const std::string& appId);

    ext_foreign_toplevel_list_v1* _list = nullptr;
    zcosmic_toplevel_info_v1* _info = nullptr;
    std::map<ext_foreign_toplevel_handle_v1*, Toplevel> _toplevels;

    zwlr_foreign_toplevel_manager_v1* _wlr = nullptr;
    struct WlrToplevel {
        std::string appId;
        bool activated = false;
    };
    std::map<zwlr_foreign_toplevel_handle_v1*, WlrToplevel> _wlrToplevels;

    const char* _mode = "khong co";
    std::string _focusedAppId;
};

} // namespace openkey

#endif // OPENKEY_LINUX_TOPLEVELWATCHER_H

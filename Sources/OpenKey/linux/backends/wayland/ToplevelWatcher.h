//
//  ToplevelWatcher.h
//  OpenKey cho Linux
//
//  Theo doi cua so nao dang duoc kich hoat, de biet app-id cua no. Day la thu
//  Smart Switch Key can: nho terminal dung tieng Anh con trinh duyet dung
//  tieng Viet, thay cho bundleID ma ban macOS dung.
//
//  app-id lay tu ext_foreign_toplevel_list_v1 (giao thuc chuan), con trang thai
//  "dang kich hoat" lay tu zcosmic_toplevel_info_v1 (mo rong cua COSMIC) vi
//  giao thuc chuan khong noi cua so nao dang focus. Thieu mo rong do thi tinh
//  nang nay tat, phan con lai cua bo go van chay binh thuong.
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

namespace openkey {

class ToplevelWatcher {
public:
    ~ToplevelWatcher();

    // Tra ve false neu compositor khong du giao thuc; khi do cu bo qua.
    bool start(ext_foreign_toplevel_list_v1* list, zcosmic_toplevel_info_v1* info);
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

    // Tra ve nullptr neu handle da bi go bo.
    Toplevel* findByExt(ext_foreign_toplevel_handle_v1* handle);
    Toplevel* findByCosmic(zcosmic_toplevel_handle_v1* handle);
    void publishFocus(const std::string& appId);

    ext_foreign_toplevel_list_v1* _list = nullptr;
    zcosmic_toplevel_info_v1* _info = nullptr;
    std::map<ext_foreign_toplevel_handle_v1*, Toplevel> _toplevels;
    std::string _focusedAppId;
};

} // namespace openkey

#endif // OPENKEY_LINUX_TOPLEVELWATCHER_H

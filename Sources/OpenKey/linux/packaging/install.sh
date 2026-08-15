#!/usr/bin/env bash
#
# Cài OpenKey cho Linux bằng một lệnh. Không cần sao chép kho mã nguồn hoặc cấu hình.
#
#   curl -fsSL https://raw.githubusercontent.com/hieu8123/h-openkey/master/Sources/OpenKey/linux/packaging/install.sh | bash
#
# Gỡ cài đặt:
#   bash install.sh --uninstall
#
set -euo pipefail

REPO="hieu8123/h-openkey"
PREFIX="${OPENKEY_PREFIX:-$HOME/.local}"
UNIT_DIR="$HOME/.config/systemd/user"
WORK=""
INSTALL_USER="$(id -un)"

say()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m==>\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m==>\033[0m %s\n' "$*" >&2; exit 1; }

# return 0 là bắt buộc: bash lấy mã thoát của trap EXIT làm mã thoát của cả
# script, nên nếu WORK rỗng thì `[ -n "" ]` trả 1 và --help, --uninstall báo lỗi
# dù chạy đúng.
cleanup() { [ -n "$WORK" ] && rm -rf "$WORK"; return 0; }
trap cleanup EXIT

# xkeyboard-config dang ky san ID "custom", nhung ten mac dinh cua no la
# "A user-defined custom Layout". GNOME doc ten tu rules XML thay vi doc
# name[Group1] trong symbols, nen doi dung metadata cua entry nay de giao dien
# hien thi ro "H-OpenKey Layout". Moi phep thay the deu doi chieu so lan xuat
# hien; gap rules khac du kien thi bo qua thay vi sua nham entry cua distro.
update_xkb_layout_label() {
    local xkb_root="$1"
    local action="$2"
    local from_short from_description to_short to_description
    case "$action" in
        install)
            from_short="custom"
            from_description="A user-defined custom Layout"
            to_short="HOK"
            to_description="H-OpenKey Layout"
            ;;
        uninstall)
            from_short="HOK"
            from_description="H-OpenKey Layout"
            to_short="custom"
            to_description="A user-defined custom Layout"
            ;;
        *) die "Thao tác nhãn XKB không hợp lệ: $action" ;;
    esac

    local file short_count description_count
    for file in "$xkb_root/rules/base.xml" "$xkb_root/rules/evdev.xml"; do
        [ -f "$file" ] || continue
        if grep -Fq "<shortDescription>$to_short</shortDescription>" "$file" && \
           grep -Fq "<description>$to_description</description>" "$file"; then
            continue
        fi
        short_count="$({ grep -Fo "<shortDescription>$from_short</shortDescription>" \
            "$file" || true; } | wc -l | tr -d ' ')"
        description_count="$({ grep -Fo "<description>$from_description</description>" \
            "$file" || true; } | wc -l | tr -d ' ')"
        if [ "$short_count" != "1" ] || [ "$description_count" != "1" ]; then
            warn "Không đổi nhãn trong $file vì metadata custom khác dự kiến."
            continue
        fi
        sudo sed -i \
            -e "s|<shortDescription>$from_short</shortDescription>|<shortDescription>$to_short</shortDescription>|" \
            -e "s|<description>$from_description</description>|<description>$to_description</description>|" \
            "$file"
    done

    for file in "$xkb_root/rules/base.lst" "$xkb_root/rules/evdev.lst"; do
        [ -f "$file" ] || continue
        grep -Fq "custom          $to_description" "$file" && continue
        description_count="$({ grep -Fo "custom          $from_description" \
            "$file" || true; } | wc -l | tr -d ' ')"
        if [ "$description_count" != "1" ]; then
            warn "Không đổi nhãn trong $file vì metadata custom khác dự kiến."
            continue
        fi
        sudo sed -i \
            "s|custom          $from_description|custom          $to_description|" \
            "$file"
    done
}

# --- gỡ cài đặt -------------------------------------------------------------

uninstall() {
    say "Đang gỡ cài đặt OpenKey"
    systemctl --user disable --now h-openkey.service 2>/dev/null || true
    rm -f "$UNIT_DIR/h-openkey.service"
    rm -f "$PREFIX/bin/h-openkey"
    rm -f "$PREFIX/share/applications/h-openkey.desktop"
    rm -f "$PREFIX/share/icons/hicolor/scalable/apps/h-openkey.svg"
    rm -f "$PREFIX/share/icons/hicolor/scalable/apps/h-openkey-vi.svg"
    rm -f "$PREFIX/share/icons/hicolor/scalable/apps/h-openkey-en.svg"
    rm -f "$PREFIX/lib/systemd/user/h-openkey.service"
    rm -f "$HOME/.config/autostart/h-openkey.desktop"
    rm -f "$HOME/.config/xkb/symbols/hopenkey"
    if grep -q "Managed by H-OpenKey" \
        "$HOME/.config/xkb/rules/evdev.xml" 2>/dev/null; then
        rm -f "$HOME/.config/xkb/rules/evdev.xml"
    fi
    local xkb_root
    xkb_root="$(pkg-config --variable=xkb_base xkeyboard-config 2>/dev/null || true)"
    [ -n "$xkb_root" ] || xkb_root="/usr/share/X11/xkb"
    say "Khôi phục tên mặc định của layout custom (cần mật khẩu sudo)"
    update_xkb_layout_label "$xkb_root" uninstall
    local symbols_file
    for symbols_file in custom hopenkey; do
        if grep -q "Tu sinh boi H-OpenKey" \
            "$xkb_root/symbols/$symbols_file" 2>/dev/null; then
            say "Gỡ symbols H-OpenKey khỏi kho XKB hệ thống (cần mật khẩu sudo)"
            sudo rm -f "$xkb_root/symbols/$symbols_file"
        fi
    done
    if [ -f /etc/udev/rules.d/70-h-openkey.rules ]; then
        say "Gỡ quy tắc quyền truy cập bàn phím (cần mật khẩu sudo)"
        sudo rm -f /etc/udev/rules.d/70-h-openkey.rules
        sudo udevadm control --reload-rules 2>/dev/null || true
    fi
    if id -nG "$INSTALL_USER" | tr ' ' '\n' | grep -qx input; then
        warn "Tài khoản vẫn thuộc nhóm input; trình gỡ không tự xoá vì không biết"
        warn "quyền này có tồn tại trước H-OpenKey hay không. Muốn thu hồi, chạy:"
        warn "  sudo gpasswd -d $INSTALL_USER input"
    fi
    systemctl --user daemon-reload 2>/dev/null || true
    if command -v gsettings >/dev/null; then
        local backup="$HOME/.config/openkey/input-sources.backup"
        local sources
        local original_sources
        local engine
        sources="$(gsettings get org.gnome.desktop.input-sources sources 2>/dev/null || true)"
        original_sources="$sources"
        if [ -s "$backup" ]; then
            local saved_source
            while IFS= read -r saved_source; do
                case "$saved_source" in
                    "('ibus', 'h-direct')"|"('ibus', 'openkey')"|"('xkb', 'hopenkey')"|"('xkb', 'custom')") continue ;;
                esac
                [[ "$sources" == *"$saved_source"* ]] && continue
                case "$sources" in
                    "[]"|"@a(ss) []") sources="[$saved_source]" ;;
                    *) sources="${sources%]}, $saved_source]" ;;
                esac
            done < <(grep -oE "\\('[^']+', *'[^']+'\\)" "$backup" || true)
            say "Hợp nhất các nguồn nhập từ bản sao lưu của H-OpenKey cũ"
        fi
        for engine in h-direct openkey; do
            if printf '%s' "$sources" | grep -q "('ibus', '$engine')"; then
                sources="$(printf '%s' "$sources" | sed "s/, ('ibus', '$engine')//; s/('ibus', '$engine'), //; s/\[('ibus', '$engine')\]/[]/")"
            fi
        done
        sources="$(printf '%s' "$sources" | sed "s/, ('xkb', 'hopenkey')//; s/('xkb', 'hopenkey'), //; s/\[('xkb', 'hopenkey')\]/[]/; s/, ('xkb', 'custom')//; s/('xkb', 'custom'), //; s/\[('xkb', 'custom')\]/[]/")"
        case "$sources" in
            ""|"[]"|"@a(ss) []") sources="[('xkb', 'us')]" ;;
        esac
        if [ "$sources" != "$original_sources" ]; then
            say "Xoá các nguồn nhập của H-OpenKey khỏi danh sách nguồn nhập"
            gsettings set org.gnome.desktop.input-sources sources \
                "$sources" \
                2>/dev/null || true
        fi
    fi
    say "Đã gỡ cài đặt. Cấu hình ở ~/.config/openkey vẫn được giữ."
    exit 0
}

SKIP_DEPS=0
LOCAL_SRC=""
NO_ENABLE=0
ACCEPT_US_LAYOUT=0
while [ $# -gt 0 ]; do
    case "$1" in
        --uninstall)   uninstall ;;
        --skip-deps)   SKIP_DEPS=1 ;;
        --from-source) LOCAL_SRC="${2:-}"; shift ;;
        --no-enable)   NO_ENABLE=1 ;;
        --accept-us-layout) ACCEPT_US_LAYOUT=1 ;;
        -h|--help)
            sed -n '2,9p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) die "Không hiểu tuỳ chọn: $1" ;;
    esac
    shift
done

# --- kiểm tra môi trường ----------------------------------------------------

check_session() {
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then
        say "Phiên Wayland: dùng driver evdev/uinput trực tiếp"
    elif [ -n "${DISPLAY:-}" ]; then
        say "Phiên X11: dùng cùng driver evdev/uinput trực tiếp"
    else
        die "Không thấy WAYLAND_DISPLAY lẫn DISPLAY. Hãy chạy trong phiên đồ hoạ."
    fi
}

confirm_driver_layout() {
    local desktop="${XDG_CURRENT_DESKTOP:-}:${XDG_SESSION_DESKTOP:-}"
    case "$desktop" in
        *GNOME*|*gnome*|*Ubuntu*|*ubuntu*|*Zorin*|*zorin*|*Pop*|*pop*) ;;
        *) die "Tích hợp tự động hiện chỉ hỗ trợ GNOME; phát hiện '$desktop'. Chưa thay đổi hệ thống." ;;
    esac
    command -v gsettings >/dev/null 2>&1 || die \
        "Tích hợp tự động hiện chỉ hỗ trợ GNOME có gsettings; chưa thay đổi hệ thống."
    local sources
    sources="$(gsettings get org.gnome.desktop.input-sources sources 2>/dev/null)" \
        || die "Không đọc được nguồn nhập GNOME; chưa thay đổi hệ thống."
    local xkb_root
    xkb_root="$(pkg-config --variable=xkb_base xkeyboard-config 2>/dev/null || true)"
    [ -n "$xkb_root" ] || xkb_root="/usr/share/X11/xkb"
    if [ -e "$xkb_root/symbols/custom" ] && \
       ! grep -q "Tu sinh boi H-OpenKey" "$xkb_root/symbols/custom"; then
        die "$xkb_root/symbols/custom thuộc cấu hình khác; chưa thay đổi hệ thống."
    fi
    [[ "$sources" == *"('xkb', 'custom')"* ]] && return 0
    [ "$ACCEPT_US_LAYOUT" -eq 1 ] && return 0

    warn "H-OpenKey cần thêm H-OpenKey Layout dựa trên US (QWERTY)."
    warn "Các nguồn nhập hiện có vẫn được giữ, nhưng tiếng Việt của H-OpenKey"
    warn "chưa hỗ trợ AZERTY, QWERTZ, Dvorak hoặc Colemak."
    printf '   Tiếp tục cài và chọn H-OpenKey Layout? [y/N] '
    local answer=""
    read -r answer </dev/tty 2>/dev/null || answer="n"
    case "$answer" in
        y|Y) ;;
        *) die "Đã huỷ trước khi thay đổi hệ thống." ;;
    esac
}

# --- phụ thuộc --------------------------------------------------------------

install_deps() {
    local pm=""
    for candidate in apt-get dnf pacman zypper; do
        command -v "$candidate" >/dev/null && pm="$candidate" && break
    done
    [ -z "$pm" ] && die "Không nhận ra trình quản lý gói. Hãy cài thủ công: cmake, g++, Qt6 Widgets và libxkbcommon."

    say "Cài phụ thuộc bằng $pm (cần mật khẩu sudo)"
    case "$pm" in
        apt-get)
            sudo apt-get update
            sudo apt-get install -y --no-install-recommends \
                cmake g++ pkg-config qt6-base-dev libxkbcommon-dev
            ;;
        dnf)
            sudo dnf install -y \
                cmake gcc-c++ pkgconf-pkg-config qt6-qtbase-devel \
                libxkbcommon-devel
            ;;
        pacman)
            sudo pacman -S --needed --noconfirm \
                cmake gcc pkgconf qt6-base libxkbcommon
            ;;
        zypper)
            sudo zypper install -y \
                cmake gcc-c++ pkg-config qt6-base-devel libxkbcommon-devel
            ;;
    esac
}

# --- tải mã nguồn -----------------------------------------------------------

fetch_source() {
    WORK="$(mktemp -d)"

    # Dùng cho phát triển: biên dịch trực tiếp từ thư mục mã nguồn có sẵn.
    if [ -n "$LOCAL_SRC" ]; then
        SRC="$(cd "$LOCAL_SRC" && pwd)"
        say "Dùng mã nguồn tại chỗ: $SRC"
        return 0
    fi
    local url
    url="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" 2>/dev/null \
           | grep -o '"browser_download_url": *"[^"]*-src\.tar\.gz"' \
           | head -1 | cut -d'"' -f4 || true)"

    [ -n "$url" ] || die \
        "Bản phát hành mới nhất không có gói *-src.tar.gz; từ chối cài nhánh master."
    say "Tải bản phát hành: $url"
    curl -fsSL "$url" -o "$WORK/src.tar.gz"
    tar -C "$WORK" -xzf "$WORK/src.tar.gz"
    SRC="$(find "$WORK" -maxdepth 2 -type d -name linux -path '*/openkey-linux-*' | head -1)"
    [ -z "$SRC" ] && SRC="$(find "$WORK" -maxdepth 3 -type d -name linux | head -1)"

    [ -z "${SRC:-}" ] && die "Không tìm thấy mã nguồn trong gói vừa tải."
    say "Mã nguồn ở $SRC"
}

# --- biên dịch và cài đặt ---------------------------------------------------

build_install() {
    say "Đang biên dịch (mất khoảng một đến hai phút)"
    cmake -S "$SRC" -B "$WORK/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" >/dev/null
    cmake --build "$WORK/build" --parallel >/dev/null

    say "Chạy kiểm thử"
    ctest --test-dir "$WORK/build" --output-on-failure >/dev/null \
        || die "Kiểm thử không đạt, dừng cài để bạn không nhận một bản lỗi."

    say "Cài vào $PREFIX"
    cmake --install "$WORK/build" >/dev/null

    mkdir -p "$UNIT_DIR"
    ln -sf "$PREFIX/lib/systemd/user/h-openkey.service" "$UNIT_DIR/h-openkey.service"
    systemctl --user daemon-reload
}

# Hoan tac duy nhat cac mask co chu ky do ban H-OpenKey cu tao. Khong dung vao
# cau hinh autostart ma nguoi dung tu tat hoac bo go khac tu quan ly.
restore_other_ime_autostart() {
    local file_name
    local current
    local backup
    for file_name in org.fcitx.Fcitx5.desktop fcitx5.desktop \
                     fcitx-autostart.desktop fcitx.desktop nimf.desktop \
                     org.nimf.Nimf.desktop uim.desktop uim-xim.desktop \
                     kime.desktop gcin.desktop hime.desktop; do
        current="$HOME/.config/autostart/$file_name"
        backup="$current.disabled-by-h-openkey"
        if grep -q '^X-H-OpenKey-Disabled=true$' "$current" 2>/dev/null; then
            rm -f "$current"
            if [ -e "$backup" ] || [ -L "$backup" ]; then
                mv "$backup" "$current"
            fi
            say "Khôi phục autostart bộ gõ: $file_name"
        fi
    done
}

# Driver can doc event* va ghi uinput. Rule chi cap ACL cho nguoi dang hoat dong
# tai seat; khong them tai khoan vao nhom input co quyen keylogger vinh vien.
install_driver_access() {
    say "Cài quyền truy cập evdev/uinput (cần mật khẩu sudo)"
    sudo install -Dm0644 "$SRC/packaging/70-h-openkey.rules" \
        /etc/udev/rules.d/70-h-openkey.rules
    sudo modprobe uinput
    sudo udevadm control --reload-rules
    sudo udevadm trigger --subsystem-match=misc --action=change 2>/dev/null || true
    sudo udevadm trigger --subsystem-match=input --action=change 2>/dev/null || true
}

configure_driver() {
    "$PREFIX/bin/h-openkey" --configure-driver
    command -v gsettings >/dev/null 2>&1 \
        || die "Trình cài đặt hiện cần GNOME/gsettings để kích hoạt layout XKB."

    local sources
    sources="$(gsettings get org.gnome.desktop.input-sources sources)"
    local current_value
    local current_index
    current_value="$(gsettings get org.gnome.desktop.input-sources current)"
    current_index="$(printf '%s' "$current_value" | grep -oE '[0-9]+$' || true)"
    local original_sources="$sources"
    local updated_sources="$sources"
    local had_custom=0
    [[ "$sources" == *"('xkb', 'custom')"* ]] && had_custom=1

    # Ban 1.3.0 tung sao luu roi thay ca danh sach bang moi xkb:custom. Khi cap
    # nhat, hop nhat cac source trong backup vao cau hinh hien tai de tra lai
    # engine Nhat/Han cu ma khong lam mat source nguoi dung moi them sau do.
    local backup="$HOME/.config/openkey/input-sources.backup"
    if [ -s "$backup" ]; then
        local saved_source
        while IFS= read -r saved_source; do
            case "$saved_source" in
                "('ibus', 'h-direct')"|"('ibus', 'openkey')") continue ;;
            esac
            [[ "$updated_sources" == *"$saved_source"* ]] && continue
            case "$updated_sources" in
                "[]"|"@a(ss) []") updated_sources="[$saved_source]" ;;
                *) updated_sources="${updated_sources%]}, $saved_source]" ;;
            esac
        done < <(grep -oE "\\('[^']+', *'[^']+'\\)" "$backup" || true)
    fi

    # Don dau vet cua layout tu dang ky cu. Chi xoa rules co chu ky H-OpenKey.
    if grep -q "Managed by H-OpenKey" \
        "$HOME/.config/xkb/rules/evdev.xml" 2>/dev/null; then
        rm -f "$HOME/.config/xkb/rules/evdev.xml"
    fi
    rm -f "$HOME/.config/openkey/xkb-pending-shell"

    # xkeyboard-config da dang ky san layout "custom". Cai symbols vao ten nay
    # de Mutter doc dung keymap, sau do chi doi metadata hien thi cua entry.
    local xkb_root
    xkb_root="$(pkg-config --variable=xkb_base xkeyboard-config 2>/dev/null || true)"
    [ -n "$xkb_root" ] || xkb_root="/usr/share/X11/xkb"
    [ -d "$xkb_root/symbols" ] \
        || die "Không tìm thấy thư mục symbols trong XKB root: $xkb_root"
    if [ -e "$xkb_root/symbols/custom" ] && \
       ! grep -q "Tu sinh boi H-OpenKey" "$xkb_root/symbols/custom"; then
        die "$xkb_root/symbols/custom đã thuộc cấu hình khác; không ghi đè."
    fi
    say "Cài symbols H-OpenKey vào $xkb_root/symbols/custom (cần mật khẩu sudo)"
    sudo install -m0644 "$HOME/.config/xkb/symbols/hopenkey" \
        "$xkb_root/symbols/custom"
    say "Đặt tên nguồn nhập thành H-OpenKey Layout"
    update_xkb_layout_label "$xkb_root" install
    if grep -q "Tu sinh boi H-OpenKey" \
        "$xkb_root/symbols/hopenkey" 2>/dev/null; then
        sudo rm -f "$xkb_root/symbols/hopenkey"
    fi

    # Giữ nguyên IBus/Fcitx và các nguồn Nhật, Hàn đang có. Driver trực tiếp
    # không chiếm input-method slot; nó chỉ cần xkb:custom khi gõ tiếng Việt.
    local custom_index
    if [[ "$updated_sources" != *"('xkb', 'custom')"* ]]; then
        case "$updated_sources" in
            "[]"|"@a(ss) []") updated_sources="[('xkb', 'custom')]" ;;
            *) updated_sources="${updated_sources%]}, ('xkb', 'custom')]" ;;
        esac
    fi
    local prefix="${updated_sources%%"('xkb', 'custom')"*}"
    custom_index="$(
        { printf '%s' "$prefix" | grep -oF "('" || true; } | wc -l | tr -d ' '
    )"
    if [ "$updated_sources" != "$original_sources" ]; then
        say "Cập nhật xkb:custom; giữ nguyên và khôi phục các nguồn nhập hiện có"
        gsettings set org.gnome.desktop.input-sources sources "$updated_sources"
    fi
    if [ "$had_custom" -eq 0 ]; then
        say "Kích hoạt layout custom của H-OpenKey (US + Unicode riêng)"
        gsettings set org.gnome.desktop.input-sources current "uint32 $custom_index"
        current_index="$custom_index"
    else
        say "Giữ nguyên nguồn nhập đang dùng; xkb:custom đã có sẵn"
    fi
    if [ "$current_index" = "$custom_index" ] && \
       command -v setxkbmap >/dev/null 2>&1 && [ -n "${DISPLAY:-}" ]; then
        setxkbmap -layout custom 2>/dev/null || true
    fi
}

# --- chạy -------------------------------------------------------------------

main() {
    say "OpenKey cho Linux — bộ gõ tiếng Việt"
    check_session
    confirm_driver_layout
    if [ "$SKIP_DEPS" -eq 1 ]; then
        say "Bỏ qua bước cài phụ thuộc theo yêu cầu"
    else
        install_deps
    fi
    fetch_source
    build_install
    restore_other_ime_autostart
    install_driver_access
    configure_driver

    if [ "$NO_ENABLE" -eq 1 ]; then
        say "Xong. Bỏ qua bước bật service theo yêu cầu."
        return 0
    fi

    say "Bật chạy cùng phiên đăng nhập"
    systemctl --user enable h-openkey.service
    # enable --now khong nap lai process neu ban cu dang chay. Restart de lan
    # cai/cap nhat nao cung dua binary, watchdog va layout moi vao su dung ngay.
    systemctl --user restart h-openkey.service

    sleep 2
    if systemctl --user is-active --quiet h-openkey.service; then
        say "Xong. OpenKey đang chạy, biểu tượng \"V\" ở khay hệ thống."
        say "Chuột phải vào biểu tượng để mở bảng điều khiển."
    else
        warn "Cài xong nhưng service chưa chạy. Xem lý do bằng:"
        warn "  journalctl --user -u h-openkey.service -n 40"
    fi
}

main "$@"

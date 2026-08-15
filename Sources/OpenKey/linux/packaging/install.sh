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

# --- gỡ cài đặt -------------------------------------------------------------

uninstall() {
    say "Đang gỡ cài đặt OpenKey"
    systemctl --user disable --now h-openkey.service 2>/dev/null || true
    rm -f "$UNIT_DIR/h-openkey.service"
    rm -f "$PREFIX/bin/h-openkey"
    rm -f "$PREFIX/share/applications/h-openkey.desktop"
    rm -f "$PREFIX/share/icons/hicolor/scalable/apps/h-openkey.svg"
    rm -f "$PREFIX/lib/systemd/user/h-openkey.service"
    rm -f "$HOME/.config/xkb/symbols/hopenkey"
    if grep -q "Managed by H-OpenKey" \
        "$HOME/.config/xkb/rules/evdev.xml" 2>/dev/null; then
        rm -f "$HOME/.config/xkb/rules/evdev.xml"
    fi
    local xkb_root
    xkb_root="$(pkg-config --variable=xkb_base xkeyboard-config 2>/dev/null || true)"
    [ -n "$xkb_root" ] || xkb_root="/usr/share/X11/xkb"
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
    systemctl --user daemon-reload 2>/dev/null || true
    if command -v gsettings >/dev/null; then
        local backup="$HOME/.config/openkey/input-sources.backup"
        local sources
        local original_sources
        local engine
        if [ -s "$backup" ]; then
            sources="$(cat "$backup")"
            say "Khôi phục danh sách nguồn nhập trước khi cài H-OpenKey"
            gsettings set org.gnome.desktop.input-sources sources "$sources" \
                2>/dev/null || true
            sources=""
        else
            sources="$(gsettings get org.gnome.desktop.input-sources sources 2>/dev/null || true)"
        fi
        original_sources="$sources"
        for engine in h-direct openkey; do
            if printf '%s' "$sources" | grep -q "('ibus', '$engine')"; then
                sources="$(printf '%s' "$sources" | sed "s/, ('ibus', '$engine')//; s/('ibus', '$engine'), //; s/\[('ibus', '$engine')\]/[]/")"
            fi
        done
        sources="$(printf '%s' "$sources" | sed "s/, ('xkb', 'hopenkey')//; s/('xkb', 'hopenkey'), //; s/\[('xkb', 'hopenkey')\]/[]/; s/, ('xkb', 'custom')//; s/('xkb', 'custom'), //; s/\[('xkb', 'custom')\]/[]/")"
        if [ "$sources" != "$original_sources" ]; then
            say "Xoá các nguồn nhập của H-OpenKey khỏi danh sách nguồn nhập"
            gsettings set org.gnome.desktop.input-sources sources \
                "$sources" \
                2>/dev/null || true
        fi
    fi
    say "Đã gỡ cài đặt. Cấu hình ở ~/.config/openkey vẫn được giữ."
    say "Bật lại bộ gõ cũ, ví dụ:  systemctl --user start app-org.fcitx.Fcitx5@autostart.service"
    exit 0
}

SKIP_DEPS=0
LOCAL_SRC=""
NO_ENABLE=0
while [ $# -gt 0 ]; do
    case "$1" in
        --uninstall)   uninstall ;;
        --skip-deps)   SKIP_DEPS=1 ;;
        --from-source) LOCAL_SRC="${2:-}"; shift ;;
        --no-enable)   NO_ENABLE=1 ;;
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

    if [ -n "$url" ]; then
        say "Tải bản phát hành: $url"
        curl -fsSL "$url" -o "$WORK/src.tar.gz"
        tar -C "$WORK" -xzf "$WORK/src.tar.gz"
        SRC="$(find "$WORK" -maxdepth 2 -type d -name linux -path '*/openkey-linux-*' | head -1)"
        [ -z "$SRC" ] && SRC="$(find "$WORK" -maxdepth 3 -type d -name linux | head -1)"
    else
        warn "Bản phát hành mới nhất không kèm gói mã nguồn, tải ảnh chụp nhánh master."
        say "Tải mã nguồn từ nhánh master"
        curl -fsSL "https://github.com/$REPO/archive/refs/heads/master.tar.gz" \
            -o "$WORK/src.tar.gz"
        tar -C "$WORK" -xzf "$WORK/src.tar.gz"
        SRC="$(find "$WORK" -maxdepth 4 -type d -path '*/Sources/OpenKey/linux' | head -1)"
    fi

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

# --- bộ gõ có thể gây xung đột ----------------------------------------------

handle_other_ime() {
    local running=""
    local name=""
    local answer=""
    local file_name=""
    local f=""
    local -a desktop_files=()
    for name in fcitx5 fcitx; do
        pgrep -x "$name" >/dev/null 2>&1 && running="$name" && break
    done
    [ -z "$running" ] && return 0

    warn "Đang có bộ gõ khác chạy: $running"
    warn "Chỉ một bộ gõ được giữ input method của phiên. OpenKey sẽ khởi động"
    warn "nhưng KHÔNG gõ được chữ nào cho tới khi $running bị tắt."
    printf '   Tắt %s ngay bây giờ? [y/N] ' "$running"
    read -r answer </dev/tty 2>/dev/null || answer="n"
    case "$answer" in
        y|Y)
            systemctl --user disable --now "$running.service" 2>/dev/null || true
            if [ "$running" = "fcitx5" ]; then
                systemctl --user stop \
                    "app-org.fcitx.Fcitx5@autostart.service" 2>/dev/null || true
                desktop_files=(org.fcitx.Fcitx5.desktop fcitx5.desktop)
            else
                desktop_files=(fcitx-autostart.desktop fcitx.desktop)
            fi
            pkill -x "$running" 2>/dev/null || true
            for file_name in "${desktop_files[@]}"; do
                f="$HOME/.config/autostart/$file_name"
                [ -f "$f" ] && mv "$f" "$f.disabled" && say "Đã tắt tự khởi động: $f"
            done
            ;;
        *)
            warn "Không bật H-OpenKey để tránh hai bộ gõ chạy song song."
            return 1 ;;
    esac
}

# Driver can doc event* va ghi uinput. Rule chi cap quyen cho nguoi dang ngoi
# tai seat (uaccess); group input la duong lui cho distro khong co logind.
install_driver_access() {
    say "Cài quyền truy cập evdev/uinput (cần mật khẩu sudo)"
    sudo install -Dm0644 "$SRC/packaging/70-h-openkey.rules" \
        /etc/udev/rules.d/70-h-openkey.rules
    sudo modprobe uinput
    if getent group input >/dev/null 2>&1; then
        sudo usermod -aG input "$INSTALL_USER"
    fi
    sudo udevadm control --reload-rules
    sudo udevadm trigger --subsystem-match=misc --action=change 2>/dev/null || true
    sudo udevadm trigger --subsystem-match=input --action=change 2>/dev/null || true
}

configure_driver() {
    "$PREFIX/bin/h-openkey" --configure-driver
    command -v gsettings >/dev/null 2>&1 \
        || die "Trình cài đặt hiện cần GNOME/gsettings để kích hoạt layout XKB."

    local backup="$HOME/.config/openkey/input-sources.backup"
    local sources
    sources="$(gsettings get org.gnome.desktop.input-sources sources)"
    if [ ! -s "$backup" ]; then
        mkdir -p "$(dirname "$backup")"
        printf '%s\n' "$sources" >"$backup"
    fi

    # Don dau vet cua layout tu dang ky cu. Chi xoa rules co chu ky H-OpenKey.
    if grep -q "Managed by H-OpenKey" \
        "$HOME/.config/xkb/rules/evdev.xml" 2>/dev/null; then
        rm -f "$HOME/.config/xkb/rules/evdev.xml"
    fi
    rm -f "$HOME/.config/openkey/xkb-pending-shell"

    # xkeyboard-config da dang ky san layout "custom". Cai symbols vao ten nay
    # de Mutter doc duoc ma khong sua evdev.xml do goi he thong quan ly.
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
    if grep -q "Tu sinh boi H-OpenKey" \
        "$xkb_root/symbols/hopenkey" 2>/dev/null; then
        sudo rm -f "$xkb_root/symbols/hopenkey"
    fi

    # Chi giu mot source: neu nguoi dung chuyen sang layout khac trong khi
    # driver dang grab, cac keycode rieng se khong con sinh dung Unicode.
    say "Kích hoạt layout custom của H-OpenKey (US + Unicode riêng)"
    gsettings set org.gnome.desktop.input-sources sources "[('xkb', 'custom')]"
    gsettings set org.gnome.desktop.input-sources current 'uint32 0'
    if command -v setxkbmap >/dev/null 2>&1 && [ -n "${DISPLAY:-}" ]; then
        setxkbmap -layout custom 2>/dev/null || true
    fi
    systemctl --user unset-environment GTK_IM_MODULE QT_IM_MODULE XMODIFIERS \
        2>/dev/null || true
}

# --- chạy -------------------------------------------------------------------

main() {
    say "OpenKey cho Linux — bộ gõ tiếng Việt"
    check_session
    if [ "$SKIP_DEPS" -eq 1 ]; then
        say "Bỏ qua bước cài phụ thuộc theo yêu cầu"
    else
        install_deps
    fi
    fetch_source
    build_install
    install_driver_access
    if ! handle_other_ime; then
        NO_ENABLE=1
    else
        configure_driver
    fi

    if [ "$NO_ENABLE" -eq 1 ]; then
        say "Xong. Bỏ qua bước bật service theo yêu cầu."
        return 0
    fi

    say "Bật chạy cùng phiên đăng nhập"
    systemctl --user enable --now h-openkey.service

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

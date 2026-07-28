#!/usr/bin/env bash
#
# Cài OpenKey cho Linux bằng một lệnh. Không cần clone mã, không cần cấu hình.
#
#   curl -fsSL https://raw.githubusercontent.com/hieu8123/OpenKey/master/Sources/OpenKey/linux/packaging/install.sh | bash
#
# Gỡ ra:
#   bash install.sh --uninstall
#
set -euo pipefail

REPO="hieu8123/OpenKey"
PREFIX="${OPENKEY_PREFIX:-$HOME/.local}"
UNIT_DIR="$HOME/.config/systemd/user"
WORK=""

say()  { printf '\033[1;32m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m==>\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31m==>\033[0m %s\n' "$*" >&2; exit 1; }

cleanup() { [ -n "$WORK" ] && rm -rf "$WORK"; }
trap cleanup EXIT

# --- gỡ ra ------------------------------------------------------------------

uninstall() {
    say "Đang gỡ OpenKey"
    systemctl --user disable --now openkey.service 2>/dev/null || true
    rm -f "$UNIT_DIR/openkey.service"
    rm -f "$PREFIX/bin/openkey"
    rm -f "$PREFIX/share/applications/openkey.desktop"
    rm -f "$PREFIX/share/icons/hicolor/scalable/apps/openkey.svg"
    rm -f "$PREFIX/lib/systemd/user/openkey.service"
    systemctl --user daemon-reload 2>/dev/null || true
    say "Đã gỡ. Cấu hình ở ~/.config/openkey vẫn được giữ."
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
            sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) die "Không hiểu tuỳ chọn: $1" ;;
    esac
    shift
done

# --- kiểm tra môi trường ----------------------------------------------------

check_session() {
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then
        say "Phiên Wayland: dùng backend input-method-v2"
        # Không phải compositor nào cũng có. GNOME/Mutter thì không.
        case "${XDG_CURRENT_DESKTOP:-}" in
            *GNOME*)
                warn "GNOME/Mutter không hiện thực zwp_input_method_v2."
                warn "OpenKey sẽ không gõ được trên phiên Wayland của GNOME."
                warn "Hãy đăng nhập bằng phiên Xorg, hoặc dùng COSMIC/KDE/Sway."
                ;;
        esac
    elif [ -n "${DISPLAY:-}" ]; then
        say "Phiên X11: dùng backend XRecord + XTEST"
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
    [ -z "$pm" ] && die "Không nhận ra trình quản lý gói. Hãy cài thủ công: cmake, g++, Qt6 Widgets, libwayland, wayland-protocols, libxkbcommon, libX11, libXtst."

    say "Cài phụ thuộc bằng $pm (cần mật khẩu sudo)"
    case "$pm" in
        apt-get)
            sudo apt-get update
            sudo apt-get install -y --no-install-recommends \
                cmake g++ pkg-config qt6-base-dev \
                libwayland-dev wayland-protocols libxkbcommon-dev \
                libx11-dev libxtst-dev
            ;;
        dnf)
            sudo dnf install -y \
                cmake gcc-c++ pkgconf-pkg-config qt6-qtbase-devel \
                wayland-devel wayland-protocols-devel libxkbcommon-devel \
                libX11-devel libXtst-devel
            ;;
        pacman)
            sudo pacman -S --needed --noconfirm \
                cmake gcc pkgconf qt6-base \
                wayland wayland-protocols libxkbcommon libx11 libxtst
            ;;
        zypper)
            sudo zypper install -y \
                cmake gcc-c++ pkg-config qt6-base-devel \
                wayland-devel wayland-protocols-devel libxkbcommon-devel \
                libX11-devel libXtst-devel
            ;;
    esac
}

# --- tải mã nguồn -----------------------------------------------------------

fetch_source() {
    WORK="$(mktemp -d)"

    # Dùng cho phát triển: build thẳng từ thư mục mã nguồn có sẵn.
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
        warn "Chưa có bản phát hành nào cho Linux, tải ảnh chụp nhánh master."
        say "Tải mã nguồn từ nhánh master"
        curl -fsSL "https://github.com/$REPO/archive/refs/heads/master.tar.gz" \
            -o "$WORK/src.tar.gz"
        tar -C "$WORK" -xzf "$WORK/src.tar.gz"
        SRC="$(find "$WORK" -maxdepth 4 -type d -path '*/Sources/OpenKey/linux' | head -1)"
    fi

    [ -z "${SRC:-}" ] && die "Không tìm thấy mã nguồn trong gói vừa tải."
    say "Mã nguồn ở $SRC"
}

# --- build và cài -----------------------------------------------------------

build_install() {
    say "Đang build (mất khoảng một hai phút)"
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
    ln -sf "$PREFIX/lib/systemd/user/openkey.service" "$UNIT_DIR/openkey.service"
    systemctl --user daemon-reload
}

# --- bộ gõ đang chiếm chỗ ---------------------------------------------------

handle_other_ime() {
    local running=""
    for name in fcitx5 fcitx ibus-daemon; do
        pgrep -x "$name" >/dev/null 2>&1 && running="$name" && break
    done
    [ -z "$running" ] && return 0

    warn "Đang có bộ gõ khác chạy: $running"
    warn "Chỉ một bộ gõ được giữ input method của phiên. OpenKey sẽ khởi động"
    warn "nhưng KHÔNG gõ được chữ nào cho tới khi $running bị tắt."
    printf '   Tắt %s ngay bây giờ? [y/N] ' "$running"
    local answer=""
    read -r answer </dev/tty 2>/dev/null || answer="n"
    case "$answer" in
        y|Y)
            systemctl --user stop "app-org.fcitx.Fcitx5@autostart.service" 2>/dev/null || true
            pkill -x "$running" 2>/dev/null || true
            for f in "$HOME/.config/autostart/org.fcitx.Fcitx5.desktop" \
                     "$HOME/.config/autostart/ibus.desktop"; do
                [ -f "$f" ] && mv "$f" "$f.disabled" && say "Đã tắt tự khởi động: $f"
            done
            ;;
        *)
            warn "Bỏ qua. Tự tắt sau bằng: pkill -x $running"
            ;;
    esac
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
    handle_other_ime

    if [ "$NO_ENABLE" -eq 1 ]; then
        say "Xong. Bỏ qua bước bật service theo yêu cầu."
        return 0
    fi

    say "Bật chạy cùng phiên đăng nhập"
    systemctl --user enable --now openkey.service

    sleep 2
    if systemctl --user is-active --quiet openkey.service; then
        say "Xong. OpenKey đang chạy, biểu tượng \"V\" ở khay hệ thống."
        say "Chuột phải vào biểu tượng để mở bảng điều khiển."
    else
        warn "Cài xong nhưng service chưa chạy. Xem lý do bằng:"
        warn "  journalctl --user -u openkey.service -n 40"
    fi
}

main "$@"

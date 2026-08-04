#!/usr/bin/env python3
#
#  ibus_spike.py
#  Cong cu chan doan, khong phai mot phan cua ung dung.
#
#  Dang ky dong mot engine ten "openkey-spike" voi ibus-daemon roi in ra moi
#  phim nhan duoc. Dung de tra loi hai cau hoi truoc khi viet backend C++:
#    1. GNOME co chap nhan engine dang ky dong khong (khong co file component
#       XML nao ca)?
#    2. keycode ma IBus gui la keycode evdev hay keycode X11 (evdev + 8)?
#
#  Chay:  python3 ibus_spike.py
#  Roi chon "OpenKey (spike)" trong Settings > Keyboard > Input Sources,
#  hoac doi bang Super+Space, va go vao mot o nhap bat ky.
#
import gi

gi.require_version("IBus", "1.0")
from gi.repository import GLib, IBus  # noqa: E402


class SpikeEngine(IBus.Engine):
    __gtype_name__ = "SpikeEngine"

    def do_process_key_event(self, keyval, keycode, state):
        released = bool(state & (1 << 30))
        print(
            f"keyval=0x{keyval:04x} keycode={keycode} "
            f"(evdev+8 = {keycode + 8}) state=0x{state:08x} "
            f"{'nha' if released else 'bam'}",
            flush=True,
        )
        # Tra ve False de ung dung van nhan duoc phim goc: spike chi quan sat.
        return False

    def do_focus_in(self):
        print("focus_in", flush=True)

    def do_focus_out(self):
        print("focus_out", flush=True)

    def do_set_capabilities(self, caps):
        # Bit 1<<5 la surrounding text. Can biet de sau nay chon duong xoa.
        print(f"capabilities=0x{caps:08x} surrounding={bool(caps & (1 << 5))}", flush=True)


def main():
    IBus.init()
    bus = IBus.Bus()
    if not bus.is_connected():
        raise SystemExit("ibus-daemon chua chay. Chay truoc: ibus-daemon -drx")

    bus.connect("disconnected", lambda *_: GLib.MainLoop().quit())

    factory = IBus.Factory.new(bus.get_connection())
    # __gtype__ cua chinh lop: GLib.type_from_name da bi bo o pygobject moi.
    factory.add_engine("openkey-spike", SpikeEngine.__gtype__)

    component = IBus.Component(
        name="org.freedesktop.IBus.OpenKeySpike",
        description="OpenKey spike",
        version="0.1",
        license="GPL",
        author="",
        homepage="",
        command_line="",
        textdomain="",
    )
    component.add_engine(
        IBus.EngineDesc(
            name="openkey-spike",
            longname="OpenKey (spike)",
            description="Spike dang ky dong",
            language="vi",
            license="GPL",
            author="",
            icon="",
            layout="us",
        )
    )
    if not bus.register_component(component):
        raise SystemExit("register_component that bai")
    print("da dang ky component, dang cho ibus goi CreateEngine", flush=True)

    bus.set_global_engine_async("openkey-spike", -1, None, None, None)
    GLib.MainLoop().run()


if __name__ == "__main__":
    main()

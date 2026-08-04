#!/usr/bin/env python3
#
#  ibus_spike.py
#  Công cụ chẩn đoán, không phải một phần của ứng dụng.
#
#  Đăng ký động một engine tên "openkey-spike" với ibus-daemon rồi in ra mọi
#  phím nhận được. Dùng để trả lời hai câu hỏi trước khi viết backend C++:
#    1. GNOME có chấp nhận engine đăng ký động không (không có file component
#       XML nào cả)?
#    2. keycode mà IBus gửi là keycode evdev hay keycode X11 (evdev + 8)?
#
#  Chạy:  python3 ibus_spike.py
#  Rồi chọn "OpenKey (spike)" trong Settings > Keyboard > Input Sources,
#  hoặc đổi bằng Super+Space, và gõ vào một ô nhập bất kỳ.
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
            f"{'nhả' if released else 'bấm'}",
            flush=True,
        )
        # Trả về False để ứng dụng vẫn nhận được phím gốc: spike chỉ quan sát.
        return False

    def do_focus_in(self):
        print("focus_in", flush=True)

    def do_focus_out(self):
        print("focus_out", flush=True)

    def do_set_capabilities(self, caps):
        # Bit 1<<5 là surrounding text. Cần biết để sau này chọn đường xoá.
        print(f"capabilities=0x{caps:08x} surrounding={bool(caps & (1 << 5))}", flush=True)


def main():
    IBus.init()
    bus = IBus.Bus()
    if not bus.is_connected():
        raise SystemExit("ibus-daemon chưa chạy. Chạy trước: ibus-daemon -drx")

    bus.connect("disconnected", lambda *_: GLib.MainLoop().quit())

    factory = IBus.Factory.new(bus.get_connection())
    # __gtype__ của chính lớp: GLib.type_from_name đã bị bỏ ở pygobject mới.
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
            description="Spike đăng ký động",
            language="vi",
            license="GPL",
            author="",
            icon="",
            layout="us",
        )
    )
    if not bus.register_component(component):
        raise SystemExit("register_component thất bại")
    print("đã đăng ký component, đang chờ ibus gọi CreateEngine", flush=True)

    bus.set_global_engine_async("openkey-spike", -1, None, None, None)
    GLib.MainLoop().run()


if __name__ == "__main__":
    main()

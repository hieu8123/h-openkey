#!/usr/bin/env python3
#
#  ibus_dump_variants.py
#  Công cụ chẩn đoán. In ra chữ ký GVariant thật của các đối tượng IBus, để
#  backend C++ dựng lại cho đúng thay vì đoán.
#
import gi

gi.require_version("IBus", "1.0")
from gi.repository import IBus  # noqa: E402


def dump(label, obj):
    variant = obj.serialize_object()
    print(f"--- {label}")
    print(f"signature: {variant.get_type_string()}")
    print(f"value    : {variant.print_(True)}")
    print()


def main():
    IBus.init()

    desc = IBus.EngineDesc(
        name="openkey",
        longname="OpenKey",
        description="Bộ gõ tiếng Việt",
        language="vi",
        license="GPL",
        author="hieulc",
        icon="h-openkey",
        layout="us",
    )
    dump("IBusEngineDesc", desc)

    component = IBus.Component(
        name="org.freedesktop.IBus.OpenKey",
        description="H-OpenKey",
        version="1.2.1",
        license="GPL",
        author="hieulc",
        homepage="https://github.com/hieu8123/OpenKey",
        command_line="",
        textdomain="",
    )
    component.add_engine(desc)
    dump("IBusComponent", component)

    dump("IBusText", IBus.Text.new_from_string("tiếng Việt"))


if __name__ == "__main__":
    main()

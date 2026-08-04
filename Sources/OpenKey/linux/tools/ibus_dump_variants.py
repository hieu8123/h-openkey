#!/usr/bin/env python3
#
#  ibus_dump_variants.py
#  Cong cu chan doan. In ra chu ky GVariant that cua cac doi tuong IBus, de
#  backend C++ dung lai cho dung thay vi doan.
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
        description="Bo go tieng Viet",
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

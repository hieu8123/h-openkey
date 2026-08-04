# Chu ky GVariant cua cac doi tuong IBus

Backend IBus tu noi DBus nen phai tu dung dung cau truc GVariant ma ibus-daemon
mong doi. Doan chu ky nay la nguon loi rat kho chan doan: daemon chi im lang tu
choi, khong bao gi ca.

Cac chu ky duoi day lay tu chinh thu vien IBus tren may that bang
`tools/ibus_dump_variants.py`. Khi len IBus phien ban moi ma dang ky component
that bai, viec dau tien la chay lai script do va doi chieu.

IBus phien ban: **1.5.29-rc2**

## IBusEngineDesc

```
signature: (sa{sv}ssssssssussssssss)
value    : ('IBusEngineDesc', @a{sv} {}, 'openkey', 'OpenKey', 'Bo go tieng Viet',
            'vi', 'GPL', 'hieulc', 'h-openkey', 'us', uint32 0,
            '', '', '', '', '', '', '', '')
```

Thu tu truong:

| # | Kieu | Truong | Gia tri cua OpenKey |
| --- | --- | --- | --- |
| 1 | s | ten lop | `IBusEngineDesc` |
| 2 | a{sv} | attachments | rong |
| 3 | s | name | `openkey` |
| 4 | s | longname | `OpenKey` |
| 5 | s | description | `Bo go tieng Viet` |
| 6 | s | language | `vi` |
| 7 | s | license | `GPL` |
| 8 | s | author | `hieulc` |
| 9 | s | icon | `h-openkey` |
| 10 | s | layout | `us` |
| 11 | u | rank | `0` |
| 12-19 | s | hotkeys, symbol, setup, layout_variant, layout_option, version, textdomain, icon_prop_key | rong het |

Tam chuoi rong cuoi cung **khong duoc bo**: thieu mot cai la sai chu ky.

## IBusComponent

```
signature: (sa{sv}ssssssssavav)
value    : ('IBusComponent', @a{sv} {}, 'org.freedesktop.IBus.OpenKey', 'H-OpenKey',
            '1.2.1', 'GPL', 'hieulc', 'https://github.com/hieu8123/OpenKey', '', '',
            @av [], [<IBusEngineDesc...>])
```

Thu tu truong:

| # | Kieu | Truong | Gia tri cua OpenKey |
| --- | --- | --- | --- |
| 1 | s | ten lop | `IBusComponent` |
| 2 | a{sv} | attachments | rong |
| 3 | s | name | `org.freedesktop.IBus.OpenKey` |
| 4 | s | description | `H-OpenKey` |
| 5 | s | version | phien ban hien tai |
| 6 | s | license | `GPL` |
| 7 | s | author | `hieulc` |
| 8 | s | homepage | URL repo |
| 9 | s | exec | **rong** — ta dang ky dong, khong de daemon spawn tien trinh |
| 10 | s | textdomain | rong |
| 11 | av | observed_paths | rong |
| 12 | av | engines | mot phan tu, la IBusEngineDesc o tren |

## IBusText

```
signature: (sa{sv}sv)
value    : ('IBusText', @a{sv} {}, 'tiếng Việt', <('IBusAttrList', @a{sv} {}, @av [])>)
```

| # | Kieu | Truong |
| --- | --- | --- |
| 1 | s | ten lop, `IBusText` |
| 2 | a{sv} | attachments, rong |
| 3 | s | noi dung chu, UTF-8 |
| 4 | v | IBusAttrList: `('IBusAttrList', a{sv} rong, av rong)` |

OpenKey khong bao gio dung thuoc tinh hien thi (gach chan, mau) vi khong dung
preedit, nen IBusAttrList luon rong.

## Ket qua do khac tu spike

- `capabilities` cua ung dung GTK do duoc: `0x29`, tuc **co** bit surrounding
  text (`1<<5`). Nghia la duong `DeleteSurroundingText` dung duoc that, khong
  phai luon roi xuong BackSpace.
- GNOME **chap nhan engine dang ky dong**: khong can file component XML nao,
  `ibus engine` bao dung ten engine vua dang ky.
- `ibus list-engine` **khong** liet ke engine dang ky dong. Do la binh thuong,
  no chi doc cac component cai san bang XML. Dung dieu nay lam phep thu.

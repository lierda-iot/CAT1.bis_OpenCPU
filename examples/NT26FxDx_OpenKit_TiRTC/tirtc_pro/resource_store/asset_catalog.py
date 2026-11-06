"""The complete, versioned Pro UI resource set used by generation and packaging."""

# Macro prefix, installer symbol suffix, filename. Old bitmap names stay stable;
# additional resources are required by the matching new application.
RESOURCE_SPECS = (
    ("FONT", "font", "ui14-font.bin"),
    ("BACKGROUND", "background", "ui13-bg.bin"),
    ("FONT_META", "font_meta", "ui14-meta.bin"),
    ("FONT_DIGITS", "font_digits", "ui48-digits.bin"),
    ("ICON_PHONE", "icon_phone", "ui-phone.bin"),
    ("ICON_WECHAT", "icon_wechat", "ui-wechat.bin"),
    ("FACE_DATA", "face_data", "ui-face-v1.bin"),
)

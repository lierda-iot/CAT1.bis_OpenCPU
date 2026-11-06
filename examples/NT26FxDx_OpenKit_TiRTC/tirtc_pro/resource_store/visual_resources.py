"""Losslessly export Pro's fixed icon pixels and procedural face tables.

The v1 face format uses explicit little-endian scalar fields, never C pointers.
Its record padding matches the 32-bit target's minimum structure size so the
loader can expand records backwards in the same static allocation on either
32-bit target or 64-bit host. Original C tables remain the offline reference.
"""
from __future__ import annotations

import ast
from pathlib import Path
import re
import struct


KEYS = ("neutral", "happy", "laughing", "funny", "sad", "angry", "crying",
        "loving", "embarrassed", "surprised", "shocked", "thinking", "winking",
        "cool", "relaxed", "delicious", "kissy", "confident", "sleepy", "silly",
        "confused", "listening", "ambient")
FACE_COUNT = 23
MOTION_VALUES = 9
VARIANTS = 2
STEPS = 4
FOOTER_MAGIC = b"FACEv1\0\0"


def split_fields(text: str) -> list[str]:
    """Split a C initializer at top-level commas, preserving nested macros."""
    result, begin, depth, quoted = [], 0, 0, False
    for index, char in enumerate(text):
        if char == '"':
            quoted = not quoted
        elif not quoted:
            if char in "({[":
                depth += 1
            elif char in ")}]":
                depth -= 1
            elif char == "," and depth == 0:
                if text[begin:index].strip():
                    result.append(text[begin:index].strip())
                begin = index + 1
    if quoted or depth:
        raise ValueError("Unbalanced visual resource initializer")
    if text[begin:].strip():
        result.append(text[begin:].strip())
    return result


def braced(text: str) -> list[str]:
    if not (text.startswith("{") and text.endswith("}")):
        raise ValueError("Expected a braced visual resource initializer")
    return split_fields(text[1:-1])


def c_array(source: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\s*\[\s*\]\s*=\s*(\{.*?\});", source, re.S)
    if not match:
        raise ValueError("Missing original visual array: " + name)
    return re.sub(r"/\*.*?\*/", "", match[1], flags=re.S)


def number(text: str) -> int:
    """Evaluate only the integer arithmetic used by these immutable tables."""
    def walk(node):
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.UnaryOp):
            value = walk(node.operand)
            if isinstance(node.op, ast.USub):
                return -value
            if isinstance(node.op, ast.UAdd):
                return value
        if isinstance(node, ast.BinOp):
            left, right = walk(node.left), walk(node.right)
            if isinstance(node.op, ast.Add): return left + right
            if isinstance(node.op, ast.Sub): return left - right
            if isinstance(node.op, ast.Mult): return left * right
            if isinstance(node.op, ast.Div):
                if not right: raise ValueError("Division by zero")
                return (abs(left) // abs(right)) * (-1 if (left < 0) != (right < 0) else 1)
            if isinstance(node.op, ast.RShift): return left >> right
            if isinstance(node.op, ast.BitAnd): return left & right
        raise ValueError("Unsupported visual resource expression: " + text)
    return walk(ast.parse(text, mode="eval").body)


def _face_indices(source: str) -> dict[str, int]:
    match = re.search(r"enum\s*\{\s*(FACE_TEARS\s*=\s*21,.*?)\s*S3_FACE_POSE_VALUES\s*\};", source, re.S)
    if not match: raise ValueError("Missing face channel enum")
    result, value = {}, 0
    for item in split_fields(match[1]):
        name, *explicit = item.split("=")
        if explicit: value = number(explicit[0].strip())
        result[name.strip()] = value
        value += 1
    result["S3_FACE_POSE_VALUES"] = value
    return result


def _check_macros(source: str) -> None:
    expected = {
        "FP": "((x)*16+(w)*8),((y)*16+(h)*8),(w)*16,(h)*16,(r)*16,(s)*16,(b)*16",
        "EL": "FP(50-(w)/2,50-(h)/2,w,h,r,s,b)",
        "ER": "FP(170-(w)/2,50-(h)/2,w,h,r,s,b)",
        "MP": "FP(110-(w)/2,86-(h)/2,w,h,r,0,b)",
        "FX": "(tear)*16,(think)*16,(blush)*16,(heart)*16,(tongue)*16,(tilt)*16,(((c)>>16)&255)*16,(((c)>>8)&255)*16,((c)&255)*16",
        "BL": "FP(58-(w)/2,48-(h)/2,w,h,r,s,b)",
        "BR": "FP(162-(w)/2,48-(h)/2,w,h,r,s,b)",
        "BM": "FP(110-(w)/2,83-(h)/2,w,h,r,0,b)",
        "BX": "FX(tear,think,blush,heart,tongue,tilt,c),[FACE_STYLE_B]=256*16",
        "MK": "{t,{gx,gy,eh,eb,mw,mh,my,mt,lean}}",
    }
    joined = source.replace("\\\n", "")
    for name, value in expected.items():
        match = re.search(r"(?m)^#define\s+" + name + r"\([^\n]*?\)\s+([^\n]+)$", joined)
        if not match or re.sub(r"\s+", "", match[1]) != value:
            raise ValueError("Face macro changed; review v1 export: " + name)


def _macro(text: str, indices: dict[str, int]) -> list[tuple[int | None, int]]:
    match = re.fullmatch(r"([A-Z]+)\((.*)\)", text)
    if not match: return [(None, number(text))]
    name, arguments = match.groups()
    a = [number(item) for item in split_fields(arguments)]
    if name in ("FX", "BX"):
        if len(a) != 7: raise ValueError("Invalid face colour macro")
        values = [v * 16 for v in a[:6]] + [((a[6] >> s) & 255) * 16 for s in (16, 8, 0)]
        result = [(None, value) for value in values]
        if name == "BX": result.append((indices["FACE_STYLE_B"], 4096))
        return result
    if name in ("EL", "ER", "BL", "BR"):
        if len(a) != 5: raise ValueError("Invalid eye macro")
        w, h, r, s, b = a
        cx, cy = {"EL": (50, 50), "ER": (170, 50), "BL": (58, 48), "BR": (162, 48)}[name]
        a = [cx - w // 2, cy - h // 2, w, h, r, s, b]
    elif name in ("MP", "BM"):
        if len(a) != 4: raise ValueError("Invalid mouth macro")
        w, h, r, b = a
        a = [110 - w // 2, (86 if name == "MP" else 83) - h // 2, w, h, r, 0, b]
    elif name != "FP":
        raise ValueError("Unknown face macro: " + name)
    if len(a) != 7: raise ValueError("Invalid geometry macro")
    x, y, w, h, r, s, b = a
    return [(None, value) for value in (x * 16 + w * 8, y * 16 + h * 8, w * 16, h * 16, r * 16, s * 16, b * 16)]


def face_blob(source: str) -> bytes:
    _check_macros(source)
    indices = _face_indices(source)
    values = indices["S3_FACE_POSE_VALUES"]
    result = bytearray()
    pose_format = "<I" + "h" * values + "H"
    pose_size = (struct.calcsize(pose_format) + 3) & ~3
    for table in ("s3_face_poses", "s3_face_alternate_poses"):
        rows = braced(c_array(source, table))
        if len(rows) != FACE_COUNT: raise ValueError("Expected 23 face poses")
        for index, row in enumerate(rows):
            fields = braced(row)
            data = [0] * values
            if fields == ["0"]:
                if table != "s3_face_alternate_poses" or index not in (11, 14):
                    raise ValueError("Unexpected unavailable face variant")
                key, transition = 0xffffffff, 0
            else:
                if len(fields) != 3 or ast.literal_eval(fields[0]) != KEYS[index]:
                    raise ValueError("Face key order changed")
                key, transition, cursor = index, number(fields[2]), 0
                for expression in braced(fields[1]):
                    designated = re.fullmatch(r"\[([A-Z_]+)\]\s*=\s*(.*)", expression)
                    if designated:
                        cursor = indices[designated[1]]
                        entries = [(None, number(designated[2]))]
                    else:
                        entries = _macro(expression, indices)
                    for destination, value in entries:
                        if destination is not None: cursor = destination
                        if not 0 <= cursor < values: raise ValueError("Face channel out of range")
                        data[cursor] = value
                        cursor += 1
            encoded = struct.pack(pose_format, key, *data, transition)
            result += encoded + bytes(pose_size - len(encoded))
    rows = braced(c_array(source, "s3_face_sequences"))
    if len(rows) != FACE_COUNT: raise ValueError("Expected 23 face sequences")
    for index, row in enumerate(rows):
        key, variants = braced(row)
        if ast.literal_eval(key) != KEYS[index]: raise ValueError("Sequence key order changed")
        result += struct.pack("<I", index)
        tracks = braced(variants)
        if len(tracks) != (1 if index in (11, 14) else VARIANTS):
            raise ValueError("Unexpected face track count")
        for track in tracks:
            end, frames = braced(track)
            result += struct.pack("<H", number(end))
            frames = braced(frames)
            if len(frames) != STEPS: raise ValueError("Expected four face keyframes")
            previous = 0
            for frame in frames:
                match = re.fullmatch(r"MK\((.*)\)", frame)
                if not match: raise ValueError("Unknown keyframe syntax")
                numbers = [number(item) for item in split_fields(match[1])]
                if len(numbers) != 10 or not previous < numbers[0] < number(end):
                    raise ValueError("Invalid face sequence timing")
                previous = numbers[0]
                result += struct.pack("<H9bx", *numbers)
        if len(tracks) == 1:
            result += bytes(2 + STEPS * 12)
    result += FOOTER_MAGIC + struct.pack("<4H", FACE_COUNT, values, VARIANTS, STEPS)
    return bytes(result)


def resources(app: Path) -> list[tuple[str, str, str, bytes]]:
    source = (app / "ui/assets/modern_icons.c").read_text(encoding="utf-8")
    result = []
    for key, suffix, filename, array, expected in (
        ("ICON_PHONE", "icon_phone", "ui-phone.bin", "p4_phone_shortcut_pixels", 5292),
        ("ICON_WECHAT", "icon_wechat", "ui-wechat.bin", "s3_wechat_qr_pixels", 4448),
    ):
        payload = bytes(int(item, 16) for item in re.findall(r"0x([0-9a-fA-F]+)", c_array(source, array)))
        if len(payload) != expected: raise ValueError("Icon geometry changed: " + key)
        result.append((key, suffix, filename, payload))
    face = face_blob((app / "ui/assets/ui_face_engine.inc").read_text(encoding="utf-8"))
    result.append(("FACE_DATA", "face_data", "ui-face-v1.bin", face))
    return result

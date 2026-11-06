#!/usr/bin/env python3
"""Compare external visual resources with their original C reference tables.

Exercises the production in-place face loader and icon loader with host file
reads, including missing/truncated/corrupt assets and retry. Checks all 23 A/B
poses, all sequence fields and every icon byte; no device or network is used.
"""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

LVGL_STUB = r'''
#ifndef TEST_LVGL_H
#define TEST_LVGL_H
#include <stdbool.h>
#include <stdint.h>
#define LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_IMG_CF_TRUE_COLOR_ALPHA 1
#define LV_IMG_CF_INDEXED_1BIT 2
typedef struct { struct { unsigned always_zero,reserved,w,h,cf; } header;
    unsigned data_size; const uint8_t *data; } lv_img_dsc_t;
#endif
'''

FAKE_IO = r'''
static const char *root_path, *fail_path;
static unsigned mode, read_calls, progress_bytes;
static uint32_t crc32_bytes(const uint8_t *data,uint32_t length) {
    uint32_t crc=UINT32_MAX;
    while(length--) { crc^=*data++; for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320U & (0U-(crc&1U))); }
    return crc^UINT32_MAX;
}
static void progress(uint32_t length) { progress_bytes+=length; }
int tirtc_asset_read_verified(const char *path,uint8_t *out,uint32_t size,uint32_t crc,void (*notify)(uint32_t)) {
    char full[2048]; FILE *file; bool inject=fail_path && !strcmp(path,fail_path);
    ++read_calls;
    if(inject && mode==1U)return -2;
    snprintf(full,sizeof(full),"%s%s",root_path,path);file=fopen(full,"rb");assert(file && out);
    assert(fread(out,1,size,file)==size); assert(fgetc(file)==EOF);fclose(file);
    if(inject && mode==2U)return -74;
    if(inject && mode==3U)out[0]^=1U;
    if(crc32_bytes(out,size)!=crc)return -74;
    /* Inject after CRC solely to exercise the loader's independent format
     * guards, not to imply corrupted bytes can pass the real CRC reader. */
    if(inject && mode==4U)out[size-16U]^=1U;
    if(inject && mode==5U)out[0]=255U;
    if(inject && mode==6U)out[23U*2U*116U+4U]=out[23U*2U*116U+5U]=0U;
    if(notify)notify(size);return 0;
}
'''

MAIN = r'''
static void compare_pose(const s3_face_pose_t *a,const s3_face_pose_t *b) {
    assert((a->key==NULL)==(b->key==NULL));if(a->key)assert(!strcmp(a->key,b->key));
    assert(a->transition_ms==b->transition_ms);
    for(unsigned v=0;v<S3_FACE_POSE_VALUES;++v)assert(a->value[v]==b->value[v]);
}
static void compare_faces(void) {
    for(unsigned i=0;i<23U;++i) {
        compare_pose(&s3_face_poses[i],&reference_poses[i]);
        compare_pose(&s3_face_alternate_poses[i],&reference_alternate[i]);
        assert(!strcmp(s3_face_sequences[i].key,reference_sequences[i].key));
        for(unsigned v=0;v<2U;++v) {
            const s3_face_track_t *a=&s3_face_sequences[i].variant[v], *b=&reference_sequences[i].variant[v];
            assert(a->end_ms==b->end_ms);
            for(unsigned f=0;f<4U;++f) {
                assert(a->frame[f].at_ms==b->frame[f].at_ms);
                for(unsigned j=0;j<9U;++j)assert(a->frame[f].value[j]==b->frame[f].value[j]);
            }
        }
    }
}
int main(int argc,char **argv) {
    assert(argc==2);root_path=argv[1];
    for(mode=1U;mode<=6U;++mode) {
        unsigned failure_mode=mode;
        s3_face_resources_ready=false;fail_path="/ui-face-v1.bin";
        assert(ui_face_load_external(progress)==(mode==1U?-2:-74));
        assert(!s3_face_resources_ready);
        mode=0;assert(ui_face_load_external(progress)==0);assert(s3_face_resources_ready);
        compare_faces();mode=failure_mode;
    }
    unsigned before=read_calls;assert(ui_face_load_external(progress)==0);assert(read_calls==before);
    for(unsigned asset=0;asset<2U;++asset) {
        fail_path=asset?"/ui-wechat.bin":"/ui-phone.bin";
        for(mode=1U;mode<=3U;++mode)assert(tirtc_icons_load_external(progress)==(mode==1U?-2:-74));
    }
    mode=0;assert(tirtc_icons_load_external(progress)==0);
    assert(((uintptr_t)ui_phone_image.data & 3U)==0U);
    assert(((uintptr_t)ui_wechat_qr_image.data & 3U)==0U);
    assert(ui_phone_image.data_size==sizeof(reference_phone));
    assert(ui_wechat_qr_image.data_size==sizeof(reference_wechat));
    assert(!memcmp(ui_phone_image.data,reference_phone,sizeof(reference_phone)));
    assert(!memcmp(ui_wechat_qr_image.data,reference_wechat,sizeof(reference_wechat)));
    assert(ui_phone_image.header.w==42U && ui_phone_image.header.h==42U);
    assert(ui_wechat_qr_image.header.w==185U && ui_wechat_qr_image.header.h==185U);
    before=read_calls;mode=1;assert(tirtc_icons_load_external(progress)==0);assert(read_calls==before);
    printf("PASS: all 23 A/B poses, 46 tracks / 184 keyframes, 9740 icon bytes; 12 failure/retry cases, immutable loaded BSS; native table bytes=%zu\n",sizeof(s3_face_resource.tables));
    return 0;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc")
    parser.add_argument("--target", help="Optional clang/Zig target, e.g. x86-windows-gnu for a 32-bit layout run")
    parser.add_argument("--negative-control", action="store_true",
                        help="Deliberately corrupt a face resource; this run must fail")
    args = parser.parse_args()
    app = Path(__file__).resolve().parents[1]
    spec = importlib.util.spec_from_file_location("visual_resources", app / "resource_store/visual_resources.py")
    visual = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(visual)
    exported = visual.resources(app)
    engine = (app / "ui/assets/ui_face_engine.inc").read_text(encoding="utf-8")
    prefix = engine[:engine.index("typedef struct { int16_t x0, y0, x1, y1; } s3_face_bounds_t;")]
    prefix = prefix.replace('../../resource_store/asset_io.h', 'asset_io.h').replace('../../resource_store/asset_manifest.h', 'asset_manifest.h')
    icons = (app / "ui/assets/modern_icons.c").read_text(encoding="utf-8")
    definitions = []
    for name in ("FP", "EL", "ER", "MP", "FX", "BL", "BR", "BM", "BX", "MK"):
        match = re.search(r"(?m)^#define " + name + r"\([^\n]*", engine.replace("\\\n", ""))
        if not match: raise AssertionError("Missing reference macro " + name)
        definitions.append(match[0])
    for typename, name, target in (
        ("s3_face_pose_t", "s3_face_poses", "reference_poses"),
        ("s3_face_pose_t", "s3_face_alternate_poses", "reference_alternate"),
        ("s3_face_sequence_t", "s3_face_sequences", "reference_sequences"),
    ):
        definitions.append("static const " + typename + " " + target + "[] = " + visual.c_array(engine, name) + ";")
    for original, target in (("p4_phone_shortcut_pixels", "reference_phone"), ("s3_wechat_qr_pixels", "reference_wechat")):
        definitions.append("static const uint8_t " + target + "[] = " + visual.c_array(icons, original) + ";")
    cc = [args.cc] + (["cc"] if Path(args.cc).stem.lower() == "zig" else [])
    if args.target:
        cc += ["-target", args.target]
    with tempfile.TemporaryDirectory(prefix="pro-visual-test-") as temporary:
        work = Path(temporary)
        (work / "lvgl.h").write_text(LVGL_STUB, encoding="utf-8")
        for _, _, name, payload in exported:
            (work / name).write_bytes(payload)
        if args.negative_control:
            path = work / "ui-face-v1.bin"
            broken = bytearray(path.read_bytes())
            broken[20] ^= 1
            path.write_bytes(broken)
        # Zig release optimization can predefine NDEBUG. Test assertions must
        # stay active independently of compiler defaults or optimization flags.
        source = '#ifdef NDEBUG\n#undef NDEBUG\n#endif\n#include <assert.h>\n#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\n'
        source += '#define TIRTC_EXTERNAL_UI_ASSETS 1\n#include "' + (app / "ui/assets/modern_icons.c").as_posix() + '"\n'
        source += prefix + "\n" + "\n".join(definitions) + FAKE_IO + MAIN
        path = work / "visual-test.c"
        path.write_text(source, encoding="utf-8")
        exe = work / "visual-test.exe"
        subprocess.run([*cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + str(work),
                        "-I" + str(app / "resource_store"), str(path), "-o", str(exe)], check=True)
        subprocess.run([str(exe), str(work)], check=True, timeout=30)
    print("PASS: exported source tables and production loaders agree; no hardware writes")


if __name__ == "__main__":
    main()

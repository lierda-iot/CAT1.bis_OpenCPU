#!/usr/bin/env python3
"""Compare external fonts against original arrays through real LVGL.

Only temporary host files are written. The production asset reader is linked
against an in-memory storage device to exercise failed/truncated/CRC reads.
"""
from pathlib import Path
import argparse
import importlib.util
import re
import subprocess
import sys
import tempfile
import zlib

APP = Path(__file__).resolve().parents[1]
ROOT = APP.parents[2]
LVGL = ROOT / "components/thirdparty/lvgl/lvgl-8.3.10"
sys.path.insert(0, str(APP / "resource_store"))
from font_resources import resources, numbers, metadata

CHECKER = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui/font/tirtc_font_14.h"
#include "ui/font/tirtc_font_digits_48.h"
#include "resource_store/asset_io.h"
#include "resource_store/asset_manifest.h"
#include "storage/tirtc_storage.h"
#include "codepoints.h"
LV_FONT_DECLARE(reference_font_14);
#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static uint8_t meta[TIRTC_FONT_META_BYTES], saved_meta[TIRTC_FONT_META_BYTES];
static uint8_t digits[TIRTC_FONT_DIGITS_BYTES], saved_digits[TIRTC_FONT_DIGITS_BYTES];
static uint8_t pixels[TIRTC_FONT_BYTES];
static uint8_t *opened;
static uint32_t opened_size, position, progressed, bytes_read;
static int size_adjust, open_error, read_error, close_busy;
static int override_crc;
static unsigned failure_cases;
void liot_rtos_enter_critical(void) {}
void liot_rtos_exit_critical(void) {}
void liot_rtos_task_sleep_ms(uint32_t ms) { (void)ms; }
/* lv_txt_get_size itself does not allocate; these satisfy other lv_txt helpers. */
void *lv_mem_alloc(size_t n) { return malloc(n); }
void lv_mem_free(void *p) { free(p); }
void *lv_mem_realloc(void *p, size_t n) { return realloc(p,n); }
void *lv_memcpy(void *dst, const void *src, size_t n) { return memcpy(dst,src,n); }
int tirtc_storage_open(const char *path, const char *mode) {
    (void)mode;
    if (open_error) return open_error;
    if (!strcmp(path,"/ui14-meta.bin")) { opened=meta; opened_size=sizeof(meta); }
    else if (!strcmp(path,"/ui48-digits.bin")) { opened=digits; opened_size=sizeof(digits); }
    else return -2;
    position=0; return 1;
}
int tirtc_storage_seek(int fd, int32_t offset, int whence) {
    (void)fd; REQUIRE(offset==0);
    if (whence==2) return (int)opened_size + size_adjust;
    REQUIRE(whence==0); position=0; return 0;
}
int tirtc_storage_read(int fd, void *out, uint32_t size) {
    (void)fd;
    if (read_error && position) return read_error;
    REQUIRE(size<=4096);
    if (size>97) size=97; /* valid short reads must also assemble correctly */
    if (size>opened_size-position) size=opened_size-position;
    memcpy(out,opened+position,size); position+=size; bytes_read+=size;
    return (int)size;
}
int tirtc_storage_close(int fd) {
    (void)fd; if (close_busy) { --close_busy; return TIRTC_STORAGE_E_BUSY; }
    opened=NULL; return 0;
}
int real_asset_read_verified(const char *,uint8_t *,uint32_t,uint32_t,void (*)(uint32_t));
int tirtc_asset_read_verified(const char *path,uint8_t *dst,uint32_t size,uint32_t crc,void (*cb)(uint32_t)) {
    /* Deliberately valid CRC for structurally invalid fixtures tests the native
     * decoder independently of the production reader's separate CRC defense. */
    if (override_crc && !strcmp(path,"/ui14-meta.bin"))
        crc=tirtc_asset_crc_update(UINT32_MAX,meta,sizeof(meta))^UINT32_MAX;
    return real_asset_read_verified(path,dst,size,crc,cb);
}
static void progress(uint32_t size) { progressed+=size; }
static const uint8_t *bitmap_reader(uint32_t offset,uint32_t size) {
    REQUIRE(offset<=sizeof(pixels) && size<=sizeof(pixels)-offset);
    return pixels+offset;
}
static void read_file(const char *path,uint8_t *out,size_t size) {
    FILE *f=fopen(path,"rb"); REQUIRE(f); REQUIRE(fread(out,1,size,f)==size);
    REQUIRE(fgetc(f)==EOF); REQUIRE(fclose(f)==0);
}
static void unavailable(const lv_font_t *f) {
    lv_font_glyph_dsc_t d={0};
    REQUIRE(!f->get_glyph_dsc(f,&d,'0',0));
    REQUIRE(!f->get_glyph_bitmap(f,'0'));
}
static void equal_glyph(const lv_font_t *a,const lv_font_t *b,uint32_t cp,uint32_t next) {
    lv_font_glyph_dsc_t x={0}, y={0};
    REQUIRE(lv_font_get_glyph_dsc(a,&x,cp,next)); REQUIRE(lv_font_get_glyph_dsc(b,&y,cp,next));
    REQUIRE(!x.is_placeholder && !y.is_placeholder);
    REQUIRE(x.adv_w==y.adv_w && x.box_w==y.box_w && x.box_h==y.box_h);
    REQUIRE(x.ofs_x==y.ofs_x && x.ofs_y==y.ofs_y && x.bpp==y.bpp);
    uint32_t n=((uint32_t)x.box_w*x.box_h*x.bpp+7)/8;
    if (n) {
        const uint8_t *xp=lv_font_get_glyph_bitmap(x.resolved_font,cp);
        const uint8_t *yp=lv_font_get_glyph_bitmap(y.resolved_font,cp);
        REQUIRE(xp && yp); REQUIRE(!memcmp(xp,yp,n));
    }
}
static void equal_font(const lv_font_t *a,const lv_font_t *b) {
    REQUIRE(a->line_height==b->line_height && a->base_line==b->base_line && a->subpx==b->subpx);
    REQUIRE(a->underline_position==b->underline_position && a->underline_thickness==b->underline_thickness);
}
static void equal_layout(const lv_font_t *a,const lv_font_t *b,const char *text,int width) {
    lv_point_t x,y;
    lv_txt_get_size(&x,text,a,1,2,width,LV_TEXT_FLAG_NONE);
    lv_txt_get_size(&y,text,b,1,2,width,LV_TEXT_FLAG_NONE);
    REQUIRE(x.x==y.x && x.y==y.y);
    for (uint32_t p=0;text[p];) {
        uint32_t nx=_lv_txt_get_next_line(text+p,a,1,width,NULL,LV_TEXT_FLAG_NONE);
        uint32_t ny=_lv_txt_get_next_line(text+p,b,1,width,NULL,LV_TEXT_FLAG_NONE);
        REQUIRE(nx==ny && nx); p+=nx;
    }
}
static unsigned utf8(char *out,uint32_t cp) {
    if (cp<128) { out[0]=(char)cp; return 1; }
    if (cp<2048) { out[0]=(char)(0xc0|(cp>>6)); out[1]=(char)(0x80|(cp&63)); return 2; }
    out[0]=(char)(0xe0|(cp>>12)); out[1]=(char)(0x80|((cp>>6)&63)); out[2]=(char)(0x80|(cp&63)); return 3;
}
static void failure(int digit,int expected) {
    int got=digit?tirtc_font_digits_load_external(progress):tirtc_font_load_external(progress);
    REQUIRE(got==expected);
    unavailable(digit?&tirtc_font_digits_48:&tirtc_font_14);
    ++failure_cases;
}
int main(int argc,char **argv) {
    REQUIRE(argc==4);
    read_file(argv[1],meta,sizeof(meta)); memcpy(saved_meta,meta,sizeof(meta));
    read_file(argv[2],digits,sizeof(digits)); memcpy(saved_digits,digits,sizeof(digits));
    read_file(argv[3],pixels,sizeof(pixels));
    unavailable(&tirtc_font_14); unavailable(&tirtc_font_digits_48);
    REQUIRE(tirtc_font_set_reader(bitmap_reader,sizeof(pixels))!=0);
    REQUIRE(tirtc_font_set_bitmap(pixels,sizeof(pixels))!=0);
    REQUIRE(!tirtc_font_load_external(progress));
    REQUIRE(progressed==sizeof(meta));
    REQUIRE(!tirtc_font_digits_load_external(progress));
    REQUIRE(progressed==sizeof(meta)+sizeof(digits));
    REQUIRE(!tirtc_font_set_reader(bitmap_reader,sizeof(pixels)));
    equal_font(&reference_font_14,&tirtc_font_14);
    unsigned glyph_cases=0,layout_cases=0;
    for (unsigned i=0;i<sizeof(codepoints)/sizeof(codepoints[0]);++i) {
        equal_glyph(&reference_font_14,&tirtc_font_14,codepoints[i],0);
        equal_glyph(&reference_font_14,&tirtc_font_14,codepoints[i],codepoints[(i+1)%7010]);
        glyph_cases+=2;
    }
    equal_glyph(&reference_font_14,&tirtc_font_14,'\t',0);
    for (unsigned i=0;i<7010;i+=16) {
        char row[16*3+1]; unsigned length=0;
        for (unsigned j=i;j<i+16 && j<7010;++j) length+=utf8(row+length,codepoints[j]);
        row[length]=0;
        const int widths[]={40,180,240};
        for (unsigned k=0;k<3;++k) { equal_layout(&reference_font_14,&tirtc_font_14,row,widths[k]); ++layout_cases; }
    }
    const char required[]=" -0123456789:";
    unsigned digit_cases=0;
    equal_font(&lv_font_montserrat_48,&tirtc_font_digits_48);
    for (unsigned i=0;i<sizeof(required)-1;++i) for(unsigned j=0;j<sizeof(required);++j) {
        equal_glyph(&lv_font_montserrat_48,&tirtc_font_digits_48,required[i],required[j]); ++digit_cases;
    }
    const char *labels[]={"12:34","00:00","99:59","123456","------"};
    for (unsigned i=0;i<5;++i) { equal_layout(&lv_font_montserrat_48,&tirtc_font_digits_48,labels[i],160); ++layout_cases; }
    const uint32_t missing[]={0,'!','.','/',';','A',0x4e2d,UINT32_MAX};
    for (unsigned i=0;i<sizeof(missing)/sizeof(missing[0]);++i) {
        lv_font_glyph_dsc_t d={0};
        REQUIRE(!tirtc_font_digits_48.get_glyph_dsc(&tirtc_font_digits_48,&d,missing[i],0));
        REQUIRE(!tirtc_font_digits_48.get_glyph_bitmap(&tirtc_font_digits_48,missing[i]));
    }
    for (int digit=0;digit<2;++digit) {
        open_error=-2; failure(digit,-2); open_error=0;
        size_adjust=-1; failure(digit,TIRTC_ASSET_E_MISMATCH); size_adjust=0;
        size_adjust=1; failure(digit,TIRTC_ASSET_E_MISMATCH); size_adjust=0;
        read_error=-5; failure(digit,-5); read_error=0;
        uint8_t *p=digit?digits:meta; p[10]^=1;
        failure(digit,TIRTC_ASSET_E_MISMATCH); p[10]^=1;
        close_busy=1; failure(digit,TIRTC_STORAGE_E_BUSY);
        REQUIRE(!(digit?tirtc_font_digits_load_external(progress):tirtc_font_load_external(progress)));
    }
    override_crc=1;
    /* Dummy, non-contiguous/overflowing index, width overrun, duplicate cmap,
     * out-of-range cmap, and incomplete final bitmap coverage. */
    const unsigned positions[]={0,16,16,56080+4,56088+2,70106,56080};
    const uint8_t values[]={1,1,255,255,0,255,0};
    for (unsigned i=0;i<sizeof(positions)/sizeof(positions[0]);++i) {
        memcpy(meta,saved_meta,sizeof(meta)); meta[positions[i]]=values[i];
        if (i==4) meta[56088+3]=0;
        if (i==5) meta[70107]=255;
        failure(0,TIRTC_ASSET_E_MISMATCH);
    }
    override_crc=0; memcpy(meta,saved_meta,sizeof(meta));
    REQUIRE(!tirtc_font_load_external(progress)); REQUIRE(!tirtc_font_digits_load_external(progress));
    REQUIRE(!tirtc_font_set_bitmap(pixels,sizeof(pixels)));
    equal_glyph(&reference_font_14,&tirtc_font_14,0x4e2d,0);
    printf("PASS external fonts: 7010 Chinese glyphs (%u metric/pixel cases), %u digit cases, %u layouts, %u failure/recovery cases\n",glyph_cases,digit_cases,layout_cases,failure_cases);
    return 0;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", required=True, help="Native compiler executable, including zig.exe")
    args = parser.parse_args()
    extracted = resources(APP)
    source = (APP / "ui/font/tirtc_font_14.c").read_text(encoding="utf-8")
    offsets = numbers(source, "unicode_list_0")
    assert len(offsets) == 7010
    # Regeneration must retain the external loader without rewriting production files.
    for module_name in ("prepare_font", "prepare_digits_font"):
        spec = importlib.util.spec_from_file_location(module_name, APP / f"ui/tools/{module_name}.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        if module_name == "prepare_digits_font":
            content, header = module.generate(module.SOURCE.read_text(encoding="utf-8"))
            assert content == module.OUTPUT.read_text(encoding="utf-8")
            assert header == module.HEADER.read_text(encoding="utf-8")
            module.verify_tables(module.SOURCE.read_text(encoding="utf-8"), content)
        else:
            captured = {}
            module.write = lambda path, content: captured.update({path.name: content})
            module.build_font(argparse.Namespace(extra_text=[]))
            assert captured["tirtc_font_14.c"] == source
            assert captured["tirtc_font_14.h"] == (APP / "ui/font/tirtc_font_14.h").read_text(encoding="utf-8")
    with tempfile.TemporaryDirectory(prefix="pro-external-fonts-") as temporary:
        work = Path(temporary)
        for relative in ("ui/font/tirtc_font_14.c", "ui/font/tirtc_font_14.h",
                         "ui/font/tirtc_font_digits_48.c", "ui/font/tirtc_font_digits_48.h",
                         "resource_store/asset_io.h", "storage/tirtc_storage.h"):
            target = work / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((APP / relative).read_bytes())
        manifest = "#include <stdint.h>\n#define TIRTC_FONT_BYTES 631201U\n"
        for key, _, filename, data in extracted:
            (work / filename).write_bytes(data)
            manifest += f"#define TIRTC_{key}_BYTES {len(data)}U\n#define TIRTC_{key}_CRC32 0x{zlib.crc32(data):08x}U\n"
        (work / "resource_store/asset_manifest.h").write_text(manifest, encoding="utf-8")
        (work / "bitmap.bin").write_bytes(bytes(numbers(source, "glyph_bitmap")))
        (work / "codepoints.h").write_text("static const uint32_t codepoints[]={" + ",".join(str(cp+32) for cp in offsets) + "};\n", encoding="utf-8")
        (work / "liot_os.h").write_text("#include <stdint.h>\nvoid liot_rtos_enter_critical(void);\nvoid liot_rtos_exit_critical(void);\nvoid liot_rtos_task_sleep_ms(uint32_t);\n", encoding="utf-8")
        config = (APP / "ui/lv_conf.h").read_text(encoding="utf-8").replace("#define LV_FONT_MONTSERRAT_48 0", "#define LV_FONT_MONTSERRAT_48 1")
        (work / "lv_conf.h").write_text(config, encoding="utf-8")
        (work / "reference.c").write_text('#undef TIRTC_EXTERNAL_UI_ASSETS\n#define tirtc_font_14 reference_font_14\n#include "' + (APP / "ui/font/tirtc_font_14.c").as_posix() + '"\n', encoding="utf-8")
        actual_io = (APP / "resource_store/asset_io.c").read_text(encoding="utf-8")
        (work / "resource_store/asset_io.c").write_text(actual_io.replace("int tirtc_asset_read_verified(", "int real_asset_read_verified("), encoding="utf-8")
        (work / "check.c").write_text(CHECKER, encoding="utf-8")
        files = [work / "check.c", work / "reference.c", work / "ui/font/tirtc_font_14.c",
                 work / "ui/font/tirtc_font_digits_48.c", work / "resource_store/asset_io.c",
                 *[LVGL / f"src/{name}" for name in ("font/lv_font.c", "font/lv_font_fmt_txt.c", "font/lv_font_montserrat_14.c", "font/lv_font_montserrat_48.c", "misc/lv_utils.c", "misc/lv_txt.c", "misc/lv_printf.c")]]
        compiler = [args.cc] + (["cc"] if Path(args.cc).stem.lower() == "zig" else [])
        binary = work / "check.exe"
        command = compiler + ["-std=c99", "-O2", "-DLV_CONF_INCLUDE_SIMPLE", "-DTIRTC_EXTERNAL_UI_ASSETS=1",
                              "-I"+str(work), "-I"+str(LVGL), *map(str, files), "-o", str(binary)]
        subprocess.run(command, check=True, timeout=120)
        subprocess.run([str(binary), str(work / extracted[0][2]), str(work / extracted[1][2]), str(work / "bitmap.bin")], check=True, timeout=60)


if __name__ == "__main__":
    main()

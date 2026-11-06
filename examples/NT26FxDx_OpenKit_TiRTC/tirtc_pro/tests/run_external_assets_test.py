#!/usr/bin/env python3
"""Exercise production external-resource I/O, startup and installer failure paths.

The filesystem and RTOS are controlled host doubles. UI module loaders use the
real verified reader but not LVGL rendering; run_external_fonts_test.py and the
visual-resource tests independently compare those real loaders and pixel data.
No device, flash, generated resource or production source is modified.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
import zlib


OS_HEADER = r'''#pragma once
#include <stdint.h>
#include <stddef.h>
typedef void *liot_task_t;
void liot_rtos_enter_critical(void);
void liot_rtos_exit_critical(void);
uint32_t liot_rtos_get_running_time(void);
void liot_rtos_task_sleep_ms(uint32_t);
void *liot_rtos_malloc(size_t);
void liot_rtos_free(void *);
int liot_rtos_task_delete(liot_task_t);
int liot_rtos_task_create(liot_task_t *,uint32_t,uint32_t,const char *,void (*)(void *),void *,uint32_t);
'''

HARNESS = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asset_io.h"
#include "asset_manifest.h"
#include "assets.h"
#include "font_cache.h"
#include "tirtc_storage.h"
#include "liot_os.h"
#define REQUIRE(test) do {if(!(test)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#test);exit(1);}} while(0)
typedef struct {const char *path;uint32_t size,crc;const uint8_t *source;} resource_t;
#define RESOURCE(key,name,path) {path,TIRTC_##key##_BYTES,TIRTC_##key##_CRC32,tirtc_install_##name},
static const resource_t resources[]={TIRTC_UI_ASSET_LIST(RESOURCE)};
#undef RESOURCE
typedef struct {uint8_t *data;uint32_t size;bool exists;} file_t;
typedef struct {int file;uint32_t position;bool write,active;} descriptor_t;
static file_t files[TIRTC_UI_ASSET_COUNT];
static descriptor_t descriptors[128];
static int next_fd=1,critical,open_count,close_count,write_count,read_count,install_ok;
static int short_reads,read_fault_index=-1,read_fault_left,read_fault_code=-5;
static int close_busy_left,close_fault,write_short,sync_error,read_overreport;
static int reenter_read,reenter_start,task_error,task_creates,task_deletes;
static int allocations,frees,allocation_calls,fail_allocation_at;
static int progress_sum,cache_init_count,worker_running,retry_count,timeout_exit;
static uint32_t clock_ms;
static void (*worker_fn)(void *);
static tirtc_storage_snapshot_t storage;
static jmp_buf worker_exit;
static bool module_verified[TIRTC_UI_ASSET_COUNT];
static uint8_t module_memory[TIRTC_FONT_META_BYTES+TIRTC_FONT_DIGITS_BYTES+
 TIRTC_ICON_PHONE_BYTES+TIRTC_ICON_WECHAT_BYTES+TIRTC_FACE_DATA_BYTES];

void liot_trace(const char *format,...) {if(strstr(format,"INSTALL_OK"))++install_ok;}
void liot_rtos_enter_critical(void) {++critical;}
void liot_rtos_exit_critical(void) {assert(critical>0);--critical;}
uint32_t liot_rtos_get_running_time(void) {return clock_ms;}
void *liot_rtos_malloc(size_t bytes) {
    ++allocation_calls;if(allocation_calls==fail_allocation_at)return NULL;
    ++allocations;return malloc(bytes);
}
void liot_rtos_free(void *p) {if(p){++frees;free(p);}}
int liot_rtos_task_create(liot_task_t *task,uint32_t stack,uint32_t priority,
 const char *name,void (*fn)(void *),void *arg,uint32_t flags) {
    (void)stack;(void)priority;(void)name;(void)arg;(void)flags;
    assert(!critical);++task_creates;
    if(reenter_start){reenter_start=0;assert(tirtc_assets_start()==0);assert(task_creates==1);}
    if(task_error)return task_error;*task=(void *)1;worker_fn=fn;return 0;
}
int liot_rtos_task_delete(liot_task_t task) {
    (void)task;assert(worker_running);++task_deletes;longjmp(worker_exit,2);return 0;
}
void liot_rtos_task_sleep_ms(uint32_t ms) {
    assert(!critical);clock_ms+=ms;
    if(worker_running) {
        tirtc_assets_snapshot_t snap;tirtc_assets_get_snapshot(&snap);
        if(snap.state==TIRTC_ASSETS_RETRY_WAIT) {
            ++retry_count;assert(!tirtc_assets_ready());assert(tirtc_assets_background()==NULL);
            assert(tirtc_assets_start()==0);assert(task_creates==1);
            if(timeout_exit)longjmp(worker_exit,3);
            assert(retry_count<4);
        }
    }
}
static int index_of(const char *path) {
    for(unsigned i=0;i<TIRTC_UI_ASSET_COUNT;++i)if(!strcmp(path,resources[i].path))return (int)i;
    return -1;
}
static descriptor_t *descriptor(int fd) {
    assert(!critical);assert(fd>0 && fd<next_fd && descriptors[fd].active);return &descriptors[fd];
}
void tirtc_storage_get_snapshot(tirtc_storage_snapshot_t *out) {*out=storage;}
void tirtc_storage_poll(void) {assert(!critical);}
int tirtc_storage_open(const char *path,const char *mode) {
    int i=index_of(path);assert(!critical);assert(i>=0);++open_count;
    if(mode[0]=='r' && !files[i].exists)return TIRTC_ASSET_E_MISSING;
    assert(next_fd<128);int fd=next_fd++;
    descriptors[fd]=(descriptor_t){i,0,mode[0]=='w',true};
    if(mode[0]=='w'){files[i].exists=true;files[i].size=0;}
    return fd;
}
int tirtc_storage_seek(int fd,int32_t offset,int whence) {
    descriptor_t *d=descriptor(fd);assert(!offset);
    d->position=whence==2?files[d->file].size:0;return (int)d->position;
}
int tirtc_storage_read(int fd,void *destination,uint32_t bytes) {
    descriptor_t *d=descriptor(fd);file_t *f=&files[d->file];++read_count;
    assert(bytes<=4096U && !d->write);
    if(reenter_read){int before=open_count;reenter_read=0;
        assert(tirtc_asset_read_verified(resources[1].path,NULL,resources[1].size,resources[1].crc,NULL)==TIRTC_STORAGE_E_BUSY);
        assert(open_count==before);}
    if(d->file==read_fault_index && read_fault_left){--read_fault_left;return read_fault_code;}
    if(read_overreport){read_overreport=0;return (int)bytes+1;}
    if(bytes>f->size-d->position)bytes=f->size-d->position;
    if(short_reads && bytes>137U)bytes=137U;
    memcpy(destination,f->data+d->position,bytes);d->position+=bytes;return (int)bytes;
}
int tirtc_storage_write(int fd,const void *source,uint32_t bytes) {
    descriptor_t *d=descriptor(fd);file_t *f=&files[d->file];assert(d->write && bytes<=4096U);++write_count;
    if(write_short){write_short=0;return (int)bytes-1;}
    assert(d->position+bytes<=resources[d->file].size);
    memcpy(f->data+d->position,source,bytes);d->position+=bytes;f->size=d->position;return (int)bytes;
}
int tirtc_storage_sync(int fd) {descriptor(fd);return sync_error;}
int tirtc_storage_close(int fd) {
    descriptor_t *d=descriptor(fd);++close_count;
    if(close_busy_left){--close_busy_left;return TIRTC_STORAGE_E_BUSY;}
    d->active=false;return close_fault;
}
static void progress(uint32_t bytes) {progress_sum+=(int)bytes;}
static int load_module(const char *path,void (*callback)(uint32_t)) {
    int i=index_of(path);assert(i>=2);assert(!tirtc_assets_ready());
    /* Fixed host storage models only the module's verified-read contract;
     * per-module layout/pixels are checked by separate real-loader tests. */
    unsigned offset=0;for(int n=2;n<i;++n)offset+=resources[n].size;
    int result=tirtc_asset_read_verified(path,module_memory+offset,resources[i].size,resources[i].crc,callback);
    module_verified[i]=result==0;return result;
}
int tirtc_font_load_external(void (*p)(uint32_t)) {return load_module("/ui14-meta.bin",p);}
int tirtc_font_digits_load_external(void (*p)(uint32_t)) {return load_module("/ui48-digits.bin",p);}
int tirtc_icons_load_external(void (*p)(uint32_t)) {
    int result=load_module("/ui-phone.bin",p);return result?result:load_module("/ui-wechat.bin",p);
}
int ui_face_load_external(void (*p)(uint32_t)) {return load_module("/ui-face-v1.bin",p);}
void tirtc_font_cache_init(uint8_t *allocation,uint32_t generation) {
    assert(allocation && generation==storage.recovery_successes);assert(!tirtc_assets_ready());
    for(unsigned i=2;i<TIRTC_UI_ASSET_COUNT;++i)assert(module_verified[i]);++cache_init_count;
}
const uint8_t *tirtc_font_cache_get(uint32_t offset,uint32_t bytes) {(void)offset;(void)bytes;return NULL;}
bool tirtc_font_cache_process(void) {return false;}
bool tirtc_font_cache_worker_step(void) {assert(tirtc_assets_ready());longjmp(worker_exit,1);return false;}
@PRODUCTION@
static void init_files(void) {
    storage.state=TIRTC_STORAGE_STATE_READY;storage.filesystem_valid=true;
    storage.filesystem_free_bytes=4U*1024U*1024U;storage.recovery_successes=3;
    for(unsigned i=0;i<TIRTC_UI_ASSET_COUNT;++i) {
        files[i].data=malloc(resources[i].size+1);assert(files[i].data);
        memcpy(files[i].data,resources[i].source,resources[i].size);
        files[i].size=resources[i].size;files[i].exists=true;
    }
}
static void no_open_files(void) {for(int i=1;i<next_fd;++i)assert(!descriptors[i].active);}
static int verify(unsigned i,uint8_t *destination) {
    return tirtc_asset_read_verified(resources[i].path,destination,resources[i].size,resources[i].crc,progress);
}
static void mutate(const char *kind,unsigned i) {
    if(strstr(kind,"missing"))files[i].exists=false;
    else if(strstr(kind,"truncated"))--files[i].size;
    else if(strstr(kind,"extra"))++files[i].size;
    else if(strstr(kind,"crc"))files[i].data[resources[i].size/2]^=1U;
}
static void run_worker(bool success) {
    assert(tirtc_assets_start()==0);assert(task_creates==1 && worker_fn);
    worker_running=1;int exit_kind=setjmp(worker_exit);
    if(!exit_kind)worker_fn(NULL);
    worker_running=0;assert(!critical);
    tirtc_assets_snapshot_t snap;tirtc_assets_get_snapshot(&snap);
    if(success) {
        assert(exit_kind==1 && tirtc_assets_ready() && snap.error==0 && cache_init_count==1);
        assert(snap.total_bytes==TIRTC_UI_ASSET_TOTAL_BYTES && snap.loaded_bytes==snap.total_bytes);
        assert(allocations-frees==2 && tirtc_assets_background()!=NULL);
        assert(!memcmp(tirtc_assets_background(),resources[1].source,resources[1].size));
        for(unsigned i=2,offset=0;i<TIRTC_UI_ASSET_COUNT;offset+=resources[i].size,++i)
            assert(!memcmp(module_memory+offset,resources[i].source,resources[i].size));
    } else {
        assert(!tirtc_assets_ready() && tirtc_assets_background()==NULL && !cache_init_count);
        assert(allocations==frees);
        if(!timeout_exit){assert(exit_kind==2 && snap.state==TIRTC_ASSETS_ERROR && snap.error<0);
            assert(tirtc_assets_start()==snap.error && task_creates==1);}
    }
    no_open_files();
}
int main(int argc,char **argv) {
    assert(argc>=2 && TIRTC_UI_ASSET_COUNT==7U);const char *kind=argv[1];
    unsigned i=argc>2?(unsigned)atoi(argv[2]):0;assert(i<TIRTC_UI_ASSET_COUNT);init_files();
    if(!strncmp(kind,"io-",3)) {
        uint8_t *buffer=malloc(resources[i].size+16);assert(buffer);memset(buffer,0xa5,resources[i].size+16);
        mutate(kind,i);if(!strcmp(kind,"io-short"))short_reads=1;
        if(!strcmp(kind,"io-nested"))reenter_read=1;
        if(!strcmp(kind,"io-eof")||!strcmp(kind,"io-error")) {
            read_fault_index=(int)i;read_fault_left=1;read_fault_code=!strcmp(kind,"io-eof")?0:TIRTC_STORAGE_E_IO;
        }
        if(!strcmp(kind,"io-overreport"))read_overreport=1;
        if(strstr(kind,"close-busy"))close_busy_left=1;
        int result=verify(i,buffer);
        if(strstr(kind,"close-busy")) {
            assert(result==TIRTC_STORAGE_E_BUSY);int opens=open_count;close_busy_left=1;
            assert(verify(i,buffer)==TIRTC_STORAGE_E_BUSY && open_count==opens);
            result=verify(i,buffer);
            if(strstr(kind,"crc"))assert(open_count==opens);else assert(open_count==opens+1);
        }
        bool bad=strstr(kind,"missing")||strstr(kind,"truncated")||strstr(kind,"extra")||strstr(kind,"crc")||
                 strstr(kind,"eof")||strstr(kind,"error")||strstr(kind,"overreport");
        if(bad)assert(result<0);else {assert(result==0);assert(!memcmp(buffer,resources[i].source,resources[i].size));
            assert(progress_sum==(int)resources[i].size*(strstr(kind,"close-busy")?2:1));}
        for(unsigned n=0;n<16;++n)assert(buffer[resources[i].size+n]==0xa5);no_open_files();free(buffer);
    } else if(!strncmp(kind,"loader-",7)) {
        mutate(kind,i);bool success=!strstr(kind,"missing")&&!strstr(kind,"truncated")&&!strstr(kind,"crc");
        if(!strcmp(kind,"loader-io-retry")){read_fault_index=(int)i;read_fault_left=1;}
        if(!strcmp(kind,"loader-memory-retry"))fail_allocation_at=(int)i+1;
        if(!strcmp(kind,"loader-close-retry"))close_busy_left=1;
        if(!strcmp(kind,"loader-start-reentrant"))reenter_start=1;
        if(!strcmp(kind,"loader-storage-error")){storage.state=TIRTC_STORAGE_STATE_UNKNOWN_DATA;storage.filesystem_valid=false;success=false;}
        if(!strcmp(kind,"loader-storage-timeout")){storage.state=TIRTC_STORAGE_STATE_PROBING;storage.filesystem_valid=false;timeout_exit=1;success=false;}
        if(!strcmp(kind,"loader-task-failure")) {
            task_error=7;assert(tirtc_assets_start()==7);assert(tirtc_assets_error()==-7 && !tirtc_assets_ready());
            task_error=0;task_creates=0;
        }
        run_worker(success);
        if(strstr(kind,"retry"))assert(retry_count==1);
    } else if(!strncmp(kind,"install-",8)) {
        mutate(kind,i);
        if(!strcmp(kind,"install-all-missing"))for(unsigned j=0;j<TIRTC_UI_ASSET_COUNT;++j)files[j].exists=false;
        bool failure=!strcmp(kind,"install-space")||!strcmp(kind,"install-protected")||
            !strcmp(kind,"install-short-write")||!strcmp(kind,"install-sync-error")||!strcmp(kind,"install-read-error");
        if(failure)files[i].exists=false;
        if(!strcmp(kind,"install-space"))storage.filesystem_free_bytes=8191U;
        if(!strcmp(kind,"install-protected"))storage.read_only=true;
        if(!strcmp(kind,"install-short-write"))write_short=1;
        if(!strcmp(kind,"install-sync-error"))sync_error=TIRTC_STORAGE_E_IO;
        if(!strcmp(kind,"install-read-error")){files[i].exists=true;read_fault_index=(int)i;read_fault_left=1;}
        int result=tirtc_assets_install();
        if(failure){assert(result<0 && !install_ok);if(strstr(kind,"space")||strstr(kind,"protected")||strstr(kind,"read-error"))assert(!write_count);}
        else {assert(result==0 && install_ok==1);for(unsigned j=0;j<TIRTC_UI_ASSET_COUNT;++j){assert(files[j].exists && files[j].size==resources[j].size);assert(!memcmp(files[j].data,resources[j].source,resources[j].size));}}
        if(!strcmp(kind,"install-valid"))assert(!write_count);
        no_open_files();
    } else if(!strcmp(kind,"invalid-arguments")) {
        assert(tirtc_asset_read_verified(NULL,NULL,1,0,NULL)==TIRTC_STORAGE_E_ARGUMENT);
        assert(tirtc_asset_read_verified("/ui14-font.bin",NULL,0,0,NULL)==TIRTC_STORAGE_E_ARGUMENT);
        assert(tirtc_asset_read_verified("/ui14-font.bin",NULL,UINT32_MAX,0,NULL)==TIRTC_STORAGE_E_ARGUMENT);
        assert(!open_count);
    } else abort();
    for(unsigned j=0;j<TIRTC_UI_ASSET_COUNT;++j)free(files[j].data);
    if(s_cache_allocation)liot_rtos_free(s_cache_allocation);
    if(s_background)liot_rtos_free(s_background);
    assert(allocations==frees && !critical);return 0;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', required=True, help='Native C compiler executable; Zig is supported')
    args = parser.parse_args()
    app = Path(__file__).resolve().parents[1]
    repo = app.parents[2]
    store = app / 'resource_store'
    header = (store / 'asset_manifest.h').read_text(encoding='utf8')
    entries = re.findall(r'X\(([A-Z_]+),\s*([a-z_]+),\s*"(/[^\"]+)"\)', header)
    assert [entry[0] for entry in entries] == [
        'FONT', 'BACKGROUND', 'FONT_META', 'FONT_DIGITS', 'ICON_PHONE', 'ICON_WECHAT', 'FACE_DATA']
    manifest = json.loads((store / 'assets14/manifest.json').read_text(encoding='utf8'))
    catalog = {item['name']: item for item in manifest['files']}
    assert len(catalog) == 7
    for key, _, name in entries:
        data = (store / 'assets14' / name[1:]).read_bytes()
        item = catalog[name[1:]]
        assert item['size'] == len(data)
        assert item['sha256'] == hashlib.sha256(data).hexdigest()
        assert item['crc32'].lower() == f'{zlib.crc32(data):08x}'
        for suffix, value in [('BYTES', len(data)), ('CRC32', zlib.crc32(data))]:
            match = re.search(r'#define TIRTC_' + key + '_' + suffix + r'\s+(0x[0-9a-fA-F]+|[0-9]+)U', header)
            assert match and int(match[1], 0) == value
    cases = [('invalid-arguments', 0)]
    for kind in ['io-success', 'io-short', 'io-missing', 'io-truncated', 'io-extra', 'io-crc',
                 'loader-missing', 'loader-truncated', 'loader-crc', 'loader-io-retry',
                 'install-missing', 'install-crc']:
        cases.extend((kind, i) for i in range(7))
    cases.extend((kind, 0) for kind in [
        'io-nested', 'io-eof', 'io-error', 'io-overreport', 'io-close-busy', 'io-crc-close-busy',
        'loader-success', 'loader-close-retry', 'loader-start-reentrant', 'loader-task-failure',
        'loader-storage-error', 'loader-storage-timeout', 'loader-memory-retry',
        'install-valid', 'install-all-missing', 'install-space', 'install-protected',
        'install-short-write', 'install-sync-error', 'install-read-error'])
    cases.append(('loader-memory-retry', 1))
    includes = '\n'.join(f'#include "{(store / name).as_posix()}"' for name in
                         ['asset_io.c', 'assets.c', 'asset_installer.c'])
    with tempfile.TemporaryDirectory(prefix='pro-external-assets-test-') as temporary:
        work = Path(temporary)
        (work / 'liot_os.h').write_text(OS_HEADER, encoding='utf8')
        (work / 'liot_log.h').write_text('#pragma once\nvoid liot_trace(const char *,...);\n', encoding='utf8')
        (work / 'verify.c').write_text(HARNESS.replace('assert(', 'REQUIRE(').replace('@PRODUCTION@', includes), encoding='utf8')
        binary = work / 'verify.exe'
        command = [args.cc] + (['cc'] if Path(args.cc).stem.lower() == 'zig' else [])
        command += ['-std=c99', '-O1', '-DTIRTC_EXTERNAL_UI_ASSETS', '-DLV_CONF_INCLUDE_SIMPLE']
        command += ['-I' + str(path) for path in [work, store, app / 'storage', app / 'ui',
                     repo / 'components/thirdparty/lvgl/lvgl-8.3.10']]
        command += [str(work / 'verify.c'), str(store / 'installer_blob.c'), '-o', str(binary)]
        subprocess.run(command, check=True, timeout=180)
        for kind, index in cases:
            result = subprocess.run([str(binary), kind, str(index)], capture_output=True,
                                    text=True, errors='replace', timeout=20)
            if result.returncode:
                raise RuntimeError(f'{kind}[{index}] failed ({result.returncode}):\n{result.stdout}{result.stderr}')
    print(f'PASS: {len(cases)} external-resource I/O/startup/installer cases; 7 real blobs/header/manifest verified')
    print('Production reader/startup/installer; controlled FS/RTOS/module loaders. No hardware timing claim.')


if __name__ == '__main__':
    main()

/* Deterministic fragmented-heap and DMA lifetime model. The allocator returns
 * ordinary aligned pointers, accounts for headers, rejects allocations beyond
 * its largest available extent, and rejects interior/double frees. It does not
 * emulate camera hardware or prove timing on a board. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tirtc_video_format.h"

typedef void *liot_task_t;
typedef void *liot_mutex_t;
typedef void *liot_camera_handle_t;
typedef int demo_tirtc_owner_e;
typedef struct { int error; } tirtc_video_snapshot_t;
#define DEMO_TIRTC_OWNER_LIVE 4
#define LIOT_WAIT_FOREVER 0xffffffffU
#define LIOT_CAMERA_SUCCESS 0
/* PRODUCTION_MACROS */
/* PRODUCTION_DECLARATIONS */

typedef struct { void *pointer; size_t bytes; bool alive; } allocation_t;
static allocation_t allocations[16];
static size_t heap_initial, heap_free, extent_limit;
static unsigned allocation_calls, allocated_count, free_calls, fail_at;
static bool fail_big, pipeline_idle=true, quiesce_ready=true;
static int camera_result;
static unsigned stop_calls, quiesce_calls, deinit_calls, encoder_destroyed;
static unsigned decoder_destroyed, published_empty;
static bool require_close_order;
static unsigned expected_decoders;
static char trace_log[4096];

static size_t liot_xPortGetFreeHeapSize(void) { return heap_free; }
static size_t liot_xPortGetMaximumFreeBlockSize(void)
{ return extent_limit < heap_free ? extent_limit : heap_free; }
static void *liot_rtos_malloc(size_t bytes)
{
    unsigned i;
    allocation_calls++;
    if (allocation_calls==fail_at || (fail_big && bytes>RAW_BYTES) ||
        bytes>extent_limit || bytes+16U>heap_free) return NULL;
    for (i=0;i<16;i++) if (!allocations[i].alive) {
        allocations[i].pointer=malloc(bytes);
        assert(allocations[i].pointer);
        assert(((uintptr_t)allocations[i].pointer&3U)==0);
        allocations[i].bytes=bytes; allocations[i].alive=true;
        heap_free-=bytes+16U; allocated_count++;
        return allocations[i].pointer;
    }
    assert(!"mock allocation tracking overflow"); return NULL;
}
static void liot_rtos_free(void *pointer)
{
    unsigned i;
    assert(pointer);
    if (require_close_order) {
        assert(pipeline_idle && quiesce_ready && camera_result==0);
        assert(!s_camera && !s_encoder && !s_decoder);
        assert(encoder_destroyed==1 && decoder_destroyed==expected_decoders);
    }
    for (i=0;i<16;i++) if (allocations[i].alive && allocations[i].pointer==pointer) {
        free(pointer); allocations[i].alive=false;
        heap_free+=allocations[i].bytes+16U;
        allocated_count--; free_calls++; return;
    }
    assert(!"interior, unowned or double free");
}
static void liot_trace(const char *format, ...)
{
    size_t used=strlen(trace_log); va_list args;
    va_start(args,format);
    (void)vsnprintf(trace_log+used,sizeof(trace_log)-used,format,args);
    va_end(args);
}
static int liot_rtos_mutex_lock(liot_mutex_t mutex,uint32_t timeout)
{ (void)mutex; (void)timeout; return 0; }
static int liot_rtos_mutex_unlock(liot_mutex_t mutex) { (void)mutex; return 0; }
static void liot_rtos_enter_critical(void) {}
static void liot_rtos_exit_critical(void) {}
static void tirtc_capture_pipeline_request_stop(uint32_t generation)
{ assert(generation==s_generation); stop_calls++; }
static bool tirtc_capture_pipeline_is_idle(uint32_t generation)
{ assert(generation==s_generation); return pipeline_idle; }
static bool tirtc_camera_quiesce(uint32_t timeout)
{ assert(timeout==400U); quiesce_calls++; return quiesce_ready; }
static int Liot_CameraDeinit(liot_camera_handle_t camera)
{ assert(camera==s_camera); deinit_calls++; return camera_result; }
static void JpegE_Destroy(void *encoder)
{ assert(encoder==s_encoder); assert(free_calls==0); encoder_destroyed++; }
static void JpegD_Destroy(void *decoder)
{ assert(decoder==s_decoder); assert(free_calls==0); decoder_destroyed++; }
static int tirtc_ui_publish_frame(const void *pixels,unsigned w,unsigned h)
{ assert(!pixels && !w && !h); published_empty++; return 0; }

/* PRODUCTION_FUNCTIONS */

static void pointers_empty(void)
{
    assert(!s_memory && !s_raw && !s_raw_next && !s_jpeg);
    assert(!s_rx[0] && !s_rx[1] && !s_line && !s_output);
}
static void reset_heap(size_t bytes,size_t largest,int owner)
{
    assert(allocated_count==0); pointers_empty();
    memset(allocations,0,sizeof(allocations));
    heap_initial=heap_free=bytes; extent_limit=largest;
    allocation_calls=free_calls=fail_at=0; fail_big=false;
    stop_calls=quiesce_calls=deinit_calls=0;
    encoder_destroyed=decoder_destroyed=published_empty=0;
    pipeline_idle=quiesce_ready=true; camera_result=0;
    require_close_order=false; trace_log[0]=0;
    s_camera=s_encoder=s_decoder=NULL;
    s_owner=owner; s_generation=37; s_accept=false; s_pending=s_decoding=-1;
    s_status.error=0;
}
static void verify_buffers(bool contiguous)
{
    uint8_t *p[7]; size_t n[7]; unsigned count,i,j,k;
    p[0]=s_raw; p[1]=s_raw_next; p[2]=s_jpeg;
    n[0]=n[1]=RAW_BYTES; n[2]=JPEG_MAX; count=3;
    if (s_owner!=DEMO_TIRTC_OWNER_LIVE) {
        p[3]=s_rx[0]; p[4]=s_rx[1]; p[5]=(uint8_t *)s_output;
        p[6]=(uint8_t *)s_line;
        n[3]=n[4]=JPEG_MAX; n[5]=OUT_BYTES; n[6]=LINE_BYTES; count=7;
    } else assert(!s_rx[0] && !s_rx[1] && !s_line && !s_output);
    assert((s_memory!=NULL)==contiguous);
    assert(allocated_count==(contiguous ? 1U : count));
    for (i=0;i<count;i++) {
        assert(p[i] && !((uintptr_t)p[i]&3U));
        for (j=0;j<i;j++) assert((uintptr_t)p[i]+n[i]<=(uintptr_t)p[j] ||
                                (uintptr_t)p[j]+n[j]<=(uintptr_t)p[i]);
        memset(p[i],(int)(i+11U),n[i]);
    }
    for (i=0;i<count;i++) for (k=0;k<n[i];k++) assert(p[i][k]==i+11U);
}
static void clean_and_check(void)
{
    assert(hardware_close()); pointers_empty();
    assert(allocated_count==0 && heap_free==heap_initial);
    { unsigned old_free=free_calls; assert(hardware_close()); assert(free_calls==old_free); }
}
static void fragmented_open(int owner)
{
    reset_heap(1179732U,256U*1024U,owner);
    assert(buffers_open()); verify_buffers(false);
}
static void open_hardware(int owner,bool split)
{
    reset_heap(1179732U,split ? 256U*1024U : 1024U*1024U,owner);
    assert(buffers_open()); verify_buffers(!split);
    s_camera=(void *)(uintptr_t)1; s_encoder=(void *)(uintptr_t)2;
    s_decoder=owner==DEMO_TIRTC_OWNER_LIVE ? NULL : (void *)(uintptr_t)3;
    expected_decoders=owner==DEMO_TIRTC_OWNER_LIVE ? 0U : 1U;
    s_accept=true; s_pending=0; s_decoding=1; require_close_order=true;
}
int main(int argc,char **argv)
{
    const char *scenario; int owner; size_t need; bool split; unsigned i;
    assert(argc==4); scenario=argv[1]; owner=atoi(argv[2]); split=!strcmp(argv[3],"split");
    need=owner==DEMO_TIRTC_OWNER_LIVE ? LIVE_VIDEO_BYTES : VIDEO_BYTES;
    if (!strcmp(scenario,"contiguous")) {
        reset_heap(1179732U,1024U*1024U,owner);
        assert(buffers_open()); verify_buffers(true); assert(allocation_calls==1);
        clean_and_check(); assert(free_calls==1);
    } else if (!strcmp(scenario,"fragmented")) {
        fragmented_open(owner); clean_and_check();
    } else if (!strcmp(scenario,"malloc_fallback")) {
        reset_heap(1179732U,1024U*1024U,owner); fail_big=true;
        assert(buffers_open()); verify_buffers(false);
        assert(allocation_calls==(owner==DEMO_TIRTC_OWNER_LIVE ? 4U : 8U));
        clean_and_check();
    } else if (!strncmp(scenario,"split_failure_",14)) {
        static const char *parts[]={"raw0","raw1","jpeg","rx0","rx1","output","line"};
        char expected[80];
        reset_heap(1179732U,256U*1024U,owner); fail_at=(unsigned)atoi(scenario+14);
        assert(!buffers_open()); assert(allocation_calls==fail_at);
        snprintf(expected,sizeof(expected),"failed part=%s ",parts[fail_at-1U]);
        assert(strstr(trace_log,expected));
        assert(free_calls==fail_at-1U); clean_and_check();
        fail_at=0; assert(buffers_open()); verify_buffers(false); clean_and_check();
    } else if (!strcmp(scenario,"reserve_before")) {
        reset_heap(need+HEAP_RESUME_BYTES-1U,256U*1024U,owner);
        assert(!buffers_open()); assert(allocation_calls==0); clean_and_check();
    } else if (!strcmp(scenario,"reserve_after")) {
        /* Initial sample passes; real per-block headers consume the final
         * reserve. This also models a small concurrent SDK allocation. */
        reset_heap(need+HEAP_RESUME_BYTES+16U,256U*1024U,owner);
        assert(!buffers_open());
        assert(allocation_calls==(owner==DEMO_TIRTC_OWNER_LIVE ? 3U : 7U));
        assert(strstr(trace_log,"failed part=reserve_after "));
        clean_and_check();
    } else if (!strcmp(scenario,"raw_too_fragmented")) {
        reset_heap(1179732U,RAW_BYTES-1U,owner);
        assert(!buffers_open()); assert(allocation_calls==1); clean_and_check();
    } else if (!strcmp(scenario,"repeated")) {
        for (i=0;i<40;i++) {
            reset_heap(1179732U,i&1U ? 256U*1024U : 1024U*1024U,owner);
            assert(buffers_open()); verify_buffers(!(i&1U)); clean_and_check();
        }
    } else if (!strncmp(scenario,"hardware_",9)) {
        open_hardware(owner,split);
        if (!strcmp(scenario,"hardware_pipeline")) pipeline_idle=false;
        else if (!strcmp(scenario,"hardware_quiesce")) quiesce_ready=false;
        else if (!strcmp(scenario,"hardware_deinit")) camera_result=-9;
        else assert(!strcmp(scenario,"hardware_close_order"));
        if (strcmp(scenario,"hardware_close_order")) {
            assert(!hardware_close()); assert(!free_calls && !encoder_destroyed && !decoder_destroyed);
            assert(s_camera && s_encoder && allocated_count);
            assert(!s_accept && s_pending==-1 && s_decoding==-1);
            if (!pipeline_idle) assert(!quiesce_calls && !deinit_calls);
            if (!quiesce_ready) assert(quiesce_calls==1 && !deinit_calls);
            if (camera_result) assert(quiesce_calls==1 && deinit_calls==1);
            pipeline_idle=quiesce_ready=true; camera_result=0;
        }
        clean_and_check(); assert(encoder_destroyed==1 && decoder_destroyed==expected_decoders);
        assert(published_empty==(owner==DEMO_TIRTC_OWNER_LIVE ? 0U : 2U));
    } else assert(!"unknown scenario");
    printf("PASS video buffers %s owner=%d mode=%s\n",scenario,owner,argv[3]);
    return 0;
}

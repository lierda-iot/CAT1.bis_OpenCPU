/* V04 GC032A -> bounded MJPEG -> TiRTC, and MJPEG -> RGB565 display.
 * Original sensor/codec SDK is unchanged. Network callbacks only copy a frame;
 * a camera producer overlaps DMA with one codec worker, with no UI-thread I/O. */
#include "tirtc_video.h"
#include "../platform/device_metadata.h"
#include "tirtc_camera_fence.h"
#include "tirtc_capture_pipeline.h"
#include "tirtc_video_format.h"
#include "tirtc_ui.h"
#include "liot_camera.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_log.h"
#include "../tirtc_log.h"
#include "liot_power.h"
#include "mm_jpeg_if.h"
#include <string.h>
#include <stdio.h>

#define CAP_W TIRTC_VIDEO_CAP_W
#define CAP_H TIRTC_VIDEO_CAP_H
#define ENC_W TIRTC_VIDEO_ENC_W
#define ENC_H TIRTC_VIDEO_ENC_H
#define JPEG_MAX (64U*1024U)
#define RX_MAX_W 640U
#define RX_MAX_H 640U
#define RX_MAX_PIXELS (640U*480U)
#define OUT_W 192U
#define OUT_H 144U
#define RAW_BYTES (CAP_W*CAP_H*2U)
#define LINE_BYTES (RX_MAX_W*16U*2U)
#define OUT_BYTES (OUT_W*OUT_H*2U)
#define VIDEO_BYTES (RAW_BYTES*2U+JPEG_MAX*3U+LINE_BYTES+OUT_BYTES)
#define LIVE_VIDEO_BYTES (RAW_BYTES*2U+JPEG_MAX)
/* Keep headroom for incoming frame assembly and audio/control allocations.
 * These are admission samples, not a reservation against every SDK task. */
#define HEAP_PAUSE_BYTES (192U*1024U)
#define HEAP_RESUME_BYTES (256U*1024U)
#define BLOCK_PAUSE_BYTES (32U*1024U)
#define BLOCK_RESUME_BYTES (64U*1024U)
#define SEND_QUEUE_TARGET (48U*1024U)
#define VIDEO_ERROR_MEMORY (-1701)
#define VIDEO_ERROR_CAMERA (-1702)
#define VIDEO_ERROR_CODEC (-1703)
#define VIDEO_ERROR_FRAME (-1704)
#define VIDEO_ERROR_CLOSE (-1705)
LIOT_ADD_CAMERA(liot_gc032a_2ddr);

static liot_task_t s_task;
static liot_mutex_t s_rx_lock;
static bool s_starting, s_wanted, s_wechat, s_enabled=true;
static bool s_incoming, s_rx_clockwise;
static demo_tirtc_owner_e s_owner;
static uint32_t s_generation;
static tirtc_video_snapshot_t s_status;
/* The worker owns these pointers. The callback may access rx slots only with
 * rx_lock held while s_accept is true. Teardown first closes that admission. */
static uint8_t *s_memory, *s_raw, *s_raw_next, *s_jpeg, *s_rx[2];
static uint16_t *s_line, *s_output;
static int s_pending=-1, s_decoding=-1;
static uint32_t s_rx_len[2];
static bool s_accept;
static bool s_memory_paused; /* Video worker only. */
static liot_camera_handle_t s_camera;
static void *s_encoder, *s_decoder;
/* Per-call counters separate network delivery from local decode/drop work.
 * Snapshot only every five seconds; never print from media callbacks. */
typedef struct {
    uint32_t incoming, incoming_bytes, oversized, format_drop, lock_drop;
    uint32_t replaced, decode_drop, memory_drop, send_zero, send_error;
    uint32_t send_ms, width, height, input_jpeg, out_width, out_height;
    uint32_t crop_x, crop_y, crop_width, crop_height;
    uint32_t tx_bytes, tx_not_accepted, encode_defer;
} video_diag_t;
static video_diag_t s_diag;

static uint32_t now_ms(void) { return liot_rtos_get_running_time(); }
static bool memory_admit(void)
{
    size_t free_bytes=liot_xPortGetFreeHeapSize();
    size_t largest=liot_xPortGetMaximumFreeBlockSize();
    if (s_memory_paused) {
        if (free_bytes>=HEAP_RESUME_BYTES && largest>=BLOCK_RESUME_BYTES) s_memory_paused=false;
    } else if (free_bytes<HEAP_PAUSE_BYTES || largest<BLOCK_PAUSE_BYTES) s_memory_paused=true;
    return !s_memory_paused;
}
static bool requested(uint32_t generation)
{
    bool wanted;
    liot_rtos_enter_critical(); wanted=s_wanted && s_generation==generation; liot_rtos_exit_critical();
    return wanted;
}
static void set_error(int error)
{
    liot_rtos_enter_critical(); s_status.error=error; liot_rtos_exit_critical();
}
void tirtc_video_get_snapshot(tirtc_video_snapshot_t *out)
{
    if (!out) return;
    liot_rtos_enter_critical(); *out=s_status; liot_rtos_exit_critical();
}
const char *tirtc_video_profile_json(void)
{
    return "{\"hardware\":{\"chip_model\":\"" TIRTC_DEVICE_CHIP_MODEL
      "\",\"board_model\":\"" TIRTC_DEVICE_BOARD_MODEL "\"},"
      "\"firmware_version\":\"" TIRTC_DEVICE_FIRMWARE_VERSION "\","
      "\"profiles\":{\"call\":{\"up_audio_mt\":[\"alaw\"],\"down_audio_mt\":[\"alaw\"],"
      "\"up_video_mt\":[\"mjpeg\"],\"down_video_mt\":[\"mjpeg\"],\"audio_rate\":8000,\"audio_channels\":1,"
      "\"camera_rotation\":0,\"hor_mirror\":false,\"vert_mirror\":false,\"no_video\":false,\"aspect_ratio\":1.3333333333333333,\"object_fit\":\"contain\"},"
      "\"stream\":{\"up_audio_mt\":[\"alaw\"],\"down_audio_mt\":[\"alaw\"],"
      "\"up_audio_streamid\":10,\"up_video_streamid\":11,\"down_audio_streamid\":14,"
      "\"up_video_mt\":[\"mjpeg\"],\"down_video_mt\":[],\"audio_rate\":8000,\"audio_channels\":1,"
      "\"camera_rotation\":90,\"hor_mirror\":false,\"vert_mirror\":false,\"no_video\":false,\"aspect_ratio\":1.3333333333333333,\"object_fit\":\"contain\"},"
      /* Preserve the verified outgoing WX canvas. Incoming WX is already
       * upright and must not receive the outgoing compatibility rotation;
       * locally center-crop it to fill the landscape UI without stretching. */
      "\"voip\":{\"screen_width\":144,\"screen_height\":192,\"up_video_mt\":\"mjpeg\",\"down_video_mt\":\"mjpeg\","
      "\"down_audio_mt\":\"alaw\",\"audio_rate\":8000,\"audio_channels\":1,\"camera_rotation\":0,\"down_video_rotation\":1,"
      "\"hor_mirror\":false,\"vert_mirror\":false,\"no_video\":false,\"aspect_ratio\":1.3333333333333333,\"object_fit\":\"contain\","
      "\"video_res_mode\":\"fill_screen\",\"calling_timeout_sec\":45}}}";
}
static int start_session(demo_tirtc_owner_e owner,uint32_t generation,bool wechat,bool incoming,bool enabled)
{
    int result=0;
    if (!generation) return -2;
    liot_rtos_enter_critical();
    if (!s_task || !s_rx_lock) result=-1;
    else if (s_wanted || s_status.active)
        result=s_wanted && s_generation==generation && s_owner==owner &&
               s_wechat==wechat && s_incoming==incoming ? 0 : -3;
    else {
        memset(&s_status,0,sizeof(s_status));
        memset(&s_diag,0,sizeof(s_diag));
        s_owner=owner; s_generation=generation; s_wechat=wechat; s_enabled=enabled; s_wanted=true;
        /* The deployed WX incoming flow is upright; outgoing retains its
         * verified CW90 compatibility transform. Do not infer orientation
         * from JPEG dimensions or unused SDK frame-header fields. */
        s_incoming=incoming; s_rx_clockwise=wechat && !incoming;
        s_status.active=true; s_status.generation=generation; s_status.camera_enabled=enabled;
    }
    liot_rtos_exit_critical(); return result;
}
int tirtc_video_start(demo_tirtc_owner_e owner,uint32_t generation,bool wechat,bool incoming)
{
    if (owner!=DEMO_TIRTC_OWNER_WECHAT && owner!=DEMO_TIRTC_OWNER_DEV_CHAT) return -2;
    return start_session(owner,generation,wechat,incoming,true);
}
int tirtc_video_start_live(uint32_t generation,bool enabled)
{
    return start_session(DEMO_TIRTC_OWNER_LIVE,generation,false,false,enabled);
}
void tirtc_video_stop(uint32_t generation)
{
    liot_rtos_enter_critical(); if (s_generation==generation) s_wanted=false; liot_rtos_exit_critical();
    tirtc_capture_pipeline_request_stop(generation);
}
bool tirtc_video_is_stopped(uint32_t generation)
{
    bool stopped;
    liot_rtos_enter_critical(); stopped=s_generation!=generation || (!s_wanted && !s_status.active); liot_rtos_exit_critical();
    return stopped;
}
int tirtc_video_set_enabled(uint32_t generation,bool enabled)
{
    int result=0;
    liot_rtos_enter_critical();
    if (!s_wanted || s_generation!=generation) result=-1;
    else { s_enabled=enabled; s_status.camera_enabled=enabled; }
    liot_rtos_exit_critical();
    if (!result) (void)tirtc_capture_pipeline_set_enabled(generation,enabled);
    return result;
}
void tirtc_video_receive(uint32_t generation,const TIRTCFRAMEINFO *frame,const void *data)
{
    bool live;
    if (!frame || !data || !s_rx_lock) return;
    liot_rtos_enter_critical(); live=s_owner==DEMO_TIRTC_OWNER_LIVE; liot_rtos_exit_critical();
    if (live) return;
    if (frame->media!=TIRTC_VIDEO_JPEG || frame->length<4 || frame->length>JPEG_MAX) {
        liot_rtos_enter_critical(); s_status.dropped_frames++;
        if (frame->length>JPEG_MAX) s_diag.oversized++; else s_diag.format_drop++;
        liot_rtos_exit_critical(); return;
    }
    if (liot_rtos_mutex_lock(s_rx_lock,LIOT_NO_WAIT)!=LIOT_OSI_SUCCESS) {
        liot_rtos_enter_critical(); s_status.dropped_frames++; s_diag.lock_drop++; liot_rtos_exit_critical(); return;
    }
    if (s_accept && requested(generation)) {
        if (frame->stream_id!=(s_wechat ? 1 : 11)) {
            liot_rtos_enter_critical(); s_status.dropped_frames++; liot_rtos_exit_critical();
            (void)liot_rtos_mutex_unlock(s_rx_lock); return;
        }
        int slot=s_decoding==0 ? 1 : 0;
        liot_rtos_enter_critical();
        s_diag.incoming++; s_diag.incoming_bytes+=frame->length; s_diag.input_jpeg=frame->length;
        if (s_pending>=0) { s_status.dropped_frames++; s_diag.replaced++; }
        liot_rtos_exit_critical();
        memcpy(s_rx[slot],data,frame->length); s_rx_len[slot]=frame->length; s_pending=slot;
    }
    (void)liot_rtos_mutex_unlock(s_rx_lock);
}
/* Worker-only ownership. A successful DMA stop is required before calling
 * this for active capture; partial-open rollback has never started DMA. */
static void buffers_close(void)
{
    if (s_memory) {
        liot_rtos_free(s_memory);
    } else {
        if (s_raw) liot_rtos_free(s_raw);
        if (s_raw_next) liot_rtos_free(s_raw_next);
        if (s_jpeg) liot_rtos_free(s_jpeg);
        if (s_rx[0]) liot_rtos_free(s_rx[0]);
        if (s_rx[1]) liot_rtos_free(s_rx[1]);
        if (s_output) liot_rtos_free(s_output);
        if (s_line) liot_rtos_free(s_line);
    }
    s_memory=s_raw=s_raw_next=s_jpeg=s_rx[0]=s_rx[1]=NULL;
    s_line=s_output=NULL;
}
static bool buffers_open(void)
{
    const bool live=s_owner==DEMO_TIRTC_OWNER_LIVE;
    const size_t bytes=live ? LIVE_VIDEO_BYTES : VIDEO_BYTES;
    size_t free_bytes=liot_xPortGetFreeHeapSize();
    size_t largest=liot_xPortGetMaximumFreeBlockSize();
    const char *part="reserve";
    if (free_bytes<bytes+HEAP_RESUME_BYTES) {
        liot_trace("[VIDEO41] buffers denied reason=reserve need=%lu free=%lu max_block=%lu\r\n",
            (unsigned long)bytes,(unsigned long)free_bytes,(unsigned long)largest);
        return false;
    }
    /* Preserve the existing layout when one large block is available. Each
     * consumer only needs its own contiguous buffer, so a fragmented heap
     * need not prevent a call when it can supply those buffers separately. */
    if (largest>=bytes+64U) s_memory=liot_rtos_malloc(bytes);
    if (s_memory) {
        s_raw=s_memory; s_raw_next=s_raw+RAW_BYTES; s_jpeg=s_raw_next+RAW_BYTES;
        s_rx[0]=s_rx[1]=NULL; s_line=s_output=NULL;
        if (!live) {
            s_rx[0]=s_jpeg+JPEG_MAX; s_rx[1]=s_rx[0]+JPEG_MAX;
            s_line=(uint16_t *)(s_rx[1]+JPEG_MAX); s_output=(uint16_t *)((uint8_t *)s_line+LINE_BYTES);
        }
        liot_trace("[VIDEO41] buffers mode=contiguous need=%lu free_before=%lu max_before=%lu\r\n",
            (unsigned long)bytes,(unsigned long)free_bytes,(unsigned long)largest);
        return true;
    }
    liot_trace("[VIDEO41] buffers try=split need=%lu free_before=%lu max_before=%lu\r\n",
        (unsigned long)bytes,(unsigned long)free_bytes,(unsigned long)largest);
    /* Same allocator and DMA domain; allocate largest first. RAW frames are
     * independently contiguous and retain the SDK allocator's alignment. */
    part="raw0"; s_raw=liot_rtos_malloc(RAW_BYTES); if (!s_raw) goto failed;
    part="raw1"; s_raw_next=liot_rtos_malloc(RAW_BYTES); if (!s_raw_next) goto failed;
    part="jpeg"; s_jpeg=liot_rtos_malloc(JPEG_MAX); if (!s_jpeg) goto failed;
    if (!live) {
        part="rx0"; s_rx[0]=liot_rtos_malloc(JPEG_MAX); if (!s_rx[0]) goto failed;
        part="rx1"; s_rx[1]=liot_rtos_malloc(JPEG_MAX); if (!s_rx[1]) goto failed;
        part="output"; s_output=liot_rtos_malloc(OUT_BYTES); if (!s_output) goto failed;
        part="line"; s_line=liot_rtos_malloc(LINE_BYTES); if (!s_line) goto failed;
    }
    /* Extra allocator headers and concurrent SDK allocations must not use
     * the audio/network reserve retained by the original admission rule. */
    part="reserve_after";
    if (liot_xPortGetFreeHeapSize()<HEAP_RESUME_BYTES) goto failed;
    liot_trace("[VIDEO41] buffers mode=split need=%lu free=%lu max_block=%lu\r\n",
        (unsigned long)bytes,(unsigned long)liot_xPortGetFreeHeapSize(),
        (unsigned long)liot_xPortGetMaximumFreeBlockSize());
    return true;
failed:
    liot_trace("[VIDEO41] buffers failed part=%s need=%lu free=%lu max_block=%lu\r\n",
        part,(unsigned long)bytes,(unsigned long)liot_xPortGetFreeHeapSize(),
        (unsigned long)liot_xPortGetMaximumFreeBlockSize());
    buffers_close();
    return false;
}
static int hardware_open(void)
{
    liot_camera_config_t cfg;
    JPEG_ENC_PARAM enc={JPEG_COLOR_FMT_YUYV,ENC_W,ENC_H,TIRTC_VIDEO_JPEG_QUALITY};
    unsigned suggested=0;
    UINT8 powerup_reason=0xff;
    int powerup_result=liot_get_powerup_reason(&powerup_reason);
    liot_trace("[VIDEO40] start powerup_reason=%u ret=%d; tx=%ux%u target_fps=%u wx=%u incoming=%u rx_cw90=%u wx_rx_fill=144x192 ui_canvas=%ux%u\r\n",
               powerup_reason,powerup_result,(unsigned)ENC_W,(unsigned)ENC_H,
               (unsigned)TIRTC_VIDEO_TARGET_FPS,s_wechat?1U:0U,s_incoming?1U:0U,
               s_rx_clockwise?1U:0U,OUT_W,OUT_H);
    memset(&cfg,0,sizeof(cfg));
    if (!buffers_open()) return VIDEO_ERROR_MEMORY;
    s_encoder=JpegE_Create();
    s_decoder=s_owner==DEMO_TIRTC_OWNER_LIVE ? NULL : JpegD_Create();
    if (!s_encoder || (s_owner!=DEMO_TIRTC_OWNER_LIVE && !s_decoder) ||
        JpegE_SetParam(s_encoder,&enc,&suggested)<0) return VIDEO_ERROR_CODEC;
    /* GPIO25 is shared with LCD; its existing rail configuration is retained. */
    if (Liot_GpioInit(L_GPIO_27,L_IO_OUTPUT,L_IO_HIGH,NULL)!=L_GPIO_ERR_SUCCESS) return VIDEO_ERROR_CAMERA;
    cfg.sensor=&liot_gc032a_2ddr;
    cfg.cspi.num=LIOT_CSPI_PORT1; cfg.cspi.speed=LIOT_CAM_24_M;
    cfg.i2c.num=0; cfg.i2c.scl=(int8_t)255; cfg.i2c.sda=(int8_t)255;
    cfg.info.format=LIOT_CAMERA_OUTPUT_YUYV;
    cfg.info.resolution.width.size=TIRTC_VIDEO_SENSOR_W;
    cfg.info.resolution.height.size=TIRTC_VIDEO_SENSOR_H;
    cfg.info.resolution.width.offset=TIRTC_VIDEO_SENSOR_X;
    cfg.info.resolution.height.offset=TIRTC_VIDEO_SENSOR_Y;
    cfg.info.resolution.width.scale=TIRTC_VIDEO_SCALE;
    cfg.info.resolution.height.scale=TIRTC_VIDEO_SCALE;
    /* The vendor ignores its callback-init return code. Allocate our wait
     * object first, before it can enter hardware initialization. */
    if (!tirtc_camera_fence_prepare()) return VIDEO_ERROR_CAMERA;
    s_camera=Liot_CameraInit(&cfg);
    if (!s_camera) return VIDEO_ERROR_CAMERA;
    if (!tirtc_camera_fence_ready()) return VIDEO_ERROR_CAMERA;
    liot_trace("[VIDEO40] camera GC032A window=%ux%u offset=%u,%u scale=%u raw=%ux%u JPEG=%ux%u q%u ready buffer=%lu suggestion=%u heap=%lu max_block=%lu\r\n",
        (unsigned)TIRTC_VIDEO_SENSOR_W,(unsigned)TIRTC_VIDEO_SENSOR_H,
        (unsigned)TIRTC_VIDEO_SENSOR_X,(unsigned)TIRTC_VIDEO_SENSOR_Y,(unsigned)TIRTC_VIDEO_SCALE,
        (unsigned)CAP_W,(unsigned)CAP_H,(unsigned)ENC_W,(unsigned)ENC_H,
        (unsigned)TIRTC_VIDEO_JPEG_QUALITY,(unsigned long)(s_owner==DEMO_TIRTC_OWNER_LIVE ? LIVE_VIDEO_BYTES : VIDEO_BYTES),suggested,(unsigned long)liot_xPortGetFreeHeapSize(),
        (unsigned long)liot_xPortGetMaximumFreeBlockSize());
    return 0;
}
static bool hardware_close(void)
{
    (void)liot_rtos_mutex_lock(s_rx_lock,LIOT_WAIT_FOREVER);
    s_accept=false; s_pending=s_decoding=-1;
    (void)liot_rtos_mutex_unlock(s_rx_lock);
    tirtc_capture_pipeline_request_stop(s_generation);
    if (!tirtc_capture_pipeline_is_idle(s_generation)) return false;
    /* SDK frame-end may wake CaptureImage before DMA_END. A timed-out DMA
     * retains this allocation until its real completion, including on stop. */
    if (!tirtc_camera_quiesce(400U)) { set_error(VIDEO_ERROR_CLOSE); return false; }
    if (s_camera && Liot_CameraDeinit(s_camera)!=LIOT_CAMERA_SUCCESS) { set_error(VIDEO_ERROR_CLOSE); return false; }
    s_camera=NULL;
    if (s_encoder) JpegE_Destroy(s_encoder);
    if (s_decoder) JpegD_Destroy(s_decoder);
    s_encoder=s_decoder=NULL;
    buffers_close();
    if (s_owner!=DEMO_TIRTC_OWNER_LIVE) (void)tirtc_ui_publish_frame(NULL,0,0);
    return true;
}
/* Reject oversized/non-baseline headers before entering the opaque vendor
 * decoder. The SDK still validates Huffman/quantization and entropy data. */
static bool jpeg_geometry(const uint8_t *data,uint32_t length,unsigned *width,unsigned *height)
{
    uint32_t at=2; bool found=false;
    if (!data || length<4 || length>JPEG_MAX || data[0]!=0xff || data[1]!=0xd8) return false;
    while (at<length) {
        if (data[at++]!=0xff) return false;
        while (at<length && data[at]==0xff) at++;
        if (at>=length) return false;
        unsigned marker=data[at++];
        if (!marker || marker==0xd8 || marker==0xd9 || (marker>=0xd0 && marker<=0xd7)) return false;
        if (at+2U>length) return false;
        uint32_t size=((uint32_t)data[at]<<8)|data[at+1];
        if (size<2 || size>length-at) return false;
        if (marker>=0xc0 && marker<=0xcf && marker!=0xc4 && marker!=0xc8 && marker!=0xcc) {
            if (marker!=0xc0 || found || size<8 || data[at+2]!=8) return false;
            unsigned h=((unsigned)data[at+3]<<8)|data[at+4];
            unsigned w=((unsigned)data[at+5]<<8)|data[at+6];
            unsigned components=data[at+7];
            if (!w || !h || w>RX_MAX_W || h>RX_MAX_H || w*h>RX_MAX_PIXELS ||
                (components!=1 && components!=3) || size!=8U+3U*components) return false;
            *width=w; *height=h; found=true;
        }
        if (marker==0xda) return found;
        at+=size;
    }
    return false;
}
/* JPEG dimensions are untrusted. Decode MCU rows into a fixed 640x16 slice,
 * scale directly to the UI's 192x144 copy-in buffer; never allocate by header.
 * Outgoing WeChat slices map clockwise into output columns, without a
 * full-frame intermediate. Incoming WX retains source orientation and crops
 * to fill the landscape output. P2P retains its existing aspect-fit policy. */
static int decode_present(uint32_t generation,uint8_t *jpeg,uint32_t length)
{
    /* The sole codec worker owns these bounded maps. Recompute from every
     * accepted frame's geometry: a peer may change dimensions mid-call.
     * 672 static bytes replace division in the pixel loops without adding
     * task stack, heap allocation or a full-frame intermediate. */
    static uint16_t map_x[OUT_W], map_y[OUT_H];
    JPEG_INFO info;
    JPEG_IMAGE_BUF row;
    unsigned last_y=0, width=0, height=0, out_w=OUT_W, out_h=OUT_H;
    bool clockwise, fill;
    if (!jpeg_geometry(jpeg,length,&width,&height)) return VIDEO_ERROR_FRAME;
    liot_rtos_enter_critical();
    s_diag.width=width; s_diag.height=height;
    clockwise=s_rx_clockwise; fill=s_wechat && s_incoming;
    liot_rtos_exit_critical();
    memset(&info,0,sizeof(info));
    if (JpegD_DecodeInfo(s_decoder,jpeg,length,&info)<0) return VIDEO_ERROR_FRAME;
    if (info.uWidth!=width || info.uHeight!=height ||
        info.uEdgedWidth<width || info.uEdgedWidth>RX_MAX_W || info.uEdgedHeight<height || info.uEdgedHeight>RX_MAX_H) return VIDEO_ERROR_FRAME;
    unsigned display_w=clockwise?height:width, display_h=clockwise?width:height;
    unsigned crop_w=width, crop_h=height, crop_x=0, crop_y=0;
    if (fill) {
        /* Crop in the source coordinate space. Validated JPEG dimensions
         * cap these products at 640*192; no full-frame resize is needed.
         * Keep at least one source pixel for extreme valid aspect ratios. */
        if (width*OUT_H>height*OUT_W) {
            crop_w=height*OUT_W/OUT_H;
            if (!crop_w) crop_w=1U;
        } else {
            crop_h=width*OUT_H/OUT_W;
            if (!crop_h) crop_h=1U;
        }
        crop_x=(width-crop_w)/2U; crop_y=(height-crop_h)/2U;
    } else {
        if (display_w*OUT_H>display_h*OUT_W) out_h=display_h*OUT_W/display_w;
        else out_w=display_w*OUT_H/display_h;
    }
    if (!out_w || !out_h) return VIDEO_ERROR_FRAME;
    liot_rtos_enter_critical();
    s_diag.out_width=out_w; s_diag.out_height=out_h;
    s_diag.crop_x=crop_x; s_diag.crop_y=crop_y;
    s_diag.crop_width=crop_w; s_diag.crop_height=crop_h;
    liot_rtos_exit_critical();
    const unsigned pad_x=(OUT_W-out_w)/2U, pad_y=(OUT_H-out_h)/2U;
    if (clockwise) {
        /* Output x selects a reversed source row; output y a source column. */
        for (unsigned dx=0; dx<out_w; dx++) map_x[dx]=(uint16_t)(height-1U-dx*height/out_w);
        for (unsigned dy=0; dy<out_h; dy++) map_y[dy]=(uint16_t)(dy*width/out_h);
    } else {
        for (unsigned dx=0; dx<out_w; dx++) map_x[dx]=(uint16_t)(crop_x+dx*crop_w/out_w);
        for (unsigned dy=0; dy<out_h; dy++) map_y[dy]=(uint16_t)(crop_y+dy*crop_h/out_h);
    }
    memset(s_output,0,OUT_BYTES); memset(&row,0,sizeof(row));
    row.eFmt=JPEG_COLOR_FMT_RGB565; row.uWidth=info.uEdgedWidth; row.uHeight=16; row.pData[0]=s_line;
    for (unsigned i=0; i<(height+15U)/16U; i++) {
        unsigned y=0;
        if (!requested(generation)) return -1;
        if (JpegD_DecodeLine(s_decoder,&row,&y)<0 || y>=height || (i && y<=last_y)) return VIDEO_ERROR_FRAME;
        last_y=y;
        unsigned end=y+16U<height ? y+16U : height;
        if (clockwise) {
            /* A source row becomes one output column. Only columns whose
             * source row is present in this MCU slice touch its pixels. */
            for (unsigned dx=0; dx<out_w; dx++) {
                unsigned sy=map_x[dx];
                if (sy<y || sy>=end) continue;
                const uint16_t *src=s_line+(sy-y)*row.uWidth;
                uint16_t *dst=s_output+pad_y*OUT_W+pad_x+dx;
                for (unsigned dy=0; dy<out_h; dy++) dst[dy*OUT_W]=src[map_y[dy]];
            }
        } else {
            for (unsigned dy=0; dy<out_h; dy++) {
                unsigned sy=map_y[dy];
                if (sy<y || sy>=end) continue;
                uint16_t *dst=s_output+(dy+pad_y)*OUT_W+pad_x;
                const uint16_t *src=s_line+(sy-y)*row.uWidth;
                for (unsigned dx=0; dx<out_w; dx++) dst[dx]=src[map_x[dx]];
            }
        }
        if ((i&3U)==3U) liot_rtos_task_sleep_ms(1);
    }
    return requested(generation) ? tirtc_ui_publish_frame(s_output,OUT_W,OUT_H) : -1;
}
/* Recheck after encoding: another task may have consumed memory meanwhile.
 * Avoid adding a frame to an already delayed queue. SDK result zero means
 * not accepted; it must not be reported as a transmitted frame. */
static bool send_frame(demo_tirtc_owner_e owner,uint32_t generation,bool wechat,
                       unsigned length,uint32_t captured_at,size_t *queued)
{
    bool enabled;
    size_t allocation_budget=length+(length/1000U+1U)*64U+1024U;
    size_t largest_needed=length+2048U;
    if (largest_needed<BLOCK_PAUSE_BYTES) largest_needed=BLOCK_PAUSE_BYTES;
    if (!memory_admit() || liot_xPortGetFreeHeapSize()<HEAP_PAUSE_BYTES+allocation_budget ||
        liot_xPortGetMaximumFreeBlockSize()<largest_needed ||
        demo_tirtc_get_send_buffer_used(owner,generation,queued)!=0 ||
        *queued>=SEND_QUEUE_TARGET || length>SEND_QUEUE_TARGET-*queued) return false;
    liot_rtos_enter_critical();
    enabled=s_enabled && s_wanted && s_generation==generation;
    liot_rtos_exit_critical();
    if (!enabled) return false;
    TIRTCFRAMEINFO frame={0};
    frame.stream_id=wechat ? 1 : 11; frame.media=TIRTC_VIDEO_JPEG; frame.flags=TIRTC_FRAME_FLAG_KEY_FRAME;
    frame.ts=captured_at; frame.length=length;
    uint32_t began=now_ms();
    int ret=demo_tirtc_send_video(owner,generation,&frame,s_jpeg);
    uint32_t elapsed=now_ms()-began;
    liot_rtos_enter_critical();
    s_diag.send_ms=elapsed;
    if (!ret) s_diag.send_zero++; else if (ret<0) s_diag.send_error++;
    liot_rtos_exit_critical();
    return ret>0;
}
/* Called only by the codec worker. Release the raw lease on every exit;
 * the camera producer can reuse it as soon as the encoder has returned. */
static int encode_next(demo_tirtc_owner_e owner,uint32_t generation,bool wechat,
                       size_t *queued,unsigned *jpeg_length,bool *sent)
{
    unsigned slot=0;
    uint8_t *data=NULL;
    uint32_t captured_at=0,capture_ms=0;
    bool enabled;
    *jpeg_length=0; *sent=false;
    if (!tirtc_capture_pipeline_take(generation,&slot,&data,&captured_at,&capture_ms)) return 0;
    liot_rtos_enter_critical(); enabled=s_enabled; liot_rtos_exit_critical();
    if (!enabled || !requested(generation) || !memory_admit()) {
        tirtc_capture_pipeline_release(generation,slot);
        return 0;
    }
    uint32_t began=now_ms();
    tirtc_video_prepare_yuyv(data);
    JPEG_IMAGE_BUF raw={JPEG_COLOR_FMT_YUYV,ENC_W,ENC_H,{data,NULL,NULL}};
    unsigned length=JPEG_MAX;
    int ret=JpegE_Encode(s_encoder,&raw,s_jpeg,&length);
    uint32_t elapsed=now_ms()-began;
    tirtc_capture_pipeline_release(generation,slot);
    liot_rtos_enter_critical();
    s_status.capture_ms=capture_ms; s_status.encode_ms=elapsed;
    liot_rtos_exit_critical();
    if (ret<0 || length<4 || length>JPEG_MAX) return VIDEO_ERROR_CODEC;
    *jpeg_length=length;
    *sent=send_frame(owner,generation,wechat,length,captured_at,queued);
    liot_rtos_enter_critical();
    if (*sent) { s_status.tx_frames++; s_diag.tx_bytes+=length; }
    else { s_status.dropped_frames++; s_diag.tx_not_accepted++; }
    liot_rtos_exit_critical();
    return 1;
}

/* Counter deltas are bounded to a log interval; 64-bit arithmetic avoids
 * overflow if a stalled worker makes that interval unusually long. */
static uint32_t fps_milli(uint32_t count,uint32_t elapsed)
{
    return elapsed ? (uint32_t)((uint64_t)count*1000000U/elapsed) : 0U;
}

static void log_uplink_feedback(demo_tirtc_owner_e owner,uint32_t generation)
{
    demo_tirtc_video_send_stats_t net;
    char loss[16]="NA";
    int ret=demo_tirtc_get_video_send_stats(owner,generation,&net);
    if (ret || !net.available || net.stale) {
        TIRTC_LOG_DEBUG("[VIDEO40-NET] result=%d registration=%d registered=%u feedback=%s age_ms=%lu loss=NA\r\n",
            ret,net.registration_result,net.registered?1U:0U,
            net.available?"stale":"none",(unsigned long)net.age_ms);
        return;
    }
    if (net.loss_available) (void)snprintf(loss,sizeof(loss),"%lu",(unsigned long)net.loss_rate_ppm);
    TIRTC_LOG_DEBUG("[VIDEO40-NET] event=%lu type=%u reason=%u age_ms=%lu rtt_ms=%lu peer_bps=%lu unacked=%lu pending=%lu wait_ms=%lu sdk_drop=%lu loss_valid=%u loss_ppm=%s\r\n",
        (unsigned long)net.event_seq,net.event_type,net.reason,(unsigned long)net.age_ms,
        (unsigned long)net.rtt_ms,(unsigned long)net.receive_bitrate_bps,
        (unsigned long)net.unacked_segments,(unsigned long)net.pending_frames,
        (unsigned long)net.pending_wait_ms,(unsigned long)net.dropped_pending_frames,
        net.loss_available?1U:0U,loss);
}

static void task(void *context)
{
    (void)context;
    for (;;) {
        uint32_t generation; demo_tirtc_owner_e owner; bool wechat,wanted,active;
        liot_rtos_enter_critical();
        generation=s_generation; owner=s_owner; wechat=s_wechat; wanted=s_wanted; active=s_status.active;
        liot_rtos_exit_critical();
        if (!wanted) {
            /* A stop can arrive before this worker observes a queued start. */
            if (active) {
                while (!hardware_close()) liot_rtos_task_sleep_ms(25);
                liot_rtos_enter_critical(); s_status.ready=false; s_status.active=false; liot_rtos_exit_critical();
            }
            liot_rtos_task_sleep_ms(20); continue;
        }
        s_pending=s_decoding=-1;
        s_memory_paused=false;
        int error=hardware_open();
        if (!error && requested(generation)) {
            /* Serialize initial enable with UI stop/mute. The pipeline start
             * only updates metadata and never blocks or allocates. */
            liot_rtos_enter_critical();
            if (s_wanted && s_generation==generation &&
                tirtc_capture_pipeline_start(s_camera,generation,s_raw,s_raw_next,s_enabled)!=0)
                error=VIDEO_ERROR_CAMERA;
            liot_rtos_exit_critical();
        }
        if (!error && requested(generation)) {
            int monitor_ret=0;
            if (TIRTC_ENABLE_DEBUG_LOG)
                monitor_ret=demo_tirtc_video_send_monitor_start(owner,generation,wechat?1U:11U);
            TIRTC_LOG_DEBUG("[VIDEO40-NET] monitor registration=%d stream=%u (optional diagnostics)\r\n",
                monitor_ret,wechat?1U:11U);
            (void)liot_rtos_mutex_lock(s_rx_lock,LIOT_WAIT_FOREVER);
            s_accept=owner!=DEMO_TIRTC_OWNER_LIVE;
            (void)liot_rtos_mutex_unlock(s_rx_lock);
            liot_rtos_enter_critical(); s_status.ready=true; liot_rtos_exit_critical();
            tirtc_video_rate_t rate;
            tirtc_video_rate_reset(&rate,now_ms());
            uint32_t last_log=now_ms(),last_tx=0,last_rx=0,last_cap=0,last_bytes=0;
            size_t queued=0;
            size_t heap_min=liot_xPortGetFreeHeapSize(),block_min=liot_xPortGetMaximumFreeBlockSize(),queue_peak=0;
            unsigned last_jpeg=0, blocked=0;
            while (requested(generation)) {
                int slot;
                bool memory_ready=memory_admit();
                (void)tirtc_capture_pipeline_set_paused(generation,!memory_ready);
                tirtc_capture_pipeline_stats_t pipeline;
                tirtc_capture_pipeline_get_stats(&pipeline);
                if (pipeline.error) {
                    liot_trace("[VIDEO40-CAM] capture_failed ret=%d errors=%lu in_flight=%u\r\n",
                        pipeline.error,(unsigned long)pipeline.errors,pipeline.in_flight?1U:0U);
                    error=VIDEO_ERROR_CAMERA; break;
                }
                bool enabled;
                liot_rtos_enter_critical(); enabled=s_enabled; liot_rtos_exit_critical();
                /* Give a ready outgoing frame first use of the shared codec.
                 * Capture continues while this worker decodes remote video. */
                if (enabled && tirtc_video_rate_due(&rate,now_ms())) {
                    uint32_t admitted_at=now_ms();
                    bool queue_ready=memory_ready &&
                        demo_tirtc_get_send_buffer_used(owner,generation,&queued)==0 &&
                        queued<SEND_QUEUE_TARGET;
                    /* A nearly full queue used to consume codec time for a
                     * JPEG that send_frame() would immediately reject. Use
                     * the previous encoded size as an admission estimate;
                     * the producer still keeps its newest complete raw frame.
                     * An empty queue always allows a fresh size estimate, so
                     * a previous oversized scene cannot stall encoding forever.
                     * send_frame() still checks actual bytes and current state. */
                    if (queue_ready && queued && last_jpeg>SEND_QUEUE_TARGET-queued) {
                        queue_ready=false;
                        liot_rtos_enter_critical(); s_diag.encode_defer++; liot_rtos_exit_critical();
                    }
                    if (!queue_ready) {
                        blocked++;
                        tirtc_video_rate_advance(&rate,admitted_at);
                    } else {
                        bool sent=false;
                        unsigned length=0;
                        int ret=encode_next(owner,generation,wechat,&queued,&length,&sent);
                        if (ret<0) { error=ret; break; }
                        if (ret>0) {
                            tirtc_video_rate_advance(&rate,admitted_at);
                            last_jpeg=length;

                        }
                    }
                }
                slot=-1;
                if (owner!=DEMO_TIRTC_OWNER_LIVE) {
                    (void)liot_rtos_mutex_lock(s_rx_lock,LIOT_WAIT_FOREVER);
                    slot=s_pending; s_pending=-1; s_decoding=slot;
                    (void)liot_rtos_mutex_unlock(s_rx_lock);
                }
                if (slot>=0) {
                    /* Encoding/network admission above can change the heap
                     * state. Never reuse that earlier memory sample here. */
                    memory_ready=memory_admit();
                    uint32_t began=now_ms(); int ret=memory_ready ? decode_present(generation,s_rx[slot],s_rx_len[slot]) : VIDEO_ERROR_MEMORY;
                    uint32_t elapsed=now_ms()-began;
                    liot_rtos_enter_critical();
                    s_status.decode_ms=elapsed;
                    if (!ret) s_status.rx_frames++;
                    else { s_status.dropped_frames++; if (memory_ready) s_diag.decode_drop++; else s_diag.memory_drop++; }
                    liot_rtos_exit_critical();
                    (void)liot_rtos_mutex_lock(s_rx_lock,LIOT_WAIT_FOREVER); s_decoding=-1; (void)liot_rtos_mutex_unlock(s_rx_lock);
                }
                /* Min/max are worker samples, not allocator-wide watermarks. */
                size_t free_sample=liot_xPortGetFreeHeapSize();
                size_t block_sample=liot_xPortGetMaximumFreeBlockSize();
                if (free_sample<heap_min) heap_min=free_sample;
                if (block_sample<block_min) block_min=block_sample;
                if (queued>queue_peak) queue_peak=queued;
                if (TIRTC_ENABLE_DEBUG_LOG && now_ms()-last_log>=5000U) {
                    uint32_t log_time=now_ms(),dt=log_time-last_log;
                    tirtc_video_snapshot_t status; tirtc_video_get_snapshot(&status);
                    tirtc_capture_pipeline_get_stats(&pipeline);
                    uint32_t cap_rate=fps_milli(pipeline.captured-last_cap,dt);
                    uint32_t tx_rate=fps_milli(status.tx_frames-last_tx,dt);
                    uint32_t rx_rate=fps_milli(status.rx_frames-last_rx,dt);
                    last_log=log_time; last_cap=pipeline.captured; last_tx=status.tx_frames; last_rx=status.rx_frames;
                    TIRTC_LOG_DEBUG("[VIDEO40] gen=%lu tx=%lu rx=%lu drop=%lu cap_ms=%lu enc_ms=%lu dec_ms=%lu camera=%u queue=%lu jpeg=%u blocked=%u heap=%lu max_block=%lu mem_pause=%u\r\n",
                        (unsigned long)generation,(unsigned long)status.tx_frames,(unsigned long)status.rx_frames,(unsigned long)status.dropped_frames,
                        (unsigned long)status.capture_ms,(unsigned long)status.encode_ms,(unsigned long)status.decode_ms,enabled?1U:0U,
                        (unsigned long)queued,last_jpeg,blocked,(unsigned long)liot_xPortGetFreeHeapSize(),
                        (unsigned long)liot_xPortGetMaximumFreeBlockSize(),s_memory_paused?1U:0U);
                    video_diag_t diag;
                    liot_rtos_enter_critical(); diag=s_diag; liot_rtos_exit_critical();
                    TIRTC_LOG_DEBUG("[VIDEO40-FPS] dt_ms=%lu target=15 cap=%lu.%03lu tx=%lu.%03lu rx=%lu.%03lu cap_replaced=%lu cap_errors=%lu\r\n",
                        (unsigned long)dt,(unsigned long)(cap_rate/1000U),(unsigned long)(cap_rate%1000U),
                        (unsigned long)(tx_rate/1000U),(unsigned long)(tx_rate%1000U),
                        (unsigned long)(rx_rate/1000U),(unsigned long)(rx_rate%1000U),
                        (unsigned long)pipeline.replaced,(unsigned long)pipeline.errors);
                    uint32_t payload_bps=dt?(uint32_t)((uint64_t)(diag.tx_bytes-last_bytes)*8000U/dt):0U;
                    last_bytes=diag.tx_bytes;
                    TIRTC_LOG_DEBUG("[VIDEO40-UP] jpeg_bps=%lu bytes=%lu not_accepted=%lu blocked=%u queue_sample_peak=%lu heap_sample_min=%lu block_sample_min=%lu encode_defer=%lu\r\n",
                        (unsigned long)payload_bps,(unsigned long)diag.tx_bytes,(unsigned long)diag.tx_not_accepted,
                        blocked,(unsigned long)queue_peak,(unsigned long)heap_min,(unsigned long)block_min,
                        (unsigned long)diag.encode_defer);
                    log_uplink_feedback(owner,generation);
                    TIRTC_LOG_DEBUG("[VIDEO40-RX] in=%lu bytes=%lu size_drop=%lu format_drop=%lu lock_drop=%lu replaced=%lu decode_drop=%lu memory_drop=%lu send_zero=%lu send_error=%lu send_ms=%lu size=%lux%lu jpeg=%lu fit=%lux%lu rx_cw90=%u fill=%u crop=%lu,%lu+%lux%lu\r\n",
                        (unsigned long)diag.incoming,(unsigned long)diag.incoming_bytes,
                        (unsigned long)diag.oversized,(unsigned long)diag.format_drop,
                        (unsigned long)diag.lock_drop,(unsigned long)diag.replaced,
                        (unsigned long)diag.decode_drop,(unsigned long)diag.memory_drop,
                        (unsigned long)diag.send_zero,(unsigned long)diag.send_error,
                        (unsigned long)diag.send_ms,(unsigned long)diag.width,
                        (unsigned long)diag.height,(unsigned long)diag.input_jpeg,
                        (unsigned long)diag.out_width,(unsigned long)diag.out_height,s_rx_clockwise?1U:0U,
                        s_wechat && s_incoming?1U:0U,(unsigned long)diag.crop_x,(unsigned long)diag.crop_y,
                        (unsigned long)diag.crop_width,(unsigned long)diag.crop_height);
                    tirtc_camera_fence_stats_t capture;
                    tirtc_camera_fence_get_stats(&capture);
                    TIRTC_LOG_DEBUG("[VIDEO40-CAM] frames=%lu dma=%lu early=%lu timeout=%lu wait_ms=%lu sdk_ret=%d in_flight=%u timing_valid=%u last_dma_ms=%lu last_resume_ms=%lu last_sdk_ms=%lu rearm=%lu stop_err=%lu flush_err=%lu rearm_ret=%d\r\n",
                        (unsigned long)capture.captures,(unsigned long)capture.completions,
                        (unsigned long)capture.sdk_early_returns,(unsigned long)capture.timeouts,
                        (unsigned long)capture.last_wait_ms,capture.last_sdk_result,capture.in_flight?1U:0U,
                        capture.last_timing_valid?1U:0U,(unsigned long)capture.last_dma_ms,
                        (unsigned long)capture.last_resume_ms,(unsigned long)capture.last_sdk_ms,
                        (unsigned long)capture.rearm_prepares,(unsigned long)capture.rearm_stop_errors,
                        (unsigned long)capture.rearm_flush_errors,capture.last_rearm_result);
                }
                liot_rtos_task_sleep_ms(2);
            }
        }
        if (error) { set_error(error); liot_trace("[VIDEO40] failed ret=%d generation=%lu\r\n",error,(unsigned long)generation); }
        while (!hardware_close()) liot_rtos_task_sleep_ms(25);
        liot_rtos_enter_critical(); s_wanted=false; s_status.ready=false; s_status.active=false; liot_rtos_exit_critical();
        liot_trace("[VIDEO40] stopped generation=%lu\r\n",(unsigned long)generation);
    }
}
int tirtc_video_start_service(void)
{
    liot_task_t created=NULL;
    liot_rtos_enter_critical();
    if (s_task || s_starting) { liot_rtos_exit_critical(); return s_task ? 0 : -1; }
    s_starting=true; liot_rtos_exit_critical();
    if (!s_rx_lock && liot_rtos_mutex_create(&s_rx_lock)!=LIOT_OSI_SUCCESS) {s_rx_lock=NULL;goto failed;}
    if (tirtc_capture_pipeline_service_init()!=0) goto failed;
    /* Resume video ahead of GUI work, below audio/control; keep every yield. */
    if (liot_rtos_task_create(&created,12U*1024U,11,"call_video",task,NULL)!=LIOT_OSI_SUCCESS || !created) goto failed;
    liot_rtos_enter_critical(); s_task=created; s_starting=false; liot_rtos_exit_critical(); return 0;
failed:
    liot_rtos_enter_critical(); s_starting=false; liot_rtos_exit_critical(); return -1;
}

#include "tirtc_preferences.h"
#include <stddef.h>
#include <string.h>
#include "liot_nv.h"
#include "liot_os.h"
#include "liot_log.h"
#include "liot_fs_api.h"

/* Audited F6D_D SDK (2026-10-09): the public NVM/stat wrappers collapse NOENT and I/O
 * errors. LFS_stat is the existing, serialized baseAPI entry used by
 * liot_stat itself; retain its raw error instead. No file creation/handle or
 * SDK patch is involved. See tirtc_app.mk for the checked preference ABI hashes. */
#if defined(__arm__) && !defined(TIRTC_PREFS_F6D_ABI_VERIFIED)
#error "Verify the pinned F6D_D baseAPI/base ELF/NVM hashes before building"
#endif
extern int LFS_stat(const char *path, liot_stat_s *info);
typedef char prefs_stat_layout[(offsetof(liot_stat_s,size)==4 && offsetof(liot_stat_s,name)==8) ? 1 : -1];

/* Explicit little-endian wire layout, independent of bool/enum/padding ABI:
 * magic[4], version[1], size[1], reserved[2], sequence[4], settings[4],
 * reserved[12], CRC32[4]. Never share these files with device identity. */
#define RECORD_BYTES 32U
#define DEBOUNCE_MS 1000U
#define RETRY_MS 5000U
static const char *const s_files[2] = {"tirtc_audio_a.nvm", "tirtc_audio_b.nvm"};
static tirtc_preferences_t s_value = {8,8,true,true};
static tirtc_preferences_t s_saved = {8,8,true,true};
static bool s_initialized, s_initializing, s_dirty, s_saving, s_uncertain, s_retry;
static uint32_t s_changed_ms, s_retry_ms, s_sequence;
static int s_active_slot = -1;
static int s_load_result = 1, s_save_error;
static bool s_load_pending, s_load_busy;
static int s_load_error;
static uint32_t s_load_retry_ms, s_value_revision, s_save_revision;
static uint8_t s_edited_fields;

static bool same(const tirtc_preferences_t *a, const tirtc_preferences_t *b)
{
    return a->volume == b->volume && a->mic_gain == b->mic_gain &&
           a->speaker_enabled == b->speaker_enabled && a->mic_enabled == b->mic_enabled;
}
static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void put32(uint8_t *p, uint32_t value)
{
    for (unsigned i=0; i<4; ++i) p[i]=(uint8_t)(value>>(8U*i));
}
static uint32_t crc32(const uint8_t *p, size_t length)
{
    uint32_t crc=UINT32_MAX;
    for (size_t i=0;i<length;++i) {
        crc^=p[i];
        for (unsigned bit=0;bit<8;++bit) crc=(crc>>1)^((crc&1U)?0xedb88320U:0U);
    }
    return ~crc;
}
static void encode(uint8_t record[RECORD_BYTES], const tirtc_preferences_t *value, uint32_t sequence)
{
    memset(record,0,RECORD_BYTES);
    memcpy(record,"TAUD",4); record[4]=1; record[5]=RECORD_BYTES;
    put32(record+8,sequence);
    record[12]=value->volume; record[13]=value->mic_gain;
    record[14]=value->speaker_enabled?1U:0U; record[15]=value->mic_enabled?1U:0U;
    put32(record+28,crc32(record,28));
}
static bool decode(const uint8_t record[RECORD_BYTES], tirtc_preferences_t *value, uint32_t *sequence)
{
    if (memcmp(record,"TAUD",4) || record[4]!=1 || record[5]!=RECORD_BYTES ||
        record[6] || record[7] || !get32(record+8) || record[12]>10 || record[13]>10 ||
        record[14]>1 || record[15]>1 || get32(record+28)!=crc32(record,28)) return false;
    for (unsigned i=16;i<28;++i) if (record[i]) return false;
    value->volume=record[12]; value->mic_gain=record[13];
    value->speaker_enabled=record[14]!=0; value->mic_enabled=record[15]!=0;
    *sequence=get32(record+8);
    return true;
}
static void log_value(const char *operation, const tirtc_preferences_t *value, uint32_t sequence)
{
    liot_trace("[audio-prefs] %s seq=%lu speaker=%u mic_gain=%u spk_enabled=%u mic_enabled=%u\r\n",
               operation,(unsigned long)sequence,value->volume,value->mic_gain,
               (unsigned)value->speaker_enabled,(unsigned)value->mic_enabled);
}

typedef struct {
    tirtc_preferences_t value;
    uint32_t sequence;
    int slot, result, error;
} load_result_t;

static load_result_t read_slots(void)
{
    uint8_t records[2][RECORD_BYTES];
    tirtc_preferences_t values[2], chosen={8,8,true,true};
    uint32_t sequences[2]={0,0}, sequence=0;
    bool valid[2]={false,false};
    int slot=-1, error=0;
    bool ambiguous=false;
    for (unsigned i=0;i<2;++i) {
        liot_stat_s info;
        int ret;
        memset(&info,0,sizeof(info));
        ret=LFS_stat(s_files[i],&info);
        if (ret==-2) continue; /* Exact LittleFS NOENT; not a generic SDK error. */
        if (ret || info.type!=LIOT_FS_TYPE_FILE) {
            if (!error) error=ret<0?ret:-84;
            continue;
        }
        if (info.size!=RECORD_BYTES) continue; /* Confirmed empty/truncated record. */
        memset(records[i],0,RECORD_BYTES);
        ret=liot_nvm_fread(s_files[i],records[i],RECORD_BYTES,1);
        if (ret!=(int)RECORD_BYTES) {
            if (!error) error=ret<0?ret:-11;
            continue; /* Exists but unknown: it may contain a newer valid sequence. */
        }
        valid[i]=decode(records[i],&values[i],&sequences[i]);
    }
    if (valid[0]) slot=0;
    if (valid[1] && (slot<0 || (int32_t)(sequences[1]-sequences[0])>0)) slot=1;
    /* Equal-sequence disagreement and an exactly half-range difference have
     * no trustworthy ordering. Keep both files intact and use defaults. */
    if (valid[0] && valid[1] &&
        ((sequences[0]==sequences[1] && !same(&values[0],&values[1])) ||
         sequences[1]-sequences[0]==0x80000000U)) ambiguous=true;
    if (ambiguous) {
        /* Preserve slot A and use its sequence only as the anchor for the
         * next user-requested write to B, resolving the ambiguity safely. */
        slot=0; sequence=sequences[0];
    } else if (slot>=0) { chosen=values[slot]; sequence=sequences[slot]; }
    load_result_t result={chosen,sequence,slot,error?error:(slot>=0 && !ambiguous?0:1),error};
    return result;
}

int tirtc_preferences_init(void)
{
    load_result_t loaded;
    int result;
    uint32_t now;
    liot_rtos_enter_critical();
    if (s_initialized) { result=s_load_result; liot_rtos_exit_critical(); return result; }
    if (s_initializing) { liot_rtos_exit_critical(); return -4; }
    s_initializing=true;
    liot_rtos_exit_critical();
    loaded=read_slots(); now=liot_rtos_get_running_time();
    liot_rtos_enter_critical();
    s_value=s_saved=loaded.value; s_sequence=loaded.sequence; s_active_slot=loaded.slot;
    s_load_result=loaded.result; s_save_error=0;
    s_load_pending=loaded.error!=0; s_load_error=loaded.error;
    s_load_retry_ms=now; s_load_busy=false; s_edited_fields=0; ++s_value_revision;
    s_save_revision=0;
    s_dirty=s_saving=s_uncertain=s_retry=false;
    s_initialized=true; s_initializing=false;
    liot_rtos_exit_critical();
    if (loaded.error) liot_trace("[audio-prefs] LOAD_PENDING error=%d retry_ms=%u\r\n",loaded.error,RETRY_MS);
    else log_value(loaded.result ? "LOAD_DEFAULTS" : "LOAD_OK",&loaded.value,loaded.sequence);
    return loaded.result;
}

void tirtc_preferences_get(tirtc_preferences_t *out)
{
    if (!out) return;
    liot_rtos_enter_critical(); *out=s_value; liot_rtos_exit_critical();
}
void tirtc_preferences_get_snapshot(tirtc_preferences_snapshot_t *out)
{
    if (!out) return;
    liot_rtos_enter_critical();
    out->initialized=s_initialized; out->dirty=s_dirty; out->saving=s_saving;
    out->save_error=s_save_error; out->sequence=s_sequence;
    out->load_pending=s_load_pending; out->load_error=s_load_error;
    out->value_revision=s_value_revision; out->value=s_value;
    out->save_revision=s_save_revision;
    liot_rtos_exit_critical();
}
static void update_locked(const tirtc_preferences_t *value, uint32_t now_ms)
{
    if (!same(value,&s_value)) {
        if (s_load_pending) {
            if (value->volume!=s_value.volume) s_edited_fields|=1U;
            if (value->mic_gain!=s_value.mic_gain) s_edited_fields|=2U;
            if (value->speaker_enabled!=s_value.speaker_enabled) s_edited_fields|=4U;
            if (value->mic_enabled!=s_value.mic_enabled) s_edited_fields|=8U;
        }
        s_value=*value; s_changed_ms=now_ms;
        ++s_value_revision;
    }
    s_dirty=s_uncertain || s_saving || (s_load_pending && s_edited_fields) || !same(&s_value,&s_saved);
}
int tirtc_preferences_set(const tirtc_preferences_t *value, uint32_t now_ms)
{
    if (!value || value->volume>10 || value->mic_gain>10) return -2;
    liot_rtos_enter_critical();
    if (!s_initialized) { liot_rtos_exit_critical(); return -3; }
    update_locked(value,now_ms);
    liot_rtos_exit_critical();
    return 0;
}

int tirtc_preferences_update(tirtc_preferences_field_t field, int value,
                              uint32_t now_ms, tirtc_preferences_t *out)
{
    tirtc_preferences_t next;
    if ((unsigned)field>(unsigned)TIRTC_PREFERENCES_MIC_ENABLED ||
        value<0 || value>((field==TIRTC_PREFERENCES_VOLUME || field==TIRTC_PREFERENCES_MIC_GAIN)?10:1)) return -2;
    liot_rtos_enter_critical();
    if (!s_initialized) { liot_rtos_exit_critical(); return -3; }
    next=s_value;
    if (s_load_pending) s_edited_fields|=(uint8_t)(1U<<(unsigned)field);
    switch(field) {
    case TIRTC_PREFERENCES_VOLUME: next.volume=(uint8_t)value; break;
    case TIRTC_PREFERENCES_MIC_GAIN: next.mic_gain=(uint8_t)value; break;
    case TIRTC_PREFERENCES_SPEAKER_ENABLED: next.speaker_enabled=value!=0; break;
    case TIRTC_PREFERENCES_MIC_ENABLED: next.mic_enabled=value!=0; break;
    }
    update_locked(&next,now_ms);
    if (out) *out=s_value;
    liot_rtos_exit_critical(); return 0;
}
int tirtc_preferences_adjust_volume(int delta, uint32_t now_ms, tirtc_preferences_t *out)
{
    tirtc_preferences_t next;
    liot_rtos_enter_critical();
    if (!s_initialized) { liot_rtos_exit_critical(); return -3; }
    next=s_value;
    if (delta>10-(int)next.volume) next.volume=10;
    else if (delta<-(int)next.volume) next.volume=0;
    else next.volume=(uint8_t)((int)next.volume+delta);
    update_locked(&next,now_ms);
    if (out) *out=s_value;
    liot_rtos_exit_critical(); return 0;
}

void tirtc_preferences_poll(uint32_t now_ms)
{
    uint8_t record[RECORD_BYTES], verify[RECORD_BYTES];
    tirtc_preferences_t value, decoded;
    uint32_t sequence, verified_sequence=0;
    int slot, error=0, previous_error;
    liot_rtos_enter_critical();
    if (s_initialized && s_load_pending && !s_load_busy && !s_saving &&
        (uint32_t)(now_ms-s_load_retry_ms)>=RETRY_MS) {
        load_result_t loaded;
        tirtc_preferences_t merged;
        s_load_busy=true;
        liot_rtos_exit_critical();
        loaded=read_slots();
        uint32_t completed_ms=liot_rtos_get_running_time();
        liot_rtos_enter_critical();
        previous_error=s_load_error; s_load_error=loaded.error;
        s_load_retry_ms=completed_ms; s_load_result=loaded.result;
        if (!loaded.error) {
            merged=loaded.value;
            if (s_edited_fields&1U) merged.volume=s_value.volume;
            if (s_edited_fields&2U) merged.mic_gain=s_value.mic_gain;
            if (s_edited_fields&4U) merged.speaker_enabled=s_value.speaker_enabled;
            if (s_edited_fields&8U) merged.mic_enabled=s_value.mic_enabled;
            if (!same(&merged,&s_value)) { s_value=merged; ++s_value_revision; }
            s_saved=loaded.value; s_sequence=loaded.sequence; s_active_slot=loaded.slot;
            s_load_pending=false; s_edited_fields=0;
            s_dirty=s_uncertain || !same(&s_value,&s_saved);
        }
        s_load_busy=false;
        liot_rtos_exit_critical();
        if (!loaded.error) log_value("LOAD_RECOVERED",&loaded.value,loaded.sequence);
        else if (loaded.error!=previous_error)
            liot_trace("[audio-prefs] LOAD_PENDING error=%d retry_ms=%u\r\n",loaded.error,RETRY_MS);
        return;
    }
    if (!s_initialized || !s_dirty || s_saving ||
        s_load_pending || s_load_busy ||
        (uint32_t)(now_ms-s_changed_ms)<DEBOUNCE_MS ||
        (s_retry && (uint32_t)(now_ms-s_retry_ms)<RETRY_MS)) {
        liot_rtos_exit_critical(); return;
    }
    s_saving=true; value=s_value; slot=s_active_slot==0?1:0;
    sequence=s_sequence+1U; if (!sequence) sequence=1;
    liot_rtos_exit_critical();
    encode(record,&value,sequence);
    if (liot_nvm_fwrite(s_files[slot],record,RECORD_BYTES,1)!=(int)RECORD_BYTES) error=-10;
    else {
        memset(verify,0,RECORD_BYTES);
        if (liot_nvm_fread(s_files[slot],verify,RECORD_BYTES,1)!=(int)RECORD_BYTES) error=-11;
        else if (!decode(verify,&decoded,&verified_sequence) || memcmp(record,verify,RECORD_BYTES)) error=-12;
    }
    liot_rtos_enter_critical();
    previous_error=s_save_error; s_save_error=error;
    if (!error) {
        s_saved=value; s_sequence=sequence; s_active_slot=slot;
        ++s_save_revision;
        s_uncertain=s_retry=false; s_dirty=!same(&s_value,&s_saved);
    } else {
        /* A failed readback may hide a complete valid write. Even a later
         * change back to the old saved value must repair that uncertain slot. */
        s_uncertain=s_dirty=s_retry=true; s_retry_ms=now_ms;
    }
    s_saving=false;
    liot_rtos_exit_critical();
    if (!error) log_value("SAVE_OK",&value,sequence);
    else if (error!=previous_error)
        liot_trace("[audio-prefs] SAVE_FAILED error=%d retry_ms=%u\r\n",error,RETRY_MS);
}

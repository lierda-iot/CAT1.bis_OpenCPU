/* External storage owns SPI1 and its LittleFS instance. Never call the SDK's
 * liot_finit_ext: that implementation formats automatically after mount failure. */
#include "tirtc_storage.h"
#include "tirtc_storage_port.h"
#include "tirtc_lfs.h"
#include <limits.h>
#include <string.h>

#define CHIP_SIZE (4U * 1024U * 1024U)
#define BLOCK_SIZE 4096U
#define PAGE_SIZE 256U
#define FILE_COUNT 2U
#define IO_BUDGET_MS 10000U
#define INIT_BUDGET_MS 120000U
static tirtc_storage_snapshot_t s_snapshot, s_work;
static lfs_t s_fs;
static struct lfs_config s_cfg;
static uint8_t s_read_cache[PAGE_SIZE], s_prog_cache[PAGE_SIZE], s_lookahead[128];
static uint8_t s_tx[PAGE_SIZE + 4], s_rx[PAGE_SIZE + 4], s_scan[PAGE_SIZE];
static uint32_t s_operation_start, s_operation_budget;
static bool s_started, s_mounted, s_write_allowed;
static bool s_legacy_geometry;
static bool s_operation_mutating;
static int s_io_error;
static uint32_t s_descriptor_sequence;
static struct {
    bool initialized, read_only, legacy_geometry;
    uint32_t jedec_id, block_size, block_count, ready_state;
    uint32_t retry_started, retry_delay;
} s_recovery;
static struct {
    lfs_file_t file;
    struct lfs_file_config cfg;
    uint8_t cache[PAGE_SIZE];
    int flags;
    int token;
    bool open;
} s_files[FILE_COUNT];

void tirtc_storage_legacy_geometry(const struct lfs_config *cfg,
                                   uint32_t *size, uint32_t *count)
{
    /* Exact SDK prebuilt-image inconsistency, never enabled for writable mounts.
     * The shipped lfsutil reads this layout with -B 4096; its superblock still
     * contains the old 256/8192 geometry. No flash bytes are changed here. */
    if (s_legacy_geometry && !s_write_allowed && cfg->block_size == BLOCK_SIZE &&
        cfg->block_count == 512U && *size == 256U && *count == 8192U) {
        *size = BLOCK_SIZE; *count = 512U;
    }
}

static void publish(void)
{
    uint32_t mask = tirtc_storage_port_enter();
    s_snapshot = s_work;
    tirtc_storage_port_leave(mask);
}
static void progress(void) { s_work.progress_ms = tirtc_storage_port_ms(); publish(); }
void tirtc_storage_get_snapshot(tirtc_storage_snapshot_t *out)
{
    uint32_t mask;
    if (!out) return;
    mask = tirtc_storage_port_enter();
    *out = s_snapshot;
    tirtc_storage_port_leave(mask);
}
static void begin_operation(uint32_t budget)
{
    s_operation_start = tirtc_storage_port_ms(); s_operation_budget = budget;
    s_io_error = 0; s_operation_mutating = false;
}
static int checked_transfer(uint32_t length)
{
    int ret;
    bool budget = (uint32_t)(tirtc_storage_port_ms() - s_operation_start) >= s_operation_budget;
    if (budget)
        ret = TIRTC_STORAGE_E_TIMEOUT;
    else ret = tirtc_storage_port_transfer(s_tx, s_rx, length);
    if (ret) {
        if (!s_io_error) tirtc_storage_port_diagnostic(budget ? "budget" : "transfer", ret,
            (uint32_t)(tirtc_storage_port_ms() - s_operation_start), s_tx[0]);
        s_io_error = ret;
    }
    return ret;
}
static int status(uint8_t *value)
{
    int ret;
    s_tx[0] = 0x05; s_tx[1] = 0xff;
    ret = checked_transfer(2);
    if (!ret) *value = s_rx[1];
    return ret;
}
static int ready(void)
{
    uint32_t start = tirtc_storage_port_ms(), tries = 0;
    for (;;) {
        uint8_t sr = 0;
        int ret = status(&sr);
        if (ret) return ret;
        if (!(sr & 1U)) return 0;
        if ((uint32_t)(tirtc_storage_port_ms() - start) >= 3000U || ++tries >= 3000U) {
            if (!s_io_error) tirtc_storage_port_diagnostic("ready", TIRTC_STORAGE_E_TIMEOUT,
                (uint32_t)(tirtc_storage_port_ms() - start), 0x05U);
            return s_io_error = TIRTC_STORAGE_E_TIMEOUT;
        }
        tirtc_storage_port_delay(1);
    }
}
static int read_id(uint32_t *id)
{
    int ret;
    memset(s_tx, 0xff, 4); s_tx[0] = 0x9f;
    ret = checked_transfer(4);
    if (!ret) *id = ((uint32_t)s_rx[1] << 16) | ((uint32_t)s_rx[2] << 8) | s_rx[3];
    return ret;
}
static void command_address(uint8_t command, uint32_t address)
{
    s_tx[0] = command; s_tx[1] = (uint8_t)(address >> 16);
    s_tx[2] = (uint8_t)(address >> 8); s_tx[3] = (uint8_t)address;
}
static int raw_read(uint32_t address, void *buffer, uint32_t size)
{
    uint8_t *out = buffer;
    if (!buffer || address > CHIP_SIZE || size > CHIP_SIZE - address)
        return TIRTC_STORAGE_E_ARGUMENT;
    while (size) {
        uint32_t n = size > PAGE_SIZE ? PAGE_SIZE : size;
        int ret;
        command_address(0x03, address); memset(s_tx + 4, 0xff, n);
        ret = checked_transfer(n + 4);
        if (ret) return ret;
        memcpy(out, s_rx + 4, n);
        out += n; address += n; size -= n;
    }
    return 0;
}
static int write_enable(void)
{
    uint8_t sr;
    int ret = ready();
    if (ret) return ret;
    ret = status(&sr);
    if (ret) return ret;
    /* Never clear protection bits or write status registers automatically. */
    if (sr & 0x7cU) return s_io_error = TIRTC_STORAGE_E_PROTECTED;
    s_tx[0] = 0x06;
    ret = checked_transfer(1);
    if (ret) return ret;
    ret = status(&sr);
    if (ret) return ret;
    return (sr & 2U) ? 0 : (s_io_error = TIRTC_STORAGE_E_PROTECTED);
}
static int raw_program(uint32_t address, const void *buffer, uint32_t size)
{
    int ret;
    s_operation_mutating = true;
    if (!s_write_allowed || !buffer || !size || size > PAGE_SIZE ||
        address >= CHIP_SIZE || size > CHIP_SIZE - address ||
        address / PAGE_SIZE != (address + size - 1U) / PAGE_SIZE)
        return TIRTC_STORAGE_E_ARGUMENT;
    ret = write_enable();
    if (ret) return ret;
    command_address(0x02, address); memcpy(s_tx + 4, buffer, size);
    ret = checked_transfer(size + 4);
    if (!ret) ret = ready();
    if (!ret) ret = raw_read(address, s_scan, size);
    if (!ret && memcmp(buffer, s_scan, size)) ret = s_io_error = TIRTC_STORAGE_E_IO;
    return ret;
}
static int raw_erase(uint32_t address)
{
    uint32_t offset, i;
    int ret;
    s_operation_mutating = true;
    if (!s_write_allowed || address % BLOCK_SIZE || address >= CHIP_SIZE)
        return TIRTC_STORAGE_E_ARGUMENT;
    ret = write_enable();
    if (ret) return ret;
    command_address(0x20, address);
    ret = checked_transfer(4);
    if (!ret) ret = ready();
    for (offset = 0; !ret && offset < BLOCK_SIZE; offset += PAGE_SIZE) {
        ret = raw_read(address + offset, s_scan, PAGE_SIZE);
        if (!ret) for (i = 0; i < PAGE_SIZE; ++i) if (s_scan[i] != 0xff) {
            ret = s_io_error = TIRTC_STORAGE_E_IO; break;
        }
    }
    return ret;
}
static int block_read(const struct lfs_config *c, lfs_block_t block,
                      lfs_off_t offset, void *buffer, lfs_size_t size)
{
    if (block >= CHIP_SIZE / c->block_size || offset > c->block_size || size > c->block_size - offset)
        return LFS_ERR_CORRUPT;
    return raw_read(block * c->block_size + offset, buffer, size);
}
static int block_program(const struct lfs_config *c, lfs_block_t block,
                         lfs_off_t offset, const void *buffer, lfs_size_t size)
{
    const uint8_t *in = buffer;
    (void)c;
    if (block >= CHIP_SIZE / BLOCK_SIZE || offset > BLOCK_SIZE || size > BLOCK_SIZE - offset)
        return LFS_ERR_CORRUPT;
    while (size) {
        uint32_t n = size > PAGE_SIZE ? PAGE_SIZE : size;
        int ret = raw_program(block * BLOCK_SIZE + offset, in, n);
        if (ret) return ret;
        size -= n; offset += n; in += n;
    }
    return 0;
}
static int block_erase(const struct lfs_config *c, lfs_block_t block)
{
    (void)c;
    if (block >= CHIP_SIZE / BLOCK_SIZE) return LFS_ERR_CORRUPT;
    return raw_erase(block * BLOCK_SIZE);
}
static int block_sync(const struct lfs_config *c) { (void)c; return ready(); }
static void setup_config(void)
{
    memset(&s_cfg, 0, sizeof(s_cfg));
    s_cfg.read = block_read; s_cfg.prog = block_program;
    s_cfg.erase = block_erase; s_cfg.sync = block_sync;
    s_cfg.read_size = s_cfg.prog_size = s_cfg.cache_size = PAGE_SIZE;
    s_cfg.block_size = BLOCK_SIZE;
    s_cfg.block_cycles = 500;
    s_cfg.lookahead_size = sizeof(s_lookahead);
    s_cfg.read_buffer = s_read_cache; s_cfg.prog_buffer = s_prog_cache;
    s_cfg.lookahead_buffer = s_lookahead;
    /* Retain the shipped SDK's on-disk 2.0 format, including on grow. */
    s_cfg.disk_version = 0x00020000U;
    /* block_count=0 reads actual on-disk geometry without assuming 2/4 MiB. */
}
/* Returns 1 only after every byte was read successfully and was FF. */
static int scan_blank(uint32_t from, uint32_t to)
{
    uint32_t offset, i;
    s_work.scanned_bytes = 0; s_work.scan_total_bytes = to - from; progress();
    for (offset = from; offset < to; offset += PAGE_SIZE) {
        int ret = raw_read(offset, s_scan, PAGE_SIZE);
        if (ret) return ret;
        for (i = 0; i < PAGE_SIZE; i++) if (s_scan[i] != 0xff) return 0;
        s_work.scanned_bytes = offset + PAGE_SIZE - from;
        if (!(s_work.scanned_bytes % BLOCK_SIZE)) {
            progress(); tirtc_storage_port_delay(1);
        }
    }
    progress(); return 1;
}
static int usage(void)
{
    struct lfs_fsinfo info;
    lfs_ssize_t blocks;
    int ret = lfs_fs_stat(&s_fs, &info);
    s_work.filesystem_sample_ms = tirtc_storage_port_ms();
    if (ret) return ret;
    if ((info.block_size != BLOCK_SIZE && info.block_size != PAGE_SIZE) || info.block_count < 2U ||
        info.block_count > CHIP_SIZE / info.block_size) return LFS_ERR_CORRUPT;
    blocks = lfs_fs_size(&s_fs);
    s_work.filesystem_sample_ms = tirtc_storage_port_ms();
    if (blocks < 0) return (int)blocks;
    if ((uint32_t)blocks > info.block_count) return LFS_ERR_CORRUPT;
    s_work.filesystem_total_bytes = info.block_count * info.block_size;
    s_work.filesystem_free_bytes = (info.block_count - (uint32_t)blocks) * info.block_size;
    s_work.filesystem_valid = true;
    return 0;
}
static int verify_tree(const char *path, unsigned depth)
{
    lfs_dir_t dir; struct lfs_info info;
    char child[256]; int ret;
    if (depth > 6U) return LFS_ERR_CORRUPT;
    ret = lfs_dir_open(&s_fs, &dir, path);
    if (ret) return ret;
    while ((ret = lfs_dir_read(&s_fs, &dir, &info)) > 0) {
        size_t len = strlen(path), name = strlen(info.name);
        if (!strcmp(info.name,".") || !strcmp(info.name,"..")) continue;
        if (len + name + 2U > sizeof(child)) { ret = LFS_ERR_CORRUPT; break; }
        memcpy(child,path,len);
        if (len > 1U) child[len++] = '/';
        memcpy(child+len,info.name,name+1U);
        if (info.type == LFS_TYPE_DIR) ret = verify_tree(child,depth+1U);
        else {
            uint32_t total = 0;
            memset(&s_files[0],0,sizeof(s_files[0]));
            s_files[0].cfg.buffer = s_files[0].cache;
            ret = lfs_file_opencfg(&s_fs,&s_files[0].file,child,LFS_O_RDONLY,&s_files[0].cfg);
            if (!ret) {
                int close_ret;
                while ((ret = (int)lfs_file_read(&s_fs,&s_files[0].file,s_scan,sizeof(s_scan))) > 0) {
                    total += (uint32_t)ret;
                    if (total > info.size) { ret = LFS_ERR_CORRUPT; break; }
                }
                close_ret = lfs_file_close(&s_fs,&s_files[0].file);
                if (!ret) ret = total == info.size ? close_ret : LFS_ERR_CORRUPT;
            }
        }
        if (ret < 0) break;
        progress();
    }
    (void)lfs_dir_close(&s_fs,&dir);
    return ret < 0 ? ret : 0;
}
static bool transient_read_error(int error)
{
    return error == TIRTC_STORAGE_E_IO || error == TIRTC_STORAGE_E_TIMEOUT;
}
static void discard_mount(void)
{
    /* All file and filesystem buffers belong to this module. The pinned
     * LittleFS lfs_unmount -> lfs_deinit only releases allocated buffers; it
     * neither closes files nor flushes them. Our buffers are all static. This
     * deliberately abandons dirty file caches, never replaying a failed write.
     * The filesystem mutex is held, so no caller can retain a live lfs object. */
    if (s_mounted) (void)lfs_unmount(&s_fs);
    s_mounted = false;
    memset(s_files, 0, sizeof(s_files));
    memset(&s_fs, 0, sizeof(s_fs));
    memset(s_read_cache, 0, sizeof(s_read_cache));
    memset(s_prog_cache, 0, sizeof(s_prog_cache));
    memset(s_lookahead, 0, sizeof(s_lookahead));
}
static int probe_readonly_layout(bool legacy)
{
    int ret;
    s_write_allowed = false;
    setup_config();
    s_legacy_geometry = legacy;
    s_cfg.block_size = legacy ? BLOCK_SIZE : PAGE_SIZE;
    s_cfg.block_count = legacy ? 512U : 0U;
    ret = lfs_mount(&s_fs, &s_cfg);
    if (!ret) {
        s_mounted = true;
        ret = verify_tree("/", 0);
        if (!ret) ret = usage();
    }
    if (s_io_error) ret = s_io_error;
    discard_mount();
    s_work.filesystem_valid = false;
    return ret;
}
static void schedule_retry(uint32_t delay)
{
    s_recovery.retry_started = tirtc_storage_port_ms();
    s_recovery.retry_delay = delay;
    s_work.next_retry_ms = s_recovery.retry_started + delay;
    s_work.recovery_pending = true;
}
static void runtime_fault(int error)
{
    bool retry = s_recovery.initialized && !s_operation_mutating && transient_read_error(error);
    s_write_allowed = false;
    s_work.filesystem_valid = false;
    s_work.state = TIRTC_STORAGE_STATE_ERROR;
    s_work.last_error = error;
    s_work.recovery_pending = false;
    s_work.next_retry_ms = 0;
    discard_mount();
    if (retry) schedule_retry(5000U);
}
static void recover_mount(void)
{
    uint32_t id = 0, again = 0;
    int ret;
    ++s_work.recovery_attempts;
    s_work.progress_ms = tirtc_storage_port_ms();
    publish();
    begin_operation(IO_BUDGET_MS);
    s_write_allowed = false;
    ret = tirtc_storage_port_recover();
    if (!ret) ret = read_id(&id);
    if (!ret) ret = read_id(&again);
    /* A different/absent/unstable chip is not a transient filesystem retry.
     * Retain the last known identity/capacity for diagnosis, marked invalid. */
    if (!ret && (id != s_recovery.jedec_id || again != s_recovery.jedec_id)) {
        s_work.physical_valid = false;
        ret = TIRTC_STORAGE_E_ARGUMENT;
    }
    if (!ret) ret = ready();
    if (!ret) {
        setup_config();
        s_cfg.block_size = s_recovery.block_size;
        s_cfg.block_count = s_recovery.block_count;
        s_legacy_geometry = s_recovery.legacy_geometry;
        ret = lfs_mount(&s_fs, &s_cfg);
        if (!ret) {
            s_mounted = true;
            ret = usage();
            if (!ret && s_work.filesystem_total_bytes !=
                s_recovery.block_size * s_recovery.block_count) ret = LFS_ERR_CORRUPT;
        }
    }
    if (s_io_error) ret = s_io_error;
    s_work.last_error = ret;
    if (!ret) {
        s_work.read_only = s_recovery.read_only;
        s_work.physical_valid = true;
        s_write_allowed = !s_work.read_only;
        s_work.state = s_recovery.ready_state;
        s_work.recovery_pending = false;
        s_work.next_retry_ms = 0;
        s_recovery.retry_delay = 0;
        ++s_work.recovery_successes;
    } else {
        s_work.filesystem_valid = false;
        s_work.state = TIRTC_STORAGE_STATE_ERROR;
        discard_mount();
        s_work.recovery_pending = false;
        s_work.next_retry_ms = 0;
        if (!s_operation_mutating && transient_read_error(ret)) {
            uint32_t delay = s_recovery.retry_delay >= 30000U ? 60000U :
                s_recovery.retry_delay * 2U;
            schedule_retry(delay);
        }
    }
    s_work.progress_ms = tirtc_storage_port_ms();
    tirtc_storage_port_diagnostic("recover", ret,
        (uint32_t)(s_work.progress_ms - s_operation_start), 0);
    publish();
}
int tirtc_storage_init(void)
{
    uint32_t id = 0, again = 0;
    int ret;
    if (s_started) return s_snapshot.filesystem_valid ? 0 : TIRTC_STORAGE_E_NOT_READY;
    s_started = true; s_work.state = TIRTC_STORAGE_STATE_PROBING; progress();
    ret = tirtc_storage_port_init();
    if (ret) goto error_without_lock;
    ret = tirtc_storage_port_lock(100);
    if (ret) goto error_without_lock;
    begin_operation(INIT_BUDGET_MS);
    ret = read_id(&id);
    if (!ret) ret = read_id(&again);
    if (ret) goto error;
    s_work.jedec_id = id;
    if (!id || id == 0xffffffU || id != again) {
        s_work.state = TIRTC_STORAGE_STATE_ABSENT;
        ret = TIRTC_STORAGE_E_NOT_READY; goto done;
    }
    if ((id & 255U) >= 16U && (id & 255U) <= 24U) {
        s_work.physical_total_bytes = 1U << (id & 255U);
        s_work.physical_valid = true;
    }
    /* V04 BOM P25Q32SH: Puya 0x85, 3.3 V Q-series type 0x60, 32 Mbit 0x16.
     * Other devices are reported but never written with assumed geometry. */
    if (id != 0x856016U) {
        s_work.state = TIRTC_STORAGE_STATE_UNSUPPORTED;
        ret = TIRTC_STORAGE_E_ARGUMENT; goto done;
    }
    ret = ready();
    if (ret) goto error;
    setup_config();
    ret = lfs_mount(&s_fs, &s_cfg);
    if (ret && !s_io_error) {
        int native_result = probe_readonly_layout(false), legacy_result;
        if (s_io_error) { ret = s_io_error; goto error; }
        legacy_result = probe_readonly_layout(true);
        if (s_io_error) { ret = s_io_error; goto error; }
        /* The SDK image stores 256-byte geometry but actually uses 4096-byte
         * sectors. A genuine 256-byte image can ALSO expose an older, valid
         * metadata tree through that compatibility view. Never choose by
         * probe order or merely by mount success: require one full valid view.
         * If both validate, leave every byte intact and reject ambiguity. */
        if (!native_result && !legacy_result) {
            s_work.filesystem_total_bytes = s_work.filesystem_free_bytes = 0;
            s_work.state = TIRTC_STORAGE_STATE_UNKNOWN_DATA;
            ret = LFS_ERR_CORRUPT; goto done;
        }
        setup_config();
        s_legacy_geometry = !legacy_result;
        if (!native_result || !legacy_result) {
            s_cfg.block_size = s_legacy_geometry ? BLOCK_SIZE : PAGE_SIZE;
            s_cfg.block_count = s_legacy_geometry ? 512U : 0U;
            ret = lfs_mount(&s_fs, &s_cfg);
            if (!ret) s_work.read_only = true;
        } else {
            s_work.filesystem_total_bytes = s_work.filesystem_free_bytes = 0;
            ret = legacy_result;
        }
    }
    if (!ret) {
        s_mounted = true;
        ret = usage();
        if (ret) goto error;
        if (s_work.read_only) {
            ret = verify_tree("/",0);
            if (ret) goto error;
        }
        s_work.preserved_existing = true;
        /* Do not repurpose unknown data beyond a valid existing filesystem. */
        if (!s_work.read_only && s_work.filesystem_total_bytes == CHIP_SIZE / 2U) {
            ret = scan_blank(CHIP_SIZE / 2U, CHIP_SIZE);
            if (ret < 0) goto error;
            if (ret == 1) {
                s_write_allowed = true;
                ret = lfs_fs_grow(&s_fs, CHIP_SIZE / BLOCK_SIZE);
                if (ret) goto error;
                s_work.expanded = true;
            }
        }
        s_write_allowed = !s_work.read_only;
    } else {
        if (s_io_error) { ret = s_io_error; goto error; }
        ret = scan_blank(0, CHIP_SIZE);
        if (ret < 0) goto error;
        if (!ret) {
            s_work.state = TIRTC_STORAGE_STATE_UNKNOWN_DATA;
            ret = LFS_ERR_CORRUPT; goto done;
        }
        /* An independent second full pass catches unstable/all-high reads. */
        ret = scan_blank(0, CHIP_SIZE);
        if (ret < 0) goto error;
        if (!ret) { s_work.state = TIRTC_STORAGE_STATE_UNKNOWN_DATA; ret = LFS_ERR_CORRUPT; goto done; }
        ret = read_id(&again);
        if (ret || again != id) { if (!ret) ret = TIRTC_STORAGE_E_IO; goto error; }
        s_cfg.block_count = CHIP_SIZE / BLOCK_SIZE;
        s_write_allowed = true;
        ret = lfs_format(&s_fs, &s_cfg);
        if (ret) goto error;
        ret = lfs_mount(&s_fs, &s_cfg);
        if (ret) goto error;
        s_mounted = true;
    }
    ret = usage();
    if (ret || s_io_error) { if (s_io_error) ret = s_io_error; goto error; }
    s_work.state = !s_work.read_only && s_work.filesystem_total_bytes == CHIP_SIZE ?
        TIRTC_STORAGE_STATE_READY : TIRTC_STORAGE_STATE_READY_LIMITED;
    s_recovery.initialized = true;
    s_recovery.jedec_id = id;
    s_recovery.block_size = s_cfg.block_size;
    s_recovery.block_count = s_work.filesystem_total_bytes / s_cfg.block_size;
    s_recovery.read_only = s_work.read_only;
    s_recovery.legacy_geometry = s_legacy_geometry;
    s_recovery.ready_state = s_work.state;
    ret = 0; goto done;
error:
    s_write_allowed = false; s_work.filesystem_valid = false;
    s_work.state = TIRTC_STORAGE_STATE_ERROR;
done:
    s_work.last_error = s_io_error ? s_io_error : ret; publish();
    tirtc_storage_port_unlock(); return ret;
error_without_lock:
    s_work.state = TIRTC_STORAGE_STATE_ERROR; s_work.last_error = ret; publish(); return ret;
}
void tirtc_storage_poll(void)
{
    int ret;
    if (tirtc_storage_port_lock(0)) return;
    if (s_work.recovery_pending) {
        if ((uint32_t)(tirtc_storage_port_ms() - s_recovery.retry_started) >=
            s_recovery.retry_delay) recover_mount();
        tirtc_storage_port_unlock(); return;
    }
    if (!s_mounted || (s_work.state != TIRTC_STORAGE_STATE_READY &&
        s_work.state != TIRTC_STORAGE_STATE_READY_LIMITED)) {
        tirtc_storage_port_unlock(); return;
    }
    begin_operation(IO_BUDGET_MS);
    ret = usage();
    s_work.last_error = s_io_error ? s_io_error : ret;
    if (ret || s_io_error) runtime_fault(s_work.last_error);
    publish(); tirtc_storage_port_unlock();
}
static bool valid_path(const char *path)
{
    size_t n;
    if (!path || path[0] != '/') return false;
    n = strlen(path);
    return n > 1 && n <= 255U;
}
static int file_lock(void)
{
    int ret = tirtc_storage_port_lock(100);
    if (ret) return ret;
    if (!s_mounted || (!s_write_allowed && !s_work.read_only) || !s_work.filesystem_valid) {
        tirtc_storage_port_unlock(); return TIRTC_STORAGE_E_NOT_READY;
    }
    begin_operation(IO_BUDGET_MS); return 0;
}
static int finish_file(int result)
{
    s_work.last_error = s_io_error ? s_io_error : (result < 0 ? result : 0);
    /* Faulty media stops subsequent writes; logical NOENT/NOSPC need no unmount. */
    if (s_io_error || transient_read_error(result)) runtime_fault(s_work.last_error);
    if (s_io_error && result >= 0) result = s_io_error;
    publish(); tirtc_storage_port_unlock(); return result;
}
static int mode_flags(const char *mode)
{
    if (!mode) return -1;
    if (!strcmp(mode,"r") || !strcmp(mode,"rb")) return LFS_O_RDONLY;
    if (!strcmp(mode,"r+") || !strcmp(mode,"rb+") || !strcmp(mode,"r+b")) return LFS_O_RDWR;
    if (!strcmp(mode,"w") || !strcmp(mode,"wb")) return LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC;
    if (!strcmp(mode,"w+") || !strcmp(mode,"wb+") || !strcmp(mode,"w+b")) return LFS_O_RDWR | LFS_O_CREAT | LFS_O_TRUNC;
    if (!strcmp(mode,"a") || !strcmp(mode,"ab")) return LFS_O_WRONLY | LFS_O_CREAT | LFS_O_APPEND;
    if (!strcmp(mode,"a+") || !strcmp(mode,"ab+") || !strcmp(mode,"a+b")) return LFS_O_RDWR | LFS_O_CREAT | LFS_O_APPEND;
    return -1;
}
static int fd_slot(int fd)
{
    unsigned i;
    if (fd <= 0) return -1;
    for (i = 0; i < FILE_COUNT; ++i)
        if (s_files[i].open && s_files[i].token == fd) return (int)i;
    return -1;
}
int tirtc_storage_open(const char *path, const char *mode)
{
    unsigned i; int ret, flags = mode_flags(mode);
    if (!valid_path(path) || flags < 0) return TIRTC_STORAGE_E_ARGUMENT;
    ret = file_lock(); if (ret) return ret;
    if (s_work.read_only && flags != LFS_O_RDONLY) return finish_file(TIRTC_STORAGE_E_PROTECTED);
    /* Never wrap/reuse tokens: a stale token must not alias a recovered file. */
    if (s_descriptor_sequence >= (uint32_t)INT_MAX) return finish_file(TIRTC_STORAGE_E_BUSY);
    for (i=0; i<FILE_COUNT; ++i) if (!s_files[i].open) break;
    if (i == FILE_COUNT) return finish_file(TIRTC_STORAGE_E_BUSY);
    memset(&s_files[i], 0, sizeof(s_files[i]));
    s_files[i].cfg.buffer = s_files[i].cache;
    s_operation_mutating = flags != LFS_O_RDONLY;
    ret = lfs_file_opencfg(&s_fs, &s_files[i].file, path, flags, &s_files[i].cfg);
    if (!ret) {
        s_files[i].open = true; s_files[i].flags = flags;
        s_files[i].token = (int)++s_descriptor_sequence; ret = s_files[i].token;
    }
    return finish_file(ret);
}
int tirtc_storage_read(int fd, void *data, uint32_t size)
{
    int ret, slot;
    if ((!data && size) || size > 4096U) return TIRTC_STORAGE_E_ARGUMENT;
    ret = file_lock(); if (ret) return ret;
    slot = fd_slot(fd);
    if (slot < 0 || !(s_files[slot].flags & LFS_O_RDONLY)) return finish_file(LFS_ERR_BADF);
    /* Reading an update-mode file can flush its buffered writes internally. */
    s_operation_mutating = s_files[slot].flags != LFS_O_RDONLY;
    return finish_file((int)lfs_file_read(&s_fs,&s_files[slot].file,data,size));
}
int tirtc_storage_write(int fd, const void *data, uint32_t size)
{
    int ret, slot;
    if ((!data && size) || size > 4096U) return TIRTC_STORAGE_E_ARGUMENT;
    ret = file_lock(); if (ret) return ret;
    if (s_work.read_only) return finish_file(TIRTC_STORAGE_E_PROTECTED);
    slot = fd_slot(fd);
    if (slot < 0 || !(s_files[slot].flags & LFS_O_WRONLY)) return finish_file(LFS_ERR_BADF);
    s_operation_mutating = true;
    return finish_file((int)lfs_file_write(&s_fs,&s_files[slot].file,data,size));
}
int tirtc_storage_seek(int fd, int32_t offset, int whence)
{
    int slot, ret = file_lock(); if (ret) return ret;
    slot = fd_slot(fd);
    if (slot < 0) return finish_file(LFS_ERR_BADF);
    if (whence < 0 || whence > 2) return finish_file(TIRTC_STORAGE_E_ARGUMENT);
    s_operation_mutating = s_files[slot].flags != LFS_O_RDONLY;
    return finish_file((int)lfs_file_seek(&s_fs,&s_files[slot].file,offset,whence));
}
int tirtc_storage_sync(int fd)
{
    int slot, ret = file_lock(); if (ret) return ret;
    slot = fd_slot(fd);
    if (slot < 0) return finish_file(LFS_ERR_BADF);
    s_operation_mutating = s_files[slot].flags != LFS_O_RDONLY;
    return finish_file(lfs_file_sync(&s_fs,&s_files[slot].file));
}
int tirtc_storage_close(int fd)
{
    int slot, ret = tirtc_storage_port_lock(100); if (ret) return ret;
    slot = fd_slot(fd);
    if (slot < 0) { tirtc_storage_port_unlock(); return LFS_ERR_BADF; }
    if (!s_mounted || !s_work.filesystem_valid) {
        /* A defensive no-I/O close even if a future fault path left a slot. */
        s_files[slot].open = false;
        tirtc_storage_port_unlock(); return TIRTC_STORAGE_E_NOT_READY;
    }
    begin_operation(IO_BUDGET_MS);
    s_operation_mutating = s_files[slot].flags != LFS_O_RDONLY;
    ret = lfs_file_close(&s_fs,&s_files[slot].file); s_files[slot].open = false;
    return finish_file(ret);
}
int tirtc_storage_mkdir(const char *path)
{
    int ret; if (!valid_path(path)) return TIRTC_STORAGE_E_ARGUMENT;
    ret = file_lock(); if (ret) return ret;
    s_operation_mutating = true;
    return finish_file(s_work.read_only ? TIRTC_STORAGE_E_PROTECTED : lfs_mkdir(&s_fs,path));
}
int tirtc_storage_remove(const char *path)
{
    int ret; if (!valid_path(path)) return TIRTC_STORAGE_E_ARGUMENT;
    ret = file_lock(); if (ret) return ret;
    s_operation_mutating = true;
    return finish_file(s_work.read_only ? TIRTC_STORAGE_E_PROTECTED : lfs_remove(&s_fs,path));
}

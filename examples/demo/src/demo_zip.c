#include "zip.h"
#include "unzip.h"

#include "liot_log.h"
#include "liot_os.h"
#include "liot_fs_api.h"
#include "liot_nw.h"

#include <string.h>

#define TAG "ZIP_DEMO"

#define ZIP_FILE_PATH       "test_archive.zip"
#define SRC_FILE_NAME       "hello.txt"
#define EXTRACT_FILE_NAME   "hello_extracted.txt"
#define READ_BUF_SIZE       256

static const char test_data[] = "Hello from minizip on OpenCPU! This is a compression test.";

static void demo_create_zip(void)
{
    zipFile zf = zipOpen(ZIP_FILE_PATH, APPEND_STATUS_CREATE);
    if (zf == NULL) {
        liot_trace("[%s] zipOpen failed", TAG);
        return;
    }

    zip_fileinfo zi;
    memset(&zi, 0, sizeof(zi));

    int err = zipOpenNewFileInZip(zf, SRC_FILE_NAME, &zi,
                                  NULL, 0, NULL, 0, NULL,
                                  Z_DEFLATED, Z_DEFAULT_COMPRESSION);
    if (err != ZIP_OK) {
        liot_trace("[%s] zipOpenNewFileInZip failed: %d", TAG, err);
        zipClose(zf, NULL);
        return;
    }

    err = zipWriteInFileInZip(zf, test_data, (unsigned int)strlen(test_data));
    if (err != ZIP_OK) {
        liot_trace("[%s] zipWriteInFileInZip failed: %d", TAG, err);
    }

    zipCloseFileInZip(zf);
    zipClose(zf, NULL);
    liot_trace("[%s] zip created OK", TAG);
}

static void demo_extract_zip(void)
{
    unzFile uf = unzOpen(ZIP_FILE_PATH);
    if (uf == NULL) {
        liot_trace("[%s] unzOpen failed", TAG);
        return;
    }

    int err = unzGoToFirstFile(uf);
    if (err != UNZ_OK) {
        liot_trace("[%s] unzGoToFirstFile failed: %d", TAG, err);
        unzClose(uf);
        return;
    }

    unz_file_info file_info;
    char filename[128];
    err = unzGetCurrentFileInfo(uf, &file_info, filename, sizeof(filename),
                               NULL, 0, NULL, 0);
    if (err != UNZ_OK) {
        liot_trace("[%s] unzGetCurrentFileInfo failed: %d", TAG, err);
        unzClose(uf);
        return;
    }
    liot_trace("[%s] found file: %s, uncompressed size: %lu",
              TAG, filename, file_info.uncompressed_size);

    err = unzOpenCurrentFile(uf);
    if (err != UNZ_OK) {
        liot_trace("[%s] unzOpenCurrentFile failed: %d", TAG, err);
        unzClose(uf);
        return;
    }

    char buf[READ_BUF_SIZE];
    int bytes_read = unzReadCurrentFile(uf, buf, sizeof(buf) - 1);
    if (bytes_read > 0) {
        buf[bytes_read] = '\0';
        liot_trace("[%s] extracted content: %s", TAG, buf);
    } else {
        liot_trace("[%s] unzReadCurrentFile failed: %d", TAG, bytes_read);
    }

    LFILE fd = liot_fopen(EXTRACT_FILE_NAME, "w");
    if (fd >= 0) {
        liot_fwrite(buf, 1, bytes_read, fd);
        liot_fclose(fd);
        liot_trace("[%s] written to %s", TAG, EXTRACT_FILE_NAME);
    }

    unzCloseCurrentFile(uf);
    unzClose(uf);
    liot_trace("[%s] extract done", TAG);
}

static void demo_verify_extracted(void)
{
    LFILE fd = liot_fopen(EXTRACT_FILE_NAME, "r");
    if (fd < 0) {
        liot_trace("[%s] verify: open failed", TAG);
        return;
    }

    char buf[READ_BUF_SIZE];
    int len = liot_fread(buf, 1, sizeof(buf) - 1, fd);
    liot_fclose(fd);

    if (len > 0) {
        buf[len] = '\0';
        if (strcmp(buf, test_data) == 0) {
            liot_trace("[%s] PASS: data matches original", TAG);
        } else {
            liot_trace("[%s] FAIL: data mismatch", TAG);
        }
    } else {
        liot_trace("[%s] verify: read failed", TAG);
    }
}

static void demo_cleanup(void)
{
    liot_remove(ZIP_FILE_PATH);
    liot_remove(EXTRACT_FILE_NAME);
    liot_trace("[%s] cleanup done", TAG);
}

void liot_zip_demo_thread(void *argv)
{
    liot_rtos_task_sleep_s(10);

    liot_trace("[%s] === minizip demo start ===", TAG);

    demo_create_zip();
    demo_extract_zip();
    demo_verify_extracted();
    demo_cleanup();

    liot_trace("[%s] === minizip demo end ===", TAG);

    liot_rtos_task_delete(NULL);
}
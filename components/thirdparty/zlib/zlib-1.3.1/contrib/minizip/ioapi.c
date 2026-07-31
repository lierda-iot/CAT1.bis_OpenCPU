/* ioapi.h -- IO base function header for compress/uncompress .zip
   part of the MiniZip project - ( http://www.winimage.com/zLibDll/minizip.html )

         Copyright (C) 1998-2010 Gilles Vollant (minizip) ( http://www.winimage.com/zLibDll/minizip.html )

         Modifications for Zip64 support
         Copyright (C) 2009-2010 Mathias Svensson ( http://result42.com )

         For more info read MiniZip_info.txt

*/

#include "ioapi.h"
#include "liot_fs_api.h"

voidpf call_zopen64 (const zlib_filefunc64_32_def* pfilefunc, const void*filename, int mode) {
    if (pfilefunc->zfile_func64.zopen64_file != NULL)
        return (*(pfilefunc->zfile_func64.zopen64_file)) (pfilefunc->zfile_func64.opaque,filename,mode);
    else
    {
        return (*(pfilefunc->zopen32_file))(pfilefunc->zfile_func64.opaque,(const char*)filename,mode);
    }
}

long call_zseek64 (const zlib_filefunc64_32_def* pfilefunc,voidpf filestream, ZPOS64_T offset, int origin) {
    if (pfilefunc->zfile_func64.zseek64_file != NULL)
        return (*(pfilefunc->zfile_func64.zseek64_file)) (pfilefunc->zfile_func64.opaque,filestream,offset,origin);
    else
    {
        uLong offsetTruncated = (uLong)offset;
        if (offsetTruncated != offset)
            return -1;
        else
            return (*(pfilefunc->zseek32_file))(pfilefunc->zfile_func64.opaque,filestream,offsetTruncated,origin);
    }
}

ZPOS64_T call_ztell64 (const zlib_filefunc64_32_def* pfilefunc, voidpf filestream) {
    if (pfilefunc->zfile_func64.zseek64_file != NULL)
        return (*(pfilefunc->zfile_func64.ztell64_file)) (pfilefunc->zfile_func64.opaque,filestream);
    else
    {
        uLong tell_uLong = (uLong)(*(pfilefunc->ztell32_file))(pfilefunc->zfile_func64.opaque,filestream);
        if ((tell_uLong) == MAXU32)
            return (ZPOS64_T)-1;
        else
            return tell_uLong;
    }
}

void fill_zlib_filefunc64_32_def_from_filefunc32(zlib_filefunc64_32_def* p_filefunc64_32, const zlib_filefunc_def* p_filefunc32) {
    p_filefunc64_32->zfile_func64.zopen64_file = NULL;
    p_filefunc64_32->zopen32_file = p_filefunc32->zopen_file;
    p_filefunc64_32->zfile_func64.zread_file = p_filefunc32->zread_file;
    p_filefunc64_32->zfile_func64.zwrite_file = p_filefunc32->zwrite_file;
    p_filefunc64_32->zfile_func64.ztell64_file = NULL;
    p_filefunc64_32->zfile_func64.zseek64_file = NULL;
    p_filefunc64_32->zfile_func64.zclose_file = p_filefunc32->zclose_file;
    p_filefunc64_32->zfile_func64.zerror_file = p_filefunc32->zerror_file;
    p_filefunc64_32->zfile_func64.opaque = p_filefunc32->opaque;
    p_filefunc64_32->zseek32_file = p_filefunc32->zseek_file;
    p_filefunc64_32->ztell32_file = p_filefunc32->ztell_file;
}



static voidpf ZCALLBACK fopen_file_func(voidpf opaque, const char* filename, int mode) {
    LFILE fd;
    const char* mode_fopen = NULL;
    (void)opaque;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
        mode_fopen = "rb";
    else
    if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
        mode_fopen = "r+b";
    else
    if (mode & ZLIB_FILEFUNC_MODE_CREATE)
        mode_fopen = "wb";

    if ((filename!=NULL) && (mode_fopen != NULL))
    {
        fd = liot_fopen(filename, mode_fopen);
        if (fd > 0)
            return (voidpf)(unsigned long)fd;
    }
    return NULL;
}

static voidpf ZCALLBACK fopen64_file_func(voidpf opaque, const void* filename, int mode) {
    return fopen_file_func(opaque, (const char*)filename, mode);
}


static uLong ZCALLBACK fread_file_func(voidpf opaque, voidpf stream, void* buf, uLong size) {
    uLong ret;
    LFILE fd = (LFILE)(unsigned long)stream;
    (void)opaque;
    ret = (uLong)liot_fread(buf, 1, (size_t)size, fd);
    if ((int)ret < 0)
        ret = 0;
    return ret;
}

static uLong ZCALLBACK fwrite_file_func(voidpf opaque, voidpf stream, const void* buf, uLong size) {
    uLong ret;
    LFILE fd = (LFILE)(unsigned long)stream;
    (void)opaque;
    ret = (uLong)liot_fwrite((void*)buf, 1, (size_t)size, fd);
    if ((int)ret < 0)
        ret = 0;
    return ret;
}

static long ZCALLBACK ftell_file_func(voidpf opaque, voidpf stream) {
    long ret;
    LFILE fd = (LFILE)(unsigned long)stream;
    (void)opaque;
    ret = (long)liot_ftell(fd);
    return ret;
}


static ZPOS64_T ZCALLBACK ftell64_file_func(voidpf opaque, voidpf stream) {
    ZPOS64_T ret;
    LFILE fd = (LFILE)(unsigned long)stream;
    (void)opaque;
    ret = (ZPOS64_T)liot_ftell(fd);
    return ret;
}

static long ZCALLBACK fseek_file_func(voidpf opaque, voidpf stream, uLong offset, int origin) {
    int fseek_origin=0;
    long ret;
    LFILE fd = (LFILE)(unsigned long)stream;
    (void)opaque;
    switch (origin)
    {
    case ZLIB_FILEFUNC_SEEK_CUR :
        fseek_origin = LIOT_SEEK_CUR;
        break;
    case ZLIB_FILEFUNC_SEEK_END :
        fseek_origin = LIOT_SEEK_END;
        break;
    case ZLIB_FILEFUNC_SEEK_SET :
        fseek_origin = LIOT_SEEK_SET;
        break;
    default: return -1;
    }
    ret = 0;
    if (liot_fseek(fd, (long)offset, fseek_origin) < 0)
        ret = -1;
    return ret;
}

static long ZCALLBACK fseek64_file_func(voidpf opaque, voidpf stream, ZPOS64_T offset, int origin) {
    return fseek_file_func(opaque, stream, (uLong)offset, origin);
}


static int ZCALLBACK fclose_file_func(voidpf opaque, voidpf stream) {
    int ret;
    LFILE fd = (LFILE)(unsigned long)stream;
    (void)opaque;
    ret = liot_fclose(fd);
    return ret;
}

static int ZCALLBACK ferror_file_func(voidpf opaque, voidpf stream) {
    (void)opaque;
    (void)stream;
    return 0;
}

void fill_fopen_filefunc(zlib_filefunc_def* pzlib_filefunc_def) {
    pzlib_filefunc_def->zopen_file = fopen_file_func;
    pzlib_filefunc_def->zread_file = fread_file_func;
    pzlib_filefunc_def->zwrite_file = fwrite_file_func;
    pzlib_filefunc_def->ztell_file = ftell_file_func;
    pzlib_filefunc_def->zseek_file = fseek_file_func;
    pzlib_filefunc_def->zclose_file = fclose_file_func;
    pzlib_filefunc_def->zerror_file = ferror_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_fopen64_filefunc(zlib_filefunc64_def* pzlib_filefunc_def) {
    pzlib_filefunc_def->zopen64_file = fopen64_file_func;
    pzlib_filefunc_def->zread_file = fread_file_func;
    pzlib_filefunc_def->zwrite_file = fwrite_file_func;
    pzlib_filefunc_def->ztell64_file = ftell64_file_func;
    pzlib_filefunc_def->zseek64_file = fseek64_file_func;
    pzlib_filefunc_def->zclose_file = fclose_file_func;
    pzlib_filefunc_def->zerror_file = ferror_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

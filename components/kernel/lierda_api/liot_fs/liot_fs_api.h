/**
 * @file liot_fs_api.h
 * @brief Lierda File System API Interface
 * @details This header file provides the file system interface for Lierda IoT platforms,
 *          including file operations, directory management, and error handling.
 * @version 1.0.0
 * @date 2025-08-18
 */
#ifndef LIERDA_FS_API_H
#define LIERDA_FS_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_type.h"

/**
 * @brief Enable checksum verification for LittleFS
 * @note When enabled, file system will perform checksum validation on data blocks
 */
//#define  LIOT_LFS_CHECKSUM_ON   (1)

/**
 * @brief Maximum file name length including null terminator
 */
#define LIOT_FS_NAME_MAX 255

/**
 * @brief Seek from beginning of file
 */
#define LIOT_SEEK_SET 0

/**
 * @brief Seek from current position
 */
#define LIOT_SEEK_CUR 1

/**
 * @brief Seek from end of file
 */
#define LIOT_SEEK_END 2

/**
 * @brief File type identifier
 */
#define LIOT_FS_TYPE_FILE 0x001

/**
 * @brief Directory type identifier
 */
#define LIOT_FS_TYPE_DIR  0x002

/**
 * @brief File handler type
 * @details Opaque handle returned by liot_fopen and used for subsequent file operations
 */
typedef INT32 LFILE;

/**
 * @brief File system error codes
 * @details Comprehensive error codes for all file system operations
 */
typedef enum
{
    LIOT_FS_OK                      = 0,                /**< Operation successful */
    LIOT_FS_INVALID_INPUT_VALUE     = -1,               /**< Invalid input parameter */
    LIOT_FS_LARGER_THAN_FILESIZE    = -2,               /**< Requested offset exceeds file size */
    LIOT_FS_READ_ZERO               = -3,               /**< Read returned zero bytes */
    LIOT_FS_DRIVE_FULL              = -4,               /**< Storage drive is full */
    LIOT_FS_MOV_ERROR               = -5,               /**< Error moving file/directory */
    LIOT_FS_FILE_NOT_FOUND          = -6,               /**< File not found */
    LIOT_FS_INVALID_FILE_NAME       = -7,               /**< Invalid file name format */
    LIOT_FS_FILE_ALREADY_EXISTED    = -8,               /**< File already exists */
    LIOT_FS_FAILED_TO_CREATE_FILE   = -9,               /**< File creation failed */
    LIOT_FS_FAILED_TO_WRITE_FILE    = -10,              /**< File write operation failed */
    LIOT_FS_FAILED_TO_OPEN_FILE     = -11,              /**< File open operation failed */
    LIOT_FS_FAILED_TO_READ_FILE     = -12,              /**< File read operation failed */
    LIOT_FS_EXCEED_MAX_LENGTH       = -13,              /**< Exceeded maximum allowed length */
    LIOT_FS_REACH_MAX_OPENFILE_NUM  = -14,              /**< Reached maximum open file limit */
    LIOT_FS_IS_READONLY             = -15,              /**< File or directory is read-only */
    LIOT_FS_GET_SIZE_FAIL           = -16,              /**< Failed to retrieve file size */
    LIOT_FS_INVALID_FILE_DESCRIPTOR = -17,              /**< Invalid file descriptor */
    LIOT_FS_LIST_FILE_FAIL          = -18,              /**< Failed to list directory contents */
    LIOT_FS_DELETE_FILE_FAIL        = -19,              /**< File deletion failed */
    LIOT_FS_GET_DISK_INFO_FAIL      = -20,              /**< Failed to retrieve disk information */
    LIOT_FS_NO_SPACE                = -21,              /**< Insufficient disk space */
    LIOT_FS_TIME_OUT                = -22,              /**< Operation timed out */
    LIOT_FS_FILE_NOT_FOUND_2        = -23,              /**< Alternative file not found code */
    LIOT_FS_FILE_TOO_LARGE          = -24,              /**< File exceeds maximum allowed size */
    LIOT_FS_FILE_ALREADY_EXIST      = -25,              /**< Alternative file exists code */
    LIOT_FS_INVALID_PARAMETER       = -26,              /**< Alternative invalid parameter code */
    LIOT_FS_ALREADY_OPERATION       = -27,              /**< File already in use/opened */
    LIOT_FS_ERROR_GENERAL           = -28,              /**< General unspecified error */
    LIOT_FS_RENAME_ERROR            = -29,              /**< File rename operation failed */
    LIOT_FS_QUIT_DATE_MODE          = -30,              /**< Date mode exited unexpectedly */
    LIOT_FS_CLOSE_FAIL              = -31,              /**< File close operation failed */
    LIOT_FS_SEEK_FAIL               = -32,              /**< Seek operation failed */
    LIOT_FS_TELL_FAIL               = -33,              /**< Tell position operation failed */
    LIOT_FS_FAILED_TO_GET_STAT      = -34,              /**< Failed to get file status */
    LIOT_FS_NOT_EXIST               = -35,              /**< Resource does not exist */
    LIOT_FS_RENAME_FAIL             = -36,              /**< Alternative rename failed code */
    LIOT_FS_TRUNCATE_FAIL           = -37,              /**< File truncate operation failed */
    LIOT_FS_DIR_MAKE_FAIL           = -38,              /**< Directory creation failed */
    LIOT_FS_DIR_OPEN_FAIL           = -39,              /**< Directory open failed */
    LIOT_FS_DIR_CLOSE_FAIL          = -40,              /**< Directory close failed */
    LIOT_FS_DIR_READ_FAIL           = -41,              /**< Directory read failed */
    LIOT_FS_DIR_TELL_FAIL           = -42,              /**< Directory tell position failed */
    LIOT_FS_DIR_SEEK_FAIL           = -43,              /**< Directory seek failed */
    LIOT_FS_DIR_REMOVE_FAIL         = -44,              /**< Directory removal failed */
    LIOT_FS_DIR_DIR_ALREADY_EXIST   = -45,              /**< Directory already exists */
    LIOT_FS_ERROR_MAX,                                  /**< Maximum error code value */
} liot_file_errcode_e;

/**
 * @brief File status structure
 * @details Contains metadata information about a file or directory
 */
typedef struct
{
    UINT8 type;                                         /**< File type (LIOT_FS_TYPE_FILE or LIOT_FS_TYPE_DIR) */
    UINT32 size;                                         /**< File size in bytes */
    CHAR name[LIOT_FS_NAME_MAX + 1];                     /**< Null-terminated file name */
} liot_stat_s;

/**
 * @brief Directory handle structure
 * @details Opaque handle for directory operations
 */
typedef struct
{
    INT32 fs_index;  ///< internal fs index
    INT32 _reserved; ///< reserved
} LDIR;

/**
 * @brief Directory entry structure
 * @details Contains information about a directory entry returned by liot_readdir
 */
typedef struct
{
    UINT8 d_type;     ///< type of file (LIOT_FS_TYPE_FILE or LIOT_FS_TYPE_DIR)
    INT32 d_info;     ///< Implementation-specific file information
    CHAR d_name[256]; ///< Null-terminated file name
} ldirent;

/**
 * @brief Open a file
 * @param[in] file_name Path and name of the file to open
 * @param[in] mode File access mode string
 *                 - "r": Read mode (file must exist)
 *                 - "w": Write mode (create or truncate)
 *                 - "a": Append mode (create or append)
 *                 - "r+": Read/write mode (file must exist)
 *                 - "w+": Read/write mode (create or truncate)
 *                 - "a+": Read/append mode (create or append)
 * @return File handle (LFILE) on success, negative error code on failure
 * @retval LIOT_FS_FAILED_TO_OPEN_FILE if file cannot be opened
 * @retval LIOT_FS_INVALID_PARAMETER if invalid parameters are provided
 */
LFILE liot_fopen(const char *file_name, const char *mode);
/**
 * @brief Close an open file
 * @param[in] fd File handle to close
 * @return 0 on success, negative error code on failure
 * @retval LIOT_FS_CLOSE_FAIL if closing fails
 * @retval LIOT_FS_INVALID_FILE_DESCRIPTOR if invalid file handle
 */
int liot_fclose(LFILE fd);

/**
 * @brief Delete a file from the filesystem
 * @details Removes the specified file from the filesystem. Requires write permissions
 *          in the parent directory. Cannot delete directories or open files.
 * @param[in] file_name Full path of the file to be deleted
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @note Does not return error for non-existent files on some filesystem implementations
 */
int liot_remove(const char *file_name);

/**
 * @brief Read data from an open file stream
 * @details Reads up to @p num elements of @p size bytes from the file stream
 *          pointed to by @p fd into the buffer. Advances file position indicator.
 * @param[out] buffer Pointer to buffer where read data will be stored
 * @param[in] size Size in bytes of each element to be read
 * @param[in] num Maximum number of elements to read
 * @param[in] fd File descriptor returned by liot_fopen
 * @return Number of elements successfully read (may be less than @p num),
 *         0 if end-of-file is reached, or LIOT_FILE_ERR on error
 * @warning Buffer must be pre-allocated with at least (size * num) bytes
 */
int liot_fread(void *buffer, size_t size, size_t num, LFILE fd);

/**
 * @brief Write data to an open file stream
 * @details Writes up to @p num elements of @p size bytes from the buffer to
 *          the file stream pointed to by @p fd. Advances file position indicator.
 * @param[in] buffer Pointer to data to be written
 * @param[in] size Size in bytes of each element to be written
 * @param[in] num Number of elements to write
 * @param[in] fd File descriptor returned by liot_fopen (must be opened in write mode)
 * @return Number of elements successfully written, or LIOT_FILE_ERR on failure
 * @note Actual write may be delayed in buffered implementations - use liot_fflush() to ensure
 *       data is written to storage
 */
int liot_fwrite(void *buffer, size_t size, size_t num, LFILE fd);

/**
 * @brief Reposition file position indicator
 * @details Sets the file position indicator for the file stream pointed to by @p fd.
 *          The new position is determined by @p offset bytes from @p origin.
 * @param[in] fd File descriptor returned by liot_fopen
 * @param[in] offset Number of bytes to offset from origin
 * @param[in] origin Reference position for offset (SEEK_SET, SEEK_CUR, or SEEK_END)
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @warning Behavior is undefined for append-mode files ("a" or "a+")
 */
int liot_fseek(LFILE fd, long offset, int origin);

/**
 * @brief Set file position indicator to beginning of file
 * @details Repositions the file position indicator to the beginning of the file
 *          stream pointed to by @p fd, equivalent to liot_fseek(fd, 0, SEEK_SET)
 * @param[in] fd File descriptor returned by liot_fopen
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 */
int liot_frewind(LFILE fd);

/**
 * @brief Get current file position indicator
 * @details Returns the current value of the file position indicator for the
 *          file stream pointed to by @p fd, measured in bytes from beginning
 * @param[in] fd File descriptor returned by liot_fopen
 * @return Current file position on success, negative @ref liot_file_errcode_e on failure
 */
int liot_ftell(LFILE fd);

/**
 * @brief Get file status by file descriptor
 * @details Retrieves status information for the open file associated with @p fd
 *          and stores it in the @ref liot_stat_s structure pointed to by @p st
 * @param[in] fd File descriptor returned by liot_fopen
 * @param[out] st Pointer to liot_stat_s structure to fill with status information
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @see liot_stat() for path-based status retrieval
 */
int liot_fstat(LFILE fd, liot_stat_s *st);

/**
 * @brief Get file status by path
 * @details Retrieves status information for the file specified by @p file_name
 *          and stores it in the @ref liot_stat_s structure pointed to by @p st
 * @param[in] file_name Path to the file to get status for
 * @param[out] st Pointer to liot_stat_s structure to fill with status information
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @see liot_fstat() for descriptor-based status retrieval
 */
int liot_stat(const char *file_name, liot_stat_s *st);

/**
 * @brief Truncate file to specified length
 * @details Resizes the file associated with @p fd to exactly @p length bytes. If the file
 *          was larger than @p length, the extra data is discarded. If smaller, the file is
 *          extended with undefined content.
 * @param[in] fd File descriptor returned by liot_fopen (must be opened in write mode)
 * @param[in] length New size of the file in bytes
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @warning Not supported on all filesystem implementations
 */
int liot_ftruncate(LFILE fd, UINT32 length);

/**
 * @brief Get current file size
 * @details Returns the size of the file associated with @p fd in bytes by seeking to end
 *          and returning position. Preserves original file position.
 * @param[in] fd File descriptor returned by liot_fopen
 * @return File size in bytes on success, negative @ref liot_file_errcode_e on failure
 * @note Implemented as: long pos = liot_ftell(fd); liot_fseek(fd, 0, SEEK_END); long size = liot_ftell(fd); liot_fseek(fd, pos, SEEK_SET); return size;
 */
int liot_fsize(LFILE fd);

/**
 * @brief Check if a file exists
 * @details Determines if the specified file exists in the filesystem by attempting
 *          to retrieve its status information
 * @param[in] file_path Full path of the file to check
 * @return 1 if file exists, 0 if file does not exist, negative @ref liot_file_errcode_e on error
 * @note Returns 0 for broken symlinks on most implementations
 */
int liot_file_exist(const char *file_path);

/**
 * @brief Create a directory
 * @details Creates a new directory with the specified path and permissions mode
 * @param[in] path Full path of the directory to create
 * @param[in] mode Directory permissions (implementation-defined, typically 0755)
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @note Parent directories must exist - does not create recursive directories
 */
int liot_mkdir(const char *path, UINT32 mode);

/**
 * @brief Open a directory stream
 * @details Opens a directory stream for reading directory entries
 * @param[in] path Path of the directory to open
 * @return Pointer to LDIR directory stream on success, NULL on failure
 * @note Must be closed with liot_closedir() to avoid resource leaks
 */
LDIR *liot_opendir(const char *path);

/**
 * @brief Close a directory stream
 * @details Closes the directory stream pointed to by @p pdir and releases associated resources
 * @param[in] pdir Directory stream pointer returned by liot_opendir()
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @note Passing NULL is safe but returns LIOT_FILE_ERR
 */
int liot_closedir(LDIR *pdir);
/**
 * @brief Read a directory entry
 * @details Reads the next directory entry from the stream pointed to by @p pdir
 * @param[in] pdir Directory stream pointer returned by liot_opendir()
 * @return Pointer to ldirent structure on success, NULL if end of directory or error
 * @note Returns NULL for broken symlinks on most implementations
 */
ldirent *liot_readdir(LDIR *pdir);
/**
 * @brief Rename a file or directory
 * @details Renames the file or directory specified by @p oldpath to @p newpath
 * @param[in] oldpath Current path of the file or directory
 * @param[in] newpath New path to rename the file or directory to
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @note Overwrites @p newpath if it already exists
 */
int liot_rename(const char *oldpath, const char *newpath);

/**
 * @brief Sync file data to disk
 * @details Ensures that all data for the file associated with @p fd is written to disk
 * @param[in] fd File descriptor returned by liot_fopen (must be opened in write mode)
 * @return LIOT_FILE_SUCCESS on success, negative @ref liot_file_errcode_e on failure
 * @warning Not supported on all filesystem implementations
 */
int liot_fsync(LFILE fd);

/**
 * @brief Get free space in internal file system
 * @return Free space in bytes on success, negative error code on failure
 * @retval LIOT_FS_GET_DISK_INFO_FAIL if information cannot be retrieved
 */
int liot_internal_fs_free_size_get(void);

#ifdef __cplusplus
}
#endif

#endif // LIOT_FS_API_H
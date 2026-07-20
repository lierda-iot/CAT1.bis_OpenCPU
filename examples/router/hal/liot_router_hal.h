/**
 * @file liot_router_hal.h
 * @brief Web management service - platform-independent hardware/system HAL
 *
 * @details Business logic (core/ and api/) depends only on this interface and
 *          never calls chip/OS specific APIs directly. Porting to a new
 *          platform (another chip / Linux) only requires a new adapter that
 *          implements this interface; the business code stays untouched.
 *
 * Conventions:
 *  - Interfaces returning int: 0 means success, negative means failure.
 *  - String output interfaces: caller provides the buffer and size, the
 *    implementation guarantees a '\0' terminator.
 *  - IP/mask values are passed as host-order uint32_t; byte-order conversion
 *    is handled inside the adapter.
 */

#ifndef __WEB_HAL_H__
#define __WEB_HAL_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ───────────────────────── Storage (KV/blob) ───────────────────────── *
 * Persists Web configuration (account hash, salt, and future module config).
 * EC718 -> LittleFS file; Linux -> regular file.
 * key is a logical name, the adapter maps it to a concrete storage location. */

/**
 * @brief Read a blob of persisted data
 * @param key   Logical key name (e.g. "web_auth")
 * @param buf   Output buffer
 * @param len   Buffer size
 * @return >=0 number of bytes read; <0 on failure (not-found counts as failure)
 */
int Liot_WebHalKvRead(const char *key, void *buf, size_t len);

/**
 * @brief Write (overwrite) a blob of persisted data
 * @param key Logical key name
 * @param buf Data to write
 * @param len Data length
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalKvWrite(const char *key, const void *buf, size_t len);

/**
 * @brief Erase a blob of persisted data
 * @param key Logical key name
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalKvErase(const char *key);

/* ───────────────────────── Device information ───────────────────────── */

/** @brief Static device information */
typedef struct {
    char model[32];      /**< Model */
    char imei[16];       /**< IMEI */
    char sn[32];         /**< Serial number */
    char fwVer[48];      /**< Firmware version */
    char productId[32];  /**< Product ID */
} Liot_WebHalDevInfo_t;

/**
 * @brief Get static device information. The implementation fills each field
 *        best-effort and leaves unavailable ones as empty strings.
 * @param info [out] Device information output
 * @return 0 on success (even if some fields are empty); <0 on hard failure
 */
int Liot_WebHalDevGetInfo(Liot_WebHalDevInfo_t *info);

/* ───────────────────────── Network status ───────────────────────── */

/** @brief WAN side network status */
typedef struct {
    uint8_t  valid;      /**< 1=WAN is up */
    uint32_t ip;         /**< IP address (host order) */
    uint32_t mask;       /**< Subnet mask (host order) */
    uint32_t gw;         /**< Gateway (host order) */
    uint32_t dns1;       /**< Primary DNS (host order) */
    uint32_t dns2;       /**< Secondary DNS (host order) */
    int32_t  rssi;       /**< Signal strength (dBm), 0 if unavailable */
} Liot_WebHalWanStatus_t;

/** @brief LAN side network status */
typedef struct {
    uint32_t gatewayIp;  /**< LAN gateway = device's own LAN IP (host order) */
    uint32_t subnetMask; /**< Subnet mask (host order) */
    uint8_t  linkUp;     /**< LAN physical link state */
    uint8_t  clientCount;/**< Number of allocated DHCP clients */
} Liot_WebHalLanStatus_t;

/**
 * @brief Get WAN side network status
 * @param st [out] WAN status output
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalNetWanStatus(Liot_WebHalWanStatus_t *st);

/**
 * @brief Get LAN side network status
 * @param st [out] LAN status output
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalNetLanStatus(Liot_WebHalLanStatus_t *st);

/* ───────────────────────── System control ───────────────────────── */

/** @brief Reboot the device. Normally does not return. */
void Liot_WebHalReboot(void);

/** @brief Factory reset: clear Web config and business NV. The caller decides whether to reboot afterwards. */
int Liot_WebHalFactoryReset(void);

/** @brief Monotonic millisecond clock, used for session expiry timing (not wall clock). */
uint32_t Liot_WebHalUptimeMs(void);

/* ───────────────────────── Firmware upgrade (local upload) ───────────────────────── *
 * Typical flow: begin() -> write() chunks repeatedly -> finish() verifies and triggers upgrade.
 * EC718 -> store to LittleFS file + Liot_FotaAppUpgradeCheck;
 * Linux  -> store to file + replace/verify. */

/**
 * @brief Start a firmware upgrade session
 * @param totalSize Expected total size (for pre-check/progress), 0 if unknown
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalFwBegin(uint32_t totalSize);

/**
 * @brief Append a chunk of firmware data (in arrival order)
 * @param data Chunk data
 * @param len  Chunk length
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalFwWrite(const void *data, size_t len);

/**
 * @brief End the upload session and finalize the stored package
 * @param reboot Non-zero: immediately call Liot_WebHalFwApply() to verify+upgrade
 *               (normally does not return); 0: only store, the caller calls
 *               Liot_WebHalFwApply() later (lets it respond before upgrading)
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalFwFinish(uint8_t reboot);

/**
 * @brief Trigger the firmware upgrade: pass the stored local package to the
 *        low-level upgrade interface, which verifies then reboots to flash.
 *        On successful verification it normally does not return (device reboots).
 * @return <0 on verify/parameter failure (not upgraded); does not return on success
 */
int Liot_WebHalFwApply(void);

/** @brief Abort and clean up the current upgrade session (e.g. upload interrupted) */
void Liot_WebHalFwAbort(void);

/* ───────────────────────── Crypto / random ───────────────────────── */

/**
 * @brief Compute SHA-256
 * @param data  Input
 * @param len   Input length
 * @param out32 Output, fixed 32 bytes
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalSha256(const void *data, size_t len, uint8_t out32[32]);

/**
 * @brief Generate random bytes (for session token / salt). Should use a
 *        hardware entropy source when possible.
 * @param buf Output buffer
 * @param len Number of bytes to generate
 * @return 0 on success; <0 on failure
 */
int Liot_WebHalRandom(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* __WEB_HAL_H__ */

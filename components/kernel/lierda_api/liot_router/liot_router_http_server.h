/**
 * @file liot_router_http_server.h
 * @brief Web HTTP Server - public interface (httpd + router + session)
 *
 * @details Merged from the original httpd/router/session modules. Internal
 *          functions and structures stay static in the .c file; only the
 *          interfaces below are exported to API/business modules.
 */
#ifndef __LIOT_ROUTER_HTTP_SERVER_H__
#define __LIOT_ROUTER_HTTP_SERVER_H__
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum number of routes that can be registered */
#define HTTP_MAX_ROUTES 48
/** @brief Maximum length of a route path (including '\0') */
#define HTTP_PATH_MAX 64
/** @brief Session token length in hex chars (excluding '\0') */
#define HTTP_SESSION_TOKEN_LEN 32
/** @brief Maximum number of concurrent sessions */
#define HTTP_SESSION_MAX 4
/** @brief Session time-to-live in milliseconds (idle timeout) */
#define HTTP_SESSION_TTL_MS (10*60*1000)

/** @brief Supported HTTP request methods */
typedef enum {
    HTTP_METHOD_INVALID = 0,  /**< Unrecognized / unsupported method */
    HTTP_METHOD_GET,          /**< GET */
    HTTP_METHOD_POST          /**< POST */
} Liot_HttpMethod_e;

/** @brief Per-request context passed to a route handler */
typedef struct {
    int               sock;        /**< Client socket fd */
    Liot_HttpMethod_e method;      /**< Request method */
    const char       *path;        /**< Request path (no query string) */
    char             *query;       /**< Query string, NULL if none */
    char             *body;        /**< Request body, NULL if none */
    int               bodyLen;     /**< Bytes currently held in body */
    int               contentLen;  /**< Declared Content-Length */
    uint8_t           authed;      /**< 1=request carries a valid session */
} Liot_HttpCtx_t;

/**
 * @brief Route handler callback
 * @param ctx Request context (non-NULL)
 * @return 0 on success, negative on handler-level failure
 */
typedef int (*Liot_HttpHandler_t)(Liot_HttpCtx_t *ctx);

/**
 * @brief Start the HTTP server task
 * @param port Listen port, 0 defaults to 80
 * @return 0 on success
 */
int Liot_HttpdStart(uint16_t port);

/**
 * @brief Register a route
 * @param path    Route path, must be shorter than HTTP_PATH_MAX
 * @param method  Method the route matches
 * @param needAuth 1=requires a valid session, 0=public
 * @param handler Handler callback (non-NULL)
 * @return 0 on success, -1 on invalid args / table full / path too long
 */
int Liot_HttpRouteRegister(const char *path, Liot_HttpMethod_e method, uint8_t needAuth, Liot_HttpHandler_t handler);

/**
 * @brief Dispatch a request to its matching route
 * @param ctx Request context (non-NULL)
 * @return Handler return value on match; -1 if no route matched.
 *         Sends 401 itself when auth is required but missing.
 */
int Liot_HttpRouteDispatch(Liot_HttpCtx_t *ctx);

/**
 * @brief Send a 200 response with a JSON body
 * @param sock Client socket fd
 * @param json JSON string, NULL sends an empty body
 */
void Liot_HttpSendJson(int sock, const char *json);

/**
 * @brief Send a status response with a JSON error body
 * @param sock Client socket fd
 * @param code HTTP status code (400/401/404/500, others -> 200)
 * @param msg  Message text, NULL uses the default reason phrase
 */
void Liot_HttpSendStatus(int sock, int code, const char *msg);

/**
 * @brief Send a 200 response carrying a static asset
 * @param sock  Client socket fd
 * @param ctype Content-Type, NULL -> application/octet-stream
 * @param enc   Content-Encoding (e.g. "gzip"), NULL/empty to omit
 * @param data  Asset bytes
 * @param len   Asset length in bytes
 */
void Liot_HttpSendAsset(int sock, const char *ctype, const char *enc, const uint8_t *data, size_t len);

/**
 * @brief Extract one URL-encoded parameter value by key
 * @param data   Query or form data ("k1=v1&k2=v2")
 * @param key    Key to look up
 * @param out    [out] URL-decoded value buffer
 * @param outlen out buffer size
 * @return 0 if found (out is filled), -1 if not found / invalid args
 */
int Liot_HttpParamGet(const char *data, const char *key, char *out, size_t outlen);

/**
 * @brief Create a new session and return its token
 * @param tokenOut [out] Token buffer
 * @param outlen   tokenOut size, must be >= HTTP_SESSION_TOKEN_LEN + 1
 * @return 0 on success, -1 on invalid args / random source failure.
 *         Reuses the least-recently-active slot when the table is full.
 */
int Liot_HttpSessionCreate(char *tokenOut, size_t outlen);

/**
 * @brief Validate a session token and refresh its activity time
 * @param token Session token
 * @return 1 if valid (renewed), 0 if invalid or expired
 */
int Liot_HttpSessionValidate(const char *token);

/**
 * @brief Destroy the session matching the given token
 * @param token Session token, ignored if NULL or not found
 */
void Liot_HttpSessionDestroy(const char *token);

/** @brief Destroy all sessions (e.g. on password change) */
void Liot_HttpSessionDestroyAll(void);

/**
 * @brief Parse the session token out of a Cookie header line
 * @param headerLine Header line ("Cookie: SID=xxx; ...")
 * @param out        [out] Token buffer
 * @param outlen     out buffer size
 * @return 0 if a non-empty token was extracted, -1 otherwise
 */
int Liot_HttpSessionParseCookie(const char *headerLine, char *out, size_t outlen);

#ifdef __cplusplus
}
#endif
#endif

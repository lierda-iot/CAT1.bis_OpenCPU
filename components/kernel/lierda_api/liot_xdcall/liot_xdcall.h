#ifndef _LIOT_XDCALLH_
#define _LIOT_XDCALLH_

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    /** AI对话相关事件 */
    LIOT_XDCALL_EVENT_BOT_START_SPEAKING = 0, /**< 机器人开始说话 */
    LIOT_XDCALL_EVENT_BOT_STOP_SPEAKING  = 1, /**< 机器人停止说话 */
    LIOT_XDCALL_EVENT_BOT_TRANSCRIPTION  = 2, /**< 机器人字幕 */
    LIOT_XDCALL_EVENT_USR_TRANSCRIPTION  = 3, /**< 用户字幕 */

    /** 设备呼叫小程序事件 */
    LIOT_XDCALL_EVENT_RECV_USR_ANSWER = 10, /**< 设备呼叫小程序，小程序接听 */
    LIOT_XDCALL_EVENT_RECV_USR_REJECT = 11, /**< 设备呼叫小程序，小程序拒绝 */
    LIOT_XDCALL_EVENT_RECV_USR_HANGUP = 12, /**< 设备呼叫小程序，小程序主动挂断 */
    LIOT_XDCALL_EVENT_RECV_USR_ERROR  = 13, /**< 设备呼叫小程序，发生错误❌ */

    /** 微信通话共同事件 */
    LIOT_XDCALL_EVENT_RECV_START_CALL    = 20, /**< 服务端执行呼叫对端的动作 */
    LIOT_XDCALL_EVENT_RECV_ENTER_CALLING = 21, /**< 服务端指示当前通话已建立，进入通话中状态 */
    LIOT_XDCALL_EVENT_RECV_CALL_TIMEOUT  = 22, /**< 服务端呼叫超时 */
    LIOT_XDCALL_EVENT_RECV_CALL_BUSY     = 23, /**< 服务端发起呼叫，但是对端正忙 */

    /** 小程序呼叫设备事件 */
    LIOT_XDCALL_EVENT_RECV_USR_CALLING = 30, /**< 收到小程序呼叫 */
    LIOT_XDCALL_EVENT_RECV_USR_CANCEL  = 31, /**< 当设备没接听此时小程序取消呼叫则会收到此消息 */
    LIOT_XDCALL_EVENT_DEVICE_ANSWER    = 32, /**< 小程序呼叫设备，设备接听 */
    LIOT_XDCALL_EVENT_DEVICE_REJECT    = 33, /**< 小程序呼叫设备，设备拒绝 */
    LIOT_XDCALL_EVENT_DEVICE_HANGUP    = 34, /**< 小程序呼叫设备，设备主动挂断 */
    LIOT_XDCALL_EVENT_RECV_DISCONNECT  = 35, /**< 收到断开连接 */
    LIOT_XDCALL_EVENT_RECV_ERROR       = 36, /**< 接收错误 */
    LIOT_XDCALL_EVENT_RECV_CLOSE       = 37, /**< WebSocket连接关闭 */

    /** TRTC相关事件，ws不关心 */
    LIOT_XDCALL_EVENT_TRTC_REMOTE_USR_ENTER_ROOM = 50, /**< trtc远端用户进入房间 */
    LIOT_XDCALL_EVENT_TRTC_REMOTE_USR_EXIT_ROOM  = 51, /**< trtc远端用户退出房间 */

    /** 请求图片事件 */
    LIOT_XDCALL_EVENT_REQUEST_IMAGE = 60, /**< 请求图片 */

    /** 最大值 */
    LIOT_XDCALL_EVENT_MAX,
} LiotXDCallEventType;
/**
 * @brief MAX size of client ID.
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_CLIENT_ID (80)

/**
 * @brief MAX size of product ID.
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_PRODUCT_ID (10)

/**
 * @brief MAX size of product secret.
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_PRODUCT_SECRET (32)

/**
 * @brief MAX size of device name.
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_DEVICE_NAME (48)

/**
 * @brief MAX size of device secret.
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_DEVICE_SECRET (64)

/**
 * @brief MAX size of device cert file name.
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_DEVICE_CERT_FILE_NAME (128)

/**
 * @brief MAX size of device key file name.
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_DEVICE_SECRET_FILE_NAME (128)

/**
 * @brief Max size of base64 encoded PSK = 64, after decode: 64/4*3 = 48.
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_DECODE_PSK_LENGTH 48

/**
 * @brief Max size of device firmware version length
 *
 */
#define LIOT_XDCALL_MAX_SIZE_OF_DEVICE_VERSION_LENGTH 32

/**
 * @brief Json value
 *
 */
typedef struct {
    union {
        const char *value;
        char       *value_str;
    };
    int value_len;
} xdcallUtilsJsonValue;

/**
 * @brief 事件消息
 *
 */
typedef union {
    struct {
        xdcallUtilsJsonValue transcription;
    } BotTranscription;

    struct {
        xdcallUtilsJsonValue transcription;
    } UsrTranscription;

    struct {
        xdcallUtilsJsonValue room_id;
    } RecvCalling;

    struct {
        int code;
    } RecvError;

    struct {
        xdcallUtilsJsonValue called;
        xdcallUtilsJsonValue openid;
    } UserAnswer;

    struct {
        xdcallUtilsJsonValue stream;
        xdcallUtilsJsonValue called;
        xdcallUtilsJsonValue openid;
    } UserHangup;

    struct {
        xdcallUtilsJsonValue called;
        xdcallUtilsJsonValue openid;
    } UserReject;

    struct {
        xdcallUtilsJsonValue called;
        xdcallUtilsJsonValue openid;
        int            code;
    } UserError;

} LiotXDCallEventMsg;

/**
 * @brief 音频回调，不要阻塞
 *
 */
typedef int (*LiotXDCallRecvAudioCallback)(uint8_t *recv_data, int recv_len, void *context);

/**
 * @brief 事件回调，不要阻塞
 *
 */
typedef int (*LiotXDCallEventCallback)(LiotXDCallEventType type, LiotXDCallEventMsg *msg, void *context);

typedef enum {
    LIOT_XDCALL_AUDIO_TYPE_PCM,
    LIOT_XDCALL_AUDIO_TYPE_OPUS,
    LIOT_XDCALL_AUDIO_TYPE_MAX,
} LiotXDCallAudioType;

typedef enum {
    LIOT_XDCALL_LANGUAGE_TYPE_ZH,
    LIOT_XDCALL_LANGUAGE_TYPE_EN,
    LIOT_XDCALL_LANGUAGE_TYPE_MAX,
} LiotXDCallLanguageType;

typedef struct {
    char product_id[LIOT_XDCALL_MAX_SIZE_OF_PRODUCT_ID + 1];
    char device_name[LIOT_XDCALL_MAX_SIZE_OF_DEVICE_NAME + 1];
    char device_version[LIOT_XDCALL_MAX_SIZE_OF_DEVICE_VERSION_LENGTH + 1];
    char device_secret[LIOT_XDCALL_MAX_SIZE_OF_DEVICE_SECRET + 1];
    char product_secret[LIOT_XDCALL_MAX_SIZE_OF_PRODUCT_SECRET + 1];
} LiotXDCallDeviceInfo;

typedef struct {
    LiotXDCallAudioType      audio_type;               /**< 音频类型，支持Opus和PCM */
    LiotXDCallLanguageType   language_type;            /**< 语言类型，目前只支持中文和英文 */
    int                      frame_interval;           /**< 帧间隔，目前固定60ms */
    int                      push_recv_frame_interval; /**< 推送接收的音频数据间隔，目前固定60ms */
    LiotXDCallRecvAudioCallback recv_audio_cb;            /**< 接收音频回调，不要阻塞 */
    LiotXDCallEventCallback recv_event_cb;            /**< 接收事件回调，不要阻塞 */
    void*                 context;                  /**< 透传给recv_audio_cb和recv_event_cb的参数 */
    int         ringbuffer_size; /**< 接收环形缓冲区大小，单位字节，传0表示不需要缓冲，直接传递给recv_audio_cb */
    bool        auto_reconnect;  /**< 是否自动重连 TODO ：待实现 */
    bool        is_encrypt;      /**< 是否加密传输 TODO : 待实现 */
    const char* wxa_appid;       /**< 微信通话的微信小程序appid，NULL表示不使用微信通话 */
    const char* wxa_modelid;     /**< 微信通话的微信小程序modelid， NULL表示不使用微信通话 */
} LiotXDCallInitParams;

#define DEFAULT_LIOT_XDCALL_WS_INIT_PARAMS \
    {LIOT_XDCALL_AUDIO_TYPE_OPUS, LIOT_XDCALL_LANGUAGE_TYPE_ZH, 60, 50, NULL, NULL, NULL, 90 * 180, 1, 0, NULL, NULL}
/**
 * @brief 设备通讯录，当对话过程中说“给小明打电话”时，会从通讯录中查找对应的设备
 *        所以在通话☎️前需要更新次通讯录
 *
 */
typedef struct {
    char name[32];    /**< 用户昵称，如：妈妈、小明 */
    char open_id[64]; /**< 用户open_id，同一个用户在同一个小程序下的openid是唯一的 */
} LiotXDCallOpenids;

/**
● @brief 初始化小达Talk模块
● 建立与AI服务的连接并启动内部接收线程。
● 成功后，会通过回调触发 @ref LIOT_XDCALL_EVENT_CONNECTED 事件。
● @param params 初始化参数，包含URL、API Key及回调函数
● @return XDCallHandle* 成功返回句柄，失败返回NULL
*/
void *liot_xdcall_init(LiotXDCallDeviceInfo *device_info, LiotXDCallInitParams *params);

/**
● @brief 去初始化小达Talk模块
● @return 成功返回0，失败返回-1
*/
int liot_xdcall_deinit(void *handle);

/**
 * @brief 同步联系人信息（结构体数组版本）- 增强验证版
 * 
 * @param liot_xdcall_handle XD Talk句柄
 * @param contacts 联系人结构体数组指针
 * @param count 联系人数量
 * @return int 错误码，0表示成功
 */
int liot_xdcall_sync_contacts(void *liot_xdcall_handle, LiotXDCallOpenids *contacts, int count);

/**
● @brief 发送文本消息到AI服务
● 可用于命令、问答等纯文本交互。
● 发送后，AI的响应将通过回调 @ref LIOT_XDCALL_EVENT_TEXT_RECEIVED 返回。
● @param handle 小达Talk句柄
● @param msg 文本内容
● @return int 0表示成功，负数表示失败
*/
int liot_xdcall_send_msg(void *handle, char *msg, uint32_t len);
/**
● @brief 发送音频数据到AI服务
● 支持流式音频输入（例如PCM、Opus、MP3等格式）。
● 发送后，AI响应音频将通过 @ref LIOT_XDCALL_EVENT_AUDIO_RECEIVED 回调。
● @param handle 小达Talk句柄
● @param audio 音频数据缓冲区
● @param len 音频数据长度
● @return int 0表示成功，负数表示失败
*/
int liot_xdcall_send_audio(void *handle, uint8_t *msg, uint32_t len);

/**
 * @brief 设备对当前通话状态的回复，有如下几种情况：
 *  liot_xdcall_call_response(handle, LIOT_XDCALL_EVENT_DEVICE_ANSWER, roomid); // 小程序呼叫设备：设备应答
 *  liot_xdcall_call_response(handle, LIOT_XDCALL_EVENT_DEVICE_REJECT, roomid); // 小程序呼叫设备：设备拒绝
 *  liot_xdcall_call_response(handle, LIOT_XDCALL_EVENT_DEVICE_HANGUP, roomid); // 小程序呼叫设备：设备主动挂断
 *  liot_xdcall_call_response(handle, LIOT_XDCALL_EVENT_DEVICE_HANGUP, NULL);   // 设备呼叫小程序，设备主动挂断
 *
 * @param handle liot_xdcall init时返回的句柄
 * @param type  @LiotXDCallEventType 只支持 LIOT_XDCALL_EVENT_DEVICE_*
 * @param roomid 呼叫请求房间id，如果是设备主动挂断，则roomid为NULL
 * @return 0 for success, negative for error
 */
int liot_xdcall_call_response(void *handle, LiotXDCallEventType type, xdcallUtilsJsonValue *roomid);

#endif
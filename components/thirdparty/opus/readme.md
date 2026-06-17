# OPUS库的接口中文简介：

OPUS是一种高效率、可扩展性和低延迟的音频编解码器，同时提供了各种压缩模式（例如VoIP、音乐等）以适应不同的应用场景。
## 1. OPUS库更新

### `opus源码仓库: https://github.com/xiph/opus`

- 下载最新 release 包，如 `opus-1.4.tar.gz`
- 解压`opus-1.4.tar.gz`到opus目录下，`/opus/opus-1.4`
- 由于opus-1.4自带库中头文件`arch.h`和`API.h`重名需更改opus源文件中的包含路径,请自行搜索所有包含并更改。如:
  `opus-1.4/celt/arm/armcpu.c`文件中改 `#include "arch.h"`为`#include "../celt/arch.h"`；
  `opus-1.4/silk/dec_API.c`文件中改 `#include "API.h"`为`#include "../silk/API.h"`；

## 2. 编解码器初始化

### `OpusEncoder *opus_encoder_create(opus_int32 Fs, int channels, int application, int *error)`

创建一个OPUS编码器实例。

- `Fs`：音频采样率，有效值为8000~48000Hz之间。
- `channels`：音频通道数，可设为1（单声道）或2（立体声）。
- `application`：编码模式，包括4种预定义模式。OPUS_APPLICATION_VOIP 用于语音通话，平均比特率约为12kbps；OPUS_APPLICATION_AUDIO 用于音频流媒体，平均比特率约为64kbps；OPUS_APPLICATION_RESTRICTED_LOWDELAY 用于低延迟通信，平均比特率约为16kbps；OPUS_APPLICATION_RESTRICTED_LOWDELAY_MD 用于低延迟通信，提供更高的音频品质，平均比特率约为24kbps。
- `error`：返回错误码，0表示成功，其他值表示失败。

### `OpusDecoder *opus_decoder_create(opus_int32 Fs, int channels, int *error)`

创建一个OPUS解码器实例。

- `Fs`：音频采样率，有效值为8000~48000Hz之间。
- `channels`：音频通道数，可设为1（单声道）或2（立体声）。
- `error`：返回错误码，0表示成功，其他值表示失败。

### `void opus_encoder_destroy(OpusEncoder *st)`

销毁一个OPUS编码器实例。

- `st`：OPUS编码器实例。

### `void opus_decoder_destroy(OpusDecoder *st)`

销毁一个OPUS解码器实例。

- `st`：OPUS解码器实例。


## 3. 编码器参数设置

### `int opus_encoder_ctl(OpusEncoder *st, int request, ...)`

设置或查询OPUS编码器的参数。

- `st`：OPUS编码器实例。
- `request`：参数类型，支持的类型有很多，其中比较常用的有以下几种：
  - `OPUS_SET_BITRATE`：设置编码器的比特率。
  - `OPUS_SET_COMPLEXITY`：设置编码器的复杂度等级。
  - `OPUS_SET_SIGNAL`：指定输入信号类型。
  - `OPUS_SET_PACKET_LOSS_PERC`：设置数据包丢失率百分比。
- `...`：具体参数值。

### `int opus_encoder_get_ctl(OpusEncoder *st, int request, ...)`

查询OPUS编码器的参数。

- `st`：OPUS编码器实例。
- `request`：参数类型。
- `...`：获取的参数值。

## 4. 音频压缩

### `int opus_encode(OpusEncoder *st, const opus_int16 *pcm, int frame_size, unsigned char *data, int max_data_bytes)`

将PCM音频数据压缩为OPUS格式。

- `st`：OPUS编码器实例。
- `pcm`：指向PCM输入数据的指针。
- `frame_size`：每个OPUS帧包含的采样数。
- `data`：存放压缩数据的缓冲区。
- `max_data_bytes`：压缩数据缓冲区的最大长度。

返回实际压缩后的数据长度。

## 5. 音频解压缩

### `int opus_decode(OpusDecoder *st, const unsigned char *data, int len, opus_int16 *pcm, int frame_size, int decode_fec)`

将OPUS音频数据解压缩为PCM格式。

- `st`：OPUS解码器实例。
- `data`：存放OPUS压缩数据的缓冲区
- `len`：压缩数据的长度。
- `pcm`：存放解压缩数据的PCM缓冲区。
- `frame_size`：每个OPUS帧包含的采样数。
- `decode_fec`：是否解码前向纠错信息，0表示不解码，1表示解码。

返回实际解压缩后的数据长度。

### `int opus_decode_float(OpusDecoder *st, const unsigned char *data, int len, float *pcm, int frame_size, int decode_fec)`

类似`opus_decode()`，但输出数据类型为浮点数而非整数。

- `st`：OPUS解码器实例。
- `data`：存放OPUS压缩数据的缓冲区。
- `len`：压缩数据的长度。
- `pcm`：存放解压缩数据的浮点数缓冲区。
- `frame_size`：每个OPUS帧包含的采样数。
- `decode_fec`：是否解码前向纠错信息，0表示不解码，1表示解码。

返回实际解压缩后的数据长度。

## 6. 其他接口

### `const char *opus_strerror(int error)`

将OPUS错误码转换为字符串描述。

- `error`：OPUS库函数的返回错误码。

返回字符串描述。
### `int opus_encoder_get_max_data_size(int frame_size)`

根据指定帧大小计算最大压缩数据长度。此函数用于分配存储压缩后数据的缓冲区。

- `frame_size`：每个OPUS帧包含的采样数。

返回一个能够存储最大压缩数据量的整数。

### `int opus_packet_get_samples_per_frame(const unsigned char *data, opus_int32 Fs)`

获取OPUS数据包中每个帧的样本数。

- `data`：存放OPUS数据包的缓冲区。
- `Fs`：采样率。

返回每个帧的样本数，如果 `data` 不是有效的OPUS数据包，则返回一个负数。

### `int opus_packet_get_nb_frames(const unsigned char *data, int len)`

获取OPUS数据包中的帧数。

- `data`：存放OPUS数据包的缓冲区。
- `len`：数据包长度。

返回OPUS数据包中的帧数。

### `int opus_packet_get_nb_samples(const unsigned char *data, int len, opus_int32 Fs)`

获取OPUS数据包中的样本数。

- `data`：存放OPUS数据包的缓冲区。
- `len`：数据包长度。
- `Fs`：采样率。

返回OPUS数据包中的样本数。
/**
 * @file demo_sfdt_slave.c
 * @brief Liot_SfdtSlave 可配置 API 使用示例。
 *
 * 本示例的目的不是重新实现 SFDT，而是展示应用层如何准备
 * Liot_SfdtSlaveConfig_t 并启动 SFDT Slave。配置结构故意完整保留，
 * 方便联调阶段逐项修改、打印和比较。
 *
 * 重要说明：
 * 1. 本文件没有调用 SFDT_slaveInit()/SFDT_slaveTransferTaskEntry()，
 *    也没有直接调用 Driver_SPI 或 liot_spi_write/read/write_read()。
 *    SPI、SRDY/MRDY、SFDT 帧和 CCIO 任务均由 Liot_SfdtSlaveInit() 管理。
 * 2. 本文件通过 EXDEMO_SFDT_SLAVE_EN 作为可选 Demo 接入，并由
 *    examples/demo/src/0demo_main.c 创建本线程。底包接入
 *    liot_sfdt_slave API 及其 SFDT/CCIO 依赖后，将该开关设为 y 即可编译运行。
 * 3. CAT1/CCIO 上层只需要把数据放入对应的 spi device TX 队列；SFDT API
 *    内部任务会调用 spiDevPickTxBuf() 取数据并通过 SPI 发往 WiFi 芯片。
 *
 * 参数优化时可按下面的边界考虑：
 * - 建议保留为外部配置：SPI port/CS、SPI 时序、SRDY/MRDY GPIO、device_index
 *   以及 Liot_SfdtSpiDevConfig_t 的通道选择字段；这些参数随硬件或产品形态变化。
 * - 通常可以收敛为 API 内部固定值：Slave、8 bit、DMA_IRQ、MSB、DI_1、
 *   CS active-low、SPI 完成回调（必须由 API 接管）。
 * - 运行时参数（MTU、任务栈、握手超时、轮询周期）可保留，但应设置合理
 *   的上下限，避免应用传入值破坏 SFDT 帧或实时性。
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lierda_app_main.h"
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_sleep.h"
#include "liot_datacall.h"
#include "liot_sfdt_slave.h"

/*
 * =============== 联调开关 ===============
 *
 * 这些宏只影响示例，不属于 Liot_SfdtSlave API 的一部分。实际产品可以
 * 将它们改为工程配置项，或者直接删除条件编译并固定产品参数。
 */

/* 1：打印完整配置；0：只打印初始化结果。 */
#ifndef LIOT_SFDT_DEMO_DUMP_CONFIG
#define LIOT_SFDT_DEMO_DUMP_CONFIG             (1)
#endif

/*
 * 1：显式覆盖 SPI 和握手引脚；0：使用
 * Liot_SfdtSlaveGetDefaultConfig() 提供的板级默认值。
 *
 * 第一次移植到新硬件时建议先保持为 0，确认默认宏是否正确；若硬件
 * 走线不同，再打开此开关并填写下面的值。
 */
#ifndef LIOT_SFDT_DEMO_OVERRIDE_BOARD_CONFIG
#define LIOT_SFDT_DEMO_OVERRIDE_BOARD_CONFIG   (1)
#endif

/*
 * 1：显式覆盖 Liot_SfdtSpiDevConfig_t；0：使用 API 根据 IPOS 编译选项生成的默认值。
 *
 * Liot_SfdtSpiDevConfig_t 决定 CCIO 设备如何创建、使用哪个接收缓存以及数据属于
 * AT/IPOS/ETH 哪一类。它不是 SPI 控制器寄存器配置，不能和 cfg.spi 混淆。
 */
#ifndef LIOT_SFDT_DEMO_OVERRIDE_CCIO_CONFIG
#define LIOT_SFDT_DEMO_OVERRIDE_CCIO_CONFIG    (1)
#endif

/*
 * 仅在 LIOT_SFDT_DEMO_OVERRIDE_CCIO_CONFIG=1 时生效：
 *   0：IPOS/裸 IP 配置（默认）；1：ETH/完整 Ethernet 帧配置。
 *
 * ETH 模式要求对端和上层都按 Ethernet 帧处理数据；切换该宏前应确认
 * spi_device 的 custEthType、网络管理模块以及 WiFi 固件协议一致。
 */
#ifndef LIOT_SFDT_DEMO_CCIO_ETH_MODE
#define LIOT_SFDT_DEMO_CCIO_ETH_MODE            (0)
#endif

/*
 * 1：每隔一段时间调用 NotifyTxReady()，用于观察唤醒发送流程；
 * 0：不主动通知，完全依赖 API 内部轮询和握手事件。
 *
 * 正常产品最好在“上层确认有待发送数据”时调用 NotifyTxReady()，而不是
 * 无条件周期调用。这里的周期调用只是为了让 demo 单独运行时容易观察。
 */
#ifndef LIOT_SFDT_DEMO_PERIODIC_NOTIFY
#define LIOT_SFDT_DEMO_PERIODIC_NOTIFY          (0)
#endif

#ifndef LIOT_SFDT_DEMO_NOTIFY_PERIOD_MS
#define LIOT_SFDT_DEMO_NOTIFY_PERIOD_MS        (1000U)
#endif

/* The IPOS device and the CAT1 data call must use the same CID. */
#ifndef LIOT_SFDT_DEMO_DATA_CID
#define LIOT_SFDT_DEMO_DATA_CID                (1)
#endif

/* UART1 is connected to LN882H AT (change this for the target schematic). */
#ifndef LIOT_SFDT_DEMO_AT_UART_NUM
#define LIOT_SFDT_DEMO_AT_UART_NUM             (1U)
#endif

/*
 * 0: keep the normal CCIO/IPOS demo; 1: bypass CCIO and deliver SFDT
 * payloads to the callback below.  This is a runtime API mode, not a
 * compile-time mode inside liot_sfdt_slave.c.
 */
#ifndef LIOT_SFDT_DEMO_PASSTHROUGH
#define LIOT_SFDT_DEMO_PASSTHROUGH             (1)
#endif

/*
 * 下面是“高级配置”示例。默认关闭，避免 demo 启动后强行改变流控状态。
 * 打开后会交替发送 XOFF/XON，便于验证对端是否遵守本端流控字段。
 */
#ifndef LIOT_SFDT_DEMO_TEST_FLOW_CONTROL
#define LIOT_SFDT_DEMO_TEST_FLOW_CONTROL       (0)
#endif

#ifndef LIOT_SFDT_DEMO_FLOW_PERIOD_MS
#define LIOT_SFDT_DEMO_FLOW_PERIOD_MS          (5000U)
#endif

/* -------------------- 可选的板级覆盖值 -------------------- */
#if LIOT_SFDT_DEMO_OVERRIDE_BOARD_CONFIG
/*
 * 这些值必须按照实际原理图和 RTE_Device.h 修改，以下为当前底包分离
 * 调试硬件示例。SPI 端口、CS 和 SRDY/MRDY GPIO 必须与 WiFi 芯片端一一对应。
 */
#define LIOT_SFDT_DEMO_SPI_PORT                LIOT_SPI_PORT1
#define LIOT_SFDT_DEMO_SPI_CS                  LIOT_SPI_CS0
#define LIOT_SFDT_DEMO_SPI_SPEED_HZ            (20000000U)

/* SRDY 只配置 GPIO2 的逻辑 GPIO 编号；Pad 映射由 liot_gpio2 负责。 */
#define LIOT_SFDT_DEMO_SRDY_GPIO               L_GPIO_16

#define LIOT_SFDT_DEMO_MRDY_SOURCE            LIOT_SFDT_MRDY_SOURCE_GPIO
/* 新硬件 MRDY 接在 EC718 GPIO17，不是 wakeup pad。 */
#define LIOT_SFDT_DEMO_MRDY_GPIO              L_GPIO_17
#define LIOT_SFDT_DEMO_MRDY_EDGE              L_INT_EDGE_RISE
#define LIOT_SFDT_DEMO_MRDY_PULL              LIOT_FORCE_PULL_UP
#endif

#if LIOT_SFDT_DEMO_PASSTHROUGH
/**
 * @brief Passthrough mode RX callback - demonstrates bidirectional data flow.
 *
 * This callback receives inbound SFDT payloads and sends a reply to verify
 * TX capability. In production, the application would parse the payload and
 * respond according to its protocol; here we simply echo the first few bytes
 * and append a counter.
 *
 * @param[in] data  Payload pointer, valid only during callback execution
 * @param[in] len   Payload length in bytes
 * @param[in] arg   Application context (unused in this demo)
 * @return LIOT_SFDT_SLAVE_SUCCESS on success
 */
static int32_t _liot_sfdt_slave_demo_rx_callback(const uint8_t *data,
                                                  uint16_t len,
                                                  void *arg)
{
    static uint32_t s_rx_count = 0U;
    int32_t ret;
    uint8_t reply[64];
    uint16_t reply_len;

    (void)arg;
    s_rx_count++;

    liot_trace("[sfdt-demo] passthrough RX #%lu, len=%u\n",
               (unsigned long)s_rx_count, (unsigned int)len);

    /*
     * Echo test: send back a reply containing part of the received data plus
     * a counter. Liot_SfdtSlaveSend() copies the payload before returning,
     * so we can use a stack buffer here.
     */
    if (len > 0U && len <= 32U)
    {
        /* Small payload: echo it back with prefix. */
        reply_len = (uint16_t)snprintf((char *)reply, sizeof(reply),
                                       "ECHO#%lu:", (unsigned long)s_rx_count);
        if (reply_len < sizeof(reply) && (reply_len + len) < sizeof(reply))
        {
            memcpy(reply + reply_len, data, len);
            reply_len = (uint16_t)(reply_len + len);
        }
    }
    else
    {
        /* Large or zero-length payload: send a simple ACK. */
        reply_len = (uint16_t)snprintf((char *)reply, sizeof(reply),
                                       "ACK#%lu:len=%u",
                                       (unsigned long)s_rx_count,
                                       (unsigned int)len);
    }

    ret = Liot_SfdtSlaveSend(reply, reply_len);
    if (ret != LIOT_SFDT_SLAVE_SUCCESS)
    {
        liot_trace("[sfdt-demo] Liot_SfdtSlaveSend failed: %ld (queue may be full)\n",
                   (long)ret);
        /* TX queue full is recoverable; the application can retry later or
         * drop the reply. Here we just log it and continue. */
    }
    else
    {
        liot_trace("[sfdt-demo] passthrough TX reply len=%u queued\n",
                   (unsigned int)reply_len);
    }

    return LIOT_SFDT_SLAVE_SUCCESS;
}
#endif

/**
 * @brief 打印 cfg.spi 的每一个字段。
 *
 * 字段分类：
 * - port/cs：通常必须按板级连接修改；
 * - spiclk/bus_speed_hz：传输速率，二者关系见下方注释；
 * - mode/input/transmode 等：当前 SFDT Slave 实现有固定要求，通常不要改；
 * - irq_callback：必须保持 NULL，API 内部需要接管 SPI 完成回调。
 */
static void _liot_sfdt_slave_demo_dump_spi_config(const liot_spi_config_s *spi,
                                                  uint32_t busSpeedHz)
{
    if (!spi)
        return;

    liot_trace("[sfdt-demo] cfg.spi.port=%d (SPI port)\n", (int)spi->port);
    liot_trace("[sfdt-demo] cfg.spi.cs=%d (CS select)\n", (int)spi->cs);
    /* API 当前优先使用 bus_speed_hz；为 0 时才回退到 spi.spiclk。 */
    liot_trace("[sfdt-demo] cfg.spi.spiclk=%d, bus_speed_hz=%lu (effective=%s)\n",
               (int)spi->spiclk, (unsigned long)busSpeedHz,
               busSpeedHz ? "bus_speed_hz" : "spiclk");
    liot_trace("[sfdt-demo] cfg.spi.framesize=%lu, input_mode=%d\n",
               (unsigned long)spi->framesize, (int)spi->input_mode);
    liot_trace("[sfdt-demo] cfg.spi.cpol=%d, cpha=%d, clk_delay=%d\n",
               (int)spi->cpol, (int)spi->cpha, (int)spi->clk_delay);
    liot_trace("[sfdt-demo] cfg.spi.input_sel=%d, transmode=%d\n",
               (int)spi->input_sel, (int)spi->transmode);
    liot_trace("[sfdt-demo] cfg.spi.device_mode=%d, data_msb_lsb=%d\n",
               (int)spi->device_mode, (int)spi->data_msb_lsb);
    liot_trace("[sfdt-demo] cfg.spi.cs_polarity0=%d, cs_polarity1=%d\n",
               (int)spi->cs_polarity0, (int)spi->cs_polarity1);
    liot_trace("[sfdt-demo] cfg.spi.irq_callback=%s (must be NULL)\n",
               spi->irq_callback ? "SET" : "NULL");
}

/**
 * @brief 打印握手、任务和 CCIO 配置。
 *
 * 其中 spi_dev 的每个成员都打印出来，便于判断“应用真正传给
 * spiDevCreate() 的是什么”。bit-field 成员使用无符号整数打印。
 */
 
static void _liot_sfdt_slave_demo_dump_config(const Liot_SfdtSlaveConfig_t *cfg)
{
    if (!cfg)
        return;

    liot_trace("[sfdt-demo] -------- SFDT configuration --------\n");
    _liot_sfdt_slave_demo_dump_spi_config(&cfg->spi, cfg->bus_speed_hz);
    liot_trace("[sfdt-demo] default_tx_value=0x%08lx\n",
               (unsigned long)cfg->default_tx_value);
    liot_trace("[sfdt-demo] at_uart_num=%u\n",
               (unsigned int)cfg->at_uart_num);

    liot_trace("[sfdt-demo] SRDY: gpio=%d (liot_gpio2 mapping)\n",
               (int)cfg->srdy_gpio);
    liot_trace("[sfdt-demo] MRDY: source=%d edge=%d pull=%d\n",
               (int)cfg->mrdy_source, (int)cfg->mrdy_edge,
               (int)cfg->mrdy_pull);
    if (cfg->mrdy_source == LIOT_SFDT_MRDY_SOURCE_GPIO)
        liot_trace("[sfdt-demo] MRDY GPIO: gpio=%d\n", (int)cfg->mrdy_gpio);
    else
        liot_trace("[sfdt-demo] MRDY wakeup: pad=%d irq=%ld\n",
                   (int)cfg->mrdy_wakeup_pad, (long)cfg->mrdy_irq);

    liot_trace("[sfdt-demo] task: priority=%u stack=%u timeout=%lu poll=%u mtu=%u\n",
               (unsigned int)cfg->task_priority,
               (unsigned int)cfg->task_stack_size,
               (unsigned long)cfg->handshake_timeout_ms,
               (unsigned int)cfg->tx_poll_interval_ms,
               (unsigned int)cfg->frame_mtu);
    liot_trace("[sfdt-demo] device_index=%u\n", (unsigned int)cfg->device_index);
    liot_trace("[sfdt-demo] data_mode=%u tx_queue_depth=%u rx_callback=%s\n",
               (unsigned int)cfg->data_mode,
               (unsigned int)cfg->tx_queue_depth,
               cfg->rx_callback ? "SET" : "NULL");

    liot_trace("[sfdt-demo] spi_dev.mainUsage=%u\n",
               (unsigned int)cfg->spi_dev.mainUsage);
    liot_trace("[sfdt-demo] spi_dev.bmCreateFlag=0x%02x\n",
               (unsigned int)cfg->spi_dev.bmCreateFlag);
    liot_trace("[sfdt-demo] spi_dev.rbufFlags=0x%02x\n",
               (unsigned int)cfg->spi_dev.rbufFlags);
    liot_trace("[sfdt-demo] spi_dev.custFlags=0x%lx\n",
               (unsigned long)cfg->spi_dev.custFlags);
    liot_trace("[sfdt-demo] spi_dev.custExtras=0x%lx\n",
               (unsigned long)cfg->spi_dev.custExtras);
    liot_trace("[sfdt-demo] spi_dev.custEthType=%lu\n",
               (unsigned long)cfg->spi_dev.custEthType);
    liot_trace("[sfdt-demo] --------------------------------------\n");
}

/**
 * @brief 覆盖板级 SPI/握手参数。
 *
 * 这里只改“硬件相关”字段。协议固定项（8 bit、Slave、DMA IRQ、MSB、
 * DI_1、CS active-low 等）仍由 GetDefaultConfig() 提供，不在这里重复赋值。
 */
static void _liot_sfdt_slave_demo_apply_board_config(Liot_SfdtSlaveConfig_t *cfg)
{
#if LIOT_SFDT_DEMO_OVERRIDE_BOARD_CONFIG
    cfg->spi.port = LIOT_SFDT_DEMO_SPI_PORT;
    cfg->spi.cs = LIOT_SFDT_DEMO_SPI_CS;
    cfg->bus_speed_hz = LIOT_SFDT_DEMO_SPI_SPEED_HZ;

    cfg->srdy_gpio = LIOT_SFDT_DEMO_SRDY_GPIO;
    cfg->mrdy_source = LIOT_SFDT_DEMO_MRDY_SOURCE;
    cfg->mrdy_gpio = LIOT_SFDT_DEMO_MRDY_GPIO;
    cfg->mrdy_edge = LIOT_SFDT_DEMO_MRDY_EDGE;
    cfg->mrdy_pull = LIOT_SFDT_DEMO_MRDY_PULL;
#else
    (void)cfg;
#endif
}

/**
 * @brief 覆盖 CCIO/Liot_SfdtSpiDevConfig_t 参数。
 *
 * 这是“底包分离”时最可能需要按产品调整的区域。必须保证这些值和
 * CAT1/CCIO 上层的使用方式一致：
 * - bmCreateFlag 至少要创建 RX 和一个发送任务（当前 API 校验 RX|TX2）；
 * - rbufFlags 要选择 SPI 专用接收缓存；
 * - mainUsage/custFlags/custExtras 决定 AT、IPOS 或其他 CCIO 通道语义；
 * - custEthType 是 ETH 场景下的设备子类型扩展。
 *
 * 示例默认只保留“如何配置”的代码，实际值请按产品协议确认后再打开
 * LIOT_SFDT_DEMO_OVERRIDE_CCIO_CONFIG。
 */
static void _liot_sfdt_slave_demo_apply_ccio_config(Liot_SfdtSlaveConfig_t *cfg)
{
#if LIOT_SFDT_DEMO_OVERRIDE_CCIO_CONFIG
#if LIOT_SFDT_DEMO_CCIO_ETH_MODE
    /* ETH：spi_device 按完整 Ethernet 帧组织数据。 */
    cfg->spi_dev.mainUsage = LIOT_SFDT_CCIO_USAGE_ETH;
    cfg->spi_dev.custFlags = LIOT_SFDT_CCIO_FLAG_SPITEST;
    cfg->spi_dev.custExtras = 0U;
#else
    /* IPOS：spi_device 只传输裸 IP 包，custExtras 选择 PDP/CID 位图。 */
    cfg->spi_dev.mainUsage = LIOT_SFDT_CCIO_USAGE_IPOS;
    cfg->spi_dev.custFlags = LIOT_SFDT_CCIO_FLAG_IPOS;
    /* Must match liot_start_data_call() below. */
    cfg->spi_dev.custExtras = (1U << LIOT_SFDT_DEMO_DATA_CID);
#endif
    cfg->spi_dev.bmCreateFlag = LIOT_SFDT_CCIO_TASK_RX | LIOT_SFDT_CCIO_TASK_TX2;
    cfg->spi_dev.rbufFlags = LIOT_SFDT_CCIO_RBUF_SPI;
    /* ETH 子类型；IPOS 下通常保留默认值即可。 */
    cfg->spi_dev.custEthType = LIOT_SFDT_CCIO_ETH_MIFI;
#else
    (void)cfg;
#endif
}

/**
 * @brief SFDT Slave demo 线程。
 *
 * 推荐调用顺序：
 *
 *   GetDefaultConfig -> 修改板级/CCIO 参数 -> 打印 -> Init
 *       -> （有待发数据时 NotifyTxReady） -> Deinit
 *
 * Liot_SfdtSlaveInit() 成功后，API 内部会创建自己的 SFDT 传输任务。
 * 本 demo 线程不负责搬运数据，也不应与内部任务争抢 SPI 总线。
 */
void liot_sfdt_slave_demo_thread(void *argv)
{
    Liot_SfdtSlaveConfig_t cfg;
    int32_t ret;
#if LIOT_SFDT_DEMO_TEST_FLOW_CONTROL
    uint32_t flowElapsedMs = 0U;
    uint8_t flowXoff = 0U;
#endif
#if LIOT_SFDT_DEMO_PERIODIC_NOTIFY
    uint32_t notifyElapsedMs = 0U;
#endif

    (void)argv;

    liot_rtos_task_sleep_ms(500);
    liot_trace("==== sfdt slave demo start ====");
    Liot_AonPowerCtl(true);
    Liot_SetVoltage(L_DOMAIN_ALL, L_VOLT_3_30V);
    LiotSleepModeCfg_t mode_cfg = {LIOT_SLEEP_MODE_NORMAL};
    Liot_SleepSetMode(&mode_cfg);
    //PA Ctrl
    Liot_GpioInit(L_GPIO_5, L_IO_OUTPUT, L_IO_HIGH, NULL);  // wifi 3V3

    ret = Liot_SfdtSlaveGetDefaultConfig(&cfg);
    if (ret != LIOT_SFDT_SLAVE_SUCCESS)
    {
        liot_trace("[sfdt-demo] GetDefaultConfig failed: %ld\n", (long)ret);
        liot_rtos_task_delete(NULL);
        return;
    }

    cfg.at_uart_num = LIOT_SFDT_DEMO_AT_UART_NUM;
    cfg.at_uart_enable = 1U;
#if LIOT_SFDT_DEMO_PASSTHROUGH
    cfg.data_mode = LIOT_SFDT_DATA_MODE_PASSTHROUGH;
    cfg.rx_callback = _liot_sfdt_slave_demo_rx_callback;
    cfg.tx_queue_depth = 4U;
    cfg.tx_poll_interval_ms = 0U;
#endif

    /* Establish the CAT1 bearer used by the IPOS CID before starting SFDT.
     * Liot_SfdtSlaveInit() configures the selected AT UART itself. */
#if !LIOT_SFDT_DEMO_PASSTHROUGH
    {
        liot_datacall_errcode_e data_ret =
            liot_start_data_call(0, LIOT_SFDT_DEMO_DATA_CID,
                                 LIOT_DATA_TYPE_IP, "", "", "",
                                 LIOT_DATA_AUTH_TYPE_NONE);
        if (data_ret != LIOT_DATACALL_SUCCESS)
            liot_trace("[sfdt-demo] start data call CID%d failed: %d\n",
                       LIOT_SFDT_DEMO_DATA_CID, (int)data_ret);
    }
#endif

    /* 先应用板级修改，再应用 Liot_SfdtSpiDevConfig_t 修改，最后统一打印。 */
    _liot_sfdt_slave_demo_apply_board_config(&cfg);
    _liot_sfdt_slave_demo_apply_ccio_config(&cfg);

#if LIOT_SFDT_DEMO_DUMP_CONFIG
    _liot_sfdt_slave_demo_dump_config(&cfg);
#endif

    ret = Liot_SfdtSlaveInit(&cfg);
    if (ret != LIOT_SFDT_SLAVE_SUCCESS)
    {
        liot_trace("[sfdt-demo] Liot_SfdtSlaveInit failed: %ld\n", (long)ret);
        liot_trace("[sfdt-demo] 优先检查 SPI 端口/CS、SRDY/MRDY GPIO、");
        liot_trace("spi_dev 和 FEATURE_CCIO_ENABLE\n");
        liot_rtos_task_delete(NULL);
        return;
    }

    liot_trace("[sfdt-demo] SFDT slave initialized\n");
    liot_trace("[sfdt-demo] CAT1/CCIO 上层产生 TX 数据后，内部任务会自动取队列并发送\n");

    while (1)
    {
#if LIOT_SFDT_DEMO_PERIODIC_NOTIFY
        /*
         * NotifyTxReady() 只负责唤醒内部任务，并不携带数据。
         * 数据仍由 spiDevPickTxBuf() 从 CCIO 队列取得。
         */
        if (notifyElapsedMs >= LIOT_SFDT_DEMO_NOTIFY_PERIOD_MS)
        {
            (void)Liot_SfdtSlaveNotifyTxReady();
            notifyElapsedMs = 0U;
        }
#endif

#if LIOT_SFDT_DEMO_TEST_FLOW_CONTROL
        /*
         * 仅用于验证流控。XOFF 会在 SFDT 头中声明“本端暂时不能接收”，
         * 对端应暂停向本端发送；真实产品应根据缓存水位决定何时调用。
         */
        if (flowElapsedMs >= LIOT_SFDT_DEMO_FLOW_PERIOD_MS)
        {
            flowXoff = (uint8_t)!flowXoff;
            (void)Liot_SfdtSlaveSetFlowControl(flowXoff);
            liot_trace("[sfdt-demo] local flow=%s\n",
                       flowXoff ? "XOFF" : "XON");
            flowElapsedMs = 0U;
        }
#endif

        liot_rtos_task_sleep_ms(100U);
#if LIOT_SFDT_DEMO_PERIODIC_NOTIFY
        notifyElapsedMs += 100U;
#endif
#if LIOT_SFDT_DEMO_TEST_FLOW_CONTROL
        flowElapsedMs += 100U;
#endif
    }

    /*
     * 当前 demo 通过无限循环保持 SFDT 工作，因此正常不会执行到这里。
     * 产品退出 WiFi/SFDT 功能时，应在合适的生命周期路径调用 Deinit，
     * 由 API 负责停止任务、注销低功耗回调、销毁 CCIO 设备和关闭 SPI。
     */
    (void)Liot_SfdtSlaveDeinit();
    liot_rtos_task_delete(NULL);
}

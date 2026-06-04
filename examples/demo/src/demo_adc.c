#include "liot_adc.h"
#include "liot_log.h"

void liot_adc_demo_thread(void *argv)
{

    int val0 = 0;
    int val1 = 0;

    int volt0 = 0;
    int thermal = 0;
    int vbat = 0;
    liot_adc_errcode_e ret;

    while (1)
    {
        ret = liot_adc_get_volt_raw(LIOT_ADC_THERMAL_CHANNEL, LIOT_ADC_AIO_RESDIV_RATIO_1, &thermal); 
        liot_trace("LIOT_ADC_THERMAL_CHANNEL:  %d, ret : %d\n", thermal, ret);

        ret = liot_adc_get_volt(LIOT_ADC_VBAT_CHANNEL, &vbat); 
        liot_trace("LIOT_ADC_VBAT_CHANNEL:  %d, ret : %d\n", vbat, ret);

#if defined (CHIP_EC718)
        ret = liot_adc_get_volt_raw(LIOT_ADC0_CHANNEL, LIOT_ADC_AIO_RESDIV_RATIO_16OVER32, &val0); // Set the partial pressure coefficient to 16/32
        volt0 = val0 * 32 / 16;
#elif defined (CHIP_EC716)
        ret = liot_adc_get_volt_raw(LIOT_ADC0_CHANNEL, LIOT_ADC_AIO_RESDIV_RATIO_8OVER16, &val0); // Set the partial pressure coefficient to 16/32
        volt0 = val0 * 16 / 8;
#endif
        liot_trace("val0_raw LIOT_ADC0_CHANNEL %d, ret : %d, volt0 = %d\r\n", val0, ret, volt0);

        ret = liot_adc_get_volt(LIOT_ADC1_CHANNEL, &val1);
        liot_trace("LIOT_ADC1_CHANNEL %d, ret : %d\r\n", val1, ret);
 
        liot_rtos_task_sleep_ms(1000);
    }
}


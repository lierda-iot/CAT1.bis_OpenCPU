#ifndef TIRTC_NETWORK_SIGNAL_H
#define TIRTC_NETWORK_SIGNAL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool valid;
    bool rsrq_valid;
    bool snr_valid;
    bool rssi_valid;
    int16_t rsrp_dbm;
    int16_t rsrq_x2;
    int16_t snr_db;
    int16_t rssi_dbm;
    uint8_t bars;
} tirtc_signal_sample_t;

typedef struct {
    uint8_t bars;
    uint8_t pending;
} tirtc_signal_filter_t;

/* F6D_A reports CESQ indices for RSRP/RSRQ, whole dB for SNR, and a
 * CSQ index for RSSI. Range checks reject both documented 255 and the
 * base firmware's 127 sentinel before arithmetic or narrowing. RSRP 0/97
 * and RSRQ 0/34 represent open-ended measurement bins, not exact values.
 * Bars are a conservative radio-quality rating, not a throughput estimate.
 */
static inline tirtc_signal_sample_t tirtc_signal_decode(bool query_ok,
                                                       int rssi,
                                                       int rsrp,
                                                       int rsrq,
                                                       int snr)
{
    tirtc_signal_sample_t sample = {0};
    uint8_t quality_bars;

    if (!query_ok) return sample;

    sample.rssi_valid = rssi >= 0 && rssi <= 31;
    sample.rsrq_valid = rsrq >= 0 && rsrq <= 34;
    sample.snr_valid = snr >= -20 && snr <= 40;
    sample.valid = rsrp >= 0 && rsrp <= 97;

    if (sample.rssi_valid) sample.rssi_dbm = (int16_t)(2 * rssi - 113);
    if (sample.rsrq_valid) sample.rsrq_x2 = (int16_t)(rsrq - 40);
    if (sample.snr_valid) sample.snr_db = (int16_t)snr;
    if (!sample.valid) return sample;

    sample.rsrp_dbm = (int16_t)(rsrp - 141);
    sample.bars = sample.rsrp_dbm >= -90 ? 4 :
                  sample.rsrp_dbm >= -100 ? 3 :
                  sample.rsrp_dbm >= -110 ? 2 : 1;

    if (sample.rsrq_valid) {
        quality_bars = sample.rsrq_x2 >= -20 ? 4 :
                       sample.rsrq_x2 >= -30 ? 3 :
                       sample.rsrq_x2 >= -34 ? 2 : 1;
        if (quality_bars < sample.bars) sample.bars = quality_bars;
    }
    if (sample.snr_valid) {
        quality_bars = sample.snr_db >= 10 ? 4 :
                       sample.snr_db >= 3 ? 3 :
                       sample.snr_db >= 0 ? 2 : 1;
        if (quality_bars < sample.bars) sample.bars = quality_bars;
    }
    return sample;
}

/* First usable reading and degradation apply immediately. Promotion needs
 * two consecutive readings of the same higher candidate. A pending value
 * of zero means no promotion is waiting; decoded valid candidates are 1..4.
 */
static inline uint8_t tirtc_signal_filter_update(tirtc_signal_filter_t *filter,
                                                uint8_t candidate,
                                                bool valid)
{
    if (!valid) {
        filter->bars = 0;
        filter->pending = 0;
    } else if (filter->bars == 0 || candidate <= filter->bars) {
        filter->bars = candidate;
        filter->pending = 0;
    } else if (filter->pending == candidate) {
        filter->bars = candidate;
        filter->pending = 0;
    } else {
        filter->pending = candidate;
    }
    return filter->bars;
}

#endif

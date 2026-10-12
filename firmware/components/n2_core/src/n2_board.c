#include "n2_board.h"

typedef struct {
    int32_t num;   /* grandeur = code_mV * num / den */
    int32_t den;
} n2_scale_t;

/* TODO(recoupement schéma) : rapports nominaux, résistances 0,1 % (TOL) du bloc D. */
static const n2_scale_t k_scale[N2_ADC_COUNT] = {
    [N2_ADC_V15_BP] = { 16, 1 },
    [N2_ADC_V15_SW1] = { 16, 1 },
    [N2_ADC_V15_SW2] = { 16, 1 },
    [N2_ADC_V15_SW3] = { 16, 1 },
    [N2_ADC_V15_BA1] = { 16, 1 },
    [N2_ADC_V15_BA2] = { 16, 1 },
    [N2_ADC_V15_BA3] = { 16, 1 },
    [N2_ADC_VBAT] = { 2, 1 },
    [N2_ADC_ZONE1] = { 1, 1 },
    [N2_ADC_ZONE2] = { 1, 1 },
    [N2_ADC_ZONE3] = { 1, 1 },
    [N2_ADC_ZONE4] = { 1, 1 },
    [N2_ADC_DV0_BA1] = { 1000, 249 },  /* ampli différentiel G = 24,9k / 100k */
    [N2_ADC_DV0_BA2] = { 1000, 249 },
    [N2_ADC_DV0_BA3] = { 1000, 249 },
    [N2_ADC_SIREN] = { 11, 1 },        /* 100 kΩ / 10 kΩ */
};

int32_t n2_adc_to_mv(int ch, int16_t raw)
{
    if (ch < 0 || ch >= N2_ADC_COUNT) {
        return 0;
    }
    if (raw > 2047) {
        raw = 2047;
    } else if (raw < -2048) {
        raw = -2048;
    }
    return ((int32_t)raw * k_scale[ch].num) / k_scale[ch].den;
}

uint16_t n2_zone_permille(int16_t raw)
{
    if (raw <= 0) {
        return 0;
    }
    int32_t pm = ((int32_t)raw * 1000 + N2_SUPPLY_MV / 2) / N2_SUPPLY_MV;
    return (uint16_t)(pm > 1000 ? 1000 : pm);
}

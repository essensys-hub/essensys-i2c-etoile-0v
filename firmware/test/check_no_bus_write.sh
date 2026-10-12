#!/bin/sh
# Revue automatique, tâche 7.2 / spec i2c-passive-sniffer :
# le firmware ne contient aucune API capable d'émettre sur le bus I2C Essensys.
# Garantie principale : matérielle (isolateur unidirectionnel). Ce contrôle
# détecte une régression du code. Usage : check_no_bus_write.sh <firmware/>
set -u
FW=${1:-$(dirname "$0")/..}
SRC="$FW/main $FW/components"
fail=0

report() {
    echo "INTERDIT ($1) :"
    echo "$2"
    fail=1
}

scan() {
    # $1 = description, $2 = regex étendue
    out=$(grep -rnE --include='*.c' --include='*.h' "$2" $SRC 2>/dev/null)
    [ -n "$out" ] && report "$1" "$out"
}

# 1. Aucun périphérique capable d'émettre en esclave ou de générer des signaux.
scan "esclave I2C ou emission RMT" 'i2c_slave|i2c_new_slave_device|rmt_new_tx_channel|rmt_transmit|rmt_tx_|i2c_param_config|i2c_driver_install'

# 2. Aucune fonction de sortie appliquée aux broches d'écoute.
scan "sortie sur broche d'ecoute" '(gpio_set_level|gpio_set_direction|gpio_od_enable|gpio_matrix_out|esp_rom_gpio_connect_out_signal|gpio_set_drive_capability|gpio_hold_en|gpio_iomux_out|gpio_set_pull_mode)[[:space:]]*\([[:space:]]*N2_PIN_SNIFF'
scan "mode sortie avec broche d'ecoute" 'SNIFF.*GPIO_MODE_(OUTPUT|INPUT_OUTPUT)|GPIO_MODE_(OUTPUT|INPUT_OUTPUT).*SNIFF'

# 3. Broches uniquement via n2_pins.h (pas de numéro en dur).
scan "numero de broche en dur" 'gpio_[a-z_]+\([[:space:]]*(GPIO_NUM_)?[0-9]+[[:space:]]*[,)]|GPIO_NUM_[0-9]'

# 4. Le seul bus I2C maître est le bus local (RTC, ADC) sur ses propres broches.
out=$(grep -rnE --include='*.c' '\.(sda|scl)_io_num' $SRC | grep -vE 'N2_PIN_LOCAL_(SDA|SCL)')
[ -n "$out" ] && report "bus I2C maitre hors broches locales" "$out"

# 5. Le chemin des commandes MQTT n'atteint ni l'écoute ni un GPIO.
for f in "$FW/components/n2_core/src/mqtt_cmd.c" "$FW/components/n2_core/src/cmd_handler.c" "$FW/main/n2_net.c"; do
    [ -f "$f" ] || continue
    out=$(grep -nE 'sniff|edge_ring|i2c_master|i2c_decoder|gpio_|n2_hal_' "$f")
    [ -n "$out" ] && report "commande MQTT vers le materiel ($f)" "$out"
done

if [ "$fail" -ne 0 ]; then
    exit 1
fi
echo "check_no_bus_write : aucune API d'ecriture sur le bus Essensys"

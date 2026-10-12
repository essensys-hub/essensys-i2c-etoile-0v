#!/usr/bin/env python3
"""Données de conception du module ESP32 « espion + alarme » de la carte N2 (révision B).

Source unique partagée par gen_sch.py (schéma) et gen_pcb.py (PCB) : composants, valeurs,
empreintes, broche -> net, symboles propres au projet. Feature : n2-esp32-spy-alarm-2026-10-008.

Conventions :
- 0V      : étoile 0 V du bus (zone bus) ; GND : masse du module (zone module), reliée au 0 V
            en un seul point par le net-tie NT1. Aucun des deux n'est relié au PE.
- 'NC'    : broche volontairement non connectée (drapeau no-connect au schéma).
"""

# ---------------------------------------------------------------- empreintes
R0603 = 'Resistor_SMD:R_0603_1608Metric'
R0805 = 'Resistor_SMD:R_0805_2012Metric'
R1206 = 'Resistor_SMD:R_1206_3216Metric'
C0603 = 'Capacitor_SMD:C_0603_1608Metric'
C0805 = 'Capacitor_SMD:C_0805_2012Metric'
C1206 = 'Capacitor_SMD:C_1206_3216Metric'
LED0603 = 'LED_SMD:LED_0603_1608Metric'
SOIC8 = 'Package_SO:SOIC-8_3.9x4.9mm_P1.27mm'
SOIC8EP = 'Package_SO:SOIC-8-1EP_3.9x4.9mm_P1.27mm_EP2.41x3.3mm'
SOIC14 = 'Package_SO:SOIC-14_3.9x8.7mm_P1.27mm'
SOIC16W = 'Package_SO:SOIC-16W_7.5x10.3mm_P1.27mm'
SOT23 = 'Package_TO_SOT_SMD:SOT-23'
SOT235 = 'Package_TO_SOT_SMD:SOT-23-5'
SOT236 = 'Package_TO_SOT_SMD:SOT-23-6'
TSOT236 = 'Package_TO_SOT_SMD:TSOT-23-6'
SOT223 = 'Package_TO_SOT_SMD:SOT-223'
TO92 = 'Package_TO_SOT_THT:TO-92_Inline'
DSMA = 'Diode_SMD:D_SMA'
DSMF = 'Diode_SMD:D_SMF'
PTC1812 = 'Fuse:Fuse_1812_4532Metric'
HDR2x2 = 'Connector_PinHeader_2.54mm:PinHeader_2x02_P2.54mm_Vertical'
HDR2x4 = 'Connector_PinHeader_2.54mm:PinHeader_2x04_P2.54mm_Vertical'
TB8 = 'TerminalBlock_Phoenix:TerminalBlock_Phoenix_PT-1,5-8-3.5-H_1x08_P3.50mm_Horizontal'
TB4 = 'TerminalBlock_Phoenix:TerminalBlock_Phoenix_PT-1,5-4-3.5-H_1x04_P3.50mm_Horizontal'
# Même motif de pastilles que l'ESP32-C3-MINI-1 (13,2 x 16,6 mm, 53 pastilles, pas de 0,8 mm,
# datasheet ESP32-C3-MINI-1 v2.2 fig. 11-1) : la bibliothèque KiCad 9 n'a pas d'empreinte C3-MINI-1.
ESP_FP = 'RF_Module:ESP32-C6-MINI-1'

# ---------------------------------------------------------------- symboles propres (bibliothèque n2)
# pin : (numéro, nom, type, côté L/R/B/T, rang) ; plusieurs numéros au même (côté, rang) = broches empilées
ESP_GND = [1, 2, 11, 14] + list(range(36, 54))
CUSTOM = {
    'ESP32-C3-MINI-1': dict(
        ref='U', w=20.32,
        desc='Module Wi-Fi/BLE ESP32-C3 (RISC-V), antenne intégrée, 4 Mo flash, 3,3 V. Datasheet ESP32-C3-MINI-1 v2.2',
        ds='https://documentation.espressif.com/esp32-c3-mini-1_datasheet_en.pdf',
        pins=[('3', '3V3', 'power_in', 'T', 0), ('8', 'EN', 'input', 'L', 0),
              ('12', 'IO0', 'bidirectional', 'L', 2), ('13', 'IO1', 'bidirectional', 'L', 3),
              ('5', 'IO2', 'bidirectional', 'L', 4), ('6', 'IO3', 'bidirectional', 'L', 5),
              ('18', 'IO4', 'bidirectional', 'L', 6), ('19', 'IO5', 'bidirectional', 'L', 7),
              ('20', 'IO6', 'bidirectional', 'L', 8), ('21', 'IO7', 'bidirectional', 'L', 9),
              ('22', 'IO8', 'bidirectional', 'R', 0), ('23', 'IO9', 'bidirectional', 'R', 1),
              ('16', 'IO10', 'bidirectional', 'R', 2), ('26', 'IO18/USB_D-', 'bidirectional', 'R', 4),
              ('27', 'IO19/USB_D+', 'bidirectional', 'R', 5), ('30', 'RXD0/IO20', 'bidirectional', 'R', 7),
              ('31', 'TXD0/IO21', 'bidirectional', 'R', 8)] +
             [(str(n), 'GND', 'power_in' if n == 1 else 'passive', 'B', 0) for n in ESP_GND]),
    'CN3058E': dict(
        ref='U', w=15.24,
        desc='Chargeur linéaire CC/CV LiFePO4 1 cellule, 3,6 V, TEMP (CTN), eSOP-8. Brochage à vérifier sur la datasheet Consonance',
        ds='http://www.consonance-elec.com',
        pins=[('4', 'VIN', 'power_in', 'L', 0), ('2', 'ISET', 'passive', 'L', 2), ('1', 'TEMP', 'input', 'L', 3),
              ('5', 'BAT', 'power_out', 'R', 0), ('8', 'FB', 'input', 'R', 1),
              ('7', '~{CHRG}', 'open_collector', 'R', 2), ('6', '~{DONE}', 'open_collector', 'R', 3),
              ('3', 'GND', 'power_in', 'B', 0), ('9', 'GND', 'passive', 'B', 0)]),
}

# ---------------------------------------------------------------- composants
COMPONENTS = []   # dicts : ref, lib, value, fp, pins{num: net}, fields{}, block


def comp(ref, lib, value, fp, pins, block, **fields):
    COMPONENTS.append(dict(ref=ref, lib=lib, value=value, fp=fp, pins={str(k): v for k, v in pins.items()},
                           fields=fields, block=block))


def R(ref, val, a, b, block, fp=R0603, **f):
    comp(ref, 'Device:R', val, fp, {1: a, 2: b}, block, **f)


def Cap(ref, val, a, b, block, fp=C0603, **f):
    comp(ref, 'Device:C', val, fp, {1: a, 2: b}, block, **f)


def flag(ref, net, block):
    COMPONENTS.append(dict(ref=ref, lib='power:PWR_FLAG', value='PWR_FLAG', fp='', pins={'1': net},
                           fields={}, block=block, flag=True))


TOL = dict(Tolerance='0,1 % 25 ppm/°C')

# ---- Bloc A : écoute I2C passive (zone bus -> isolateur -> module) ----
B = 'A'
comp('JP4', 'Connector_Generic:Conn_02x02_Odd_Even', "Cavalier d'écoute (2 shunts)", HDR2x2,
     {1: 'SDA_BP', 2: 'SNF_SDA_J', 3: 'SCL_BP', 4: 'SNF_SCL_J'}, B,
     Note="Retirer les 2 shunts = aucune liaison entre le module et SDA/SCL")
R('R1', '1k', 'SNF_SDA_J', 'ISO_INA', B)
R('R2', '1k', 'SNF_SCL_J', 'ISO_INB', B)
comp('U1', 'Isolator:ISO7720D', 'ISO7720D', SOIC8,
     {1: '+5V_ISO', 2: 'ISO_INA', 3: 'ISO_INB', 4: '0V', 5: 'GND', 6: 'SNF_SCL', 7: 'SNF_SDA', 8: '3V3'}, B,
     Manufacturer='Texas Instruments', MPN='ISO7720DR (2/0 canaux ; seconde source ADuM1200ARZ, même brochage)')
Cap('C1', '100n', '+5V_ISO', '0V', B)
Cap('C2', '100n', '3V3', 'GND', B)
R('R3', '100', '+15V_BP', '+15V_ISR', B, fp=R1206, Note='Limite le courant du LDO côté bus')
comp('U2', 'Regulator_Linear:LP2950-5.0_TO92', 'LP2950-5.0', TO92, {1: '+5V_ISO', 2: '0V', 3: '+15V_ISR'}, B,
     Manufacturer='Texas Instruments', MPN='LP2950-50LPRE3 (VIN max 30 V)')
Cap('C3', '1u 50V', '+15V_ISR', '0V', B, fp=C0805)
Cap('C4', '2u2', '+5V_ISO', '0V', B)
flag('#FLG01', '0V', B)
flag('#FLG06', '+15V_ISR', B)

# ---- Bloc B : alimentation 15 V -> 5 V -> 3,3 V, batterie LiFePO4, power-path ----
B = 'B'
comp('F4', 'Device:Polyfuse', 'PTC 0,2 A 1812', PTC1812, {1: '+15V_BP', 2: '+15V_MF'}, B,
     Manufacturer='Bourns', MPN='MF-MSMF020-2 (Ihold 0,2 A, 30 V, à confirmer)')
comp('D4', 'Diode:SS14', 'SS14', DSMA, {1: '+15V_M', 2: '+15V_MF'}, B, Note='Anti-retour / inversion')
comp('D5', 'Diode:SMAJ16A', 'SMAJ16A', DSMA, {1: '+15V_M', 2: 'GND'}, B, Manufacturer='Littelfuse', MPN='SMAJ16A')
Cap('C5', '10u 35V', '+15V_M', 'GND', B, fp=C1206)
Cap('C6', '100n 50V', '+15V_M', 'GND', B)
comp('U3', 'Regulator_Switching:AP63205WU', 'AP63205WU', TSOT236,
     {1: '+5V', 2: '+15V_M', 3: '+15V_M', 4: 'GND', 5: 'BUCK_SW', 6: 'BUCK_BST'}, B,
     Manufacturer='Diodes Inc.', MPN='AP63205WU-7 (5 V fixe, 3,8-32 V)')
Cap('C7', '100n', 'BUCK_BST', 'BUCK_SW', B)
comp('L1', 'Device:L', '4,7u', 'Inductor_SMD:L_Taiyo-Yuden_NR-40xx', {1: 'BUCK_SW', 2: '+5V'}, B,
     Manufacturer='Taiyo Yuden', MPN='NR4018T4R7M')
Cap('C8', '22u 10V', '+5V', 'GND', B, fp=C1206)
Cap('C9', '22u 10V', '+5V', 'GND', B, fp=C1206)
R('R4', '100k', '+5V', 'GND', B, Note='Décharge du +5 V : commute Q1 quand le 15 V disparaît')
comp('D6', 'Diode:SS14', 'SS14', DSMA, {1: 'VSYS', 2: '+5V'}, B, Note='Power-path : secteur')
comp('Q1', 'Transistor_FET:AO3401A', 'AO3401A', SOT23, {1: '+5V', 2: 'VSYS', 3: 'VBAT'}, B,
     Note='Power-path : batterie (diode de structure + canal, basculement sans coupure)')
Cap('C10', '10u', 'VSYS', 'GND', B, fp=C0805)
comp('U4', 'n2:CN3058E', 'CN3058E', SOIC8EP,
     {4: '+5V', 2: 'CHG_ISET', 1: 'CHG_TEMP', 5: 'VBAT', 8: 'VBAT', 7: 'CHG_CHRG', 6: 'CHG_DONE', 3: 'GND', 9: 'GND'}, B,
     Manufacturer='Consonance', MPN='CN3058E (LiFePO4 3,6 V ; brochage à vérifier)')
R('R5', '9k1', 'CHG_ISET', 'GND', B, Note='ICH = 1800 V / RISET = 200 mA (à vérifier datasheet)')
R('R6', '6k2', '+5V', 'CHG_TEMP', B, Note='Fenêtre CTN 45-80 % de VIN : charge entre ~3 et ~43 °C')
comp('TH1', 'Device:Thermistor_NTC', 'CTN 10k B3435', R0603, {1: 'CHG_TEMP', 2: 'GND'}, B,
     Manufacturer='Murata', MPN='NCP18XH103F03RB (au contact du porte-pile)')
Cap('C11', '1u', '+5V', 'GND', B)
Cap('C12', '10u', 'VBAT', 'GND', B, fp=C0805)
comp('F5', 'Device:Polyfuse', 'PTC 1,5 A 1812', PTC1812, {1: 'BAT_P', 2: 'VBAT'}, B,
     Manufacturer='Littelfuse', MPN='1812L150/12MR (à confirmer)')
comp('BT1', 'Device:Battery_Cell', 'LiFePO4 18650 3,2 V 1,5 Ah', 'Battery:BatteryHolder_Keystone_1042_1x18650',
     {1: 'BAT_P', 2: 'GND'}, B, Manufacturer='Keystone', MPN='1042 (porte-pile 18650) + cellule IFR18650 1500 mAh')
comp('U5', 'Regulator_Linear:AP2112K-3.3', 'AP2112K-3.3', SOT235, {1: 'VSYS', 2: 'GND', 3: 'VSYS', 4: 'NC', 5: '3V3'}, B,
     Manufacturer='Diodes Inc.', MPN='AP2112K-3.3TRG1')
Cap('C13', '1u', 'VSYS', 'GND', B)
Cap('C14', '10u', '3V3', 'GND', B, fp=C0805)
R('R7', '10k', '+5V', 'PG_5V', B)
R('R8', '20k', 'PG_5V', 'GND', B)
R('R9', '100k', 'VBAT', 'MEAS_VBAT', B, **TOL)
R('R10', '100k', 'MEAS_VBAT', 'GND', B, **TOL)
Cap('C15', '1n', 'MEAS_VBAT', 'GND', B)
comp('NT1', 'Device:NetTie_2', 'NetTie 0V-GND', 'NetTie:NetTie-2_SMD_Pad2.0mm', {1: '0V', 2: 'GND'}, B,
     Note='Unique liaison 0 V bus / masse module ; aucune liaison au PE')
COMPONENTS[-1]['inbom'] = False
for i, n in enumerate(['GND', '+15V_M', '+5V', 'VSYS'], start=2):
    flag('#FLG%02d' % i, n, B)

# ---- Bloc C : ESP32-C3, RTC, extension d'E/S, débogage ----
B = 'C'
comp('U6', 'n2:ESP32-C3-MINI-1', 'ESP32-C3-MINI-1', ESP_FP,
     dict([(3, '3V3'), (8, 'EN_ESP'), (12, 'SNF_SCL'), (13, 'SNF_SDA'), (5, 'IO2'), (6, 'EXP_INT'),
           (18, 'I2C_SDA'), (19, 'I2C_SCL'), (20, 'SIREN_EN'), (21, 'LATCH_RST'), (22, 'LED_STAT'),
           (23, 'BOOT'), (16, 'RTC_INT'), (26, 'USB_DM'), (27, 'USB_DP'), (30, 'U0RX'), (31, 'U0TX')] +
          [(n, 'GND') for n in ESP_GND]), B,
     Manufacturer='Espressif', MPN='ESP32-C3-MINI-1-N4 (empreinte = motif MINI-1, identique au C6-MINI-1)')
Cap('C16', '10u', '3V3', 'GND', B, fp=C0805)
Cap('C17', '100n', '3V3', 'GND', B)
R('R11', '10k', '3V3', 'EN_ESP', B)
Cap('C18', '1u', 'EN_ESP', 'GND', B)
R('R12', '10k', '3V3', 'IO2', B, Note='Broche de démarrage GPIO2 = 1')
R('R13', '10k', '3V3', 'LED_STAT', B, Note='Broche de démarrage GPIO8 = 1')
R('R14', '10k', '3V3', 'BOOT', B)
R('R15', '10k', '3V3', 'RTC_INT', B)
R('R16', '1k', '3V3', 'LED1_A', B)
comp('LED1', 'Device:LED', 'LED verte (état)', LED0603, {1: 'LED_STAT', 2: 'LED1_A'}, B)
comp('SW1', 'Switch:SW_Push', 'BOOT / service', 'Button_Switch_SMD:SW_SPST_PTS645Sx43SMTR92', {1: 'BOOT', 2: 'GND'}, B,
     Note='Démarrage en téléchargement ; appui long = mode debug local')
comp('J9', 'Connector_Generic:Conn_02x04_Odd_Even', 'Débogage UART/USB', HDR2x4,
     {1: '3V3', 2: 'GND', 3: 'U0TX', 4: 'U0RX', 5: 'EN_ESP', 6: 'BOOT', 7: 'USB_DM', 8: 'USB_DP'}, B)
flag('#FLG07', 'RTC_VB', B)
R('R17', '4k7', '3V3', 'I2C_SDA', B)
R('R18', '4k7', '3V3', 'I2C_SCL', B)
comp('U7', 'Timer_RTC:RV-3028-C7', 'RV-3028-C7', 'Package_SON:MicroCrystal_C7_SON-8_1.5x3.2mm_P0.9mm',
     {1: 'NC', 2: 'RTC_INT', 3: 'I2C_SCL', 4: 'I2C_SDA', 5: 'GND', 6: 'RTC_VB', 7: '3V3', 8: 'LATCH_0V'}, B,
     Manufacturer='Micro Crystal', MPN='RV-3028-C7 32.768kHz 1ppm-TA-QC (I2C 0x52, EVI = horodatage du latch 0 V)')
Cap('C19', '100n', '3V3', 'GND', B)
Cap('C20', '11m 3,3V', 'RTC_VB', 'GND', B, fp='Capacitor_SMD:C_1210_3225Metric',
    Manufacturer='Seiko', MPN='CPH3225A (supercondensateur, chargé par le trickle charger du RV-3028)')
comp('U8', 'Interface_Expansion:TCA9534', 'TCA9534DW', SOIC16W,
     {1: 'GND', 2: 'GND', 3: 'GND', 4: 'LATCH_0V', 5: 'LATCH_OV', 6: 'CHG_CHRG', 7: 'CHG_DONE', 8: 'GND',
      9: 'ARM_IN', 10: 'PG_5V', 11: 'LED_ALM', 12: 'EXP_P7', 13: 'EXP_INT', 14: 'I2C_SCL', 15: 'I2C_SDA', 16: '3V3'}, B,
     Manufacturer='Texas Instruments', MPN='TCA9534DWR (I2C 0x20)')
Cap('C21', '100n', '3V3', 'GND', B)
R('R19', '10k', '3V3', 'EXP_INT', B)
R('R20', '10k', '3V3', 'CHG_CHRG', B)
R('R21', '10k', '3V3', 'CHG_DONE', B)
R('R22', '1k', '3V3', 'LED2_A', B)
R('R23', '10k', 'EXP_P7', 'GND', B, Note='P7 libre : tirage bas')
comp('LED2', 'Device:LED', 'LED rouge (alarme)', LED0603, {1: 'LED_ALM', 2: 'LED2_A'}, B)

# ---- Bloc D : mesures (diviseurs, amplis différentiels 0 V, ADC I2C) ----
B = 'D'
RAILS = [('15BP', '+15V_BP'), ('SW1', '+15V_SW1'), ('SW2', '+15V_SW2'), ('SW3', '+15V_SW3'),
         ('BA1', '+15V_BA1'), ('BA2', '+15V_BA2'), ('BA3', '+15V_BA3')]
for i, (tag, rail) in enumerate(RAILS):
    # résistance haute à la prise (zone bus), basse et filtre près de l'ADC (zone module)
    R('R%d' % (30 + i), '150k', rail, 'MEAS_' + tag, B, fp=R0805, **TOL)
    R('R%d' % (40 + i), '10k', 'MEAS_' + tag, 'GND', B, **TOL)
    Cap('C%d' % (30 + i), '100p' if i == 0 else '1n', 'MEAS_' + tag, 'GND', B,
        Note='C0G ; 100 pF sur MEAS_15BP (comparateur de surtension rapide)' if i == 0 else 'C0G')
for n in (1, 2, 3):
    # ampli différentiel G = 24,9k/100k : VDIFFn = VREF + 0,249 x (0V port n - 0V entrée BP)
    R('R%d' % (50 + n), '100k', '0V', 'DA_P%d' % n, B, fp=R1206, Note='Prise Kelvin 0 V port BA%d' % n, **TOL)
    R('R%d' % (53 + n), '100k', '0V', 'DA_M%d' % n, B, fp=R1206, Note='Prise Kelvin 0 V entrée BP (J1-3)', **TOL)
    R('R%d' % (56 + n), '24k9', 'DA_P%d' % n, 'VREF', B, **TOL)
    R('R%d' % (59 + n), '24k9', 'DA_M%d' % n, 'VDIFF%d' % n, B, **TOL)
comp('U9', 'Amplifier_Operational:TLV9064', 'TLV9064IDR', SOIC14,
     {3: 'DA_P1', 2: 'DA_M1', 1: 'VDIFF1', 5: 'DA_P2', 6: 'DA_M2', 7: 'VDIFF2',
      10: 'DA_P3', 9: 'DA_M3', 8: 'VDIFF3', 12: 'VREF_R', 13: 'VREF', 14: 'VREF', 4: '3V3', 11: 'GND'}, B,
     Manufacturer='Texas Instruments', MPN='TLV9064IDR (10 MHz, RRIO)')
Cap('C40', '100n', '3V3', 'GND', B)
# échelle de seuils ratiométrique (3,3 V) : 1,249 / 1,000 / 0,750 V
R('R63', '20k5', '3V3', 'VTH_H', B, **TOL)
R('R64', '2k49', 'VTH_H', 'VREF_R', B, **TOL)
R('R65', '2k49', 'VREF_R', 'VTH_L', B, **TOL)
R('R66', '7k5', 'VTH_L', 'GND', B, **TOL)
Cap('C41', '100n', 'VREF_R', 'GND', B)
ADCS = [('U10', 'GND', 'GND', ['MEAS_15BP', 'MEAS_SW1', 'MEAS_SW2', 'MEAS_SW3'], ['GND'] * 4),
        ('U11', '3V3', 'GND', ['MEAS_BA1', 'MEAS_BA2', 'MEAS_BA3', 'MEAS_VBAT'], ['GND'] * 4),
        ('U12', 'GND', '3V3', ['ZONE1_M', 'ZONE2_M', 'ZONE3_M', 'ZONE4_M'], ['GND'] * 4),
        ('U13', '3V3', '3V3', ['VDIFF1', 'VDIFF2', 'VDIFF3', 'MEAS_SIR'], ['VREF', 'VREF', 'VREF', 'GND'])]
for k, (ref, a0, a1, pos, neg) in enumerate(ADCS):
    comp(ref, 'Analog_ADC:MCP3428x-xSL', 'MCP3428-E/SL', SOIC14,
         {1: pos[0], 2: neg[0], 3: pos[1], 4: neg[1], 11: pos[2], 12: neg[2], 13: pos[3], 14: neg[3],
          5: 'GND', 6: '3V3', 7: 'I2C_SDA', 8: 'I2C_SCL', 9: a0, 10: a1}, B,
         Manufacturer='Microchip', MPN='MCP3428-E/SL (16 bits, réf. 2,048 V ; Adr0=%s Adr1=%s)' % (a0, a1))
    Cap('C%d' % (42 + k), '100n', '3V3', 'GND', B)

# ---- Bloc E : latch surtension (comparateurs + bascule) ----
B = 'E'
# LM339 : sortie basse quand IN+ < IN-. Fenêtre +/-0,25 V autour de VREF = +/-1 V d'écart de 0 V.
comp('U14', 'Comparator:LM339', 'LM339', SOIC14,
     {7: 'VTH_H', 6: 'VDIFF1', 1: 'LSET_0V', 5: 'VDIFF1', 4: 'VTH_L', 2: 'LSET_0V',
      9: 'VTH_H', 8: 'VDIFF2', 14: 'LSET_0V', 11: 'VDIFF2', 10: 'VTH_L', 13: 'LSET_0V', 3: '3V3', 12: 'GND'}, B,
     Manufacturer='Texas Instruments', MPN='LM339DR')
comp('U15', 'Comparator:LM339', 'LM339', SOIC14,
     {7: 'VTH_H', 6: 'VDIFF3', 1: 'LSET_0V', 5: 'VDIFF3', 4: 'VTH_L', 2: 'LSET_0V',
      9: 'VTH_H', 8: 'MEAS_15BP', 14: 'LSET_OV', 11: 'GND', 10: 'VREF_R', 13: 'NC', 3: '3V3', 12: 'GND'}, B,
     Manufacturer='Texas Instruments', MPN='LM339DR (U15D inutilisé)')
Cap('C46', '100n', '3V3', 'GND', B)
Cap('C47', '100n', '3V3', 'GND', B)
R('R67', '10k', '3V3', 'LSET_0V', B)
R('R68', '10k', '3V3', 'LSET_OV', B)
R('R69', '10k', '3V3', 'LATCH_RST', B, Note='Pas de réarmement pendant le reset du module')
comp('U16', '74xx:74HC74', '74HC74', SOIC14,
     {1: 'LATCH_RST', 2: 'GND', 3: 'GND', 4: 'LSET_0V', 5: 'LATCH_0V', 6: 'NC',
      13: 'LATCH_RST', 12: 'GND', 11: 'GND', 10: 'LSET_OV', 9: 'LATCH_OV', 8: 'NC', 7: 'GND', 14: '3V3'}, B,
     Manufacturer='Nexperia', MPN='74HC74D')
Cap('C48', '100n', '3V3', 'GND', B)

# ---- Bloc F : alarme (boucles équilibrées, clé, sirène 12 V) ----
B = 'F'
comp('J10', 'Connector_Generic:Conn_01x08', 'Boucles Z1-Z4', TB8,
     {1: 'Z1_T', 2: 'GND', 3: 'Z2_T', 4: 'GND', 5: 'Z3_T', 6: 'GND', 7: 'Z4_T', 8: 'GND'}, B,
     Manufacturer='Phoenix Contact', MPN='1984688 (PT 1,5/8-3,5-H)')
for n in (1, 2, 3, 4):
    R('R%d' % (70 + n), '1k', 'Z%d_T' % n, 'ZONE%d_M' % n, B, fp=R0805)
    R('R%d' % (74 + n), '10k', '3V3', 'ZONE%d_M' % n, B)
    comp('D%d' % (10 + n), 'Diode:SMF5V0A', 'SMF5V0A', DSMF, {1: 'Z%d_T' % n, 2: 'GND'}, B,
         Manufacturer='Littelfuse', MPN='SMF5.0A')
    Cap('C%d' % (50 + n), '10n', 'ZONE%d_M' % n, 'GND', B)
comp('J11', 'Connector_Generic:Conn_01x04', 'Clé ARM / sirène 12 V', TB4,
     {1: 'ARM_T', 2: 'GND', 3: 'SIREN_P', 4: 'GND'}, B, Manufacturer='Phoenix Contact', MPN='1984633 (PT 1,5/4-3,5-H)')
R('R79', '1k', 'ARM_T', 'ARM_IN', B, fp=R0805)
R('R80', '10k', '3V3', 'ARM_IN', B)
comp('D15', 'Diode:SMF5V0A', 'SMF5V0A', DSMF, {1: 'ARM_T', 2: 'GND'}, B, Manufacturer='Littelfuse', MPN='SMF5.0A')
comp('U17', 'Regulator_Switching:MT3608', 'MT3608', SOT236,
     {1: 'BOOST_SW', 2: 'GND', 3: 'BOOST_FB', 4: 'SIREN_EN', 5: 'VBAT', 6: 'NC'}, B,
     Manufacturer='Aerosemi', MPN='MT3608 (12 V depuis VBAT, actif seulement sirène en marche)')
comp('L2', 'Device:L', '22u', 'Inductor_SMD:L_Taiyo-Yuden_NR-50xx', {1: 'VBAT', 2: 'BOOST_SW'}, B,
     Manufacturer='Taiyo Yuden', MPN='NR5040T220M')
comp('D7', 'Diode:SS14', 'SS14', DSMA, {1: '+12V_S', 2: 'BOOST_SW'}, B)
R('R81', '191k', '+12V_S', 'BOOST_FB', B)
R('R82', '10k', 'BOOST_FB', 'GND', B)
Cap('C55', '22u 25V', '+12V_S', 'GND', B, fp=C1206)
Cap('C56', '10u', 'VBAT', 'GND', B, fp=C0805)
comp('U18', 'Transistor_FET_Other:BTS4140N', 'BTS4140N', SOT223,
     {1: 'SIREN_EN', 2: '+12V_S', 3: 'SIREN_P', 4: '+12V_S'}, B,
     Manufacturer='Infineon', MPN='BTS4140N (high-side protégé : limitation de courant + coupure thermique)')
R('R83', '100k', 'SIREN_EN', 'GND', B, Note='Sirène et boost coupés au reset')
comp('D16', 'Diode:SMAJ16A', 'SMAJ16A', DSMA, {1: 'SIREN_P', 2: 'GND'}, B, Manufacturer='Littelfuse', MPN='SMAJ16A')
R('R84', '100k', 'SIREN_P', 'MEAS_SIR', B, **TOL)
R('R85', '10k', 'MEAS_SIR', 'GND', B, **TOL)
Cap('C57', '1n', 'MEAS_SIR', 'GND', B)

BLOCK_TITLES = {
    'A': 'A – ÉCOUTE I2C PASSIVE : cavalier JP4, 1 kΩ série, isolateur unidirectionnel ISO7720 (2 canaux bus → module, aucun canal retour)',
    'B': 'B – ALIMENTATION : PTC propre, anti-retour, TVS, abaisseur 15→5 V, chargeur LiFePO4 3,6 V 200 mA (CTN), power-path, LDO 3,3 V',
    'C': 'C – MODULE ESP32-C3, RTC RV-3028 (supercondensateur), extension d\'E/S TCA9534, débogage',
    'D': 'D – MESURES : +15 V amont/aval des PTC (÷16), écart de 0 V (amplis différentiels G = 0,249), ADC MCP3428 x4',
    'E': 'E – LATCH SURTENSION : fenêtre ±1 V sur l\'écart de 0 V, +15 V > 20 V ; bascule 74HC74 réarmée par GPIO7',
    'F': 'F – ALARME : 4 boucles équilibrées (FDL 2 x 4,7 kΩ), clé ARM, sirène 12 V (boost MT3608 + BTS4140N)',
}

# nets « puissance » (pistes larges au routage)
POWER_NETS = {'+15V_BP', '+15V_MF', '+15V_M', '+5V', 'VSYS', 'VBAT', 'BAT_P', '3V3', '+12V_S', 'SIREN_P',
              'BUCK_SW', 'BOOST_SW', '+15V_SW1', '+15V_SW2', '+15V_SW3', '+15V_BA1', '+15V_BA2', '+15V_BA3'}

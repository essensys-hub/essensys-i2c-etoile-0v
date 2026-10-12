#!/usr/bin/env python3
"""Génère les traces SDA/SCL synthétiques du bus I2C BP <-> BA Essensys.

Format (compatible avec un export d'analyseur logique) : CSV « t_us,scl,sda »,
une ligne par changement de niveau, première ligne = état de repos.

Timing : 50 kHz (demi-période 10 µs), SDA change 2 µs après le front
descendant de SCL. Protocole : docs/i2c.md d'essensys-board-SC944D
(écriture code + données + CRC-16/MODBUS, repeated start, lecture de
5 octets : écho code, écho CRC, CRC de la réponse).

Usage : gen_traces.py <dossier_sortie>
"""
import os
import sys

HALF = 10
SETUP = 2


def crc16_modbus(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


class Bus:
    def __init__(self):
        self.t = 0
        self.scl = 1
        self.sda = 1
        self.rows = [(0, 1, 1)]

    def _set(self, dt, scl=None, sda=None):
        self.t += dt
        changed = False
        if scl is not None and scl != self.scl:
            self.scl = scl
            changed = True
        if sda is not None and sda != self.sda:
            self.sda = sda
            changed = True
        if changed:
            self.rows.append((self.t, self.scl, self.sda))

    def idle(self, us):
        self.t += us

    def start(self):
        if self.scl == 0:  # repeated start : relâcher SDA puis SCL
            self._set(SETUP, sda=1)
            self._set(HALF - SETUP, scl=1)
        self._set(5, sda=0)
        self._set(5, scl=0)

    def bit(self, b):
        self._set(SETUP, sda=b)
        self._set(HALF - SETUP, scl=1)
        self._set(HALF, scl=0)

    def byte(self, v, ack, stretch_us=0):
        for i in range(7, -1, -1):
            self.bit((v >> i) & 1)
        self.bit(0 if ack else 1)
        self.t += stretch_us  # étirement d'horloge par l'esclave (SCL tenue basse)

    def stop(self):
        self._set(SETUP, sda=0)
        self._set(HALF - SETUP, scl=1)
        self._set(5, sda=1)

    def hold(self, line, us):
        """Ligne forcée à 0 pendant us (défaut matériel), puis relâchée."""
        if line == "sda":
            self._set(0, sda=0)
            self._set(us, sda=1)
        else:
            self._set(0, scl=0)
            self._set(us, scl=1)

    def save(self, path, comment):
        with open(path, "w", encoding="ascii", newline="\n") as f:
            f.write("# " + comment + "\n")
            f.write("t_us,scl,sda\n")
            for t, c, d in self.rows:
                f.write(f"{t},{c},{d}\n")


def transaction(bus, addr, code, data, ba_present=True, corrupt_write_crc=False,
                resp_crc_bad=False, stretch_us=0):
    frame = [code] + list(data)
    crc = crc16_modbus(frame)
    w = frame + [crc & 0xFF, crc >> 8]
    if corrupt_write_crc:
        w[-1] ^= 0x55
    bus.start()
    bus.byte((addr << 1) | 0, ack=ba_present, stretch_us=stretch_us)
    if not ba_present:
        bus.stop()
        return
    for b in w:
        bus.byte(b, ack=True)
    bus.start()
    bus.byte((addr << 1) | 1, ack=True)
    received_crc = w[-2] | (w[-1] << 8)
    echo = received_crc if crc16_modbus(w[:-2]) == received_crc else 0
    resp = [code, echo & 0xFF, echo >> 8]
    rc = crc16_modbus(resp)
    resp += [rc & 0xFF, rc >> 8]
    if resp_crc_bad:
        resp[4] ^= 0x01
    for i, b in enumerate(resp):
        bus.byte(b, ack=(i < len(resp) - 1))  # le maître NACK le dernier octet
    bus.stop()


def main(out):
    os.makedirs(out, exist_ok=True)

    b = Bus(); b.idle(50)
    transaction(b, 0x11, 5, [0x01]); b.idle(50)
    b.save(os.path.join(out, "t01_actions_ba1_ok.csv"), "ACTIONS (code 5) vers 0x11, acquittee")

    b = Bus(); b.idle(50)
    transaction(b, 0x12, 1, [0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00], stretch_us=300); b.idle(50)
    b.save(os.path.join(out, "t02_forcage_ba2_stretch_ok.csv"),
           "FORCAGE_SORTIES (code 1) vers 0x12 avec etirement d'horloge 300 us")

    b = Bus(); b.idle(50)
    transaction(b, 0x13, 3, list(range(1, 17))); b.idle(50)
    b.save(os.path.join(out, "t03_tps_extinction_ba3_ok.csv"), "TPS_EXTINCTION (code 3, 16 octets) vers 0x13")

    b = Bus(); b.idle(50)
    for _ in range(3):
        transaction(b, 0x13, 5, [0x00], ba_present=False)
        b.idle(100000)
    b.save(os.path.join(out, "t04_ba3_absent_nack.csv"), "3 tentatives vers 0x13 absent (NACK adresse), 100 ms")

    b = Bus(); b.idle(50)
    transaction(b, 0x11, 2, [0, 1, 2, 0, 1, 2, 0, 1], corrupt_write_crc=True); b.idle(50)
    b.save(os.path.join(out, "t05_write_crc_error.csv"), "CONF_SORTIES vers 0x11, CRC emis faux, echo CRC nul")

    b = Bus(); b.idle(50)
    transaction(b, 0x12, 4, [10, 20, 30, 40, 50, 60, 70, 80], resp_crc_bad=True); b.idle(50)
    b.save(os.path.join(out, "t06_response_crc_error.csv"), "TPS_ACTION vers 0x12, CRC de reponse faux")

    b = Bus(); b.idle(50)
    b.start(); b.byte(0x11 << 1, ack=True); b.byte(5, ack=True); b.byte(0x01, ack=True)
    for bit in (1, 0, 1, 1):
        b.bit(bit)
    b.stop(); b.idle(50)
    b.save(os.path.join(out, "t07_truncated.csv"), "trame tronquee : STOP apres 4 bits")

    b = Bus(); b.idle(50)
    transaction(b, 0x11, 5, [0x04]); b.idle(100000)
    transaction(b, 0x12, 2, [1, 1, 0, 0, 2, 2, 0, 0]); b.idle(100000)
    transaction(b, 0x13, 4, [5, 5, 5, 5, 5, 5, 5, 5]); b.idle(100000)
    transaction(b, 0x12, 5, [0x00], ba_present=False); b.idle(50)
    b.save(os.path.join(out, "t08_sequence_mixed.csv"), "sequence : 0x11 ok, 0x12 ok, 0x13 ok, 0x12 NACK")

    b = Bus(); b.idle(1000)
    b.hold("sda", 150000); b.idle(1000)
    transaction(b, 0x11, 5, [0x01]); b.idle(50)
    b.save(os.path.join(out, "t09_sda_stuck_low.csv"), "SDA forcee basse 150 ms puis trame normale")

    b = Bus(); b.idle(50)
    b.start(); b.byte(0x12 << 1, ack=True)
    b._set(SETUP, sda=1)  # le BA relâche SDA après l'ACK puis fige SCL
    b.hold("scl", 120000)
    b.idle(1000)
    transaction(b, 0x12, 5, [0x02]); b.idle(50)
    b.save(os.path.join(out, "t10_scl_stuck_low.csv"),
           "SCL tenue basse 120 ms apres l'adresse (BA fige) puis trame normale")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])

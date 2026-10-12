#!/usr/bin/env python3
"""Génère n2-etoile-0v.kicad_pcb avec l'API pcbnew de KiCad 9.
Révision A : zone bus (étoile 0 V, ports BA), routée à la main ci-dessous.
Révision B : zone module ESP32 (n2-esp32-spy-alarm-2026-10-008), composants issus de n2_module.py,
placés ici et routés par autoroute.py (routeur maillé déterministe).
Usage : <python de KiCad> gen_pcb.py <dossier_sortie>"""
import sys, os, uuid
import wx
_app = wx.App(False)
import pcbnew
from pcbnew import VECTOR2I, FromMM as M
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n2_module as NM

OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
ROUTE = '--no-route' not in sys.argv
FPL = '/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints/'
W, H = 235.0, 68.0
XB0, XB1 = 110.3, 111.7          # fin de la zone bus / début de la zone module (gap entre plans)
ANT = (105.4, 0.0, 148.6, 6.4)    # zone interdite antenne ESP32 (courtyard du module, sans cuivre)
b = pcbnew.CreateEmptyBoard()
b.SetCopperLayerCount(2)
ds = b.GetDesignSettings()
ds.m_TrackMinWidth = M(0.25); ds.m_MinClearance = M(0.2)
tb = b.GetTitleBlock()
tb.SetTitle('N2 étoile 0 V + module ESP32'); tb.SetRevision('B'); tb.SetDate('2026-10-12')
tb.SetCompany('Essensys – essensys-i2c-etoile-0v')
tb.SetComment(0, 'Feature: n2-esp32-spy-alarm-2026-10-008 – Refs essensys-hub/essensys-feature-lifecycle#28')
tb.SetComment(1, 'FR-4 1,6 mm, 2 couches, cuivre 2 oz (70 µm) recommandé')

nets = {}
def net(n):
    n = '/' + n
    if n not in nets:
        ni = pcbnew.NETINFO_ITEM(b, n); b.Add(ni); nets[n] = ni
    return nets[n]

fps = {}
VALS = {'J1': 'BP J31 (WR-MPC3 2x2)', 'J8': 'Queue de cochon AWG20 -> J31-3 du BP'}
for k, ba in enumerate(['BA1', 'BA2', 'BA3'], 1):
    VALS.update({'JP%d' % k: 'Cavalier port %s (3 shunts)' % ba, 'F%d' % k: 'PTC >=30 V 1812', 'D%d' % k: 'SMAJ16A',
                 'J%d' % (2 * k): '%s J1 (WR-MPC3 2x2)' % ba, 'J%d' % (2 * k + 1): '%s J2 (WR-MPC3 2x2)' % ba})
MOD = {c['ref']: c for c in NM.COMPONENTS if not c.get('flag')}
# champs des composants de la révision A (identiques au schéma, contrôle de parité)
MPCX = {'Manufacturer': 'Würth Elektronik', 'MPN': '66200421122 (WR-MPC3 2x2, vertical, THT)'}
OLD_FIELDS = {'J1': MPCX, 'J8': {'Note': 'Pastille de soudure, pas de composant'}}
for k, ba in enumerate(['BA1', 'BA2', 'BA3'], 1):
    OLD_FIELDS['JP%d' % k] = {'Note': 'Retirer les 3 shunts (ou le pont 2x3) = BA isolé du bus, 0 V conservé'}
    OLD_FIELDS['F%d' % k] = {'Manufacturer': 'Littelfuse', 'MPN': '1812L050/30MR (à confirmer : Ihold@50°C >= 1,5 x I BA_ALIM)'}
    OLD_FIELDS['D%d' % k] = {'Manufacturer': 'Littelfuse', 'MPN': 'SMAJ16A'}
    OLD_FIELDS['J%d' % (2 * k)] = MPCX; OLD_FIELDS['J%d' % (2 * k + 1)] = MPCX
for r, c in MOD.items(): VALS[r] = c['value']

def fp(lib, name, ref, val, x, y, padnets, rot=0):
    f = pcbnew.FootprintLoad(FPL + lib + '.pretty', name)
    f.SetReference(ref); f.SetValue(VALS.get(ref, val))
    f.SetFPID(pcbnew.LIB_ID(lib, name))
    if ref.startswith('H'):
        f.SetAttributes(f.GetAttributes() | pcbnew.FP_BOARD_ONLY | pcbnew.FP_EXCLUDE_FROM_BOM)
    else:
        f.SetPath(pcbnew.KIID_PATH('/' + str(uuid.uuid5(uuid.NAMESPACE_URL, 'n2-etoile-0v/sym/' + ref))))
    f.SetPosition(VECTOR2I(M(x), M(y)))
    if rot: f.SetOrientationDegrees(rot)
    b.Add(f)
    for p in f.Pads():
        if p.GetNumber() in padnets: p.SetNet(net(padnets[p.GetNumber()]))
    f.Value().SetVisible(False)
    for k, v in OLD_FIELDS.get(ref, {}).items():
        f.SetField(k, v)
        fl = f.GetField(k); fl.SetVisible(False); fl.SetLayer(pcbnew.F_Fab)
    fps[ref] = f
    return f
def pad(ref, n):
    for p in fps[ref].Pads():
        if p.GetNumber() == n:
            v = p.GetPosition(); return (pcbnew.ToMM(v.x), pcbnew.ToMM(v.y))
def track(pts, n, w, layer=pcbnew.F_Cu):
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(b); t.SetStart(VECTOR2I(M(x1), M(y1))); t.SetEnd(VECTOR2I(M(x2), M(y2)))
        t.SetWidth(M(w)); t.SetLayer(layer); t.SetNet(net(n)); b.Add(t)
def via(x, y, n, d=0.8, drill=0.4):
    v = pcbnew.PCB_VIA(b); v.SetPosition(VECTOR2I(M(x), M(y))); v.SetWidth(M(d)); v.SetDrill(M(drill))
    v.SetNet(net(n)); b.Add(v)
def line(x1, y1, x2, y2, layer=pcbnew.Edge_Cuts, w=0.1):
    s = pcbnew.PCB_SHAPE(b); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(VECTOR2I(M(x1), M(y1))); s.SetEnd(VECTOR2I(M(x2), M(y2))); s.SetLayer(layer); s.SetWidth(M(w)); b.Add(s)
def txt(t, x, y, size=1.0, layer=pcbnew.F_SilkS, th=0.15):
    o = pcbnew.PCB_TEXT(b); o.SetText(t); o.SetPosition(VECTOR2I(M(x), M(y))); o.SetLayer(layer)
    o.SetTextSize(VECTOR2I(M(size), M(size))); o.SetTextThickness(M(th))
    if layer in (pcbnew.B_SilkS,): o.SetMirrored(True)
    b.Add(o)

B = pcbnew.B_Cu
SIG, PWR, GNDW = 0.5, 1.5, 3.0
# ---- contour ----
for a in [(0, 0, W, 0), (W, 0, W, H), (W, H, 0, H), (0, H, 0, 0)]: line(*a)
for i, (x, y) in enumerate([(4, 4), (101, 4), (4, H - 4), (106, H - 4), (W - 4, 4), (W - 4, H - 4)], 1):
    fp('MountingHole', 'MountingHole_3.2mm_M3', 'H%d' % i, 'M3 isolé', x, y, {})
    fps['H%d' % i].Reference().SetVisible(False)

# ===================== ZONE BUS (révision A) =====================
MPC = ('Connector_Molex', 'Molex_Micro-Fit_3.0_43045-0412_2x02_P3.00mm_Vertical')
fp(*MPC, 'J1', 'BP J31', 8, 30, {'1': 'SDA_BP', '2': 'SCL_BP', '3': '0V', '4': '+15V_BP'})
fp('Connector_Wire', 'SolderWire-0.75sqmm_1x01_D1.25mm_OD3.5mm', 'J8', 'AWG20 vers J31-3', 9.5, 50, {'1': '0V'})
track([pad('J1', '1'), (8, 12), (84, 12)], 'SDA_BP', SIG)          # tronc SDA
track([pad('J1', '2'), (11, 16), (81, 16)], 'SCL_BP', SIG)         # tronc SCL
track([pad('J1', '4'), (11, 37), (17, 37), (17, 20), (78, 20)], '+15V_BP', 2.0)  # tronc +15 V
track([pad('J1', '3'), (8, 39), (9.5, 41), pad('J8', '1')], '0V', GNDW)

BAS = ['BA1', 'BA2', 'BA3']
LBL = ['BA1 SC940D', 'BA2 SC941C', 'BA3 SC942C']
for k in range(3):
    x0 = 28 + 28 * k; ba = BAS[k]; n = k + 1
    jp, fr, dr = 'JP%d' % n, 'F%d' % n, 'D%d' % n
    ja, jb = 'J%d' % (2 * n), 'J%d' % (2 * n + 1)
    fp('Connector_PinHeader_2.54mm', 'PinHeader_2x03_P2.54mm_Vertical', jp, 'cavalier', x0, 26,
       {'1': 'SDA_BP', '2': 'SDA_' + ba, '3': 'SCL_BP', '4': 'SCL_' + ba, '5': '+15V_BP', '6': '+15V_SW%d' % n})
    fp('Fuse', 'Fuse_1812_4532Metric', fr, 'PTC 30V', x0 + 13, 34, {'1': '+15V_SW%d' % n, '2': '+15V_' + ba})
    fp('Diode_SMD', 'D_SMA', dr, 'SMAJ16A', x0 + 17.14, 40, {'1': '+15V_' + ba, '2': '0V'})
    a, bb = x0 - 4, x0 + 8
    fp(*MPC, ja, ba + ' J1', a, 52, {'1': 'SDA_' + ba, '2': 'SCL_' + ba, '3': '0V', '4': '+15V_' + ba})
    fp(*MPC, jb, ba + ' J2', bb, 52, {'1': 'SDA_' + ba, '2': 'SCL_' + ba, '3': '0V', '4': '+15V_' + ba})
    via(x0, 12, 'SDA_BP'); track([(x0, 12), pad(jp, '1')], 'SDA_BP', SIG, B)
    via(x0 - 3, 16, 'SCL_BP'); track([(x0 - 3, 16), (x0 - 3, 28.54), pad(jp, '3')], 'SCL_BP', SIG, B)
    track([(x0 - 6, 20), (x0 - 6, 31.08), pad(jp, '5')], '+15V_BP', PWR)
    track([pad(jp, '2'), (x0 + 7, 26), (x0 + 7, 46), (bb, 46), pad(jb, '1')], 'SDA_' + ba, SIG, B)
    track([(x0 + 7, 46), (a, 46), pad(ja, '1')], 'SDA_' + ba, SIG, B)
    track([pad(jp, '4'), (x0 + 5, 28.54), (x0 + 5, 44)], 'SCL_' + ba, SIG, B)
    via(x0 + 5, 44, 'SCL_' + ba)
    track([(x0 + 5, 44), (x0 + 5, 49), (bb + 3, 49), pad(jb, '2')], 'SCL_' + ba, SIG)
    track([(x0 + 5, 49), (a + 3, 49), pad(ja, '2')], 'SCL_' + ba, SIG)
    track([pad(jp, '6'), (pad(fr, '1')[0], 31.08), pad(fr, '1')], '+15V_SW%d' % n, PWR)
    track([pad(fr, '2'), pad(dr, '1')], '+15V_' + ba, PWR)
    track([pad(dr, '1'), (x0 + 15.14, 43), (x0 + 16.5, 43)], '+15V_' + ba, PWR)
    via(x0 + 16.5, 43, '+15V_' + ba, 1.2, 0.6)
    track([(x0 + 16.5, 43), (x0 + 16.5, 59), (bb + 3, 59), pad(jb, '4')], '+15V_' + ba, PWR, B)
    track([(bb + 3, 59), (a + 3, 59), pad(ja, '4')], '+15V_' + ba, PWR, B)
    track([pad(dr, '2'), (x0 + 19.14, 45)], '0V', 1.8)
    txt(LBL[k], x0 + 4, 64.5, 1.2)
    txt('JP%d ouvert = BA isolé' % n, x0 + 3, 34.6, 0.8, th=0.12)

# ===================== ZONE MODULE (révision B) =====================
# nets « non connectés » tels que nommés par le netlist KiCad (contrôle de parité schéma/PCB)
NC_NETS = {('U5', '4'): 'unconnected-(U5-NC-Pad4)', ('U7', '1'): 'unconnected-(U7-CLKOUT-Pad1)',
           ('U15', '13'): 'unconnected-(U15-Pad13)', ('U16', '6'): 'unconnected-(U16A-~{Q}-Pad6)',
           ('U16', '8'): 'unconnected-(U16B-~{Q}-Pad8)', ('U17', '6'): 'unconnected-(U17-NC-Pad6)'}
def mod(ref, x, y, rot=0):
    c = MOD[ref]
    lib, name = c['fp'].split(':')
    f = fp(lib, name, ref, c['value'], x, y, {k: v for k, v in c['pins'].items() if v != 'NC'}, rot)
    for k, v in c['pins'].items():
        if v == 'NC':
            nn = NC_NETS[(ref, k)]
            if nn not in nets:
                ni = pcbnew.NETINFO_ITEM(b, nn); b.Add(ni); nets[nn] = ni
            for p in f.Pads():
                if p.GetNumber() == k: p.SetNet(nets[nn])
    for k, v in c['fields'].items():
        f.SetField(k, v)
        fl = f.GetField(k); fl.SetVisible(False); fl.SetLayer(pcbnew.F_Fab)
    return f

def court(f):
    bb = f.GetCourtyard(pcbnew.F_CrtYd).BBox()
    return pcbnew.ToMM(bb.GetX()), pcbnew.ToMM(bb.GetY()), pcbnew.ToMM(bb.GetRight()), pcbnew.ToMM(bb.GetBottom())

def put(ref, x0, y0, rot=0):
    """Place ref avec le coin haut-gauche de son courtyard en (x0, y0) ; renvoie (x1, y1) du courtyard."""
    f = mod(ref, 0, 0, rot)
    cx0, cy0, cx1, cy1 = court(f)
    f.SetPosition(VECTOR2I(M(x0 - cx0), M(y0 - cy0)))
    return x0 + cx1 - cx0, y0 + cy1 - cy0

def row(x, y, groups, gap=0.35, cgap=0.35):
    """groups : ref (rot 0), (ref, rot) ou liste = colonne de passifs verticaux. Renvoie x de fin."""
    for g in groups:
        if isinstance(g, list):
            yy, xm = y, x
            for r in g:
                rr, rot = (r if isinstance(r, tuple) else (r, 90))
                x1, y1 = put(rr, x, yy, rot); yy = y1 + cgap; xm = max(xm, x1)
            x = xm + gap
        else:
            rr, rot = (g if isinstance(g, tuple) else (g, 0))
            x1, _ = put(rr, x, y, rot); x = x1 + gap
    return x

# ---- zone bus : écoute, LDO isolateur, prises de mesure ----
mod('JP4', 93, 9.5); mod('R1', 100, 9.5); mod('R2', 100, 12.0); mod('C1', 104.5, 9.5)
mod('U1', 111, 13); mod('C4', 104.5, 12.5)
mod('U2', 99, 18); mod('C3', 105, 18, 90); mod('R3', 95, 19, 90)
mod('R30', 104, 23)
mod('NT1', 111, 37)
for k in range(3):
    x0 = 28 + 28 * k
    mod('R%d' % (31 + k), x0 + 8, 38.5)          # prise +15V_SWn (amont PTC)
    mod('R%d' % (34 + k), x0 + 12, 44.5)         # prise +15V_BAn (aval PTC)
    mod('R%d' % (51 + k), x0 - 4, 61)            # Kelvin 0 V port n
    track([pad('J%d' % (2 * k + 2), '3'), pad('R%d' % (51 + k), '1')], '0V', 0.3)
for i, y in enumerate((46, 49, 52)):
    mod('R%d' % (54 + i), 4.5, y, 180)           # Kelvin 0 V entrée BP (pastille J8 -> J31-3)
    track([pad('J8', '1'), pad('R%d' % (54 + i), '1')], '0V', 0.3)

# ---- zone module ----
mod('U6', 127, 12)                                # ESP32-C3-MINI-1, antenne en bord haut
mod('C2', 116, 13, 90); mod('C16', 118.5, 8.2, 90); mod('C17', 118.5, 11.5, 90)
mod('R11', 118.5, 15.9, 90); mod('C18', 116, 17.2, 90); mod('R12', 118.5, 19, 90)
for i, r in enumerate(['R13', 'R14', 'R15', 'LED1', 'R16']):
    mod(r, 134 + 2 * i, 20, 90)
mod('J9', 138, 9); mod('SW1', 148, 12)
# alimentation 15 V -> 5 V -> 3,3 V
mod('F4', 115.3, 30, 90); mod('D4', 119.8, 30, 90); mod('D5', 124.2, 30, 90)
mod('C5', 128, 28, 90); mod('C6', 128, 32.5, 90)
mod('U3', 132.5, 30); mod('C7', 132.5, 26.5); mod('L1', 138, 30)
mod('C8', 138, 35); mod('C9', 138, 38); mod('R4', 133, 34)
mod('D6', 131, 38); mod('Q1', 125, 38); mod('C10', 120, 38, 90)
mod('U5', 125, 43); mod('C13', 120.5, 43, 90); mod('C14', 129.5, 43, 90)
mod('R7', 134, 42.5); mod('R8', 134, 44.5)
# chargeur LiFePO4 + batterie
mod('BT1', 175, 56.6)
mod('U4', 118, 50); mod('R5', 115, 55); mod('R6', 115, 57.5); mod('C11', 119.5, 55, 90)
mod('C12', 124, 55, 90); mod('F5', 127, 49, 90); mod('TH1', 128, 62)
mod('R9', 115, 60.5); mod('R10', 115, 63); mod('C15', 119.5, 61.5, 90)
# bornier alarme + entrées
mod('J10', 190, 6.5, 180); mod('J11', 207.5, 6.5, 180)
for n in (1, 2, 3, 4):
    xz = 190 - 7 * (n - 1)
    mod('D%d' % (10 + n), xz - 1.75, 14.2, 90); mod('R%d' % (70 + n), xz + 1.6, 14.2, 90)
    mod('C%d' % (50 + n), xz - 1.6, 18.6, 90); mod('R%d' % (74 + n), xz + 1.6, 18.6, 90)
mod('D15', 205.75, 14.2, 90); mod('R79', 209.1, 14.2, 90); mod('R80', 209.1, 18.6, 90)
mod('D16', 200.5, 15.5, 90)
# sirène : boost + high-side
row(212, 11.5, ['U17', ['R81', 'R82'], 'L2'])
row(212, 19.0, ['D7', ['C56'], ('C55', 90)])
row(194, 24.0, [('U18', 0), ['R83'], ['R84', 'R85'], ['C57']])
# extension d'E/S, RTC, I2C
mod('U7', 155.5, 13.5); mod('C19', 158.3, 13.5, 90); mod('C20', 161.2, 15.2, 90)
row(145, 21.0, [['R17', 'R18'], ['C21'], ('U8', 0), ['R19', 'R20'], ['R21', 'R23'], ['R22', 'LED2']])
mod('U12', 184.5, 27.5); mod('C44', 189.5, 27.5, 90)
# chaîne analogique (y 31 -> 44)
row(141, 32.5, [['R40', 'C30', 'R41'], ['C31', 'R42', 'C32'], ['R43', 'C33', 'C42'], 'U10',
                ['R44', 'C34', 'R45'], ['C35', 'R46', 'C36'], ['C43'], 'U11',
                ['R57', 'R60'], ['R58', 'R61'], ['R59', 'R62'], 'U9', ['C40', 'R63'], ['R64', 'R65'], ['R66', 'C41'],
                'U13', ['C45']], gap=1.1, cgap=0.9)
# latch : comparateurs + bascule, à droite
mod('U14', 216.5, 31); mod('U15', 227, 31); mod('U16', 221.5, 40.6)
mod('C46', 221.5, 28, 90); mod('R67', 221.5, 32.2, 90); mod('C47', 232, 28, 90); mod('R68', 232, 32.2, 90)
mod('C48', 216, 40.6, 90); mod('R69', 227, 40.6, 90)

# ---- plans : 0 V (zone bus), GND (zone module) ; zone interdite antenne ----
def zone(netname, pts, layer):
    z = pcbnew.ZONE(b); z.SetLayer(layer); z.SetNet(net(netname))
    ol = z.Outline(); ol.NewOutline()
    for (x, y) in pts: ol.Append(M(x), M(y))
    z.SetLocalClearance(M(0.4)); z.SetMinThickness(M(0.3))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    z.SetIsFilled(False); b.Add(z)
for layer in (pcbnew.F_Cu, B):
    zone('0V', [(0.5, 0.5), (XB0, 0.5), (XB0, H - 0.5), (0.5, H - 0.5)], layer)
    zone('GND', [(XB1, 0.5), (W - 0.5, 0.5), (W - 0.5, H - 0.5), (XB1, H - 0.5)], layer)
ka = pcbnew.ZONE(b); ka.SetIsRuleArea(True); ka.SetLayerSet(pcbnew.LSET.AllCuMask())
ka.SetDoNotAllowTracks(True); ka.SetDoNotAllowVias(True); ka.SetDoNotAllowZoneFills(True)
ka.SetDoNotAllowPads(False); ka.SetDoNotAllowFootprints(False)
ol = ka.Outline(); ol.NewOutline()
for (x, y) in [(ANT[0], ANT[1]), (ANT[2], ANT[1]), (ANT[2], ANT[3]), (ANT[0], ANT[3])]: ol.Append(M(x), M(y))
ka.SetZoneName('antenne_ESP32'); b.Add(ka)

# vias de couture 0 V (zone bus)
for x in range(12, 100, 8): via(x, 65, '0V')
for (x, y) in [(4, 20), (4, 40), (4, 56), (20, 44), (22, 64), (106, 20), (106, 40), (106, 56), (14, 56)]:
    via(x, y, '0V')

# ---- sérigraphie ----
txt('N2 ÉTOILE 0 V  rév. B  –  essensys-i2c-etoile-0v', 55, 2.6, 1.2)
txt('0 V TBTS – NE PAS RELIER À LA TERRE', 55, 7.2, 1.3, th=0.2)
txt('← BP J31', 20.5, 27, 1.0)
txt('0V → J31-3 (AWG20)', 13.5, 54.5, 0.8, th=0.12)
txt('1 SDA  2 SCL  3 0V  4 +15V', 40, 66.8, 0.8, th=0.12)
txt('Fab : FR-4 1,6 mm, 2 couches, Cu 2 oz, HASL, vernis vert, sérigraphie blanche', 55, 4, 0.9, pcbnew.B_SilkS, 0.15)
txt('Trous M3 NON reliés : support DIN isolant, 0 V jamais au PE', 55, 6, 0.9, pcbnew.B_SilkS, 0.15)
txt('ZONE MODULE (GND) – TBTS – AUCUNE LIAISON AU PE', 172, 66.6, 0.9, pcbnew.B_SilkS, 0.15)

if ROUTE:
    import autoroute
    autoroute.route_board(b, nets, NM.POWER_NETS, skip={'/0V', '/GND'}, keepouts=[ANT],
                          xsplit=(XB0, XB1), size=(W, H))

# ---- repères sérigraphiés des composants du module : position libre la plus proche, sinon masqués ----
def bbox(it, m=0.0):
    r = it.GetBoundingBox()
    return (ToMM(r.GetX()) - m, ToMM(r.GetY()) - m, ToMM(r.GetRight()) + m, ToMM(r.GetBottom()) + m)
def inter(a, c): return not (a[2] <= c[0] or c[2] <= a[0] or a[3] <= c[1] or c[3] <= a[1])
from pcbnew import ToMM
obst = []
for f in b.GetFootprints():
    for p in f.Pads():
        if p.IsOnLayer(pcbnew.F_Cu) or p.IsOnLayer(pcbnew.F_Mask): obst.append(bbox(p, 0.25))
    for g in f.GraphicalItems():
        if g.GetLayer() == pcbnew.F_SilkS: obst.append(bbox(g, 0.2))
    if f.GetReference() not in MOD and f.Reference().IsVisible(): obst.append(bbox(f.Reference(), 0.2))
for t in b.GetTracks():
    if t.GetClass() == 'PCB_VIA': obst.append(bbox(t, 0.15))
for d in b.GetDrawings():
    if d.GetLayer() == pcbnew.F_SilkS: obst.append(bbox(d, 0.2))
obst.append((ANT[0], ANT[1], ANT[2], ANT[3]))
hidden = []
for ref in sorted(MOD, key=lambda r: (r[0], int(''.join(ch for ch in r if ch.isdigit()) or 0))):
    f = fps[ref]; t = f.Reference()
    t.SetTextSize(VECTOR2I(M(0.8), M(0.8))); t.SetTextThickness(M(0.12)); t.SetTextAngleDegrees(0)
    cx0, cy0, cx1, cy1 = court(f)
    mx, my = (cx0 + cx1) / 2, (cy0 + cy1) / 2
    done = False
    for dd in (0.0, 0.8, 1.6):
        for (x, y) in [(mx, cy0 - 0.5 - dd), (mx, cy1 + 0.5 + dd), (cx0 - 1.2 - dd - 0.4 * len(ref), my),
                       (cx1 + 1.2 + dd + 0.4 * len(ref), my)]:
            t.SetPosition(VECTOR2I(M(x), M(y)))
            bx = bbox(t, 0.1)
            if bx[0] < 0.8 or bx[1] < 0.8 or bx[2] > W - 0.8 or bx[3] > H - 0.8: continue
            if any(inter(bx, o) for o in obst): continue
            obst.append(bx); done = True; break
        if done: break
    if not done:
        t.SetVisible(False); hidden.append(ref)
print('repères masqués (gardés en F.Fab) :', len(hidden))

pcbnew.SaveBoard(os.path.join(OUT, 'n2-etoile-0v.kicad_pcb'), b)
print('ok')

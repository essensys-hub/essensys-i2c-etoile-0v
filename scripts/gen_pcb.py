#!/usr/bin/env python3
"""Génère n2-etoile-0v.kicad_pcb avec l'API pcbnew de KiCad 9.
Usage : <python de KiCad> gen_pcb.py <dossier_sortie>"""
import sys, os, uuid
import wx
_app = wx.App(False)
import pcbnew
from pcbnew import VECTOR2I, FromMM as M

OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
FPL = '/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints/'
W, H = 110.0, 68.0
b = pcbnew.CreateEmptyBoard()
b.SetCopperLayerCount(2)
ds = b.GetDesignSettings()
ds.m_TrackMinWidth = M(0.25); ds.m_MinClearance = M(0.2)
tb = b.GetTitleBlock()
tb.SetTitle('N2 étoile 0 V'); tb.SetRevision('A'); tb.SetDate('2026-10-12')
tb.SetCompany('Essensys – essensys-i2c-etoile-0v')
tb.SetComment(0, 'Feature: i2c-surge-protection-2026-10-007 – Refs #1')
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
for i, (x, y) in enumerate([(4, 4), (W - 4, 4), (4, H - 4), (W - 4, H - 4)], 1):
    fp('MountingHole', 'MountingHole_3.2mm_M3', 'H%d' % i, 'M3 isolé', x, y, {})
    fps['H%d' % i].Reference().SetVisible(False)

MPC = ('Connector_Molex', 'Molex_Micro-Fit_3.0_43045-0412_2x02_P3.00mm_Vertical')
# ---- entrée BP ----
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
    # descentes du bus BP vers le cavalier
    via(x0, 12, 'SDA_BP'); track([(x0, 12), pad(jp, '1')], 'SDA_BP', SIG, B)
    via(x0 - 3, 16, 'SCL_BP'); track([(x0 - 3, 16), (x0 - 3, 28.54), pad(jp, '3')], 'SCL_BP', SIG, B)
    track([(x0 - 6, 20), (x0 - 6, 31.08), pad(jp, '5')], '+15V_BP', PWR)
    # côté port
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
    # 0 V : piste courte et large TVS -> 0 V des embases
    track([pad(dr, '2'), (x0 + 19.14, 45)], '0V', 1.8)
    txt(LBL[k], x0 + 4, 64.5, 1.2)
    txt('JP%d ouvert = BA isolé' % n, x0 + 3, 34.6, 0.8, th=0.12)

# ---- plans 0 V (2 faces) ----
for layer in (pcbnew.F_Cu, B):
    z = pcbnew.ZONE(b); z.SetLayer(layer); z.SetNet(net('0V'))
    ol = z.Outline(); ol.NewOutline()
    for (x, y) in [(0.5, 0.5), (W - 0.5, 0.5), (W - 0.5, H - 0.5), (0.5, H - 0.5)]: ol.Append(M(x), M(y))
    z.SetLocalClearance(M(0.4)); z.SetMinThickness(M(0.3))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetIsFilled(False); b.Add(z)
# vias de couture 0 V
for x in range(12, 100, 8): via(x, 65, '0V')
for (x, y) in [(4, 20), (4, 40), (4, 56), (20, 44), (22, 64), (106, 20), (106, 40), (106, 56), (14, 56)]:
    via(x, y, '0V')

txt('N2 ÉTOILE 0 V  rév. A  –  essensys-i2c-etoile-0v', 55, 2.6, 1.2)
txt('0 V TBTS – NE PAS RELIER À LA TERRE', 55, 7.2, 1.3, th=0.2)
txt('← BP J31', 20.5, 27, 1.0)
txt('0V → J31-3 (AWG20)', 13.5, 54.5, 0.8, th=0.12)
txt('1 SDA  2 SCL  3 0V  4 +15V', 40, 66.8, 0.8, th=0.12)
txt('Fab : FR-4 1,6 mm, 2 couches, Cu 2 oz, HASL, vernis vert, sérigraphie blanche', 55, 4, 0.9, pcbnew.B_SilkS, 0.15)
txt('Trous M3 NON reliés : support DIN isolant, 0 V jamais au PE', 55, 6, 0.9, pcbnew.B_SilkS, 0.15)

# zones remplies par kicad-cli pcb drc --refill-zones --save-board
pcbnew.SaveBoard(os.path.join(OUT, 'n2-etoile-0v.kicad_pcb'), b)
print('ok')

#!/usr/bin/env python3
"""Génère n2-etoile-0v.kicad_sch (KiCad 9) et la bibliothèque de projet n2-etoile-0v.kicad_sym.
Partie bus (révision A, i2c-surge-protection-2026-10-007) + module ESP32 espion/alarme
(révision B, n2-esp32-spy-alarm-2026-10-008, données dans n2_module.py).
Usage : python3 gen_sch.py <dossier_sortie>"""
import sys, os, uuid, copy, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n2_module as M

KSYM = '/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols/'
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
NAME = 'n2-etoile-0v'
U = lambda s: str(uuid.uuid5(uuid.NAMESPACE_URL, 'n2-etoile-0v/' + s))
ROOT = U('root')
G = 2.54

# ---------- S-expression ----------
def tok(s):
    i, n = 0, len(s)
    while i < n:
        c = s[i]
        if c in '()':
            yield c; i += 1
        elif c.isspace():
            i += 1
        elif c == '"':
            j = i + 1; buf = []
            while s[j] != '"':
                if s[j] == '\\': buf.append(s[j:j+2]); j += 2
                else: buf.append(s[j]); j += 1
            yield ('S', ''.join(buf)); i = j + 1
        else:
            j = i
            while j < n and not s[j].isspace() and s[j] not in '()': j += 1
            yield s[i:j]; i = j
def parse(s):
    st = [[]]
    for t in tok(s):
        if t == '(': st.append([])
        elif t == ')': x = st.pop(); st[-1].append(x)
        else: st[-1].append(t)
    return st[0]
def dump(x, ind=0):
    if isinstance(x, tuple): return '"' + x[1] + '"'
    if isinstance(x, str): return x
    if all(not isinstance(e, list) for e in x):
        return '(' + ' '.join(dump(e) for e in x) + ')'
    pad = '\t' * (ind + 1)
    return '(' + dump(x[0]) + ''.join('\n' + pad + dump(e, ind + 1) for e in x[1:]) + '\n' + '\t' * ind + ')'
def S(v): return ('S', v.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n'))
def f(v): return ('%.3f' % v).rstrip('0').rstrip('.')

_libs = {}
def lib(name):
    if name not in _libs:
        _libs[name] = parse(open(KSYM + name + '.kicad_sym').read())[0]
    return _libs[name]
def find_sym(libname, sym):
    for e in lib(libname):
        if isinstance(e, list) and e[0] == 'symbol' and e[1][1] == sym: return copy.deepcopy(e)
    raise KeyError(sym)
def flat_sym(libname, sym):
    s = find_sym(libname, sym)
    ext = [e for e in s if isinstance(e, list) and e[0] == 'extends']
    if not ext: return s
    parent = flat_sym(libname, ext[0][1][1])
    pname = parent[1][1]
    parent[1] = S(sym)
    childprops = {e[1][1]: e for e in s if isinstance(e, list) and e[0] == 'property'}
    for i, e in enumerate(parent):
        if isinstance(e, list) and e[0] == 'property' and e[1][1] in childprops:
            parent[i] = childprops[e[1][1]]
        if isinstance(e, list) and e[0] == 'symbol':
            e[1] = S(e[1][1].replace(pname, sym, 1))
    return parent
def pins_of(sym):
    """num -> (x, y, angle, unit) ; ignore la représentation De Morgan (style 2)."""
    res = {}
    def walk(x, unit):
        for e in x:
            if isinstance(e, list):
                if e[0] == 'symbol' and isinstance(e[1], tuple):
                    parts = e[1][1].rsplit('_', 2)
                    if len(parts) == 3 and parts[2] == '2': continue
                    walk(e, int(parts[1]) if len(parts) == 3 else unit)
                elif e[0] == 'pin':
                    at = [a for a in e if isinstance(a, list) and a[0] == 'at'][0]
                    num = [a for a in e if isinstance(a, list) and a[0] == 'number'][0][1][1]
                    if num not in res:
                        res[num] = (float(at[1]), float(at[2]), int(float(at[3])), unit)
                else: walk(e, unit)
    walk(sym, 0); return res

# ---------- symboles propres au projet (bibliothèque n2) ----------
def fx(size=1.27, hide=False, j=None):
    e = ['effects', ['font', ['size', f(size), f(size)]]]
    if j: e.append(['justify'] + j)
    if hide: e.append(['hide', 'yes'])
    return e
def custom_symbol(name, spec, fp):
    w = spec['w']
    rows = max([p[4] for p in spec['pins'] if p[3] in 'LR'] + [0]) + 1
    top = G * math.ceil((rows + 1) / 2.0)
    bot = top - G * (rows + 1)
    hw = w / 2
    pins, seen = [], set()
    tb = {}
    for (num, nm, typ, side, rank) in spec['pins']:
        if side == 'L': at = (-hw - G, top - G * (rank + 1), 0)
        elif side == 'R': at = (hw + G, top - G * (rank + 1), 180)
        elif side == 'T': at = (-hw + G * (rank + 2), top + G, 270)
        else: at = (G * rank, bot - G, 90)
        key = (side, rank)
        hide = key in seen; seen.add(key)
        p = ['pin', typ, 'line', ['at', f(at[0]), f(at[1]), str(at[2])], ['length', f(G)]]
        if hide: p.append(['hide', 'yes'])
        p += [['name', S(nm), fx()], ['number', S(num), fx()]]
        pins.append(p)
    props = [['property', S('Reference'), S(spec['ref']), ['at', '0', f(top + 5.08), '0'], fx()],
             ['property', S('Value'), S(name), ['at', '0', f(bot - 5.08), '0'], fx()],
             ['property', S('Footprint'), S(fp), ['at', '0', '0', '0'], fx(hide=True)],
             ['property', S('Datasheet'), S(spec['ds']), ['at', '0', '0', '0'], fx(hide=True)],
             ['property', S('Description'), S(spec['desc']), ['at', '0', '0', '0'], fx(hide=True)]]
    return (['symbol', S(name), ['pin_names', ['offset', '1.016']], ['exclude_from_sim', 'no'],
             ['in_bom', 'yes'], ['on_board', 'yes']] + props +
            [['symbol', S(name + '_0_1'), ['rectangle', ['start', f(-hw), f(top)], ['end', f(hw), f(bot)],
                                           ['stroke', ['width', '0.254'], ['type', 'default']],
                                           ['fill', ['type', 'background']]]],
             ['symbol', S(name + '_1_1')] + pins, ['embedded_fonts', 'no']])

CUSTOM_FP = {}
for c in M.COMPONENTS:
    if c['lib'].startswith('n2:'): CUSTOM_FP[c['lib'][3:]] = c['fp']
custom_lib = {n: custom_symbol(n, M.CUSTOM[n], CUSTOM_FP[n]) for n in M.CUSTOM}

# ---------- bibliothèque embarquée ----------
libsyms, pinmap = [], {}
def need(lid):
    if lid in pinmap: return
    l, s = lid.split(':')
    sy = copy.deepcopy(custom_lib[s]) if l == 'n2' else flat_sym(l, s)
    pinmap[lid] = pins_of(sy)
    sy[1] = S(lid); libsyms.append(sy)
for lid in ['Connector_Generic:Conn_02x02_Odd_Even', 'Connector_Generic:Conn_02x03_Odd_Even',
            'Connector_Generic:Conn_01x01', 'Device:Polyfuse', 'Diode:SMAJ16A']:
    need(lid)

items = []
DIRS = {0: (-1, 0), 180: (1, 0), 90: (0, 1), 270: (0, -1)}

def place(lid, ref, val, fp, x, y, nets, extra=None, side=False, unit=1, inbom=True, onboard=True,
          ref_at=None, val_at=None, hide_val=False):
    """nets: broche -> net ; un fil de 2,54 mm et un label par broche ('NC' = drapeau no-connect)."""
    need(lid)
    if ref_at is None:
        if side: ref_at, val_at, jj = (x + G, y - 1.27), (x + G, y + 1.27), ['left']
        else: ref_at, val_at, jj = (x, y - 7.62), (x, y + 10.16), None
    else: jj = None
    hidden = ref.startswith('#')
    props = [['property', S('Reference'), S(ref), ['at', f(ref_at[0]), f(ref_at[1]), '0'], fx(j=jj, hide=hidden)],
             ['property', S('Value'), S(val), ['at', f(val_at[0]), f(val_at[1]), '0'], fx(j=jj, hide=hidden or hide_val)],
             ['property', S('Footprint'), S(fp), ['at', f(x), f(y), '0'], fx(hide=True)],
             ['property', S('Datasheet'), S(''), ['at', f(x), f(y), '0'], fx(hide=True)]]
    for k, v in (extra or {}).items():
        props.append(['property', S(k), S(v), ['at', f(x), f(y), '0'], fx(hide=True)])
    upins = [p for p, d in pinmap[lid].items() if d[3] in (0, unit)]
    items.append(['symbol', ['lib_id', S(lid)], ['at', f(x), f(y), '0'], ['unit', str(unit)],
                  ['exclude_from_sim', 'no'], ['in_bom', 'yes' if inbom else 'no'],
                  ['on_board', 'yes' if onboard else 'no'], ['dnp', 'no'],
                  ['uuid', U('sym/' + ref) if unit == 1 else U('sym/%s/%d' % (ref, unit))]] + props +
                 [['pin', S(p), ['uuid', U('pin/%s/%s' % (ref, p))]] for p in upins] +
                 [['instances', ['project', S(NAME), ['path', S('/' + ROOT), ['reference', S(ref)], ['unit', str(unit)]]]]])
    done = set()
    for p, net in nets.items():
        if p not in upins: continue
        px, py, a, _ = pinmap[lid][p]
        sx, sy = x + px, y - py
        if (sx, sy) in done: continue
        done.add((sx, sy))
        if net == 'NC':
            items.append(['no_connect', ['at', f(sx), f(sy)], ['uuid', U('nc/%s/%s' % (ref, p))]])
            continue
        d = DIRS[a]
        ex, ey = sx + d[0] * G, sy + d[1] * G
        items.append(['wire', ['pts', ['xy', f(sx), f(sy)], ['xy', f(ex), f(ey)]],
                      ['stroke', ['width', '0'], ['type', 'default']], ['uuid', U('w/%s/%s' % (ref, p))]])
        ang, just = {(-1, 0): ('180', ['right', 'bottom']), (1, 0): ('0', ['left', 'bottom']),
                     (0, 1): ('270', ['right', 'bottom']), (0, -1): ('90', ['left', 'bottom'])}[d]
        items.append(['label', S(net), ['at', f(ex), f(ey), ang], fx(j=just), ['uuid', U('l/%s/%s' % (ref, p))]])

def text(t, x, y, size=1.27, n=None, bold=False):
    font = ['font', ['size', f(size), f(size)]] + ([['bold', 'yes']] if bold else [])
    items.append(['text', S(t), ['exclude_from_sim', 'no'], ['at', f(x), f(y), '0'],
                  ['effects', font, ['justify', 'left', 'bottom']], ['uuid', U('t/' + (n or t[:40]))]])

# ================= Partie bus (révision A, inchangée) =================
MPC = 'Connector_Molex:Molex_Micro-Fit_3.0_43045-0412_2x02_P3.00mm_Vertical'
MPCX = {'Manufacturer': 'Würth Elektronik', 'MPN': '66200421122 (WR-MPC3 2x2, vertical, THT)'}
place('Connector_Generic:Conn_02x02_Odd_Even', 'J1', 'BP J31 (WR-MPC3 2x2)', MPC, 40.64, 63.5,
      {'1': 'SDA_BP', '2': 'SCL_BP', '3': '0V', '4': '+15V_BP'}, MPCX)
place('Connector_Generic:Conn_01x01', 'J8', 'Queue de cochon AWG20 -> J31-3 du BP',
      'Connector_Wire:SolderWire-0.75sqmm_1x01_D1.25mm_OD3.5mm', 40.64, 96.52, {'1': '0V'},
      {'Note': 'Pastille de soudure, pas de composant'}, inbom=False)
text('ENTRÉE BP (J31)\nBrochage : 1 SDA, 2 SCL, 3 0 V, 4 +15 V', 25.4, 48.26, n='bp')
text('Queue de cochon AWG20 <= 15 cm\nsoudée sur la broche traversante J31-3 du BP\n« 0 V TBTS – NE PAS RELIER À LA TERRE »', 25.4, 83.82, n='qdc')

BAS = [('SC940D', 'BA1'), ('SC941C', 'BA2'), ('SC942C', 'BA3')]
for k, (carte, ba) in enumerate(BAS, start=1):
    X0 = 101.6 + (k - 1) * 101.6
    text('PORT %s : %s (J1 et J2 du BA)' % (ba, carte), X0 - 7.62, 38.1, 1.8, n='port%d' % k)
    place('Connector_Generic:Conn_02x03_Odd_Even', 'JP%d' % k, 'Cavalier port %s (3 shunts)' % ba,
          'Connector_PinHeader_2.54mm:PinHeader_2x03_P2.54mm_Vertical', X0, 58.42,
          {'1': 'SDA_BP', '2': 'SDA_%s' % ba, '3': 'SCL_BP', '4': 'SCL_%s' % ba,
           '5': '+15V_BP', '6': '+15V_SW%d' % k},
          {'Note': 'Retirer les 3 shunts (ou le pont 2x3) = BA isolé du bus, 0 V conservé'})
    place('Device:Polyfuse', 'F%d' % k, 'PTC >=30 V 1812', 'Fuse:Fuse_1812_4532Metric', X0 + 40.64, 63.5,
          {'1': '+15V_SW%d' % k, '2': '+15V_%s' % ba},
          {'Manufacturer': 'Littelfuse', 'MPN': '1812L050/30MR (à confirmer : Ihold@50°C >= 1,5 x I BA_ALIM)'}, side=True)
    place('Diode:SMAJ16A', 'D%d' % k, 'SMAJ16A', 'Diode_SMD:D_SMA', X0 + 40.64, 88.9,
          {'1': '+15V_%s' % ba, '2': '0V'}, {'Manufacturer': 'Littelfuse', 'MPN': 'SMAJ16A'})
    for j, (ref, jn) in enumerate([('J%d' % (2 * k), 'J1'), ('J%d' % (2 * k + 1), 'J2')]):
        place('Connector_Generic:Conn_02x02_Odd_Even', ref, '%s %s (WR-MPC3 2x2)' % (ba, jn), MPC,
              X0 + j * 48.26, 111.76,
              {'1': 'SDA_%s' % ba, '2': 'SCL_%s' % ba, '3': '0V', '4': '+15V_%s' % ba}, MPCX)

text('N2 « étoile 0 V » – rév. B : carte d\'étoile sur rail DIN + module ESP32 espion/alarme\n'
     'Partie bus (change i2c-surge-protection-2026-10-007, Refs #1) : SDA/SCL/+15 V/0 V du BP distribués vers chaque port BA ;\n'
     'par port : cavalier (SDA, SCL, +15 V), PTC puis TVS SMAJ16A. Le 0 V n\'est jamais coupé et n\'est JAMAIS relié au PE.\n'
     'Module (change n2-esp32-spy-alarm-2026-10-008, essensys-hub/essensys-feature-lifecycle#28) : blocs A à F ci-dessous.\n'
     'Masse du module GND reliée au 0 V en un seul point (NT1). Écoute strictement passive : aucun canal module → bus.',
     25.4, 33.0, 1.6, n='titre')

# ================= Module ESP32 (révision B) =================
def label_len(net): return 1.15 * len(net) + 1.0

def unit_bbox(lid, unit, nets, val):
    xs, ys = [-2.54, 2.54], [-2.54, 2.54]
    for p, (px, py, a, u) in pinmap[lid].items():
        if u not in (0, unit): continue
        d = DIRS[a]
        sx, sy = px, -py
        ex, ey = sx + d[0] * G, sy + d[1] * G
        net = nets.get(p)
        L = label_len(net) if net and net != 'NC' else 0
        xs += [sx, ex + d[0] * L]; ys += [sy, ey + d[1] * L]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    vw = 1.1 * max(len(val), 4) / 2
    x0, x1 = min(x0, -vw), max(x1, vw)
    return x0, x1, y0 - 3.81, y1 + 3.81

def snap(v): return round(v / G) * G

BLOCK_W = 255.0
blocks = {}
for c in M.COMPONENTS:
    blocks.setdefault(c['block'], []).append(c)

def layout_block(bk, X0, Y0):
    text(M.BLOCK_TITLES[bk], X0, Y0, 1.8, n='blk' + bk, bold=True)
    cx, cy, rowh = X0, Y0 + 7.62, 0
    for c in blocks[bk]:
        lid = c['lib']; need(lid)
        units = sorted({d[3] for d in pinmap[lid].values() if d[3] > 0}) or [1]
        for u in units:
            x0, x1, y0, y1 = unit_bbox(lid, u, c['pins'], c['value'])
            w, h = x1 - x0 + 3.81, y1 - y0 + 2.54
            if cx + w > X0 + BLOCK_W and cx > X0:
                cx, cy, rowh = X0, cy + rowh, 0
            ox, oy = snap(cx - x0 + 1.27), snap(cy - y0 + 1.27)
            flagged = c.get('flag')
            place(lid, c['ref'], c['value'], c['fp'], ox, oy, c['pins'], c['fields'], unit=u,
                  inbom=(not flagged) and c.get('inbom', True), onboard=not flagged,
                  ref_at=(ox, oy + y0 + 1.9), val_at=(ox, oy + y1 - 1.0), hide_val=(u > 1))
            cx += w; rowh = max(rowh, h)
    return cy + rowh

y = 140.0
bottom = y
for row in (('A', 'B', 'C'), ('D', 'E', 'F')):
    ends = []
    for i, bk in enumerate(row):
        ends.append(layout_block(bk, 25.4 + i * (BLOCK_W + 10.16), y))
    y = snap(max(ends) + 15.24)
paper = 'A1' if y < 580 else 'A0'

sch = ['kicad_sch', ['version', '20250114'], ['generator', S('eeschema')], ['generator_version', S('9.0')],
       ['uuid', ROOT], ['paper', S(paper)],
       ['title_block', ['title', S('N2 étoile 0 V + module ESP32 espion/alarme')], ['date', S('2026-10-12')], ['rev', S('B')],
        ['company', S('Essensys – essensys-i2c-etoile-0v')],
        ['comment', '1', S('Feature: n2-esp32-spy-alarm-2026-10-008 – Refs essensys-hub/essensys-feature-lifecycle#28')],
        ['comment', '2', S('Partie bus : i2c-surge-protection-2026-10-007 – Refs essensys-hub/essensys-board-SC944D#1')],
        ['comment', '3', S('Brochage WR-MPC3 : 1 SDA, 2 SCL, 3 0 V, 4 +15 V ; 0 V et GND jamais reliés au PE')]],
       ['lib_symbols'] + libsyms] + items + [['sheet_instances', ['path', S('/'), ['page', S('1')]]],
       ['embedded_fonts', 'no']]
open(os.path.join(OUT, NAME + '.kicad_sch'), 'w').write(dump(sch) + '\n')

# bibliothèque de projet (symboles propres) + table
symlib = ['kicad_symbol_lib', ['version', '20241209'], ['generator', S('kicad_symbol_editor')],
          ['generator_version', S('9.0')]] + [custom_lib[n] for n in sorted(custom_lib)]
open(os.path.join(OUT, NAME + '.kicad_sym'), 'w').write(dump(symlib) + '\n')
symlibs = sorted({lid.split(':')[0] for lid in pinmap} - {'n2'})
fplibs = sorted({c['fp'].split(':')[0] for c in M.COMPONENTS if c['fp']} |
                {'Connector_Molex', 'Connector_Wire', 'Connector_PinHeader_2.54mm', 'Fuse', 'Diode_SMD', 'MountingHole'})
row = '\t(lib (name "%s")(type "KiCad")(uri "%s")(options "")(descr "%s"))\n'
open(os.path.join(OUT, 'sym-lib-table'), 'w').write(
    '(sym_lib_table\n\t(version 7)\n' + row % ('n2', '${KIPRJMOD}/%s.kicad_sym' % NAME, 'Symboles propres N2') +
    ''.join(row % (l, '${KICAD9_SYMBOL_DIR}/%s.kicad_sym' % l, '') for l in symlibs) + ')\n')
open(os.path.join(OUT, 'fp-lib-table'), 'w').write(
    '(fp_lib_table\n\t(version 7)\n' +
    ''.join(row % (l, '${KICAD9_FOOTPRINT_DIR}/%s.pretty' % l, '') for l in fplibs) + ')\n')
print('ok', paper, len(M.COMPONENTS), 'composants module')

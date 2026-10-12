#!/usr/bin/env python3
"""Génère n2-etoile-0v.kicad_sch (KiCad 9) à partir des bibliothèques standard.
Usage : python3 gen_sch.py <dossier_sortie>"""
import sys, os, uuid, copy

KSYM = '/Applications/KiCad/KiCad.app/Contents/SharedSupport/symbols/'
OUT = sys.argv[1] if len(sys.argv) > 1 else '.'
NAME = 'n2-etoile-0v'
U = lambda s: str(uuid.uuid5(uuid.NAMESPACE_URL, 'n2-etoile-0v/' + s))
ROOT = U('root')

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
    res = {}
    def walk(x):
        for e in x:
            if isinstance(e, list):
                if e[0] == 'pin':
                    at = [a for a in e if isinstance(a, list) and a[0] == 'at'][0]
                    num = [a for a in e if isinstance(a, list) and a[0] == 'number'][0][1][1]
                    res[num] = (float(at[1]), float(at[2]), int(float(at[3])))
                else: walk(e)
    walk(sym); return res

SYMS = {
    'Connector_Generic:Conn_02x02_Odd_Even': ('Connector_Generic', 'Conn_02x02_Odd_Even'),
    'Connector_Generic:Conn_02x03_Odd_Even': ('Connector_Generic', 'Conn_02x03_Odd_Even'),
    'Connector_Generic:Conn_01x01': ('Connector_Generic', 'Conn_01x01'),
    'Device:Polyfuse': ('Device', 'Polyfuse'),
    'Diode:SMAJ16A': ('Diode', 'SMAJ16A'),
}
libsyms, pinmap = [], {}
for lid, (l, s) in SYMS.items():
    sy = flat_sym(l, s); pinmap[lid] = pins_of(sy)
    sy[1] = S(lid); libsyms.append(sy)

items = []
def fnt(h=False, j=None):
    e = ['effects', ['font', ['size', '1.27', '1.27']]]
    if j: e.append(['justify'] + j)
    if h: e.append(['hide', 'yes'])
    return e
def f(v): return ('%.2f' % v).rstrip('0').rstrip('.')

def place(lid, ref, val, fp, x, y, nets, extra=None, side=False):
    """nets: dict pin -> net name ; pose un fil de 2,54 mm et un label par broche."""
    if side:  # référence et valeur à droite (symboles verticaux)
        ra, va, jj = (x + 2.54, y - 1.27), (x + 2.54, y + 1.27), ['left']
    else:
        ra, va, jj = (x, y - 7.62), (x, y + 10.16), None
    props = [['property', S('Reference'), S(ref), ['at', f(ra[0]), f(ra[1]), '0'], fnt(j=jj)],
             ['property', S('Value'), S(val), ['at', f(va[0]), f(va[1]), '0'], fnt(j=jj)],
             ['property', S('Footprint'), S(fp), ['at', f(x), f(y), '0'], fnt(True)],
             ['property', S('Datasheet'), S(''), ['at', f(x), f(y), '0'], fnt(True)]]
    for k, v in (extra or {}).items():
        props.append(['property', S(k), S(v), ['at', f(x), f(y), '0'], fnt(True)])
    items.append(['symbol', ['lib_id', S(lid)], ['at', f(x), f(y), '0'], ['unit', '1'],
                  ['exclude_from_sim', 'no'], ['in_bom', 'yes'], ['on_board', 'yes'], ['dnp', 'no'],
                  ['uuid', U('sym/' + ref)]] + props +
                 [['pin', S(p), ['uuid', U('pin/%s/%s' % (ref, p))]] for p in pinmap[lid]] +
                 [['instances', ['project', S(NAME), ['path', S('/' + ROOT), ['reference', S(ref)], ['unit', '1']]]]])
    for p, net in nets.items():
        px, py, a = pinmap[lid][p]
        sx, sy = x + px, y - py
        d = {0: (-1, 0), 180: (1, 0), 90: (0, 1), 270: (0, -1)}[a]
        ex, ey = sx + d[0] * 2.54, sy + d[1] * 2.54
        items.append(['wire', ['pts', ['xy', f(sx), f(sy)], ['xy', f(ex), f(ey)]],
                      ['stroke', ['width', '0'], ['type', 'default']], ['uuid', U('w/%s/%s' % (ref, p))]])
        ang, just = {(-1, 0): ('180', ['right', 'bottom']), (1, 0): ('0', ['left', 'bottom']),
                     (0, 1): ('270', ['right', 'bottom']), (0, -1): ('90', ['left', 'bottom'])}[d]
        items.append(['label', S(net), ['at', f(ex), f(ey), ang], fnt(j=just), ['uuid', U('l/%s/%s' % (ref, p))]])

def text(t, x, y, size=1.27, n=None):
    items.append(['text', S(t), ['exclude_from_sim', 'no'], ['at', f(x), f(y), '0'],
                  ['effects', ['font', ['size', f(size), f(size)]], ['justify', 'left', 'bottom']],
                  ['uuid', U('t/' + (n or t[:40]))]])

MPC = 'Connector_Molex:Molex_Micro-Fit_3.0_43045-0412_2x02_P3.00mm_Vertical'
MPCX = {'Manufacturer': 'Würth Elektronik', 'MPN': '66200421122 (WR-MPC3 2x2, vertical, THT)'}
# --- Entrée BP ---
place('Connector_Generic:Conn_02x02_Odd_Even', 'J1', 'BP J31 (WR-MPC3 2x2)', MPC, 40.64, 63.5,
      {'1': 'SDA_BP', '2': 'SCL_BP', '3': '0V', '4': '+15V_BP'}, MPCX)
place('Connector_Generic:Conn_01x01', 'J8', 'Queue de cochon AWG20 -> J31-3 du BP',
      'Connector_Wire:SolderWire-0.75sqmm_1x01_D1.25mm_OD3.5mm', 40.64, 96.52, {'1': '0V'},
      {'Note': 'Pastille de soudure, pas de composant'})
for it in items:  # pastille : hors BOM (comme l'empreinte)
    if it[0] == 'symbol' and ['uuid', U('sym/J8')] in it: it[it.index(['in_bom', 'yes'])] = ['in_bom', 'no']
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

text('N2 « étoile 0 V » – carte passive sur rail DIN (change OpenSpec i2c-surge-protection-2026-10-007, Refs #1)\n'
     'Bus en étoile : SDA/SCL/+15 V/0 V du BP distribués vers chaque port BA. Par port : cavalier (SDA, SCL, +15 V), PTC puis TVS SMAJ16A sur le +15 V.\n'
     'Le 0 V n\'est jamais coupé et n\'est JAMAIS relié au PE. Plan 0 V double face, cuivre 2 oz.', 25.4, 22.86, 1.6, n='titre')

sch = ['kicad_sch', ['version', '20250114'], ['generator', S('eeschema')], ['generator_version', S('9.0')],
       ['uuid', ROOT], ['paper', S('A3')],
       ['title_block', ['title', S('N2 étoile 0 V')], ['date', S('2026-10-12')], ['rev', S('A')],
        ['company', S('Essensys – essensys-i2c-etoile-0v')],
        ['comment', '1', S('Feature: i2c-surge-protection-2026-10-007 – Refs #1')],
        ['comment', '2', S('Brochage WR-MPC3 : 1 SDA, 2 SCL, 3 0 V, 4 +15 V')]],
       ['lib_symbols'] + libsyms] + items + [['sheet_instances', ['path', S('/'), ['page', S('1')]]],
       ['embedded_fonts', 'no']]
open(os.path.join(OUT, NAME + '.kicad_sch'), 'w').write(dump(sch) + '\n')
print('ok')

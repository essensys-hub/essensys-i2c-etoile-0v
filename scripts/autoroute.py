#!/usr/bin/env python3
"""Routeur maillé déterministe pour la carte N2 (KiCad 9, API pcbnew + numpy).

Principe : grille de 0,125 mm sur les deux couches. Le cuivre de chaque net est rasterisé dans des
cartes de comptage gonflées de (isolement + demi-largeur de piste) ; une cellule est libre pour un net
si seul son propre cuivre la couvre. A* 8 directions (diagonale autorisée seulement si les deux
cellules orthogonales voisines sont libres), via = changement de couche. Les nets sont reliés
composante par composante (la plus proche d'abord). Les nets de masse (0V, GND) ne sont pas routés :
ils passent par les plans, avec une via de descente près de chaque pastille CMS et des vias de couture.
Plusieurs passes réordonnent les nets en échec (arrachage complet puis reroutage).
"""
import heapq, math
import numpy as np
import pcbnew
from pcbnew import VECTOR2I, FromMM as M, ToMM

RES = 0.125
CLR = 0.2
EPS = 0.03
EDGE = 0.5
VIA_D, VIA_DR = 0.6, 0.3
WIDTHS = (0.25, 0.4, 0.6)         # classes de largeur
HOLE_SEP = 0.95                   # entraxe mini entre trous de vias (0,3 + 0,25 + marge)
DIRS = [(0, 1, 1.0), (1, 0, 1.0), (0, -1, 1.0), (-1, 0, 1.0),
        (1, 1, 1.4142), (1, -1, 1.4142), (-1, 1, 1.4142), (-1, -1, 1.4142)]
VIA_COST = 10.0
SOFT_COST = 12.0
BOTTOM_COST = 1.25
LAYERS = (pcbnew.F_Cu, pcbnew.B_Cu)


def margin(w): return CLR + w / 2 + EPS


class Grid:
    def __init__(self, W, H):
        self.nx = int(round(W / RES)) + 1
        self.ny = int(round(H / RES)) + 1
        self.W, self.H = W, H


# ------------------------------------------------------------------ formes
class Shape:
    """Forme de cuivre : 'rect' (cx, cy, hx, hy, ang, r), 'circle' (cx, cy, r), 'seg' (x1, y1, x2, y2, hw)."""
    def __init__(self, kind, params, layers, net, pad=False, routed=False):
        self.kind, self.p, self.layers, self.net, self.pad, self.routed = kind, params, layers, net, pad, routed

    def bbox(self, m):
        k, p = self.kind, self.p
        if k == 'rect':
            cx, cy, hx, hy, ang, r = p
            R = math.hypot(hx, hy)
            return cx - R - m, cy - R - m, cx + R + m, cy + R + m
        if k == 'circle':
            cx, cy, r = p
            return cx - r - m, cy - r - m, cx + r + m, cy + r + m
        x1, y1, x2, y2, hw = p
        return min(x1, x2) - hw - m, min(y1, y2) - hw - m, max(x1, x2) + hw + m, max(y1, y2) + hw + m

    def dist(self, X, Y):
        k, p = self.kind, self.p
        if k == 'circle':
            return np.hypot(X - p[0], Y - p[1]) - p[2]
        if k == 'seg':
            x1, y1, x2, y2, hw = p
            dx, dy = x2 - x1, y2 - y1
            L2 = dx * dx + dy * dy
            if L2 < 1e-12:
                return np.hypot(X - x1, Y - y1) - hw
            t = np.clip(((X - x1) * dx + (Y - y1) * dy) / L2, 0, 1)
            return np.hypot(X - x1 - t * dx, Y - y1 - t * dy) - hw
        cx, cy, hx, hy, ang, r = p
        c, s = math.cos(ang), math.sin(ang)
        u = (X - cx) * c + (Y - cy) * s
        v = -(X - cx) * s + (Y - cy) * c
        ax, ay = hx - r, hy - r
        du, dv = np.abs(u) - ax, np.abs(v) - ay
        out = np.hypot(np.maximum(du, 0), np.maximum(dv, 0))
        ins = np.minimum(np.maximum(du, dv), 0)
        return out + ins - r


def pad_shapes(pad, netname):
    pos = pad.GetPosition(); cx, cy = ToMM(pos.x), ToMM(pos.y)
    sz = pad.GetSize(); hx, hy = ToMM(sz.x) / 2, ToMM(sz.y) / 2
    ang = -math.radians(pad.GetOrientation().AsDegrees())
    layers = [L for L in LAYERS if pad.IsOnLayer(L)]
    sh = pad.GetShape()
    out = []
    if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
        d = ToMM(pad.GetDrillSize().x) / 2
        return [Shape('circle', (cx, cy, d + 0.05), list(LAYERS), None)]
    out = _pad_raw(pad, netname, cx, cy, hx, hy, ang, layers, sh)
    for o in out: o.pad = True
    return out


def _pad_raw(pad, netname, cx, cy, hx, hy, ang, layers, sh):
    out = []
    if sh == pcbnew.PAD_SHAPE_CIRCLE:
        out.append(Shape('circle', (cx, cy, hx), layers, netname))
    elif sh == pcbnew.PAD_SHAPE_OVAL:
        if hx >= hy:
            dx, dy = (hx - hy) * math.cos(-ang), (hx - hy) * math.sin(-ang)
            out.append(Shape('seg', (cx - dx, cy - dy, cx + dx, cy + dy, hy), layers, netname))
        else:
            dx, dy = -(hy - hx) * math.sin(-ang), (hy - hx) * math.cos(-ang)
            out.append(Shape('seg', (cx - dx, cy - dy, cx + dx, cy + dy, hx), layers, netname))
    else:
        r = 0.0
        if sh == pcbnew.PAD_SHAPE_ROUNDRECT:
            r = ToMM(pad.GetRoundRectCornerRadius(pcbnew.F_Cu))
        out.append(Shape('rect', (cx, cy, hx, hy, ang, min(r, hx, hy)), layers, netname))
    if pad.HasHole() and pad.GetAttribute() == pcbnew.PAD_ATTRIB_PTH:
        d = ToMM(max(pad.GetDrillSize().x, pad.GetDrillSize().y)) / 2
        out.append(Shape('circle', (cx, cy, d), list(LAYERS), netname))
    return out


def track_shape(t, netname):
    if t.GetClass() == 'PCB_VIA':
        p = t.GetPosition()
        return Shape('circle', (ToMM(p.x), ToMM(p.y), ToMM(t.GetWidth(pcbnew.F_Cu)) / 2), list(LAYERS), netname)
    s, e = t.GetStart(), t.GetEnd()
    return Shape('seg', (ToMM(s.x), ToMM(s.y), ToMM(e.x), ToMM(e.y), ToMM(t.GetWidth()) / 2), [t.GetLayer()], netname)


def split_seg(sh, maxlen=4.0):
    if sh.kind != 'seg': return [sh]
    x1, y1, x2, y2, hw = sh.p
    L = math.hypot(x2 - x1, y2 - y1)
    n = max(1, int(math.ceil(L / maxlen)))
    return [Shape('seg', (x1 + (x2 - x1) * i / n, y1 + (y2 - y1) * i / n,
                          x1 + (x2 - x1) * (i + 1) / n, y1 + (y2 - y1) * (i + 1) / n, hw), sh.layers, sh.net,
                  routed=sh.routed)
            for i in range(n)]


# ------------------------------------------------------------------ routeur
class Router:
    def __init__(self, board, nets, power, skip, keepouts, size, log=print):
        self.b, self.nets, self.skip, self.log = board, nets, skip, log
        W, H = size
        self.g = Grid(W, H)
        self.power = power
        ny, nx = self.g.ny, self.g.nx
        self.cnt = {w: [np.zeros((ny, nx), np.int16) for _ in LAYERS] for w in WIDTHS}
        self.cntr = {w: [np.zeros((ny, nx), np.int16) for _ in LAYERS] for w in WIDTHS}
        self.robj = {}        # net -> [(objet pcbnew, [shapes], trou)]
        self.holes = np.zeros((ny, nx), np.int16)      # vias / trous existants (gonflés HOLE_SEP)
        self.static = np.zeros((ny, nx), bool)         # bords + zones interdites
        ys = np.arange(ny) * RES; xs = np.arange(nx) * RES
        self.static[:, :] = False
        self.edge = {}
        for w in WIDTHS:
            m = EDGE + w / 2 + EPS
            e = np.zeros((ny, nx), bool)
            e[ys < m, :] = True; e[ys > H - m, :] = True; e[:, xs < m] = True; e[:, xs > W - m] = True
            for (x0, y0, x1, y1) in keepouts:
                e[(ys[:, None] > y0 - m) & (ys[:, None] < y1 + m) & (xs[None, :] > x0 - m) & (xs[None, :] < x1 + m)] = True
            self.edge[w] = e
        self.items = {}       # net -> liste de Shape
        self.added = []       # objets pcbnew ajoutés par le routeur
        self.stitch = []      # vias de couture (supprimables si isolées)
        self.touched = set()  # nets arrachés partiellement (restes à nettoyer)
        self.drops = {}       # pastille de masse -> via de descente
        self.load()

    # -------- rasterisation
    def window(self, sh, m):
        x0, y0, x1, y1 = sh.bbox(m)
        j0, j1 = max(0, int(math.floor(x0 / RES))), min(self.g.nx - 1, int(math.ceil(x1 / RES)))
        i0, i1 = max(0, int(math.floor(y0 / RES))), min(self.g.ny - 1, int(math.ceil(y1 / RES)))
        if j1 < j0 or i1 < i0: return None
        X = np.arange(j0, j1 + 1) * RES; Y = np.arange(i0, i1 + 1) * RES
        return i0, i1, j0, j1, sh.dist(X[None, :], Y[:, None])

    def stamp(self, sh, sign=1):
        mmax = margin(max(WIDTHS))
        win = self.window(sh, mmax)
        if win is None: return
        i0, i1, j0, j1, d = win
        cm = self.cntr if sh.routed else self.cnt
        for w in WIDTHS:
            mk = (d < margin(w)).astype(np.int16) * sign
            for li, L in enumerate(LAYERS):
                if L in sh.layers:
                    cm[w][li][i0:i1 + 1, j0:j1 + 1] += mk

    def stamp_hole(self, x, y, sign=1):
        sh = Shape('circle', (x, y, 0.0), list(LAYERS), None)
        win = self.window(sh, HOLE_SEP)
        if win is None: return
        i0, i1, j0, j1, d = win
        self.holes[i0:i1 + 1, j0:j1 + 1] += (d < HOLE_SEP).astype(np.int16) * sign

    def add_shape(self, sh):
        out = split_seg(sh)
        for s in out:
            self.items.setdefault(s.net, []).append(s)
            self.stamp(s)
        return out

    def add_routed(self, net, obj, sh, hole=None):
        sh.routed = True
        shapes = self.add_shape(sh)
        if hole: self.stamp_hole(*hole)
        self.robj.setdefault(net, []).append((obj, shapes, hole))

    def rip(self, net):
        for obj, shapes, hole in self.robj.pop(net, []):
            self.b.Remove(obj)
            if obj in self.added: self.added.remove(obj)
            for s in shapes:
                self.stamp(s, -1)
                self.items[net].remove(s)
            if hole: self.stamp_hole(hole[0], hole[1], -1)

    def load(self):
        b = self.b
        for f in b.GetFootprints():
            for p in f.Pads():
                n = p.GetNetname() or None
                for sh in pad_shapes(p, n):
                    self.add_shape(sh)
                if p.HasHole():
                    q = p.GetPosition(); self.stamp_hole(ToMM(q.x), ToMM(q.y))
            for gi in f.GraphicalItems():           # cuivre de net-tie, etc.
                if gi.GetLayer() in LAYERS and gi.GetClass() == 'PCB_SHAPE':
                    bb = gi.GetBoundingBox()
                    cx, cy = ToMM(bb.GetCenter().x), ToMM(bb.GetCenter().y)
                    self.add_shape(Shape('rect', (cx, cy, ToMM(bb.GetWidth()) / 2, ToMM(bb.GetHeight()) / 2, 0, 0),
                                         [gi.GetLayer()], None))
        for t in b.GetTracks():
            self.add_shape(track_shape(t, t.GetNetname()))
            if t.GetClass() == 'PCB_VIA':
                q = t.GetPosition(); self.stamp_hole(ToMM(q.x), ToMM(q.y))

    # -------- connectivité d'un net
    def components(self, net):
        shapes = self.items.get(net, [])
        cellmap = {}
        parent = list(range(len(shapes)))
        def find(a):
            while parent[a] != a:
                parent[a] = parent[parent[a]]; a = parent[a]
            return a
        cells = []
        for k, sh in enumerate(shapes):
            win = self.window(sh, 0.0)
            cs = set()
            if win is not None:
                i0, i1, j0, j1, d = win
                ii, jj = np.nonzero(d <= 0.0)
                for li, L in enumerate(LAYERS):
                    if L in sh.layers:
                        for a, c in zip(ii.tolist(), jj.tolist()):
                            key = (li, a + i0, c + j0)
                            cs.add(key)
                            o = cellmap.get(key)
                            if o is None: cellmap[key] = k
                            else:
                                ra, rb = find(o), find(k)
                                if ra != rb: parent[ra] = rb
            cells.append(cs)
        groups = {}
        for k in range(len(shapes)):
            groups.setdefault(find(k), []).append(k)
        comps = []
        for g in groups.values():
            comps.append(dict(shapes=[shapes[k] for k in g]))
        return comps

    def inner_cells(self, comp, w):
        """Cellules franchement à l'intérieur du cuivre (extrémité de piste valide)."""
        out = set()
        for sh in comp['shapes']:
            if sh.kind == 'circle' and sh.p[2] < 0.2: continue
            win = self.window(sh, 0.0)
            if win is None: continue
            i0, i1, j0, j1, d = win
            lim = -0.06 if sh.kind != 'seg' else -(min(0.06, sh.p[4] * 0.5))
            ii, jj = np.nonzero(d <= lim)
            for li, L in enumerate(LAYERS):
                if L in sh.layers:
                    out.update((li, a + i0, c + j0) for a, c in zip(ii.tolist(), jj.tolist()))
        return out

    # -------- A*
    def free_maps(self, net, w, box, soft=False):
        i0, i1, j0, j1 = box
        sl = (slice(i0, i1 + 1), slice(j0, j1 + 1))
        shp = (i1 - i0 + 1, j1 - j0 + 1)
        own = {k: [np.zeros(shp, np.int16) for _ in LAYERS] for k in ('f', 'r')}
        ownv = {k: [np.zeros(shp, np.int16) for _ in LAYERS] for k in ('f', 'r')}
        padv = np.zeros(shp, dtype=bool)
        mv = margin(VIA_D)
        for sh in self.items.get(net, []):
            x0, y0, x1, y1 = sh.bbox(mv)
            if x1 < j0 * RES or x0 > j1 * RES or y1 < i0 * RES or y0 > i1 * RES: continue
            win = self.window(sh, max(margin(w), mv))
            if win is None: continue
            a0, a1, b0, b1, d = win
            A0, A1, B0, B1 = max(a0, i0), min(a1, i1), max(b0, j0), min(b1, j1)
            if A1 < A0 or B1 < B0: continue
            dd = d[A0 - a0:A1 - a0 + 1, B0 - b0:B1 - b0 + 1]
            k = 'r' if sh.routed else 'f'
            for li, L in enumerate(LAYERS):
                if L in sh.layers:
                    own[k][li][A0 - i0:A1 - i0 + 1, B0 - j0:B1 - j0 + 1] += (dd < margin(w))
                    ownv[k][li][A0 - i0:A1 - i0 + 1, B0 - j0:B1 - j0 + 1] += (dd < mv)
            if sh.pad:
                padv[A0 - i0:A1 - i0 + 1, B0 - j0:B1 - j0 + 1] |= (dd < mv)
        hard = [((self.cnt[w][li][sl] - own['f'][li]) <= 0) & ~self.edge[w][sl] for li in range(2)]
        rocc = [((self.cntr[w][li][sl] - own['r'][li]) > 0) for li in range(2)]
        vhard = [((self.cnt[VIA_D][li][sl] - ownv['f'][li]) <= 0) for li in range(2)]
        vrocc = [((self.cntr[VIA_D][li][sl] - ownv['r'][li]) > 0) for li in range(2)]
        via_ok = vhard[0] & vhard[1] & ~padv & (self.holes[sl] <= 0) & ~self.edge[VIA_D][sl]
        if soft:
            return hard, via_ok, [rocc[0], rocc[1]], (vrocc[0] | vrocc[1])
        free = [hard[li] & ~rocc[li] for li in range(2)]
        via_ok = via_ok & ~vrocc[0] & ~vrocc[1]
        return free, via_ok

    def astar(self, net, w, src, dst, box, soft=False):
        i0, i1, j0, j1 = box
        h, wd = i1 - i0 + 1, j1 - j0 + 1
        if soft:
            free, via_ok, rocc, vrocc = self.free_maps(net, w, box, soft=True)
            pb = [rocc[0].tobytes(), rocc[1].tobytes()]
            # une via sur une piste routée d'un autre net : autorisée mais pénalisée
            via_ok = via_ok
            vp = vrocc.tobytes()
        else:
            free, via_ok = self.free_maps(net, w, box)
            pb = None
        fb = [free[0].tobytes(), free[1].tobytes()]
        vb = via_ok.tobytes()
        N = h * wd
        tgt = set()
        tx = []; ty = []
        for (li, a, c) in dst:
            if i0 <= a <= i1 and j0 <= c <= j1:
                k = (a - i0) * wd + (c - j0)
                if fb[li][k]:
                    tgt.add(li * N + k); tx.append(c); ty.append(a)
        if not tgt: return None
        tx0, tx1, ty0, ty1 = min(tx), max(tx), min(ty), max(ty)
        def hfun(a, c):
            dx = max(tx0 - c, 0, c - tx1); dy = max(ty0 - a, 0, a - ty1)
            return max(dx, dy) + 0.4142 * min(dx, dy)
        g = {}
        came = {}
        heap = []
        for (li, a, c) in src:
            if i0 <= a <= i1 and j0 <= c <= j1:
                k = (a - i0) * wd + (c - j0)
                if fb[li][k]:
                    s = li * N + k
                    g[s] = 0.0; came[s] = (-1, -1)
                    heapq.heappush(heap, (hfun(a, c), 0.0, s))
        if not heap: return None
        closed = set()
        found = None
        exp = 0
        while heap:
            f, gc, s = heapq.heappop(heap)
            if s in closed: continue
            closed.add(s)
            if s in tgt:
                found = s; break
            exp += 1
            if exp > 600000: break
            li, k = divmod(s, N)
            a, c = divmod(k, wd)
            pdir = came[s][1]
            lay = fb[li]
            lc = BOTTOM_COST if li == 1 else 1.0
            for di, (da, dc, cost) in enumerate(DIRS):
                na, nc = a + da, c + dc
                if na < 0 or na >= h or nc < 0 or nc >= wd: continue
                nk = na * wd + nc
                if not lay[nk]: continue
                if da and dc and (not lay[a * wd + nc] or not lay[na * wd + c]): continue
                ns = li * N + nk
                if ns in closed: continue
                ng = gc + cost * lc + (0.0 if di == pdir else 0.15)
                if pb is not None and pb[li][nk]: ng += SOFT_COST
                if ng < g.get(ns, 1e18):
                    g[ns] = ng; came[ns] = (s, di)
                    heapq.heappush(heap, (ng + hfun(na, nc), ng, ns))
            if vb[k]:
                ol = 1 - li
                if fb[ol][k]:
                    ns = ol * N + k
                    if ns not in closed:
                        ng = gc + VIA_COST + (SOFT_COST if (pb is not None and vp[k]) else 0.0)
                        if ng < g.get(ns, 1e18):
                            g[ns] = ng; came[ns] = (s, 8)
                            heapq.heappush(heap, (ng + hfun(a, c), ng, ns))
        if found is None: return None
        path = []
        s = found
        while s != -1:
            li, k = divmod(s, N)
            a, c = divmod(k, wd)
            path.append((li, a + i0, c + j0))
            s = came[s][0]
        return path[::-1]

    # -------- création des pistes
    def commit(self, net, w, path):
        ni = self.nets[net]
        pts = []
        def flush(li, pts):
            if len(pts) < 2: return
            # fusion des points alignés
            keep = [pts[0]]
            for k in range(1, len(pts) - 1):
                (a0, c0), (a1, c1), (a2, c2) = keep[-1], pts[k], pts[k + 1]
                if (a1 - a0) * (c2 - c1) == (c1 - c0) * (a2 - a1) and (a1 - a0) * (a2 - a1) >= 0 and (c1 - c0) * (c2 - c1) >= 0:
                    continue
                keep.append(pts[k])
            keep.append(pts[-1])
            for (a0, c0), (a1, c1) in zip(keep, keep[1:]):
                t = pcbnew.PCB_TRACK(self.b)
                t.SetStart(VECTOR2I(M(c0 * RES), M(a0 * RES))); t.SetEnd(VECTOR2I(M(c1 * RES), M(a1 * RES)))
                t.SetWidth(M(w)); t.SetLayer(LAYERS[li]); t.SetNet(ni)
                self.b.Add(t); self.added.append(t)
                self.add_routed(net, t, track_shape(t, net))
        cur = path[0][0]
        pts = [(path[0][1], path[0][2])]
        for (li, a, c) in path[1:]:
            if li != cur:
                flush(cur, pts)
                v = pcbnew.PCB_VIA(self.b)
                v.SetPosition(VECTOR2I(M(c * RES), M(a * RES))); v.SetWidth(M(VIA_D)); v.SetDrill(M(VIA_DR))
                v.SetNet(ni); self.b.Add(v); self.added.append(v)
                self.add_routed(net, v, track_shape(v, net), (c * RES, a * RES))
                cur = li; pts = [(a, c)]
            else:
                pts.append((a, c))
        flush(cur, pts)

    def width_for(self, net):
        n = net.lstrip('/')
        if n == '3V3': return 0.4
        if n in self.power: return 0.6
        return 0.25

    def pick(self, net):
        comps = self.components(net)
        if len(comps) <= 1: return None
        def center(cp):
            xs = [sum(s.bbox(0)[0::2]) / 2 for s in cp['shapes']]; ys = [sum(s.bbox(0)[1::2]) / 2 for s in cp['shapes']]
            return xs, ys
        main = max(range(len(comps)), key=lambda k: len(comps[k]['shapes']))
        mx, my = center(comps[main])
        best, bd = None, 1e9
        for k, cp in enumerate(comps):
            if k == main: continue
            xs, ys = center(cp)
            d = min(math.hypot(x1 - x2, y1 - y2) for x1, y1 in zip(mx, my) for x2, y2 in zip(xs, ys))
            if d < bd: bd, best = d, k
        return comps[best], comps[main]

    def widths(self, net):
        ws = [self.width_for(net)]
        while ws[-1] > WIDTHS[0]:
            ws.append(WIDTHS[WIDTHS.index(ws[-1]) - 1])
        return ws

    def boxes(self, allc):
        for pad_mm in (6.0, 15.0, None):
            if pad_mm is None:
                yield (0, self.g.ny - 1, 0, self.g.nx - 1)
            else:
                p = int(pad_mm / RES)
                yield (max(0, min(c[1] for c in allc) - p), min(self.g.ny - 1, max(c[1] for c in allc) + p),
                       max(0, min(c[2] for c in allc) - p), min(self.g.nx - 1, max(c[2] for c in allc) + p))

    def route_net(self, net):
        for guard in range(200):
            pr = self.pick(net)
            if pr is None: return True
            a, bm = pr
            ok = False
            for w in self.widths(net):
                src = self.inner_cells(a, w); dst = self.inner_cells(bm, w)
                if not src or not dst: continue
                for box in self.boxes(list(src) + list(dst)):
                    path = self.astar(net, w, src, dst, box)
                    if path:
                        self.commit(net, w, path); ok = True; break
                if ok: break
            if not ok:
                return False
        return False

    def soft_victims(self, net):
        """Chemin en traversant les pistes routées des autres nets (pénalisées) ; renvoie les nets à arracher."""
        pr = self.pick(net)
        if pr is None: return set()
        a, bm = pr
        w = self.widths(net)[-1]
        src = self.inner_cells(a, w); dst = self.inner_cells(bm, w)
        if not src or not dst: return None
        return self.victims_between(net, w, src, dst)

    def victims_between(self, net, w, src, dst):
        path = None
        for box in self.boxes(list(src) + list(dst)):
            path = self.astar(net, w, src, dst, box, soft=True)
            if path: break
        if not path: return None
        pts = {0: [], 1: []}
        vias = []
        for k, (li, aa, cc) in enumerate(path):
            pts[li].append((cc * RES, aa * RES))
            if k and path[k - 1][0] != li: vias.append((cc * RES, aa * RES))
        P = {li: (np.array([p[0] for p in v]), np.array([p[1] for p in v])) for li, v in pts.items() if v}
        V = (np.array([p[0] for p in vias]), np.array([p[1] for p in vias])) if vias else None
        victims = {}
        for other, lst in self.robj.items():
            if other == net: continue
            for ent in lst:
                obj, shapes, hole = ent
                hit = False
                for sh in shapes:
                    for li, L in enumerate(LAYERS):
                        if L in sh.layers and li in P:
                            if np.min(sh.dist(P[li][0], P[li][1])) < margin(w): hit = True; break
                    if not hit and V is not None:
                        if np.min(sh.dist(V[0], V[1])) < margin(VIA_D): hit = True
                    if not hit and hole and V is not None:
                        if np.min(np.hypot(V[0] - hole[0], V[1] - hole[1])) < HOLE_SEP: hit = True
                    if hit: break
                if hit: victims.setdefault(other, []).append(ent)
        return victims

    def rip_partial(self, net, ents):
        """Arrache seulement les segments/vias donnés, puis les morceaux de piste devenus orphelins."""
        self.touched.add(net)
        lst = self.robj.get(net, [])
        def drop(ent):
            obj, shapes, hole = ent
            self.b.Remove(obj)
            if obj in self.added: self.added.remove(obj)
            for s_ in shapes:
                self.stamp(s_, -1)
                self.items[net].remove(s_)
            if hole: self.stamp_hole(hole[0], hole[1], -1)
            lst.remove(ent)
        for ent in list(ents):
            if ent in lst: drop(ent)
        owner = {}
        for ent in lst:
            for s_ in ent[1]: owner[id(s_)] = ent
        for c in self.components(net):
            if all(s_.routed for s_ in c['shapes']):
                for ent in {id(owner[id(s_)]): owner[id(s_)] for s_ in c['shapes'] if id(s_) in owner}.values():
                    if ent in lst: drop(ent)

    def prune_dangling(self, net):
        """Supprime les segments/vias routés dont une extrémité ne touche rien (restes d'arrachage)."""
        removed = 0
        while True:
            lst = self.robj.get(net, [])
            gone = None
            for ent in lst:
                obj, shapes, hole = ent
                mine = set(id(x) for x in shapes)
                others = [x for x in self.items.get(net, []) if id(x) not in mine]
                def touches(x, y, li, tol):
                    X, Y = np.array([x]), np.array([y])
                    return any(LAYERS[li] in o.layers and o.dist(X, Y)[0] <= tol for o in others)
                if obj.GetClass() == 'PCB_VIA':
                    q = obj.GetPosition(); x, y = ToMM(q.x), ToMM(q.y)
                    if not (touches(x, y, 0, 0.31) and touches(x, y, 1, 0.31)):
                        gone = ent; break
                else:
                    li = LAYERS.index(obj.GetLayer())
                    ok = True
                    for q in (obj.GetStart(), obj.GetEnd()):
                        if not touches(ToMM(q.x), ToMM(q.y), li, 0.005):
                            ok = False; break
                    if not ok:
                        gone = ent; break
            if gone is None: break
            self.rip_partial(net, [gone]); removed += 1
        return removed

    def rip_all(self):
        for o in self.added:
            self.b.Remove(o)
        self.added = []

    # -------- masse : vias de descente + couture
    def ground_vias(self, gnets, regions):
        n_add = 0
        for net in gnets:
            ni = self.nets[net]
            for f in self.b.GetFootprints():
                for p in f.Pads():
                    if p.GetNetname() != net or p.HasHole(): continue
                    if not p.IsOnLayer(pcbnew.F_Cu): continue
                    key = (f.GetReference(), p.GetNumber(), p.GetPosition().x, p.GetPosition().y)
                    old = self.drops.get(key)
                    if old is not None and any(e[0] is old for e in self.robj.get(net, [])): continue
                    comp = dict(shapes=pad_shapes(p, net))
                    src = self.inner_cells(comp, 0.4)
                    if not src: continue
                    allc = list(src)
                    pdm = int(3.0 / RES)
                    box = (max(0, min(c[1] for c in allc) - pdm), min(self.g.ny - 1, max(c[1] for c in allc) + pdm),
                           max(0, min(c[2] for c in allc) - pdm), min(self.g.nx - 1, max(c[2] for c in allc) + pdm))
                    free, via_ok = self.free_maps(net, 0.4, box)
                    # BFS sur F.Cu depuis la pastille jusqu'à une cellule où une via est possible
                    i0, j0 = box[0], box[2]
                    h, wd = free[0].shape
                    from collections import deque
                    q = deque(); seen = {}
                    for (li, a, c) in src:
                        if li == 0 and free[0][a - i0, c - j0]:
                            q.append((a - i0, c - j0)); seen[(a - i0, c - j0)] = None
                    hit = None
                    while q:
                        a, c = q.popleft()
                        if via_ok[a, c] and (a + i0, c + j0) not in src:
                            hit = (a, c); break
                        for da, dc, _ in DIRS[:4]:
                            na, nc = a + da, c + dc
                            if 0 <= na < h and 0 <= nc < wd and (na, nc) not in seen and free[0][na, nc]:
                                seen[(na, nc)] = (a, c); q.append((na, nc))
                    if hit is None: continue
                    path = []
                    s = hit
                    while s is not None:
                        path.append((0, s[0] + i0, s[1] + j0)); s = seen[s]
                    path = path[::-1] + [(1, hit[0] + i0, hit[1] + j0)]
                    self.commit(net, 0.4, path); n_add += 1
                    self.drops[key] = self.added[-1]
        # vias de couture sur une maille de 4 mm
        for net, (x0, y0, x1, y1) in regions:
            box = (int(y0 / RES), int(y1 / RES), int(x0 / RES), int(x1 / RES))
            free, via_ok = self.free_maps(net, 0.4, box)
            step = int(4.0 / RES)
            for a in range(0, via_ok.shape[0], step):
                for c in range(0, via_ok.shape[1], step):
                    if via_ok[a, c]:
                        # vérifier à nouveau (les vias précédentes modifient les cartes)
                        A, C = a + box[0], c + box[2]
                        if self.holes[A, C] > 0: continue
                        if any(m[VIA_D][li][A, C] > 0 for m in (self.cnt, self.cntr) for li in (0, 1)): continue
                        v = pcbnew.PCB_VIA(self.b)
                        v.SetPosition(VECTOR2I(M(C * RES), M(A * RES))); v.SetWidth(M(VIA_D)); v.SetDrill(M(VIA_DR))
                        v.SetNet(self.nets[net]); self.b.Add(v); self.added.append(v)
                        self.add_routed(net, v, track_shape(v, net), (C * RES, A * RES)); n_add += 1
                        self.stitch.append((net, v))
        return n_add


def _fill(board):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def _ground_clusters(board, net):
    """Composantes connexes d'un net de masse en tenant compte du remplissage des zones.
    Renvoie une liste de listes d'ancres (x, y, indice de couche)."""
    frags = []   # (zone, couche, indice)
    for z in board.Zones():
        if z.GetNetname() != net or z.GetIsRuleArea(): continue
        for li, L in enumerate(LAYERS):
            if not z.IsOnLayer(L): continue
            ps = z.GetFilledPolysList(L)
            ps.BuildBBoxCaches()
            for i in range(ps.OutlineCount()):
                bb = ps.Outline(i).BBox()
                frags.append((ps, li, i, (ToMM(bb.GetX()), ToMM(bb.GetY()), ToMM(bb.GetRight()), ToMM(bb.GetBottom()))))
    anchors = []   # (x, y, [couches], groupe direct)
    for f in board.GetFootprints():
        for p in f.Pads():
            if p.GetNetname() != net: continue
            q = p.GetPosition()
            anchors.append((ToMM(q.x), ToMM(q.y), [li for li, L in enumerate(LAYERS) if p.IsOnLayer(L)], p))
    for t in board.GetTracks():
        if t.GetNetname() != net: continue
        if t.GetClass() == 'PCB_VIA':
            q = t.GetPosition(); anchors.append((ToMM(q.x), ToMM(q.y), [0, 1], t))
        else:
            li = LAYERS.index(t.GetLayer())
            for q in (t.GetStart(), t.GetEnd()):
                anchors.append((ToMM(q.x), ToMM(q.y), [li], t))
    n = len(frags) + len(anchors)
    parent = list(range(n))
    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]; a = parent[a]
        return a
    def union(a, c):
        ra, rc = find(a), find(c)
        if ra != rc: parent[ra] = rc
    byobj = {}
    for k, (x, y, lis, obj) in enumerate(anchors):
        key = id(obj)
        if key in byobj: union(len(frags) + k, byobj[key])
        else: byobj[key] = len(frags) + k
        pt = VECTOR2I(M(x), M(y))
        for fi, (ps, li, i, fb) in enumerate(frags):
            if li in lis and fb[0] <= x <= fb[2] and fb[1] <= y <= fb[3] and ps.Contains(pt, i, 0, True):
                union(fi, len(frags) + k)
    # contacts directs entre pistes/pastilles : extrémités communes
    pos = {}
    for k, (x, y, lis, obj) in enumerate(anchors):
        for li in lis:
            kk = (round(x, 3), round(y, 3), li)
            if kk in pos: union(len(frags) + k, pos[kk])
            else: pos[kk] = len(frags) + k
    groups = {}
    for k, a in enumerate(anchors):
        groups.setdefault(find(len(frags) + k), []).append(a)
    # les fragments vides (sans ancre) ne comptent pas
    return [[(x, y, lis) for (x, y, lis, o) in g] for g in groups.values()]


def repair_ground(R, gnets, rounds=6, log=print):
    """Relie entre elles les composantes d'un plan de masse fragmenté (pistes + vias via le routeur)."""
    b = R.b
    for r in range(rounds):
        _fill(b)
        todo = 0
        for net in gnets:
            cl = _ground_clusters(b, net)
            cl.sort(key=len, reverse=True)
            if len(cl) <= 1: continue
            todo += len(cl) - 1
            def cells(group):
                out = set()
                for (x, y, lis) in group:
                    j, i = int(round(x / RES)), int(round(y / RES))
                    for li in lis:
                        out.add((li, i, j))
                return out
            main = cells(cl[0])
            for g in cl[1:]:
                src = cells(g)
                ok = False
                for w in (0.4, 0.25):
                    for box in R.boxes(list(src) + list(main)):
                        path = R.astar(net, w, src, main, box)
                        if path:
                            R.commit(net, w, path); ok = True; break
                    if ok: break
                if not ok:
                    for w in (0.25,):
                        vic = R.victims_between(net, w, src, main)
                        if vic is None or set(vic) & {'/GND', '/0V'}: continue
                        for v in sorted(vic): R.rip_partial(v, vic[v])
                        for box in R.boxes(list(src) + list(main)):
                            path = R.astar(net, w, src, main, box)
                            if path:
                                R.commit(net, w, path); ok = True; break
                        bad = rrr(R, sorted(vic), log=log)
                        if bad: log('  après arrachage pour la masse, échecs %s' % bad)
                if ok: main |= src
        log('masse, tour %d : %d composantes à relier' % (r + 1, todo), flush=True) if log is print else log('masse, tour %d : %d composantes à relier' % (r + 1, todo))
        if not todo: break
    # vias de couture isolées sur une couche : supprimées
    _fill(b)
    removed = 0
    for net, v in R.stitch:
        q = v.GetPosition()
        ok = 0
        for z in b.Zones():
            if z.GetNetname() != net or z.GetIsRuleArea(): continue
            for L in LAYERS:
                if z.IsOnLayer(L) and z.GetFilledPolysList(L).Contains(q):
                    ok += 1
        if ok < 2:
            b.Remove(v); removed += 1
    if removed: _fill(b)
    log('vias de couture isolées supprimées : %d' % removed)


def rrr(R, order, max_iter=3000, protect=('/GND', '/0V'), log=print, partial=True):
    """Routage avec arrachage ciblé : un net en échec arrache les nets routés qui bloquent son chemin."""
    from collections import deque, Counter
    queue = deque(order)
    rips = Counter(); tries = Counter()
    failed = set(); it = 0
    while queue and it < max_iter:
        it += 1
        n = queue.popleft()
        if R.route_net(n):
            failed.discard(n); continue
        tries[n] += 1
        victims = R.soft_victims(n)
        if not victims or tries[n] > 30 or any(rips[v] > 60 or v in protect for v in victims):
            failed.add(n); continue
        for v in sorted(victims):
            if v in ('/GND', '/0V'):          # masse : on n'arrache que les descentes gênantes
                R.rip_partial(v, victims[v]); continue
            if partial: R.rip_partial(v, victims[v])
            else: R.rip(v)
            rips[v] += 1
            if v not in queue: queue.append(v)
        if not R.route_net(n):
            queue.append(n)
    failed |= set(queue)
    log('  RRR : %d itérations, %d arrachages' % (it, sum(rips.values())))
    return sorted(n for n in failed if len(R.components(n)) > 1)


def route_board(board, nets, power, skip, keepouts, xsplit, size, max_iter=3000, log=print):
    from collections import deque, Counter
    names = sorted(n for n in nets if n not in skip)
    R = Router(board, nets, power, skip, keepouts, size, log)
    todo = [n for n in names if len(R.components(n)) > 1]
    def est(n):
        cs = R.components(n)
        bb = [s.bbox(0) for c in cs for s in c['shapes']]
        return (0 if n.lstrip('/') in power else 1,
                -((max(b[2] for b in bb) - min(b[0] for b in bb)) + (max(b[3] for b in bb) - min(b[1] for b in bb))))
    xb0, xb1 = xsplit
    W, H = size
    nv = R.ground_vias(['/0V', '/GND'], [])
    log('vias de descente de masse : %d' % nv)
    failed = rrr(R, sorted(todo, key=est), partial=False, protect=())
    log('routage : %d nets, échecs %s' % (len(todo), failed))
    for k in range(3):
        if not failed: break
        failed = rrr(R, failed, partial=(k % 2 == 0), protect=())
        log('reprise %d : échecs %s' % (k + 1, failed))
    nv = R.ground_vias(['/0V', '/GND'], [('/GND', (xb1 + 0.5, 0.8, W - 0.8, H - 0.8))])
    log('vias de masse (descentes manquantes + couture) : %d' % nv)
    import time
    left = []
    for k in range(3):
        repair_ground(R, ['/0V', '/GND'], log=log)
        t0 = time.time()
        np_ = sum(R.prune_dangling(n) for n in sorted(R.touched) if n not in ('/0V', '/GND'))
        R.touched = set()
        log('restes de piste supprimés : %d (%.0f s)' % (np_, time.time() - t0))
        left = [n for n in names if len(R.components(n)) > 1]
        if not left: break
        left = rrr(R, left, partial=False, protect=())
        log('nets repris : échecs %s' % left)
    _fill(board)
    gl = {n: len(_ground_clusters(board, n)) for n in ('/0V', '/GND')}
    log('nets non routés : %s ; composantes de masse : %s' % (left, gl))
    return left

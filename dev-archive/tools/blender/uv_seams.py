"""Find the UV islands of a mesh, and every place where one edge of skin is two edges in the atlas.

Shared by seam_measure.py (which measures the step across those places) and hd_hands_paint.py (which cancels it),
so there is exactly one implementation of the awkward part.

The awkward part: a game mesh DUPLICATES its vertices along every UV seam. The two sides of a seam are separate
geometry that happens to sit in the same place, so they never share an edge and the usual "same edge, different
UV" test finds nothing at all (it found 14 islands and 0 seams on Claire, 2026-09-07). Boundary edges have to be
paired up by POSITION instead.
"""
from collections import defaultdict
import numpy as np


def label_islands(me, uvd, vco, in_paint):
    """-> (island_of_face, seams, n_open)

    island_of_face : (n_faces,) int, faces joined only where they share an edge AND agree on its UVs
    seams          : list of (fa, ua0, ua1, fb, ub0, ub1, v0, v1, w0, w1)
                     fa/fb    the two faces, one either side
                     ua*/ub*  each side's own UVs for the shared edge, lined up the same way round in space
                     v0/v1    the edge's vertices (face A's copies)
                     w0/w1    the same edge in face A's WINDING order -- consistent all the way round a boundary
                              loop, which is what lets a measurement keep its sign along a seam that circles a wrist
    n_open         : boundary edges with no partner anywhere in space (genuine holes in the mesh)
    """
    edges = defaultdict(list)
    for poly in me.polygons:
        vi = list(poly.vertices); li = list(poly.loop_indices); n = len(vi)
        for k in range(n):
            ia, ib = vi[k], vi[(k + 1) % n]; la, lb = li[k], li[(k + 1) % n]
            ua = np.array(uvd[la].uv, np.float64); ub = np.array(uvd[lb].uv, np.float64)
            wind = (ia, ib)
            if ia > ib: ia, ib, ua, ub = ib, ia, ub, ua
            edges[(ia, ib)].append((poly.index, ua, ub, wind))

    parent = list(range(len(me.polygons)))

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]; x = parent[x]
        return x

    def union(x, y):
        rx, ry = find(x), find(y)
        if rx != ry: parent[ry] = rx

    seams = []
    loose = defaultdict(list)
    for key, ents in edges.items():
        if len(ents) == 2:
            (fa, ua0, ua1, wa), (fb, ub0, ub1, wb) = ents
            if max(np.abs(ua0 - ub0).max(), np.abs(ua1 - ub1).max()) > 1e-6:
                if in_paint[fa] and in_paint[fb]:
                    seams.append((fa, ua0, ua1, fb, ub0, ub1, key[0], key[1], wa[0], wa[1]))
            else:
                union(fa, fb)
        elif len(ents) == 1:
            f, u0, u1, w0 = ents[0]
            p0 = np.round(vco[key[0]].astype(np.float64), 6); p1 = np.round(vco[key[1]].astype(np.float64), 6)
            loose[tuple(sorted([tuple(p0), tuple(p1)]))].append((f, key[0], key[1], u0, u1, w0))

    n_open = 0
    for pk, ents in loose.items():
        if len(ents) < 2:
            n_open += 1; continue
        for i in range(len(ents)):
            for j in range(i + 1, len(ents)):
                (fa, va0, va1, ua0, ua1, wa) = ents[i]
                (fb, vb0, vb1, ub0, ub1, wb) = ents[j]
                if not (in_paint[fa] and in_paint[fb]): continue
                if not np.allclose(vco[va0], vco[vb0], atol=1e-6):     # line them up the same way round in space
                    vb0, vb1, ub0, ub1 = vb1, vb0, ub1, ub0
                if max(np.abs(ua0 - ub0).max(), np.abs(ua1 - ub1).max()) <= 1e-6:
                    union(fa, fb); continue        # same place AND same UV: split geometry, not a seam
                seams.append((fa, ua0, ua1, fb, ub0, ub1, va0, va1, wa[0], wa[1]))

    island = np.array([find(i) for i in range(len(me.polygons))])
    return island, seams, n_open


def face_frames(me, uvd, vco):
    """-> dict face -> (T, B, N, Tn, Bn, handedness) or None. T = dP/du and B = dP/dv in metres per unit UV, so a
    millimetre of skin can be turned into a UV step, and a tangent-space normal into a world one."""
    out = {}
    for poly in me.polygons:
        vi = list(poly.vertices)
        uvs = {int(v): np.array(uvd[l].uv, np.float64) for v, l in zip(poly.vertices, poly.loop_indices)}
        p0, p1, p2 = (vco[vi[0]].astype(np.float64), vco[vi[1]].astype(np.float64), vco[vi[2]].astype(np.float64))
        w0, w1, w2 = uvs[vi[0]], uvs[vi[1]], uvs[vi[2]]
        e1 = p1 - p0; e2 = p2 - p0; d1 = w1 - w0; d2 = w2 - w0
        det = d1[0] * d2[1] - d2[0] * d1[1]
        n = np.cross(e1, e2); an = np.linalg.norm(n)
        if abs(det) < 1e-14 or an < 1e-14:
            out[poly.index] = None; continue
        r = 1.0 / det
        T = (e1 * d2[1] - e2 * d1[1]) * r
        B = (e2 * d1[0] - e1 * d2[0]) * r
        N = n / an
        Tn = T - N * np.dot(N, T); Tn /= (np.linalg.norm(Tn) + 1e-15)
        hand = 1.0 if np.dot(np.cross(N, Tn), B) >= 0 else -1.0
        Bn = hand * np.cross(N, Tn)
        out[poly.index] = (T, B, N, Tn, Bn, hand)
    return out

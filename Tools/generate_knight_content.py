#!/usr/bin/env python3
"""Generate UV-mapped character meshes, terrain props, textures, and WAV clips."""

from __future__ import annotations

import math
import os
import struct
import wave
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def clamp(v: float, lo: float = 0.0, hi: float = 1.0) -> float:
    return lo if v < lo else hi if v > hi else v


def mix(a, b, t):
    return tuple(a[i] * (1.0 - t) + b[i] * t for i in range(3))


def write_png(path: Path, width: int, height: int, pixels: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = b""
    stride = width * 3
    for y in range(height):
        raw += b"\x00" + pixels[y * stride : (y + 1) * stride]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def noise2(x: int, y: int, seed: int) -> float:
    n = (x * 374761393 + y * 668265263 + seed * 1274126177) & 0xFFFFFFFF
    n = (n ^ (n >> 13)) * 1274126177
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def fill_rect(buf: bytearray, w: int, x0: int, y0: int, x1: int, y1: int, color, seed: int) -> None:
    for y in range(y0, y1):
        for x in range(x0, x1):
            n = noise2(x, y, seed) * 0.18 - 0.09
            r = int(clamp(color[0] + n) * 255)
            g = int(clamp(color[1] + n * 0.9) * 255)
            b = int(clamp(color[2] + n * 0.7) * 255)
            i = (y * w + x) * 3
            buf[i : i + 3] = bytes((r, g, b))


# Atlas 256x256: 8x8 tiles of 32px. UV helper uses tile indices.
ATLAS = 256
TILE = 32


def uv_tile(tx: int, ty: int, u: float, v: float) -> tuple[float, float]:
    return ((tx + u) * TILE / ATLAS, 1.0 - (ty + v) * TILE / ATLAS)


class Mesh:
    def __init__(self) -> None:
        self.v: list[tuple[float, float, float]] = []
        self.vt: list[tuple[float, float]] = []
        self.faces: list[tuple[int, int, int, int, int, int]] = []

    def add_tri(self, p0, p1, p2, uv0, uv1, uv2) -> None:
        i0 = len(self.v)
        self.v.extend((p0, p1, p2))
        self.vt.extend((uv0, uv1, uv2))
        self.faces.append((i0 + 1, i0 + 1, i0 + 2, i0 + 2, i0 + 3, i0 + 3))

    def add_quad(self, p00, p10, p11, p01, uv00, uv10, uv11, uv01) -> None:
        self.add_tri(p00, p10, p11, uv00, uv10, uv11)
        self.add_tri(p00, p11, p01, uv00, uv11, uv01)

    def write(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w") as f:
            f.write("# NOVA3D generated character\n")
            for x, y, z in self.v:
                f.write(f"v {x:.5f} {y:.5f} {z:.5f}\n")
            for u, v in self.vt:
                f.write(f"vt {u:.5f} {v:.5f}\n")
            for a, at, b, bt, c, ct in self.faces:
                f.write(f"f {a}/{at} {b}/{bt} {c}/{ct}\n")


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def add_capsule(mesh: Mesh, a, b, radius: float, segs: int, tx: int, ty: int) -> None:
    ax, ay, az = a
    bx, by, bz = b
    dx, dy, dz = bx - ax, by - ay, bz - az
    length = math.sqrt(dx * dx + dy * dy + dz * dz) or 1.0
    dx, dy, dz = dx / length, dy / length, dz / length
    up = (0.0, 1.0, 0.0) if abs(dy) < 0.9 else (1.0, 0.0, 0.0)
    sx = up[1] * dz - up[2] * dy
    sy = up[2] * dx - up[0] * dz
    sz = up[0] * dy - up[1] * dx
    sl = math.sqrt(sx * sx + sy * sy + sz * sz) or 1.0
    sx, sy, sz = sx / sl, sy / sl, sz / sl
    ux = dy * sz - dz * sy
    uy = dz * sx - dx * sz
    uz = dx * sy - dy * sx
    rings = 8
    pts = []
    uvs = []
    for i in range(rings + 1):
        t = i / rings
        cx, cy, cz = lerp(a, b, t)
        flare = 1.0
        if t < 0.12:
            flare = math.sin((t / 0.12) * math.pi * 0.5)
        elif t > 0.88:
            flare = math.sin(((1.0 - t) / 0.12) * math.pi * 0.5)
        r = radius * max(flare, 0.18)
        ring = []
        uv_ring = []
        for k in range(segs):
            ang = (k / segs) * math.pi * 2.0
            ca, sa = math.cos(ang), math.sin(ang)
            px = cx + (sx * ca + ux * sa) * r
            py = cy + (sy * ca + uy * sa) * r
            pz = cz + (sz * ca + uz * sa) * r
            ring.append((px, py, pz))
            uv_ring.append(uv_tile(tx, ty, k / segs, t))
        pts.append(ring)
        uvs.append(uv_ring)
    for i in range(rings):
        for k in range(segs):
            n = (k + 1) % segs
            mesh.add_quad(
                pts[i][k],
                pts[i][n],
                pts[i + 1][n],
                pts[i + 1][k],
                uvs[i][k],
                uv_tile(tx, ty, (k + 1) / segs, i / rings),
                uv_tile(tx, ty, (k + 1) / segs, (i + 1) / rings),
                uvs[i + 1][k],
            )


def add_sphere(mesh: Mesh, c, radius: float, slices: int, stacks: int, tx: int, ty: int) -> None:
    cx, cy, cz = c
    for i in range(stacks):
        v0 = i / stacks
        v1 = (i + 1) / stacks
        phi0 = (v0 - 0.5) * math.pi
        phi1 = (v1 - 0.5) * math.pi
        for k in range(slices):
            u0 = k / slices
            u1 = (k + 1) / slices
            a0, a1 = u0 * math.pi * 2.0, u1 * math.pi * 2.0

            def sph(phi, ang):
                return (
                    cx + radius * math.cos(phi) * math.sin(ang),
                    cy + radius * math.sin(phi),
                    cz + radius * math.cos(phi) * math.cos(ang),
                )

            mesh.add_quad(
                sph(phi0, a0),
                sph(phi0, a1),
                sph(phi1, a1),
                sph(phi1, a0),
                uv_tile(tx, ty, u0, v0),
                uv_tile(tx, ty, u1, v0),
                uv_tile(tx, ty, u1, v1),
                uv_tile(tx, ty, u0, v1),
            )


def add_box(mesh: Mesh, c, sx, sy, sz, tx: int, ty: int) -> None:
    hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
    cx, cy, cz = c
    p = [
        (cx - hx, cy - hy, cz - hz),
        (cx + hx, cy - hy, cz - hz),
        (cx + hx, cy + hy, cz - hz),
        (cx - hx, cy + hy, cz - hz),
        (cx - hx, cy - hy, cz + hz),
        (cx + hx, cy - hy, cz + hz),
        (cx + hx, cy + hy, cz + hz),
        (cx - hx, cy + hy, cz + hz),
    ]
    faces = (
        (0, 1, 2, 3),
        (5, 4, 7, 6),
        (4, 0, 3, 7),
        (1, 5, 6, 2),
        (3, 2, 6, 7),
        (4, 5, 1, 0),
    )
    for a, b, c2, d in faces:
        mesh.add_quad(
            p[a],
            p[b],
            p[c2],
            p[d],
            uv_tile(tx, ty, 0, 0),
            uv_tile(tx, ty, 1, 0),
            uv_tile(tx, ty, 1, 1),
            uv_tile(tx, ty, 0, 1),
        )


def humanoid(mesh: Mesh, *, knight: bool, armed: bool, bandit: bool) -> None:
    steel, dark, gold, skin = (0, 0), (1, 0), (2, 0), (3, 0)
    leather, cloth, wood, mail = (0, 1), (1, 1), (2, 1), (3, 1)
    body = mail if knight else leather
    add_capsule(mesh, (0.0, 0.12, 0.0), (0.0, 0.92, 0.04), 0.16 if bandit else 0.18, 14, *body)
    add_capsule(mesh, (0.0, 0.92, 0.04), (0.0, 1.28, 0.02), 0.22 if knight else 0.19, 16, *(steel if knight else leather))
    add_sphere(mesh, (0.0, 1.48, 0.02), 0.13, 16, 10, *skin)
    if knight:
        add_sphere(mesh, (0.0, 1.50, 0.02), 0.155, 16, 10, *steel)
        add_box(mesh, (0.0, 1.48, 0.14), 0.16, 0.08, 0.06, *dark)
        add_box(mesh, (0.0, 1.62, 0.0), 0.22, 0.06, 0.22, *gold)
        add_capsule(mesh, (0.0, 0.85, -0.12), (0.0, 1.25, -0.28), 0.07, 10, 1, 1)
    else:
        add_sphere(mesh, (0.0, 1.56, 0.0), 0.18, 14, 8, *cloth)
        add_capsule(mesh, (0.0, 1.40, -0.02), (0.0, 1.72, -0.08), 0.12, 12, *cloth)
    # legs
    add_capsule(mesh, (-0.09, 0.92, 0.02), (-0.11, 0.48, 0.02), 0.085, 12, *(steel if knight else leather))
    add_capsule(mesh, (0.09, 0.92, 0.02), (0.11, 0.48, 0.02), 0.085, 12, *(steel if knight else leather))
    add_capsule(mesh, (-0.11, 0.48, 0.02), (-0.12, 0.10, 0.04), 0.07, 12, *(steel if knight else leather))
    add_capsule(mesh, (0.11, 0.48, 0.02), (0.12, 0.10, 0.04), 0.07, 12, *(steel if knight else leather))
    add_sphere(mesh, (-0.12, 0.07, 0.08), 0.07, 10, 6, *leather)
    add_sphere(mesh, (0.12, 0.07, 0.08), 0.07, 10, 6, *leather)
    # arms
    add_capsule(mesh, (-0.22, 1.22, 0.04), (-0.32, 0.92, 0.06), 0.065, 12, *(steel if knight else leather))
    add_capsule(mesh, (0.22, 1.22, 0.04), (0.34, 0.92, 0.08), 0.065, 12, *(steel if knight else leather))
    add_capsule(mesh, (-0.32, 0.92, 0.06), (-0.34, 0.62, 0.10), 0.055, 12, *skin)
    add_capsule(mesh, (0.34, 0.92, 0.08), (0.36, 0.62, 0.12), 0.055, 12, *skin)
    add_sphere(mesh, (-0.34, 0.58, 0.12), 0.055, 8, 6, *skin)
    add_sphere(mesh, (0.36, 0.58, 0.14), 0.055, 8, 6, *skin)
    if knight:
        add_box(mesh, (0.0, 1.18, 0.16), 0.28, 0.22, 0.08, *steel)
        add_box(mesh, (-0.18, 0.55, -0.02), 0.04, 0.42, 0.28, *steel)
    if armed:
        if knight:
            add_box(mesh, (0.38, 0.92, 0.14), 0.04, 0.04, 0.90, *steel)
            add_box(mesh, (0.38, 0.62, 0.14), 0.16, 0.04, 0.04, *gold)
            add_sphere(mesh, (0.38, 0.48, 0.14), 0.045, 8, 6, *gold)
        else:
            add_capsule(mesh, (0.38, 0.58, 0.14), (0.42, 1.15, 0.18), 0.035, 8, *wood)
            add_sphere(mesh, (0.42, 1.18, 0.18), 0.07, 8, 6, *wood)


def make_characters(out: Path) -> None:
    for name, kwargs in (
        ("knight_unarmed.obj", dict(knight=True, armed=False, bandit=False)),
        ("knight_armed.obj", dict(knight=True, armed=True, bandit=False)),
        ("bandit.obj", dict(knight=False, armed=True, bandit=True)),
    ):
        mesh = Mesh()
        humanoid(mesh, **kwargs)
        mesh.write(out / name)


def make_props(out: Path) -> None:
    crate = Mesh()
    add_box(crate, (0.0, 0.35, 0.0), 0.7, 0.7, 0.7, 2, 1)
    crate.write(out / "crate.obj")
    rock = Mesh()
    add_sphere(rock, (0.0, 0.28, 0.0), 0.42, 12, 8, 4, 2)
    add_sphere(rock, (0.18, 0.18, 0.12), 0.22, 10, 6, 4, 2)
    rock.write(out / "rock.obj")
    tree = Mesh()
    add_capsule(tree, (0.0, 0.0, 0.0), (0.0, 1.4, 0.0), 0.14, 10, 2, 1)
    add_sphere(tree, (0.0, 1.7, 0.0), 0.7, 12, 8, 0, 2)
    add_sphere(tree, (0.35, 1.55, 0.1), 0.38, 10, 6, 0, 2)
    tree.write(out / "tree.obj")


def make_character_atlas(path: Path) -> None:
    w = h = 256
    buf = bytearray(w * h * 3)
    tiles = {
        (0, 0): (0.72, 0.74, 0.78),  # steel
        (1, 0): (0.28, 0.30, 0.34),  # dark steel
        (2, 0): (0.78, 0.62, 0.22),  # gold
        (3, 0): (0.86, 0.68, 0.54),  # skin
        (0, 1): (0.42, 0.28, 0.18),  # leather
        (1, 1): (0.55, 0.14, 0.14),  # cloth red
        (2, 1): (0.45, 0.30, 0.16),  # wood
        (3, 1): (0.58, 0.60, 0.66),  # mail
        (0, 2): (0.22, 0.42, 0.18),  # leaves
        (4, 2): (0.48, 0.46, 0.42),  # stone (rock uses tx=4)
    }
    for (tx, ty), color in tiles.items():
        fill_rect(buf, w, tx * TILE, ty * TILE, (tx + 1) * TILE, (ty + 1) * TILE, color, tx * 17 + ty)
    # chainmail dots
    for y in range(TILE, TILE * 2):
        for x in range(TILE * 3, TILE * 4):
            if (x + y) % 4 == 0:
                i = (y * w + x) * 3
                buf[i : i + 3] = bytes((90, 94, 104))
    write_png(path, w, h, bytes(buf))


def make_ground_atlas(path: Path) -> None:
    w = h = 256
    buf = bytearray(w * h * 3)
    for y in range(h):
        for x in range(w):
            n = noise2(x, y, 9) * 0.12
            n2 = noise2(x // 8, y // 8, 3) * 0.08
            r = int(clamp(0.34 + n + n2) * 255)
            g = int(clamp(0.46 + n * 0.7) * 255)
            b = int(clamp(0.22 + n * 0.4) * 255)
            i = (y * w + x) * 3
            buf[i : i + 3] = bytes((r, g, b))
    write_png(path, w, h, bytes(buf))


def make_stone_atlas(path: Path) -> None:
    w = h = 128
    buf = bytearray(w * h * 3)
    for y in range(h):
        for x in range(w):
            n = noise2(x, y, 21) * 0.16
            mortar = 1.0 if (x % 32 < 2 or y % 16 < 2) else 0.0
            r = int(clamp(0.48 + n - mortar * 0.18) * 255)
            g = int(clamp(0.46 + n - mortar * 0.16) * 255)
            b = int(clamp(0.42 + n - mortar * 0.14) * 255)
            i = (y * w + x) * 3
            buf[i : i + 3] = bytes((r, g, b))
    write_png(path, w, h, bytes(buf))


def write_wav(path: Path, samples: list[float], rate: int = 22050) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = b"".join(struct.pack("<h", int(clamp(s, -1.0, 1.0) * 32767)) for s in samples)
        w.writeframes(frames)


def env(i: int, n: int, atk: int, rel: int) -> float:
    a = min(1.0, i / max(1, atk))
    r = min(1.0, (n - i) / max(1, rel))
    return a * r


def synth_sword(n: int, rate: int) -> list[float]:
    out = []
    rng = 0xC0FFEE
    lp = 0.0
    for i in range(n):
        rng = (rng * 1664525 + 1013904223) & 0xFFFFFFFF
        noise = ((rng & 0xFFFF) / 32768.0) - 1.0
        t = i / rate
        metal = math.sin(2 * math.pi * (620 + 80 * math.sin(40 * t)) * t) * 0.22
        low = math.sin(2 * math.pi * 90 * t) * 0.18
        lp = lp * 0.86 + noise * 0.14
        e = env(i, n, 40, 900)
        out.append((metal + low + lp * 0.35) * e * 0.9)
    return out


def synth_hit(n: int, rate: int) -> list[float]:
    out = []
    rng = 0xBADC0DE
    for i in range(n):
        rng = (rng * 1664525 + 1013904223) & 0xFFFFFFFF
        noise = ((rng & 0xFFFF) / 32768.0) - 1.0
        t = i / rate
        thump = math.sin(2 * math.pi * (70 + 40 * (1 - i / n)) * t)
        e = env(i, n, 20, 700)
        out.append((thump * 0.55 + noise * 0.25) * e)
    return out


def synth_step(n: int, rate: int) -> list[float]:
    out = []
    rng = 0xA11CE
    for i in range(n):
        rng = (rng * 1664525 + 1013904223) & 0xFFFFFFFF
        noise = ((rng & 0xFFFF) / 32768.0) - 1.0
        e = env(i, n, 30, 400)
        out.append(noise * 0.22 * e * (0.6 + 0.4 * math.sin(i * 0.05)))
    return out


def synth_whoosh(n: int, rate: int) -> list[float]:
    out = []
    rng = 0xF00D
    lp = 0.0
    for i in range(n):
        rng = (rng * 1664525 + 1013904223) & 0xFFFFFFFF
        noise = ((rng & 0xFFFF) / 32768.0) - 1.0
        lp = lp * 0.92 + noise * 0.08
        e = env(i, n, 80, 500)
        out.append(lp * 0.45 * e)
    return out


def make_wavs(out: Path) -> None:
    rate = 22050
    write_wav(out / "sword.wav", synth_sword(int(rate * 0.28), rate), rate)
    write_wav(out / "hit.wav", synth_hit(int(rate * 0.22), rate), rate)
    write_wav(out / "footstep.wav", synth_step(int(rate * 0.12), rate), rate)
    write_wav(out / "whoosh.wav", synth_whoosh(int(rate * 0.18), rate), rate)
    write_wav(out / "land.wav", synth_hit(int(rate * 0.16), rate), rate)
    write_wav(out / "death.wav", synth_whoosh(int(rate * 0.4), rate), rate)
    write_wav(out / "jump.wav", synth_whoosh(int(rate * 0.16), rate), rate)


def copy_into(dst: Path) -> None:
    chars = dst / "Assets" / "Characters"
    tex = dst / "Assets" / "Textures"
    audio = dst / "Assets" / "Audio"
    models = dst / "Assets" / "Models"
    make_characters(chars)
    make_props(chars)
    make_character_atlas(tex / "characters.png")
    make_ground_atlas(tex / "ground.png")
    make_stone_atlas(tex / "stone.png")
    make_wavs(audio)
    # keep Models/knight.obj as the armed hero for older scenes
    armed = (chars / "knight_armed.obj").read_text()
    models.mkdir(parents=True, exist_ok=True)
    (models / "knight.obj").write_text(armed)


def main() -> None:
    copy_into(ROOT)
    game = Path.home() / "Desktop" / "KnightBandits"
    copy_into(game)
    print("generated", ROOT / "Assets" / "Characters")
    print("generated", game)


if __name__ == "__main__":
    main()

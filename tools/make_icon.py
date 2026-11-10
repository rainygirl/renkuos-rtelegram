#!/usr/bin/env python3
"""Write the vector_icon resource of RTelegram.rdef and assets/RTelegram.hvif.

The family look of R Twitter and R Bluesky: a thin isometric box with the
mark printed on its lid. Here the mark is a paper plane, drawn from a few
points in lid coordinates (u, v in 0..1) and projected onto the lid.
"""
import io
import os
import re


def coord(value):
    # Two-byte HVIF coordinate: ((b0 & 0x7f) << 8 | b1) / 102 - 128.
    encoded = int(round((value + 128.0) * 102))
    return bytes([0x80 | (encoded >> 8), encoded & 0xFF])


def polygon(points):
    unique = []
    for p in points:
        if (not unique or abs(unique[-1][0] - p[0]) > 0.05
                or abs(unique[-1][1] - p[1]) > 0.05):
            unique.append(p)
    if len(unique) > 255:
        raise SystemExit("too many points: %d" % len(unique))
    data = bytearray([0x02 | 0x08, len(unique)])   # closed, no curves
    for px, py in unique:
        data += coord(px) + coord(py)
    return data



PLANE = [(0.12, 0.47), (0.88, 0.16), (0.70, 0.84), (0.49, 0.63),
         (0.41, 0.79), (0.38, 0.57)]
FOLD = [(0.38, 0.57), (0.80, 0.24), (0.49, 0.63), (0.41, 0.79)]


def lid(points):
    return [(32.0 + 26.0 * u - 26.0 * v, 9.0 + 13.0 * u + 13.0 * v)
            for u, v in points]


def build():
    styles = [
        (224, 235, 242, 255),  # bright top
        (112, 153, 181, 255),  # left side
        (62, 103, 132, 255),   # right side
        (42, 171, 238, 255),   # the plane, sky blue
        (38, 66, 84, 255),     # lower rim shadow
        (25, 130, 200, 255),   # the plane's fold
    ]
    far, right, near, left = (32, 9), (58, 22), (32, 35), (6, 22)
    depth = 7
    shapes = [
        (4, [(7, 24), (32, 37), (57, 24), (57, 31), (32, 44), (7, 31)]),
        (1, [left, near, (32, near[1] + depth), (left[0], left[1] + depth)]),
        (2, [right, near, (32, near[1] + depth), (right[0], right[1] + depth)]),
        (0, [far, right, near, left]),
        (3, lid(PLANE)),
        (5, lid(FOLD)),
    ]
    data = bytearray(b"ncif")
    data.append(len(styles))
    for color in styles:
        data.append(1)
        data += bytes(color)
    data.append(len(shapes))
    for _, points in shapes:
        data += polygon(points)
    data.append(len(shapes))
    for index, (style, _) in enumerate(shapes):
        data += bytes([0x0A, style, 1, index, 0])
    return bytes(data)


def main():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    path = os.path.join(here, "RTelegram.rdef")
    text = io.open(path, encoding="utf-8").read()
    data = build()
    value = "".join("%02X" % b for b in data)
    block = "resource vector_icon {\n" + "".join(
        '\t$"%s"\n' % value[i:i + 64] for i in range(0, len(value), 64)) + "};\n"
    text = re.sub(r"resource vector_icon \{.*?\};\n", lambda _: block, text,
                  flags=re.S)
    io.open(path, "w", encoding="utf-8").write(text)
    assets = os.path.join(here, "assets")
    os.makedirs(assets, exist_ok=True)
    with open(os.path.join(assets, "RTelegram.hvif"), "wb") as f:
        f.write(data)
    print("RTelegram.rdef, assets/RTelegram.hvif: wrote %d-byte HVIF" % len(data))


if __name__ == "__main__":
    main()

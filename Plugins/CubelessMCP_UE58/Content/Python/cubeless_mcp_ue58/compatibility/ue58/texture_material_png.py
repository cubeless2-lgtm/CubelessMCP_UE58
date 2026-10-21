"""Original stdlib UV polygon PNG renderer; no Engine, numpy or PIL dependency."""
import struct
import zlib
from pathlib import Path
from typing import Dict,Any,List,Sequence,Tuple
Color=Tuple[int,int,int,int]

def _png_chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def _write_png_rgba(path: str, width: int, height: int, pixels: bytearray) -> None:
    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        row_start = y * stride
        raw.extend(pixels[row_start : row_start + stride])

    png = bytearray()
    png.extend(b"\x89PNG\r\n\x1a\n")
    png.extend(_png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)))
    png.extend(_png_chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(_png_chunk(b"IEND", b""))

    with open(path, "wb") as png_file:
        png_file.write(png)


def _set_pixel(pixels: bytearray, width: int, height: int, x: int, y: int, color: Color) -> None:
    if x < 0 or y < 0 or x >= width or y >= height:
        return
    idx = (y * width + x) * 4
    pixels[idx : idx + 4] = bytes(color)


def _draw_line(
    pixels: bytearray,
    width: int,
    height: int,
    start: Tuple[int, int],
    end: Tuple[int, int],
    color: Color,
    thickness: int = 2,
) -> None:
    x0, y0 = start
    x1, y1 = end
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    radius = max(0, thickness // 2)

    while True:
        for ox in range(-radius, radius + 1):
            for oy in range(-radius, radius + 1):
                _set_pixel(pixels, width, height, x0 + ox, y0 + oy, color)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def _uv_to_pixel(uv: Sequence[float], size: int, padding: int) -> Tuple[int, int]:
    drawable = max(1, size - padding * 2 - 1)
    u = max(0.0, min(1.0, float(uv[0])))
    v = max(0.0, min(1.0, float(uv[1])))
    x = int(round(padding + u * drawable))
    y = int(round(padding + (1.0 - v) * drawable))
    return x, y


def render_uv_layout_png(
    polygons: List[List[List[float]]],
    output_path: str,
    image_size: int = 1024,
    transparent: bool = True,
) -> Dict[str, Any]:
    """Render UV polygon edges into a PNG using only the standard library."""
    output = Path(output_path).expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    size = max(128, min(4096, int(image_size)))
    background: Color = (0, 0, 0, 0) if transparent else (0, 0, 0, 255)
    line_color: Color = (255, 255, 255, 255)
    pixels = bytearray(background * (size * size))
    padding = max(4, size // 128)
    segment_count = 0

    for polygon in polygons:
        if len(polygon) < 2:
            continue
        points = [_uv_to_pixel(uv, size, padding) for uv in polygon]
        for index, point in enumerate(points):
            next_point = points[(index + 1) % len(points)]
            _draw_line(pixels, size, size, point, next_point, line_color, thickness=max(1, size // 512))
            segment_count += 1

    _write_png_rgba(str(output), size, size, pixels)
    return {
        "success": True,
        "output_path": str(output),
        "image_size": size,
        "polygon_count": len(polygons),
        "line_segment_count": segment_count,
        "transparent": transparent,
    }



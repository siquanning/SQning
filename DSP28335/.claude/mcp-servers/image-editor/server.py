#!/usr/bin/env python
"""Image editing MCP server — Pillow-based, local only."""
import json
import sys
import base64
import io
import os
from pathlib import Path
from typing import Any

from mcp.server import Server, NotificationOptions
from mcp.server.models import InitializationCapabilities
from mcp.server.stdio import stdio_server
from mcp.types import Tool, TextContent, ImageContent
from PIL import Image, ImageEnhance, ImageFilter, ImageDraw, ImageFont


server = Server("image-editor")


def _image_to_data_uri(img: Image.Image, fmt: str = "PNG") -> str:
    buf = io.BytesIO()
    img.save(buf, format=fmt)
    return base64.b64encode(buf.getvalue()).decode()


def _load_image(path: str) -> Image.Image:
    if not os.path.isfile(path):
        raise FileNotFoundError(f"File not found: {path}")
    return Image.open(path)


# ── tools ──────────────────────────────────────────────

@server.tool()
async def get_image_info(path: str) -> list[TextContent]:
    """Get metadata from an image file: size, format, mode, file size."""
    try:
        img = Image.open(path)
        w, h = img.size
        fsize = os.path.getsize(path)
        info = {
            "path": path,
            "format": img.format,
            "mode": img.mode,
            "width": w,
            "height": h,
            "file_size_bytes": fsize,
            "info": {k: str(v) for k, v in img.info.items() if k not in ("icc_profile",)},
        }
        return [TextContent(type="text", text=json.dumps(info, indent=2, ensure_ascii=False))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


@server.tool()
async def resize_image(
    path: str, width: int, height: int, output: str = "", keep_aspect: bool = True
) -> list[TextContent]:
    """Resize an image. If keep_aspect=True, fits within width x height."""
    try:
        img = _load_image(path)
        if keep_aspect:
            img.thumbnail((width, height), Image.LANCZOS)
        else:
            img = img.resize((width, height), Image.LANCZOS)
        out = output or path
        img.save(out)
        w, h = img.size
        return [TextContent(type="text", text=json.dumps({"saved": out, "width": w, "height": h}, indent=2))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


@server.tool()
async def crop_image(
    path: str, left: int, top: int, right: int, bottom: int, output: str = ""
) -> list[TextContent]:
    """Crop an image with (left, top, right, bottom) pixel coordinates."""
    try:
        img = _load_image(path)
        cropped = img.crop((left, top, right, bottom))
        out = output or path
        cropped.save(out)
        return [TextContent(type="text", text=json.dumps({"saved": out, "size": list(cropped.size)}, indent=2))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


@server.tool()
async def rotate_image(path: str, degrees: float, output: str = "", expand: bool = True) -> list[TextContent]:
    """Rotate an image by degrees (counter-clockwise)."""
    try:
        img = _load_image(path)
        rotated = img.rotate(degrees, expand=expand, resample=Image.BICUBIC)
        out = output or path
        rotated.save(out)
        return [TextContent(type="text", text=json.dumps({"saved": out, "size": list(rotated.size)}, indent=2))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


@server.tool()
async def convert_format(path: str, fmt: str, output: str) -> list[TextContent]:
    """Convert image format (PNG, JPEG, BMP, GIF, WEBP, TIFF)."""
    try:
        img = _load_image(path)
        img.save(output, format=fmt.upper())
        return [TextContent(type="text", text=json.dumps({"saved": output, "format": fmt.upper()}, indent=2))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


@server.tool()
async def adjust_brightness(
    path: str, factor: float, output: str = ""
) -> list[TextContent]:
    """Adjust brightness. factor=1.0 is original, <1 darker, >1 brighter."""
    try:
        img = _load_image(path)
        enhancer = ImageEnhance.Brightness(img)
        result = enhancer.enhance(factor)
        out = output or path
        result.save(out)
        return [TextContent(type="text", text=json.dumps({"saved": out, "factor": factor}, indent=2))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


@server.tool()
async def adjust_contrast(
    path: str, factor: float, output: str = ""
) -> list[TextContent]:
    """Adjust contrast. factor=1.0 is original, <1 less contrast, >1 more contrast."""
    try:
        img = _load_image(path)
        enhancer = ImageEnhance.Contrast(img)
        result = enhancer.enhance(factor)
        out = output or path
        result.save(out)
        return [TextContent(type="text", text=json.dumps({"saved": out, "factor": factor}, indent=2))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


@server.tool()
async def flip_image(path: str, direction: str, output: str = "") -> list[TextContent]:
    """Flip image: direction = 'horizontal' or 'vertical'."""
    try:
        img = _load_image(path)
        if direction.lower() == "horizontal":
            result = img.transpose(Image.FLIP_LEFT_RIGHT)
        elif direction.lower() == "vertical":
            result = img.transpose(Image.FLIP_TOP_BOTTOM)
        else:
            return [TextContent(type="text", text="Error: direction must be 'horizontal' or 'vertical'")]
        out = output or path
        result.save(out)
        return [TextContent(type="text", text=json.dumps({"saved": out, "direction": direction}, indent=2))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


@server.tool()
async def add_text_overlay(
    path: str, text: str, x: int, y: int, output: str = "",
    font_size: int = 24, color: str = "white"
) -> list[TextContent]:
    """Add text overlay at (x,y) position. Color can be name or #RRGGBB."""
    try:
        img = _load_image(path).convert("RGBA")
        overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
        draw = ImageDraw.Draw(overlay)
        try:
            font = ImageFont.truetype("arial.ttf", font_size)
        except OSError:
            font = ImageFont.load_default()
        draw.text((x, y), text, fill=color, font=font)
        result = Image.alpha_composite(img, overlay)
        out = output or path
        result.save(out)
        return [TextContent(type="text", text=json.dumps({"saved": out, "text": text}, indent=2, ensure_ascii=False))]
    except Exception as e:
        return [TextContent(type="text", text=f"Error: {e}")]


# ── main ───────────────────────────────────────────────

async def main():
    async with stdio_server() as (reader, writer):
        await server.run(reader, writer, server.create_initialization_options())

if __name__ == "__main__":
    import asyncio
    asyncio.run(main())

#!/usr/bin/env node
import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";
import sharp from "sharp";
import { readFileSync, statSync } from "fs";
import { basename, extname } from "path";

const server = new Server(
  { name: "image-editor", version: "1.0.0" },
  { capabilities: { tools: {} } }
);

// ── helpers ──────────────────────────────────────────

function loadImage(path) {
  try {
    return sharp(path);
  } catch {
    throw new Error(`Cannot open image: ${path}`);
  }
}

async function getInfo(path) {
  const meta = await sharp(path).metadata();
  const fsize = statSync(path).size;
  return {
    path,
    format: meta.format,
    width: meta.width,
    height: meta.height,
    channels: meta.channels,
    hasAlpha: meta.hasAlpha,
    file_size_bytes: fsize,
  };
}

// ── tools ────────────────────────────────────────────

const TOOLS = [
  {
    name: "get_image_info",
    description: "Get metadata from an image: size, format, channels, file size.",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string", description: "Absolute path to the image file." },
      },
      required: ["path"],
    },
  },
  {
    name: "resize_image",
    description: "Resize an image. If keepAspect=true, fits within the given dimensions.",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string" },
        width: { type: "number" },
        height: { type: "number" },
        output: { type: "string", description: "Output path (defaults to overwriting input)." },
        keepAspect: { type: "boolean", description: "Maintain aspect ratio (default: true)." },
      },
      required: ["path", "width", "height"],
    },
  },
  {
    name: "crop_image",
    description: "Crop an image with pixel coordinates.",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string" },
        left: { type: "number" },
        top: { type: "number" },
        right: { type: "number" },
        bottom: { type: "number" },
        output: { type: "string", description: "Output path." },
      },
      required: ["path", "left", "top", "right", "bottom"],
    },
  },
  {
    name: "rotate_image",
    description: "Rotate an image by degrees (counter-clockwise).",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string" },
        degrees: { type: "number" },
        output: { type: "string" },
        background: { type: "string", description: "Background color for uncovered areas (default: transparent/black)." },
      },
      required: ["path", "degrees"],
    },
  },
  {
    name: "convert_format",
    description: "Convert image format (png, jpeg, webp, tiff, avif, gif).",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string" },
        format: { type: "string", description: "Target format: png, jpeg, webp, tiff, avif, gif." },
        output: { type: "string" },
        quality: { type: "number", description: "Quality 1-100 (JPEG/WEBP only)." },
      },
      required: ["path", "format", "output"],
    },
  },
  {
    name: "adjust_brightness",
    description: "Adjust brightness linearly.",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string" },
        factor: { type: "number", description: "Multiplier: 1.0=original, <1 darker, >1 brighter." },
        output: { type: "string" },
      },
      required: ["path", "factor"],
    },
  },
  {
    name: "adjust_contrast",
    description: "Adjust contrast linearly.",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string" },
        factor: { type: "number", description: "Multiplier: 1.0=original, <1 less contrast, >1 more contrast." },
        output: { type: "string" },
      },
      required: ["path", "factor"],
    },
  },
  {
    name: "flip_image",
    description: "Flip an image horizontally or vertically.",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string" },
        direction: { type: "string", enum: ["horizontal", "vertical"] },
        output: { type: "string" },
      },
      required: ["path", "direction"],
    },
  },
  {
    name: "add_text_overlay",
    description: "Add SVG text overlay at a position. Uses SVG for rendering, supports font-size, color, bold.",
    inputSchema: {
      type: "object",
      properties: {
        path: { type: "string" },
        text: { type: "string" },
        x: { type: "number", description: "X position in pixels." },
        y: { type: "number", description: "Y position in pixels." },
        output: { type: "string" },
        fontSize: { type: "number", description: "Font size in px (default: 24)." },
        color: { type: "string", description: "CSS color (default: 'white')." },
        bold: { type: "boolean", description: "Bold text (default: false)." },
      },
      required: ["path", "text", "x", "y"],
    },
  },
];

// ── handlers ─────────────────────────────────────────

server.setRequestHandler(ListToolsRequestSchema, async () => ({ tools: TOOLS }));

server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;
  const out = (extra) => ({ content: [{ type: "text", text: JSON.stringify(extra, null, 2) }] });
  const err = (msg) => ({ content: [{ type: "text", text: `Error: ${msg}` }] });

  try {
    switch (name) {
      case "get_image_info":
        return out(await getInfo(args.path));

      case "resize_image": {
        const { path, width, height, output, keepAspect = true } = args;
        const p = loadImage(path);
        if (keepAspect) p.resize(width, height, { fit: "inside", withoutEnlargement: false });
        else p.resize(width, height, { fit: "fill" });
        const dest = output || path;
        await p.toFile(dest + ".tmp");
        const { renameSync } = await import("fs");
        renameSync(dest + ".tmp", dest);
        const meta = await sharp(dest).metadata();
        return out({ saved: dest, width: meta.width, height: meta.height });
      }

      case "crop_image": {
        const { path, left, top, right: r, bottom: b, output } = args;
        const dest = output || path;
        await sharp(path).extract({ left, top, width: r - left, height: b - top }).toFile(dest + ".tmp");
        const { renameSync } = await import("fs");
        renameSync(dest + ".tmp", dest);
        const meta = await sharp(dest).metadata();
        return out({ saved: dest, width: meta.width, height: meta.height });
      }

      case "rotate_image": {
        const { path, degrees, output, background } = args;
        const dest = output || path;
        await sharp(path).rotate(degrees, { background: background || { r: 0, g: 0, b: 0, alpha: 0 } }).toFile(dest + ".tmp");
        const { renameSync } = await import("fs");
        renameSync(dest + ".tmp", dest);
        const meta = await sharp(dest).metadata();
        return out({ saved: dest, width: meta.width, height: meta.height });
      }

      case "convert_format": {
        const { path, format, output, quality } = args;
        const opts = {};
        if (quality !== undefined) opts.quality = quality;
        await sharp(path).toFormat(format, opts).toFile(output);
        return out({ saved: output, format });
      }

      case "adjust_brightness": {
        const { path, factor, output } = args;
        const dest = output || path;
        await sharp(path).linear(factor, 0).toFile(dest + ".tmp");
        const { renameSync } = await import("fs");
        renameSync(dest + ".tmp", dest);
        return out({ saved: dest, factor });
      }

      case "adjust_contrast": {
        const { path, factor, output } = args;
        const dest = output || path;
        // Contrast: multiply around 128 midpoint
        await sharp(path)
          .linear(factor, -(128 * (factor - 1)))
          .toFile(dest + ".tmp");
        const { renameSync } = await import("fs");
        renameSync(dest + ".tmp", dest);
        return out({ saved: dest, factor });
      }

      case "flip_image": {
        const { path, direction, output } = args;
        const dest = output || path;
        await sharp(path)[direction === "horizontal" ? "flop" : "flip"]().toFile(dest + ".tmp");
        const { renameSync } = await import("fs");
        renameSync(dest + ".tmp", dest);
        return out({ saved: dest, direction });
      }

      case "add_text_overlay": {
        const { path, text: txt, x, y, output, fontSize = 24, color = "white", bold = false } = args;
        const dest = output || path;
        const weight = bold ? "bold" : "normal";
        const svgText = `<svg width="10000" height="10000">
          <text x="0" y="${fontSize}" font-family="Arial, sans-serif" font-size="${fontSize}" font-weight="${weight}" fill="${color}">${txt.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;")}</text>
        </svg>`;
        const svgBuf = Buffer.from(svgText);
        await sharp(path)
          .composite([{ input: svgBuf, left: x, top: y }])
          .toFile(dest + ".tmp");
        const { renameSync } = await import("fs");
        renameSync(dest + ".tmp", dest);
        return out({ saved: dest, text: txt, position: { x, y } });
      }

      default:
        return err(`Unknown tool: ${name}`);
    }
  } catch (e) {
    return err(e.message);
  }
});

// ── main ─────────────────────────────────────────────

const transport = new StdioServerTransport();
await server.connect(transport);

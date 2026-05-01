/**
 * Builds one .glb per logic-kit node (gates, math, memory, route, time/flow, I/O, sensors) + OLED PNG sets.
 * Ports: named scene graph, glTF `extras` from userData, and near-invisible hit spheres for raycasts.
 *
 * Usage: npm install && node export-logic-kit.mjs
 * Output:
 *   ../../Assets/Packages/LogicKit/glb/<node_id>.glb
 *   ../../Assets/Packages/LogicKit/textures/<node_id>/screen_<state>.png  (256² canonical)
 *   ../../Assets/Packages/LogicKit/textures/<node_id>/r128/ and /r512/ screen_<state>.png
 *   ../../Assets/Packages/LogicKit/textures/<node_id>/remap_preview/mode_<m>/screen_idle.png
 *   ../../Assets/Packages/LogicKit/logic_kit_nodes.json
 */

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

import { createCanvas, ImageData } from "@napi-rs/canvas";

import * as THREE from "three";
import { GLTFExporter } from "three/examples/jsm/exporters/GLTFExporter.js";
import { RoundedBoxGeometry } from "three/examples/jsm/geometries/RoundedBoxGeometry.js";
import { PNG } from "pngjs";

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// GLTFExporter rasterizes DataTexture via a 2D canvas; provide a minimal DOM in Node.
if (typeof globalThis.ImageData === "undefined") {
  globalThis.ImageData = ImageData;
}

if (typeof globalThis.document === "undefined") {
  globalThis.document = {
    createElement(tagName) {
      if (String(tagName).toLowerCase() !== "canvas") {
        throw new Error(`createElement polyfill: unsupported <${tagName}>`);
      }
      return createCanvas(2, 2);
    },
  };
}

if (typeof globalThis.FileReader === "undefined") {
  globalThis.FileReader = class FileReaderPolyfill {
    readAsArrayBuffer(blob) {
      blob
        .arrayBuffer()
        .then((buf) => {
          this.result = buf;
          queueMicrotask(() => this.onloadend?.());
        })
        .catch((e) => this.onerror?.(e));
    }
    readAsDataURL(blob) {
      blob
        .arrayBuffer()
        .then((buf) => {
          const b64 = Buffer.from(buf).toString("base64");
          this.result = `data:application/octet-stream;base64,${b64}`;
          queueMicrotask(() => this.onloadend?.());
        })
        .catch((e) => this.onerror?.(e));
    }
  };
}

/** Screen states: one PNG per state per resolution profile; embed `idle` @ r256 on the GLB. */
const SCREEN_STATE_KEYS = [
  "off",
  "idle",
  "active",
  "error",
  "boot",
  "busy",
  "warn",
  "disabled",
  "ok",
  "pulse",
];

/**
 * After drawing, remap RGBA so on-disk PNGs and the embedded GLB match RawIron’s sampler + RoundedBox front UVs.
 * `rotate180` fixes upside-down / inverted readout on the kit screen in the engine preview.
 * Change to `none` | `flipV` | `flipH` if a future renderer/UV layout differs.
 */
const SCREEN_PIXEL_REMAP_MODE = "flipV";

/** Idle-only PNGs under remap_preview/ so you can retarget another engine without reauthoring art. */
const SCREEN_REMAP_PREVIEW_MODES = ["none", "flipV", "flipH", "rotate180"];
const EXPORT_SCREEN_REMAP_PREVIEWS = true;

/** Widths (px) for OLED PNG sets; r256 files stay in the node folder for backward compatibility. */
const SCREEN_TEXTURE_PROFILES = [
  { key: "r256", subdir: "", px: 256 },
  { key: "r128", subdir: "r128", px: 128 },
  { key: "r512", subdir: "r512", px: 512 },
];

/**
 * @param {Uint8ClampedArray | Uint8Array} data
 * @param {"none" | "rotate180" | "flipV" | "flipH"} mode
 */
function applyScreenPixelRemap(data, width, height, mode) {
  if (mode === "none" || width < 1 || height < 1) {
    return;
  }
  const n = width * height * 4;
  const src = new Uint8Array(n);
  src.set(data);

  if (mode === "rotate180") {
    for (let y = 0; y < height; y++) {
      for (let x = 0; x < width; x++) {
        const si = (y * width + x) * 4;
        const di = ((height - 1 - y) * width + (width - 1 - x)) * 4;
        data[di] = src[si];
        data[di + 1] = src[si + 1];
        data[di + 2] = src[si + 2];
        data[di + 3] = src[si + 3];
      }
    }
    return;
  }

  if (mode === "flipV") {
    const rowBytes = width * 4;
    for (let y = 0; y < height >> 1; y++) {
      const y2 = height - 1 - y;
      const ra = y * rowBytes;
      const rb = y2 * rowBytes;
      for (let i = 0; i < rowBytes; i++) {
        const a = ra + i;
        const b = rb + i;
        const t0 = data[a];
        data[a] = data[b];
        data[b] = t0;
      }
    }
    return;
  }

  if (mode === "flipH") {
    const row = width * 4;
    for (let y = 0; y < height; y++) {
      const rowOff = y * row;
      for (let x = 0; x < width / 2; x++) {
        const x2 = width - 1 - x;
        for (let c = 0; c < 4; c++) {
          const a = rowOff + x * 4 + c;
          const b = rowOff + x2 * 4 + c;
          const t0 = data[a];
          data[a] = data[b];
          data[b] = t0;
        }
      }
    }
  }
}

const SCREEN_STATE_STYLE = {
  off: {
    footer: "SYS.STANDBY // OFF",
    glowMul: 0.45,
    headerAlpha: 0.42,
    bodyAlpha: 0.32,
    scanlineAlpha: 0.55,
    dimOverlay: 0.52,
  },
  idle: {
    footer: "SYS.OK // ONLINE",
    glowMul: 1.0,
    headerAlpha: 1.0,
    bodyAlpha: 1.0,
    scanlineAlpha: 0.4,
    dimOverlay: 0,
  },
  active: {
    footer: "RUN // LIVE",
    glowMul: 1.2,
    headerAlpha: 1.0,
    bodyAlpha: 1.0,
    scanlineAlpha: 0.34,
    dimOverlay: 0,
    extraLine: "LOGIC ARMED",
  },
  error: {
    footer: "FAULT // HALT",
    glowMul: 1.0,
    headerAlpha: 0.88,
    bodyAlpha: 0.72,
    scanlineAlpha: 0.45,
    dimOverlay: 0,
    errorTint: true,
  },
  boot: {
    footer: "POST // BOOT",
    glowMul: 0.95,
    headerAlpha: 0.95,
    bodyAlpha: 0.92,
    scanlineAlpha: 0.42,
    dimOverlay: 0,
    extraLine: "SELF TEST",
  },
  busy: {
    footer: "WAIT // BUSY",
    glowMul: 1.08,
    headerAlpha: 1.0,
    bodyAlpha: 1.0,
    scanlineAlpha: 0.36,
    dimOverlay: 0,
    extraLine: "PROCESSING",
  },
  warn: {
    footer: "WARN // CHECK",
    glowMul: 1.05,
    headerAlpha: 1.0,
    bodyAlpha: 0.95,
    scanlineAlpha: 0.4,
    dimOverlay: 0,
    warnTint: true,
  },
  disabled: {
    footer: "LOCK // OFF",
    glowMul: 0.38,
    headerAlpha: 0.38,
    bodyAlpha: 0.3,
    scanlineAlpha: 0.55,
    dimOverlay: 0.58,
  },
  ok: {
    footer: "PASS // OK",
    glowMul: 1.08,
    headerAlpha: 1.0,
    bodyAlpha: 1.0,
    scanlineAlpha: 0.36,
    dimOverlay: 0,
    footerColor: "#44ff99",
  },
  pulse: {
    footer: "SYNC // PULSE",
    glowMul: 1.22,
    headerAlpha: 1.0,
    bodyAlpha: 1.0,
    scanlineAlpha: 0.33,
    dimOverlay: 0,
    extraLine: "CLK EDGE",
  },
};

const nodeKit = [
  { id: "gate_and", color: "#00ffff", inputs: ["A", "B"], outputs: ["Out"] },
  { id: "gate_or", color: "#00ffff", inputs: ["A", "B"], outputs: ["Out"] },
  { id: "gate_xor", color: "#00ffff", inputs: ["A", "B"], outputs: ["Out"] },
  { id: "gate_not", color: "#00ffff", inputs: ["In"], outputs: ["Out"] },
  { id: "gate_nand", color: "#00ffff", inputs: ["A", "B"], outputs: ["Out"] },
  { id: "gate_nor", color: "#00ffff", inputs: ["A", "B"], outputs: ["Out"] },
  { id: "math_add", color: "#ffdd00", inputs: ["A", "B"], outputs: ["Sum", "Carry"] },
  { id: "math_sub", color: "#ffdd00", inputs: ["A", "B"], outputs: ["Diff", "Borrow"] },
  { id: "math_mult", color: "#ffdd00", inputs: ["A", "B"], outputs: ["Prod"] },
  { id: "math_div", color: "#ffdd00", inputs: ["A", "B"], outputs: ["Quot", "Rem"] },
  { id: "math_mod", color: "#ffdd00", inputs: ["Val", "Mod"], outputs: ["Rem"] },
  { id: "math_compare", color: "#ffdd00", inputs: ["A", "B"], outputs: ["A>B", "A==B", "A<B"] },
  { id: "mem_latch", color: "#aa44ff", inputs: ["Set", "Reset"], outputs: ["Q", "Not_Q"] },
  { id: "mem_flipflop", color: "#aa44ff", inputs: ["Data", "Clock"], outputs: ["Q", "Not_Q"] },
  { id: "mem_register", color: "#aa44ff", inputs: ["Data", "Write", "Read"], outputs: ["Value"] },
  { id: "mem_ram_array", color: "#aa44ff", inputs: ["Addr", "Data", "W_En", "R_En"], outputs: ["Value"] },
  { id: "mem_counter", color: "#aa44ff", inputs: ["Inc", "Dec", "Reset"], outputs: ["Val", "Zero", "Max"] },
  { id: "mem_variable", color: "#aa44ff", inputs: ["Set", "Get"], outputs: ["Value"] },
  { id: "route_mux", color: "#ff3366", inputs: ["Sel", "A", "B"], outputs: ["Out"] },
  { id: "route_demux", color: "#ff3366", inputs: ["Sel", "In"], outputs: ["A", "B"] },
  { id: "route_pack", color: "#ff3366", inputs: ["B0", "B1", "B2", "B3"], outputs: ["BusOut"] },
  { id: "route_unpack", color: "#ff3366", inputs: ["BusIn"], outputs: ["B0", "B1", "B2", "B3"] },
  { id: "route_merge", color: "#ff3366", inputs: ["In_1", "In_2", "In_3"], outputs: ["Out"] },
  { id: "route_split", color: "#ff3366", inputs: ["In"], outputs: ["Out_1", "Out_2", "Out_3"] },
  { id: "time_clock", color: "#3388ff", inputs: ["Enable", "Set_Hz"], outputs: ["Tick"] },
  { id: "time_delay", color: "#3388ff", inputs: ["In", "Set_Ms"], outputs: ["Out"] },
  { id: "flow_sequencer", color: "#3388ff", inputs: ["Step", "Reset"], outputs: ["S0", "S1", "S2", "S3"] },
  { id: "flow_do_once", color: "#3388ff", inputs: ["Trigger", "Reset"], outputs: ["Fired"] },
  { id: "flow_random", color: "#3388ff", inputs: ["Trigger"], outputs: ["Val", "Min", "Max"] },
  { id: "flow_relay", color: "#3388ff", inputs: ["Trig", "En", "Dis"], outputs: ["Out"] },
  { id: "io_button", color: "#00ffaa", inputs: ["Enable", "Disable"], outputs: ["Press", "Release"] },
  { id: "io_keypad", color: "#00ffaa", inputs: ["Enable", "Reset"], outputs: ["Val", "Enter"] },
  { id: "io_display", color: "#00ffaa", inputs: ["SetText", "SetColor"], outputs: ["Done"] },
  { id: "io_audio", color: "#00ffaa", inputs: ["Play", "Stop", "SetVol"], outputs: ["Done"] },
  { id: "io_logger", color: "#00ffaa", inputs: ["Log", "Warn", "Err"], outputs: [] },
  { id: "io_trigger", color: "#00ffaa", inputs: ["Arm", "Disarm"], outputs: ["Touch", "Untouch"] },

  // --- Extra boolean ---
  { id: "gate_buf", color: "#00ffff", inputs: ["In"], outputs: ["Out"] },
  { id: "gate_xnor", color: "#00ffff", inputs: ["A", "B"], outputs: ["Out"] },
  { id: "gate_xor", color: "#00ffff", inputs: ["A", "B"], outputs: ["Out"] },

  // --- Analog / scalar math ---
  { id: "math_abs", color: "#ffdd00", inputs: ["In"], outputs: ["Out"] },
  { id: "math_min", color: "#ffdd00", inputs: ["A", "B"], outputs: ["Out"] },
  { id: "math_max", color: "#ffdd00", inputs: ["A", "B"], outputs: ["Out"] },
  { id: "math_clamp", color: "#ffdd00", inputs: ["Val", "Lo", "Hi"], outputs: ["Out"] },
  { id: "math_lerp", color: "#ffdd00", inputs: ["A", "B", "T"], outputs: ["Out"] },
  { id: "math_sign", color: "#ffdd00", inputs: ["In"], outputs: ["Sign", "Zero"] },
  { id: "math_round", color: "#ffdd00", inputs: ["In"], outputs: ["Out"] },

  // --- Memory / signal conditioning ---
  { id: "mem_edge", color: "#aa44ff", inputs: ["Sig", "Reset"], outputs: ["Rise", "Fall"] },
  { id: "mem_chatter", color: "#aa44ff", inputs: ["Sig", "Ms", "Rst"], outputs: ["Stable", "Raw"] },
  { id: "mem_sample", color: "#aa44ff", inputs: ["Sig", "Cap", "Hold"], outputs: ["Out"] },

  // --- Routing ---
  { id: "route_tee", color: "#ff3366", inputs: ["In"], outputs: ["A", "B"] },
  { id: "route_pass", color: "#ff3366", inputs: ["In", "En"], outputs: ["Out"] },
  { id: "route_select", color: "#ff3366", inputs: ["Sel", "In0", "In1", "In2"], outputs: ["Out"] },

  // --- Time / flow (extra) ---
  { id: "flow_rise", color: "#3388ff", inputs: ["In", "Arm"], outputs: ["Pulse"] },
  { id: "flow_fall", color: "#3388ff", inputs: ["In", "Arm"], outputs: ["Pulse"] },
  { id: "flow_dbnc", color: "#3388ff", inputs: ["In", "Ms", "Rst"], outputs: ["Out"] },
  { id: "flow_oneshot", color: "#3388ff", inputs: ["Trig", "Ms"], outputs: ["Out", "Busy"] },
  { id: "time_watch", color: "#3388ff", inputs: ["Start", "Stop", "Rst"], outputs: ["Ms", "Run"] },

  // --- Sensors & world probes (engine binds actor / physics / queries) ---
  { id: "sense_prox", color: "#66ddff", inputs: ["En", "Range"], outputs: ["Near", "Far"] },
  { id: "sense_ray", color: "#66ddff", inputs: ["En", "Len"], outputs: ["Hit", "Miss"] },
  { id: "sense_overlap", color: "#66ddff", inputs: ["En", "Rst"], outputs: ["Enter", "Exit"] },
  { id: "sense_zone", color: "#66ddff", inputs: ["Arm", "Clr"], outputs: ["Inside", "Count"] },
  { id: "sense_line", color: "#66ddff", inputs: ["En", "Dist"], outputs: ["Hit", "Frac"] },
  { id: "sense_tag", color: "#66ddff", inputs: ["Tag", "Poll"], outputs: ["Seen", "Cnt"] },
  { id: "sense_scalar", color: "#66ddff", inputs: ["Key", "Poll"], outputs: ["Val", "Ok"] },
  { id: "sense_axis", color: "#66ddff", inputs: ["Src", "Poll"], outputs: ["X", "Y", "Z"] },
  { id: "sense_noise", color: "#66ddff", inputs: ["Seed", "Trig"], outputs: ["Val", "Norm"] },
  { id: "sense_tick", color: "#66ddff", inputs: ["En", "Div"], outputs: ["Tick", "Idx"] },
];

function hexToColor(hex) {
  return new THREE.Color(hex);
}

/**
 * Mirrors the reference HTML OLED draw (canvas 2D), with per-state footer / glow / dimming.
 * @param {{ pxWidth?: number, pixelRemapMode?: string }} [opts]
 */
async function drawOledPngBuffer(nodeId, colorHex, blockWidth, blockHeight, stateKey, opts = {}) {
  const { pxWidth = 256, pixelRemapMode = SCREEN_PIXEL_REMAP_MODE } = opts;
  const style = SCREEN_STATE_STYLE[stateKey];
  if (!style) {
    throw new Error(`Unknown screen state: ${stateKey}`);
  }

  const scale = pxWidth / 256;
  const pxHeight = Math.max(
    Math.max(24, Math.round(48 * scale)),
    Math.floor((blockHeight / blockWidth) * pxWidth),
  );

  const canvas = createCanvas(pxWidth, pxHeight);
  const ctx = canvas.getContext("2d");

  const m = Math.max(2, Math.round(15 * scale));
  const scanStep = Math.max(2, Math.round(4 * scale));
  const scanH = Math.max(1, Math.round(2 * scale));

  ctx.fillStyle = "#0a0b10";
  ctx.fillRect(0, 0, pxWidth, pxHeight);

  ctx.fillStyle = `rgba(0, 0, 0, ${style.scanlineAlpha})`;
  for (let y = 0; y < pxHeight; y += scanStep) {
    ctx.fillRect(0, y, pxWidth, scanH);
  }

  const parts = nodeId.split("_");

  ctx.globalAlpha = style.headerAlpha;
  ctx.shadowColor = colorHex;
  ctx.shadowBlur = 15 * style.glowMul * scale;
  ctx.fillStyle = colorHex;
  ctx.font = `bold ${Math.max(8, Math.round(24 * scale))}px Consolas, 'DejaVu Sans Mono', 'Courier New', monospace`;
  ctx.textAlign = "center";
  ctx.fillText(`[ ${parts[0].toUpperCase()} ]`, pxWidth / 2, Math.round(50 * scale));

  ctx.fillStyle = style.errorTint ? "#ffaaaa" : "#ffffff";
  ctx.shadowBlur = 10 * style.glowMul * scale;
  ctx.globalAlpha = style.bodyAlpha;
  const mainFontLarge = Math.max(10, Math.round(34 * scale));
  const mainFontMed = Math.max(8, Math.round(28 * scale));
  if (parts.length === 2) {
    ctx.font = `bold ${mainFontLarge}px Consolas, 'DejaVu Sans Mono', 'Courier New', monospace`;
    ctx.fillText(parts[1].toUpperCase(), pxWidth / 2, Math.round(120 * scale));
  } else if (parts.length >= 3) {
    ctx.font = `bold ${mainFontMed}px Consolas, 'DejaVu Sans Mono', 'Courier New', monospace`;
    ctx.fillText(parts[1].toUpperCase(), pxWidth / 2, Math.round(100 * scale));
    ctx.fillText(parts[2].toUpperCase(), pxWidth / 2, Math.round(140 * scale));
  }

  if (style.extraLine) {
    ctx.globalAlpha = 0.92 * style.bodyAlpha;
    ctx.font = `${Math.max(6, Math.round(12 * scale))}px Consolas, 'DejaVu Sans Mono', 'Courier New', monospace`;
    ctx.fillStyle = colorHex;
    ctx.fillText(style.extraLine, pxWidth / 2, pxHeight - Math.round(52 * scale));
  }

  ctx.shadowBlur = 0;
  ctx.globalAlpha = 1;
  ctx.strokeStyle = colorHex;
  ctx.lineWidth = Math.max(1, Math.round(2 * scale));
  ctx.strokeRect(m, m, pxWidth - 2 * m, pxHeight - 2 * m);

  ctx.font = `${Math.max(6, Math.round(10 * scale))}px Consolas, 'DejaVu Sans Mono', 'Courier New', monospace`;
  let footerFill = colorHex;
  if (style.errorTint) {
    footerFill = "#ff3366";
  } else if (style.warnTint) {
    footerFill = "#ffcc33";
  } else if (style.footerColor) {
    footerFill = style.footerColor;
  }
  ctx.fillStyle = footerFill;
  ctx.fillText(style.footer, pxWidth / 2, pxHeight - Math.round(25 * scale));

  if (style.dimOverlay > 0) {
    ctx.fillStyle = `rgba(0, 0, 0, ${style.dimOverlay})`;
    ctx.fillRect(0, 0, pxWidth, pxHeight);
  }

  const img = ctx.getImageData(0, 0, pxWidth, pxHeight);
  applyScreenPixelRemap(img.data, pxWidth, pxHeight, pixelRemapMode);
  ctx.putImageData(img, 0, 0);

  const encoded = await canvas.encode("png");
  return Buffer.isBuffer(encoded) ? encoded : Buffer.from(encoded);
}

function textureRelPath(nodeId, subdir, fileName) {
  if (!subdir) {
    return path.posix.join("textures", nodeId, fileName);
  }
  return path.posix.join("textures", nodeId, subdir, fileName);
}

function pngBufferToDataTexture(pngBuffer, label) {
  const png = PNG.sync.read(pngBuffer);
  const data = new Uint8Array(png.data);
  const tex = new THREE.DataTexture(data, png.width, png.height, THREE.RGBAFormat, THREE.UnsignedByteType);
  tex.name = label;
  // Rows already match RawIron + SCREEN_PIXEL_REMAP_MODE; GLTFExporter must not flip again.
  tex.flipY = false;
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.needsUpdate = true;
  return tex;
}

function portExtras(nodeId, portName, portKind, portIndex) {
  return {
    RawIronLogicPort: true,
    nodeId,
    portName,
    portKind,
    portIndex,
  };
}

function buildNodeMesh(node, screenTextureIdle) {
  const maxPorts = Math.max(node.inputs.length, node.outputs.length);
  const blockWidth = 1.4;
  const blockDepth = 0.5;
  const portSpacing = 0.35;
  const blockHeight = Math.max(1.6, maxPorts * portSpacing + 0.8);

  const matChassis = new THREE.MeshStandardMaterial({
    color: 0x18181a,
    roughness: 0.6,
    metalness: 0.7,
  });
  const matBracket = new THREE.MeshStandardMaterial({
    color: 0x222225,
    roughness: 0.7,
    metalness: 0.8,
  });
  const matHexNut = new THREE.MeshStandardMaterial({
    color: 0x887733,
    roughness: 0.3,
    metalness: 0.9,
  });
  const matSocket = new THREE.MeshStandardMaterial({ color: 0x050505, roughness: 0.9 });
  const matInPin = new THREE.MeshStandardMaterial({
    color: 0x0088ff,
    emissive: 0x0055ff,
    emissiveIntensity: 1.0,
    roughness: 0.2,
  });
  const matOutPin = new THREE.MeshStandardMaterial({
    color: 0xff6600,
    emissive: 0xff4400,
    emissiveIntensity: 1.0,
    roughness: 0.2,
  });

  const c = hexToColor(node.color);
  const matScreen = new THREE.MeshStandardMaterial({
    map: screenTextureIdle,
    emissiveMap: screenTextureIdle,
    emissive: c,
    emissiveIntensity: 0.85,
    roughness: 0.2,
    metalness: 0.05,
  });

  const hexGeo = new THREE.CylinderGeometry(0.14, 0.14, 0.04, 6);
  hexGeo.rotateZ(Math.PI / 2);
  const socketGeo = new THREE.CylinderGeometry(0.1, 0.1, 0.08, 16);
  socketGeo.rotateZ(Math.PI / 2);
  const pinGeo = new THREE.CylinderGeometry(0.04, 0.04, 0.25, 16);
  pinGeo.rotateZ(Math.PI / 2);
  const ledGeo = new THREE.SphereGeometry(0.05, 8, 8);
  const hitGeo = new THREE.SphereGeometry(0.13, 10, 10);

  const hitMat = new THREE.MeshBasicMaterial({
    color: 0x000000,
    transparent: true,
    opacity: 0.03,
    depthWrite: false,
    toneMapped: false,
  });

  const group = new THREE.Group();
  group.name = node.id;
  group.userData = {
    RawIronLogicKit: true,
    kitVersion: 4,
    nodeId: node.id,
    screenStates: SCREEN_STATE_KEYS,
    defaultScreenState: "idle",
    screenMaterialIndex: 4,
  };

  const bracketGeo = new RoundedBoxGeometry(blockWidth + 0.2, 0.1, blockDepth + 0.2, 2, 0.02);
  const bracket = new THREE.Mesh(bracketGeo, matBracket);
  bracket.name = "bracket";
  bracket.position.y = -(blockHeight / 2) + 0.05;
  group.add(bracket);

  const chassisGeo = new RoundedBoxGeometry(blockWidth, blockHeight, blockDepth, 4, 0.04);
  const materials = [matChassis, matChassis, matChassis, matChassis, matScreen, matChassis];
  const chassis = new THREE.Mesh(chassisGeo, materials);
  chassis.name = "chassis";
  chassis.userData = {
    RawIronLogicChassis: true,
    nodeId: node.id,
    screenMaterialIndex: 4,
    defaultScreenState: "idle",
    screenStates: SCREEN_STATE_KEYS,
  };
  group.add(chassis);

  const ledMat = new THREE.MeshStandardMaterial({
    color: c,
    emissive: c,
    emissiveIntensity: 2.0,
  });
  const led = new THREE.Mesh(ledGeo, ledMat);
  led.name = "status_led";
  led.position.set(blockWidth / 2 - 0.2, blockHeight / 2 - 0.2, blockDepth / 2 + 0.01);
  group.add(led);

  function createPort(portName, isInput, portIndex, sideOffset) {
    const portGroup = new THREE.Group();
    portGroup.name = isInput ? `in_${portName}` : `out_${portName}`;
    const yPos = blockHeight / 2 - 0.5 - portIndex * portSpacing;
    portGroup.position.set(sideOffset, yPos, 0);
    portGroup.userData = portExtras(node.id, portName, isInput ? "input" : "output", portIndex);

    const hexNut = new THREE.Mesh(hexGeo, matHexNut);
    hexNut.name = "hex_nut";
    hexNut.position.x = isInput ? -0.02 : 0.02;
    portGroup.add(hexNut);

    const socket = new THREE.Mesh(socketGeo, matSocket);
    socket.name = "socket";
    socket.position.x = isInput ? -0.06 : 0.06;
    portGroup.add(socket);

    const pinX = isInput ? -0.1 : 0.1;
    const pin = new THREE.Mesh(pinGeo, isInput ? matInPin : matOutPin);
    pin.name = "pin";
    pin.position.x = pinX;
    pin.userData = {
      ...portExtras(node.id, portName, isInput ? "input" : "output", portIndex),
      RawIronLogicPortPin: true,
    };
    portGroup.add(pin);

    const hit = new THREE.Mesh(hitGeo, hitMat.clone());
    hit.name = isInput ? `port_hit_in_${portName}` : `port_hit_out_${portName}`;
    hit.position.x = pinX;
    hit.renderOrder = 1;
    hit.userData = {
      ...portExtras(node.id, portName, isInput ? "input" : "output", portIndex),
      RawIronLogicPortHit: true,
    };
    portGroup.add(hit);

    return portGroup;
  }

  node.inputs.forEach((pName, i) => {
    group.add(createPort(pName, true, i, -blockWidth / 2));
  });
  node.outputs.forEach((pName, i) => {
    group.add(createPort(pName, false, i, blockWidth / 2));
  });

  return group;
}

async function main() {
  const kitRoot = path.resolve(__dirname, "../../Assets/Packages/LogicKit");
  const outDir = path.join(kitRoot, "glb");
  const texRoot = path.join(kitRoot, "textures");
  fs.mkdirSync(outDir, { recursive: true });
  fs.mkdirSync(texRoot, { recursive: true });

  const exporter = new GLTFExporter();
  const catalog = [];

  for (const node of nodeKit) {
    const maxPorts = Math.max(node.inputs.length, node.outputs.length);
    const blockWidth = 1.4;
    const portSpacing = 0.35;
    const blockHeight = Math.max(1.6, maxPorts * portSpacing + 0.8);

    const nodeTexDir = path.join(texRoot, node.id);
    fs.mkdirSync(nodeTexDir, { recursive: true });

    const screenStates = {};
    const screenTexturesByProfile = {};
    let idlePngBuffer256 = null;

    for (const profile of SCREEN_TEXTURE_PROFILES) {
      const profileDir = profile.subdir ? path.join(nodeTexDir, profile.subdir) : nodeTexDir;
      fs.mkdirSync(profileDir, { recursive: true });
      const map = {};
      for (const state of SCREEN_STATE_KEYS) {
        const pngBuffer = await drawOledPngBuffer(node.id, node.color, blockWidth, blockHeight, state, {
          pxWidth: profile.px,
          pixelRemapMode: SCREEN_PIXEL_REMAP_MODE,
        });
        const fname = `screen_${state}.png`;
        const abs = path.join(profileDir, fname);
        fs.writeFileSync(abs, pngBuffer);
        map[state] = textureRelPath(node.id, profile.subdir, fname);
        if (profile.key === "r256" && state === "idle") {
          idlePngBuffer256 = pngBuffer;
        }
      }
      screenTexturesByProfile[profile.key] = map;
      if (profile.key === "r256") {
        Object.assign(screenStates, map);
      }
    }

    const remapPreviewIdle = {};
    if (EXPORT_SCREEN_REMAP_PREVIEWS) {
      for (const mode of SCREEN_REMAP_PREVIEW_MODES) {
        const sub = path.posix.join("remap_preview", `mode_${mode}`);
        const prevDir = path.join(nodeTexDir, "remap_preview", `mode_${mode}`);
        fs.mkdirSync(prevDir, { recursive: true });
        const buf = await drawOledPngBuffer(node.id, node.color, blockWidth, blockHeight, "idle", {
          pxWidth: 256,
          pixelRemapMode: mode,
        });
        const fname = "screen_idle.png";
        fs.writeFileSync(path.join(prevDir, fname), buf);
        remapPreviewIdle[mode] = textureRelPath(node.id, sub, fname);
      }
    }

    const idleTex = pngBufferToDataTexture(idlePngBuffer256, `${node.id}_screen_idle`);
    const root = buildNodeMesh(node, idleTex);

    const buffer = await exporter.parseAsync(root, { binary: true });
    if (!(buffer instanceof ArrayBuffer)) {
      throw new Error(`Expected ArrayBuffer for ${node.id}`);
    }
    const dest = path.join(outDir, `${node.id}.glb`);
    fs.writeFileSync(dest, Buffer.from(buffer));
    console.log("wrote", dest);

    idleTex.dispose();

    catalog.push({
      id: node.id,
      glb: path.posix.join("glb", `${node.id}.glb`),
      color: node.color,
      inputs: node.inputs,
      outputs: node.outputs,
      screenStates,
      screenTexturesByProfile,
      screenTexturesR128: screenTexturesByProfile.r128,
      screenTexturesR512: screenTexturesByProfile.r512,
      remapPreviewIdle: EXPORT_SCREEN_REMAP_PREVIEWS ? remapPreviewIdle : undefined,
      embeddedScreen: { profile: "r256", state: "idle", px: 256 },
      defaultScreenState: "idle",
      screenMaterialIndex: 4,
      portSchema: {
        inputs: node.inputs.map((name, portIndex) => ({
          name,
          scenePathHint: `in_${name}`,
          hitMeshNameHint: `port_hit_in_${name}`,
          portKind: "input",
          portIndex,
        })),
        outputs: node.outputs.map((name, portIndex) => ({
          name,
          scenePathHint: `out_${name}`,
          hitMeshNameHint: `port_hit_out_${name}`,
          portKind: "output",
          portIndex,
        })),
      },
    });
  }

  const indexPath = path.join(kitRoot, "logic_kit_nodes.json");
  fs.writeFileSync(
    indexPath,
    JSON.stringify(
      {
        kitVersion: 4,
        screenPixelRemapMode: SCREEN_PIXEL_REMAP_MODE,
        screenStateKeys: SCREEN_STATE_KEYS,
        screenTextureProfiles: SCREEN_TEXTURE_PROFILES.map((p) => ({
          key: p.key,
          px: p.px,
          subdir: p.subdir || null,
        })),
        screenRemapPreviewModes: EXPORT_SCREEN_REMAP_PREVIEWS ? SCREEN_REMAP_PREVIEW_MODES : [],
        nodes: catalog,
      },
      null,
      2,
    ),
  );
  console.log("wrote", indexPath);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});

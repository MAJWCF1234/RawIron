/**
 * Combined engine module (formerly ./engine/*.js and structuralPrimitives).
 */
import * as THREE from 'three';

export const GameAssets = {
    IMG_PREFIX: './images/',
    MODEL_ASSET_PATH_PREFIX: './models/',
    MODEL_DATA_PATH_PREFIX: './models/',
    textureManifest: {},
    availableModels: [],
    animationLibraryProfiles: {},
    externalModels: {}
};

export const STRINGS = {
    msg: {
        pointerLockFailed: "Pointer Lock failed. Spectator Mode enabled.",
        webglContextLost: "WebGL context lost. Please reload the page.",
        webglContextRestored: "WebGL context restored. Resuming...",
        levelLoadError: "ERROR: Could not load level.",
        levelInvalid: "Invalid level data. Check level JSON format.",
        levelNotObject: "Level file must be a JSON object (not an array or plain value).",
        levelJsonParse: "Level file is not valid JSON.",
        accessGranted: "ACCESS GRANTED",
        newObjective: "NEW OBJECTIVE:",
        somethingAware: "SOMETHING IS AWARE OF YOU",
        staticSpike: "STATIC SPIKE DETECTED",
        equipped: "Equipped:",
        clickToEnter: "[ CLICK TO ENTER THE STATIC ]",
        clickToResume: "[ CLICK TO RESUME ]",
        clickToRestart: "[ CLICK TO RESTART ]",
        retry: "RETRY",
        continue: "CONTINUE",
        loading: "LOADING...",
        ready: "Ready.",
        unstuckSuccess: "UNSTUCK RECOVERY TRIGGERED",
        unstuckFailed: "UNSTUCK RECOVERY FAILED"
    },
    ui: {
        gameTitle: "ANOMALOUS ECHO",
        echoPaused: "ECHO PAUSED",
        connectionLost: "CONNECTION LOST",
        objective: "OBJECTIVE:",
        interact: "[E] INTERACT",
        debugReport: "SYSTEM DEBUG REPORT",
        copy: "Copy",
        copied: "Copied!",
        copyFailed: "Copy failed"
    }
};

export const DEFAULT_KEYS = {
    forward: 'KeyW',
    backward: 'KeyS',
    left: 'KeyA',
    right: 'KeyD'
};

/**
 * Shared numeric and transform utility helpers.
 */

export function clampFiniteNumber(raw, fallback, min, max) {
    const n = Number(raw);
    if (!Number.isFinite(n)) return fallback;
    const lo = min < max ? min : max;
    const hi = min < max ? max : min;
    return Math.max(lo, Math.min(hi, n));
}

export function clampFiniteInteger(raw, fallback, min, max) {
    return Math.round(clampFiniteNumber(raw, fallback, min, max));
}

export const clampPickupMotion = clampFiniteNumber;

export function finiteVec3Components(arr, fallback = [0, 0, 0]) {
    const base = Array.isArray(arr) && arr.length >= 3 ? arr : fallback;
    const fx = Number(base[0]);
    const fy = Number(base[1]);
    const fz = Number(base[2]);
    return [
        Number.isFinite(fx) ? fx : fallback[0],
        Number.isFinite(fy) ? fy : fallback[1],
        Number.isFinite(fz) ? fz : fallback[2]
    ];
}

export function finiteVec2Components(arr, fallback = [1, 1]) {
    const base = Array.isArray(arr) && arr.length >= 2 ? arr : fallback;
    const fx = Number(base[0]);
    const fy = Number(base[1]);
    return [
        Number.isFinite(fx) ? fx : fallback[0],
        Number.isFinite(fy) ? fy : fallback[1]
    ];
}

export function finiteQuatComponents(arr) {
    const b = Array.isArray(arr) && arr.length >= 4 ? arr : [0, 0, 0, 1];
    const x = Number(b[0]);
    const y = Number(b[1]);
    const z = Number(b[2]);
    const w = Number(b[3]);
    const qx = Number.isFinite(x) ? x : 0;
    const qy = Number.isFinite(y) ? y : 0;
    const qz = Number.isFinite(z) ? z : 0;
    const qw = Number.isFinite(w) ? w : 1;
    const len = Math.hypot(qx, qy, qz, qw);
    if (!Number.isFinite(len) || len < 1e-8) return [0, 0, 0, 1];
    return [qx / len, qy / len, qz / len, qw / len];
}

export function finiteScaleComponents(arr, fallback = [1, 1, 1]) {
    const sc = finiteVec3Components(arr, fallback);
    const MAX_ABS_SCALE = 512;
    const clamp = (v) => {
        if (!Number.isFinite(v) || Math.abs(v) < 1e-4) return 1;
        return Math.max(-MAX_ABS_SCALE, Math.min(MAX_ABS_SCALE, v));
    };
    return [clamp(sc[0]), clamp(sc[1]), clamp(sc[2])];
}

export function pointInsideAuthoringVolume(point, volume) {
    if (!point || !volume || !volume.position) return false;
    const px = Number(point.x);
    const py = Number(point.y);
    const pz = Number(point.z);
    if (!Number.isFinite(px) || !Number.isFinite(py) || !Number.isFinite(pz)) return false;
    const cx = Number(volume.position.x);
    const cy = Number(volume.position.y);
    const cz = Number(volume.position.z);
    if (!Number.isFinite(cx) || !Number.isFinite(cy) || !Number.isFinite(cz)) return false;

    if (volume.shape === 'box') {
        const sz = volume.size;
        const sx = sz && Number.isFinite(Number(sz.x)) ? Math.abs(Number(sz.x)) : 1;
        const sy = sz && Number.isFinite(Number(sz.y)) ? Math.abs(Number(sz.y)) : 1;
        const szZ = sz && Number.isFinite(Number(sz.z)) ? Math.abs(Number(sz.z)) : 1;
        const hx = sx * 0.5;
        const hy = sy * 0.5;
        const hz = szZ * 0.5;
        return Math.abs(px - cx) <= hx && Math.abs(py - cy) <= hy && Math.abs(pz - cz) <= hz;
    }
    if (volume.shape === 'cylinder') {
        const radius = Math.max(0.25, Number.isFinite(Number(volume.radius)) ? Number(volume.radius) : 0.5);
        const height = Math.max(
            0.25,
            Number.isFinite(Number(volume.height))
                ? Number(volume.height)
                : (volume.size && Number.isFinite(Number(volume.size.y)) ? Number(volume.size.y) : 2)
        );
        const dx = px - cx;
        const dz = pz - cz;
        if (dx * dx + dz * dz > radius * radius) return false;
        return Math.abs(py - cy) <= height * 0.5;
    }
    const r = Number(volume.radius);
    if (!Number.isFinite(r)) return false;
    const dx = px - cx;
    const dy = py - cy;
    const dz = pz - cz;
    return dx * dx + dy * dy + dz * dz <= r * r;
}

const TAU = Math.PI * 2;

function remainderIeee(x, y) {
    if (!Number.isFinite(x) || !Number.isFinite(y) || y === 0) return NaN;
    const q = x / y;
    const lo = Math.floor(q);
    const hi = Math.ceil(q);
    if (lo === hi) return x - y * lo;
    const dl = Math.abs(q - lo);
    const du = Math.abs(q - hi);
    if (dl < du) return x - y * lo;
    if (du < dl) return x - y * hi;
    return x - y * (lo % 2 === 0 ? lo : hi);
}

export function normalizeYawRadians(angle) {
    const a = Number(angle);
    if (!Number.isFinite(a)) return 0;
    return remainderIeee(a, TAU);
}

export function stepYawToward(currentYaw, desiredYaw, maxRadiansPerStep) {
    let c = Number(currentYaw);
    const d = Number(desiredYaw);
    let cap = Number(maxRadiansPerStep);
    if (!Number.isFinite(c)) c = 0;
    if (!Number.isFinite(d)) return { newYaw: c, alignment: 1 };
    cap = Math.max(0.001, Number.isFinite(cap) ? cap : 0.001);
    const delta = normalizeYawRadians(d - c);
    const step = Math.max(-cap, Math.min(cap, delta));
    const newYaw = c + step;
    const remaining = normalizeYawRadians(d - newYaw);
    let alignment = Math.cos(remaining);
    if (!Number.isFinite(alignment)) alignment = 1;
    return { newYaw, alignment: Math.max(-1, Math.min(1, alignment)) };
}

/**
 * Runtime IDs for the web shell: prefix sanitize + 10-char suffix.
 */
const RUNTIME_ID_ALPHABET = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
const SUFFIX_LENGTH = 10;
const ALPHABET_SIZE = 62;
const REJECTION_CUTOFF = 252;

function isAsciiSpace(character) {
    const code = character.charCodeAt(0);
    return code === 32 || (code >= 9 && code <= 13);
}

export function sanitizeRuntimeIdPrefix(prefix = 'rt') {
    let trimmed = String(prefix ?? 'rt');
    let start = 0;
    let end = trimmed.length;
    while (start < end && isAsciiSpace(trimmed[start])) start += 1;
    while (end > start && isAsciiSpace(trimmed[end - 1])) end -= 1;
    trimmed = trimmed.slice(start, end);

    let sanitized = '';
    let previousWasHyphen = false;
    for (let i = 0; i < trimmed.length; i += 1) {
        const c = trimmed[i];
        const code = c.charCodeAt(0);
        const valid =
            (code >= 48 && code <= 57) ||
            (code >= 65 && code <= 90) ||
            (code >= 97 && code <= 122) ||
            c === '_' ||
            c === '-';
        if (valid) {
            sanitized += c;
            previousWasHyphen = false;
        } else if (!previousWasHyphen) {
            sanitized += '-';
            previousWasHyphen = true;
        }
    }
    while (sanitized.startsWith('-')) sanitized = sanitized.slice(1);
    while (sanitized.endsWith('-')) sanitized = sanitized.slice(0, -1);
    return sanitized.length > 0 ? sanitized : 'rt';
}

function randomSuffixUniform() {
    if (typeof crypto !== 'undefined' && typeof crypto.getRandomValues === 'function') {
        let suffix = '';
        while (suffix.length < SUFFIX_LENGTH) {
            const byte = new Uint8Array(1);
            crypto.getRandomValues(byte);
            if (byte[0] < REJECTION_CUTOFF) {
                suffix += RUNTIME_ID_ALPHABET[byte[0] % ALPHABET_SIZE];
            }
        }
        return suffix;
    }
    let fallback = '';
    for (let i = 0; i < SUFFIX_LENGTH; i += 1) {
        fallback += RUNTIME_ID_ALPHABET[Math.floor(Math.random() * ALPHABET_SIZE)];
    }
    return fallback;
}

export function createRuntimeId(prefix = 'rt') {
    const safePrefix = sanitizeRuntimeIdPrefix(prefix);
    return `${safePrefix}_${randomSuffixUniform()}`;
}

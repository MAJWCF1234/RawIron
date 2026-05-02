/** Math, volumes, spawns, ground probe, decal presets, runtime IDs (aligned with RawIron C++ where noted). */
export const GameAssets = {
    IMG_PREFIX: './images/',
    MODEL_ASSET_PATH_PREFIX: './models/',
    MODEL_DATA_PATH_PREFIX: './models/',
    textureManifest: {}
};

/** Level validation messages used by `index.js` when loading JSON. */
export const STRINGS = {
    msg: {
        levelInvalid: 'Invalid level data. Check level JSON format.',
        levelNotObject: 'Level file must be a JSON object (not an array or plain value).',
        levelJsonParse: 'Level file is not valid JSON.'
    }
};

export const DEFAULT_KEYS = {
    forward: 'KeyW',
    backward: 'KeyS',
    left: 'KeyA',
    right: 'KeyD'
};

// --- Math finite (C++: `ri::math::FiniteVec3FromSpan`, `FiniteQuatComponents`, …) ---

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

// --- Authoring volumes (C++: `ri::spatial::PointInsideAuthoringVolume`) ---

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

// --- Player starts (C++: `ri::validation::LevelPlayerSpawn`) ---

/** First `player_start` in array order (matches `TryGetPrimaryPlayerStart`). */
export function tryGetPrimaryPlayerStart(spawners) {
    if (!Array.isArray(spawners)) return null;
    for (const s of spawners) {
        if (s && typeof s.type === 'string' && s.type.toLowerCase() === 'player_start') {
            return s;
        }
    }
    return null;
}

// --- Ground probe (C++: `ri::trace::ProbeGroundAtFeet`) ---

/**
 * Single downward ray from above the feet AABB; the heavier multi-probe scoring lives in `ri::trace`.
 * @param {object} game
 * @param {import('three').Vector3} baseFeetPosition
 * @param {object} ctx
 */
export function probeGroundAtFeet(game, baseFeetPosition, ctx) {
    if (!game?.findGroundHit || !baseFeetPosition) return null;
    const probeLift = Math.max(0.2, ctx.maxDistance + 0.1);
    ctx.groundProbeOrigin.copy(baseFeetPosition);
    ctx.groundProbeOrigin.y += probeLift;
    const hit = game.findGroundHit(ctx.groundProbeOrigin, {
        maxDistance: probeLift + 0.1,
        minNormalY: ctx.minNormalY,
        traceTag: 'player'
    });
    if (!hit?.point) return null;
    const relativeY = hit.point.y - baseFeetPosition.y;
    if (relativeY > Math.max(ctx.allowAbove, ctx.getGroundProbeAllowAbove())) return null;
    hit.clearance = Math.max(0, baseFeetPosition.y - hit.point.y);
    hit.supportDelta = relativeY;
    return hit;
}

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

const DECAL_PRESET_HINTS = {
    blood: { alphaTest: 0.45, metalness: 0.02, roughnessFactor: 0.98, emissiveIntensity: 0.05, uvScalePerMeterU: 0, uvScalePerMeterV: 0 },
    cable: { alphaTest: 0.35, metalness: 0.08, roughnessFactor: 0.92, emissiveIntensity: 0, uvScalePerMeterU: 5.0, uvScalePerMeterV: 2.222 },
    hazard: { alphaTest: 0.35, metalness: 0.18, roughnessFactor: 0.84, emissiveIntensity: 0.18, uvScalePerMeterU: 1.25, uvScalePerMeterV: 1.25 }
};

/** Returns hints that mirror C++ `DecalAuthoringMaterialHints`, or null when the preset is unknown. */
export function resolveAuthoringDecalPreset(preset) {
    if (typeof preset !== 'string') return null;
    return DECAL_PRESET_HINTS[preset.toLowerCase()] || null;
}

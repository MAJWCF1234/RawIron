/**
 * Combined engine module (formerly ./engine/*.js and structuralPrimitives).
 */
import mitt from 'mitt';
import * as THREE from 'three';

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

function equalsInsensitive(a, b) {
    return a.length === b.length && a.toLowerCase() === b.toLowerCase();
}

function normalizeWhitespace(value) {
    return value.replace(/\s+/g, '_');
}

function toTitleCase(value) {
    let out = '';
    let uppercaseNext = true;
    for (let i = 0; i < value.length; i += 1) {
        const ch = value[i];
        if (ch === '_' || ch === '-') {
            if (out.length > 0 && out[out.length - 1] !== ' ') out += ' ';
            uppercaseNext = true;
            continue;
        }
        if (uppercaseNext) {
            out += ch.toUpperCase();
            uppercaseNext = false;
        } else {
            out += ch.toLowerCase();
        }
    }
    return out;
}

export function normalizeKeyboardInputId(inputId) {
    let normalized = String(inputId || '').replace(/\s+/g, '');
    if (!normalized) return '';
    normalized = normalizeWhitespace(normalized);

    if (equalsInsensitive(normalized, 'mouse1') || equalsInsensitive(normalized, 'mouseleft')) return 'MouseLeft';
    if (equalsInsensitive(normalized, 'mouse2') || equalsInsensitive(normalized, 'mouseright')) return 'MouseRight';
    if (equalsInsensitive(normalized, 'mouse3') || equalsInsensitive(normalized, 'mousemiddle')) return 'MouseMiddle';
    if (equalsInsensitive(normalized, 'space') || equalsInsensitive(normalized, 'spacebar')) return 'Space';
    if (equalsInsensitive(normalized, 'esc')) return 'Escape';
    if (equalsInsensitive(normalized, 'ctrl') || equalsInsensitive(normalized, 'control')) return 'ControlLeft';
    if (equalsInsensitive(normalized, 'shift')) return 'ShiftLeft';
    if (equalsInsensitive(normalized, 'alt')) return 'AltLeft';
    return normalized;
}

export function formatNormalizedKeyboardLabel(normalized) {
    if (!normalized) return '';
    if (normalized === 'Space') return 'Space';
    if (normalized === 'Tab') return 'Tab';
    if (normalized === 'Escape') return 'Esc';
    if (normalized === 'ShiftLeft' || normalized === 'ShiftRight') return 'Shift';
    if (normalized === 'ControlLeft' || normalized === 'ControlRight') return 'Ctrl';
    if (normalized === 'AltLeft' || normalized === 'AltRight') return 'Alt';
    if (normalized === 'MouseLeft') return 'Mouse 1';
    if (normalized === 'MouseRight') return 'Mouse 2';
    if (normalized === 'MouseMiddle') return 'Mouse 3';
    if (normalized.startsWith('Digit') && normalized.length === 6) return normalized.slice(5);
    if (normalized.startsWith('Key') && normalized.length === 4) return normalized.slice(3);
    if (normalized.startsWith('F') && normalized.length <= 3) {
        const rest = normalized.slice(1);
        if (rest.length > 0 && [...rest].every((c) => c >= '0' && c <= '9')) return normalized;
    }
    return toTitleCase(normalized);
}

export function formatInputLabelFromInputId(code) {
    const n = normalizeKeyboardInputId(code);
    return formatNormalizedKeyboardLabel(n);
}

export function keyCodeToLabel(code) {
    return formatInputLabelFromInputId(String(code || ''));
}

export function buildExternalModelCandidateTypes(modelConfig = {}) {
    const candidates = [];
    const baseType = String(modelConfig?.type || '').toLowerCase();
    if (baseType) candidates.push(baseType);
    if (Array.isArray(modelConfig?.fallbackTypes)) {
        modelConfig.fallbackTypes.forEach((candidate) => {
            const normalized = String(candidate || '').toLowerCase();
            if (normalized && !candidates.includes(normalized)) candidates.push(normalized);
        });
    }
    const inferredExt = String(modelConfig?.path || '').split('.').pop().toLowerCase();
    if (inferredExt && !candidates.includes(inferredExt)) candidates.push(inferredExt);
    return candidates;
}

const DEFAULT_EPSILON = 1e-5;

function cloneVertexArray(vertices = []) {
    return vertices.map((vertex) => vertex.clone());
}

function dedupeSequentialVertices(vertices, epsilon = DEFAULT_EPSILON) {
    const result = [];
    for (const vertex of vertices) {
        if (!vertex) continue;
        const previous = result[result.length - 1];
        if (previous && previous.distanceToSquared(vertex) <= (epsilon * epsilon)) continue;
        result.push(vertex.clone());
    }
    if (result.length >= 2) {
        const first = result[0];
        const last = result[result.length - 1];
        if (first.distanceToSquared(last) <= (epsilon * epsilon)) result.pop();
    }
    return result;
}

function dedupeVertices(vertices, epsilon = DEFAULT_EPSILON) {
    const unique = [];
    for (const vertex of vertices) {
        if (!vertex) continue;
        if (unique.some((candidate) => candidate.distanceToSquared(vertex) <= (epsilon * epsilon))) continue;
        unique.push(vertex.clone());
    }
    return unique;
}

function computePolygonPlane(vertices) {
    if (!Array.isArray(vertices) || vertices.length < 3) return null;
    for (let index = 0; index < vertices.length - 2; index += 1) {
        const a = vertices[index];
        const b = vertices[index + 1];
        const c = vertices[index + 2];
        const edgeA = b.clone().sub(a);
        const edgeB = c.clone().sub(a);
        const normal = edgeA.cross(edgeB);
        if (normal.lengthSq() <= 1e-12) continue;
        normal.normalize();
        return new THREE.Plane().setFromNormalAndCoplanarPoint(normal, a);
    }
    return null;
}

function makePlaneBasis(normal) {
    const tangent = new THREE.Vector3();
    const bitangent = new THREE.Vector3();
    const reference = Math.abs(normal.y) < 0.99
        ? new THREE.Vector3(0, 1, 0)
        : new THREE.Vector3(1, 0, 0);
    tangent.crossVectors(reference, normal).normalize();
    bitangent.crossVectors(normal, tangent).normalize();
    return { tangent, bitangent };
}

export function classifyPointToPlane(point, plane, epsilon = DEFAULT_EPSILON) {
    const distance = plane.distanceToPoint(point);
    if (distance > epsilon) return 'front';
    if (distance < -epsilon) return 'back';
    return 'coplanar';
}

export function createPlaneFromPointNormal(point, normal) {
    const safeNormal = normal.clone().normalize();
    return new THREE.Plane().setFromNormalAndCoplanarPoint(safeNormal, point.clone());
}

export function createAxisAlignedBoxSolid(min = [-0.5, -0.5, -0.5], max = [0.5, 0.5, 0.5]) {
    const lo = new THREE.Vector3(min[0], min[1], min[2]);
    const hi = new THREE.Vector3(max[0], max[1], max[2]);
    const v = {
        lbf: new THREE.Vector3(lo.x, lo.y, hi.z),
        rbf: new THREE.Vector3(hi.x, lo.y, hi.z),
        rbb: new THREE.Vector3(hi.x, lo.y, lo.z),
        lbb: new THREE.Vector3(lo.x, lo.y, lo.z),
        ltf: new THREE.Vector3(lo.x, hi.y, hi.z),
        rtf: new THREE.Vector3(hi.x, hi.y, hi.z),
        rtb: new THREE.Vector3(hi.x, hi.y, lo.z),
        ltb: new THREE.Vector3(lo.x, hi.y, lo.z)
    };
    const polygons = [
        { vertices: [v.lbf, v.rbf, v.rtf, v.ltf] }, // front
        { vertices: [v.rbb, v.lbb, v.ltb, v.rtb] }, // back
        { vertices: [v.lbb, v.lbf, v.ltf, v.ltb] }, // left
        { vertices: [v.rbf, v.rbb, v.rtb, v.rtf] }, // right
        { vertices: [v.ltf, v.rtf, v.rtb, v.ltb] }, // top
        { vertices: [v.lbb, v.rbb, v.rbf, v.lbf] }  // bottom
    ].map((polygon) => ({
        vertices: cloneVertexArray(polygon.vertices),
        plane: computePolygonPlane(polygon.vertices)
    }));
    return { polygons };
}

export function sortCoplanarPoints(points, plane, epsilon = DEFAULT_EPSILON) {
    const unique = dedupeVertices(points, epsilon);
    if (unique.length <= 2) return unique;
    const centroid = unique.reduce((sum, point) => sum.add(point), new THREE.Vector3()).multiplyScalar(1 / unique.length);
    const { tangent, bitangent } = makePlaneBasis(plane.normal);
    return unique.sort((a, b) => {
        const va = a.clone().sub(centroid);
        const vb = b.clone().sub(centroid);
        const angleA = Math.atan2(va.dot(bitangent), va.dot(tangent));
        const angleB = Math.atan2(vb.dot(bitangent), vb.dot(tangent));
        return angleA - angleB;
    });
}

export function clipConvexPolygonByPlane(polygon, plane, epsilon = DEFAULT_EPSILON) {
    const vertices = Array.isArray(polygon?.vertices) ? polygon.vertices : [];
    if (vertices.length < 3) return { front: null, back: null, intersections: [] };

    const frontVertices = [];
    const backVertices = [];
    const intersections = [];

    for (let index = 0; index < vertices.length; index += 1) {
        const current = vertices[index];
        const next = vertices[(index + 1) % vertices.length];
        const currentDistance = plane.distanceToPoint(current);
        const nextDistance = plane.distanceToPoint(next);
        const currentFront = currentDistance >= -epsilon;
        const currentBack = currentDistance <= epsilon;

        if (currentFront) frontVertices.push(current.clone());
        if (currentBack) backVertices.push(current.clone());

        const crosses = (currentDistance > epsilon && nextDistance < -epsilon)
            || (currentDistance < -epsilon && nextDistance > epsilon);
        if (!crosses) continue;

        const t = currentDistance / (currentDistance - nextDistance);
        const intersection = current.clone().lerp(next, t);
        frontVertices.push(intersection.clone());
        backVertices.push(intersection.clone());
        intersections.push(intersection);
    }

    const front = dedupeSequentialVertices(frontVertices, epsilon);
    const back = dedupeSequentialVertices(backVertices, epsilon);
    return {
        front: front.length >= 3 ? {
            plane: polygon.plane?.clone?.() || computePolygonPlane(front),
            vertices: front
        } : null,
        back: back.length >= 3 ? {
            plane: polygon.plane?.clone?.() || computePolygonPlane(back),
            vertices: back
        } : null,
        intersections: dedupeVertices(intersections, epsilon)
    };
}

export function clipConvexSolidByPlane(solid, splitPlane, epsilon = DEFAULT_EPSILON) {
    const polygons = Array.isArray(solid?.polygons) ? solid.polygons : [];
    const frontPolygons = [];
    const backPolygons = [];
    const cutPoints = [];

    for (const polygon of polygons) {
        const clipped = clipConvexPolygonByPlane(polygon, splitPlane, epsilon);
        if (clipped.front) frontPolygons.push(clipped.front);
        if (clipped.back) backPolygons.push(clipped.back);
        cutPoints.push(...clipped.intersections);
    }

    const uniqueCutPoints = dedupeVertices(cutPoints, epsilon);
    if (uniqueCutPoints.length >= 3) {
        const frontCapVertices = sortCoplanarPoints(uniqueCutPoints, splitPlane, epsilon);
        const backCapVertices = [...frontCapVertices].reverse().map((vertex) => vertex.clone());
        frontPolygons.push({
            plane: splitPlane.clone(),
            vertices: frontCapVertices
        });
        backPolygons.push({
            plane: splitPlane.clone().negate(),
            vertices: backCapVertices
        });
    }

    return {
        front: frontPolygons.length > 0 ? { polygons: frontPolygons } : null,
        back: backPolygons.length > 0 ? { polygons: backPolygons } : null,
        split: uniqueCutPoints.length >= 2,
        capPoints: uniqueCutPoints
    };
}

export function buildBufferGeometryFromConvexSolid(solid) {
    const positions = [];
    const normals = [];
    const polygons = Array.isArray(solid?.polygons) ? solid.polygons : [];

    for (const polygon of polygons) {
        const vertices = Array.isArray(polygon?.vertices) ? polygon.vertices : [];
        if (vertices.length < 3) continue;
        const plane = polygon.plane || computePolygonPlane(vertices);
        if (!plane) continue;
        const normal = plane.normal.clone().normalize();
        for (let index = 1; index < vertices.length - 1; index += 1) {
            const tri = [vertices[0], vertices[index], vertices[index + 1]];
            for (const vertex of tri) {
                positions.push(vertex.x, vertex.y, vertex.z);
                normals.push(normal.x, normal.y, normal.z);
            }
        }
    }

    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
    geometry.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));
    geometry.computeBoundingBox();
    geometry.computeBoundingSphere();
    return geometry;
}

const PLANE_EPSILON = 1e-4;

function clonePolygon(polygon) {
    return {
        plane: polygon?.plane?.clone?.() || null,
        vertices: Array.isArray(polygon?.vertices) ? polygon.vertices.map((vertex) => vertex.clone()) : []
    };
}

function cloneSolid(solid) {
    return {
        polygons: Array.isArray(solid?.polygons) ? solid.polygons.map(clonePolygon) : []
    };
}

export function computeSolidBounds(solid) {
    const bounds = new THREE.Box3();
    let hasVertex = false;
    for (const polygon of solid?.polygons || []) {
        for (const vertex of polygon?.vertices || []) {
            bounds.expandByPoint(vertex);
            hasVertex = true;
        }
    }
    return hasVertex ? bounds : null;
}

export function transformSolid(solid, matrix) {
    const normalMatrix = new THREE.Matrix3().getNormalMatrix(matrix);
    return {
        polygons: (solid?.polygons || []).map((polygon) => {
            const vertices = (polygon?.vertices || []).map((vertex) => vertex.clone().applyMatrix4(matrix));
            const plane = polygon?.plane?.clone?.() || null;
            if (plane) {
                const coplanarPoint = vertices[0]?.clone?.() || new THREE.Vector3();
                const normal = plane.normal.clone().applyMatrix3(normalMatrix).normalize();
                return {
                    vertices,
                    plane: new THREE.Plane().setFromNormalAndCoplanarPoint(normal, coplanarPoint)
                };
            }
            return { vertices, plane: null };
        })
    };
}

export function createWorldSpaceBoxSolid(matrix, min = [-0.5, -0.5, -0.5], max = [0.5, 0.5, 0.5]) {
    const local = createAxisAlignedBoxSolid(min, max);
    return transformSolid(local, matrix);
}

export function extractConvexPlanesFromGeometry(geometry, matrix) {
    if (!geometry) return [];
    const source = geometry.index ? geometry.toNonIndexed() : geometry.clone();
    const position = source.getAttribute('position');
    const planes = [];

    for (let index = 0; index < position.count; index += 3) {
        const a = new THREE.Vector3().fromBufferAttribute(position, index).applyMatrix4(matrix);
        const b = new THREE.Vector3().fromBufferAttribute(position, index + 1).applyMatrix4(matrix);
        const c = new THREE.Vector3().fromBufferAttribute(position, index + 2).applyMatrix4(matrix);
        const plane = new THREE.Plane().setFromCoplanarPoints(a, b, c);
        if (!Number.isFinite(plane.normal.lengthSq()) || plane.normal.lengthSq() <= 1e-8) continue;
        const duplicate = planes.some((candidate) => {
            const sameNormal = candidate.normal.distanceToSquared(plane.normal) <= (PLANE_EPSILON * PLANE_EPSILON);
            const flippedNormal = candidate.normal.distanceToSquared(plane.normal.clone().negate()) <= (PLANE_EPSILON * PLANE_EPSILON);
            const sameConstant = Math.abs(candidate.constant - plane.constant) <= PLANE_EPSILON;
            const flippedConstant = Math.abs(candidate.constant + plane.constant) <= PLANE_EPSILON;
            return (sameNormal && sameConstant) || (flippedNormal && flippedConstant);
        });
        if (!duplicate) planes.push(plane);
    }

    source.dispose();
    return planes;
}

export function subtractConvexPlanesFromSolid(solid, planes) {
    let insidePieces = [cloneSolid(solid)];
    const outsidePieces = [];
    for (const plane of planes || []) {
        const nextInsidePieces = [];
        for (const candidate of insidePieces) {
            const clipped = clipConvexSolidByPlane(candidate, plane);
            if (clipped.front) outsidePieces.push(clipped.front);
            if (clipped.back) nextInsidePieces.push(clipped.back);
        }
        insidePieces = nextInsidePieces;
        if (insidePieces.length === 0) break;
    }
    return outsidePieces;
}

export function intersectSolidWithConvexPlanes(solid, planes) {
    let pieces = [cloneSolid(solid)];
    for (const plane of planes || []) {
        const nextPieces = [];
        for (const candidate of pieces) {
            const clipped = clipConvexSolidByPlane(candidate, plane);
            if (clipped.back) nextPieces.push(clipped.back);
        }
        pieces = nextPieces;
        if (pieces.length === 0) break;
    }
    return pieces;
}

export function buildCompiledGeometryNodesFromSolids(baseNode, solids, idPrefix) {
    return (solids || []).map((solid, index) => ({
        ...baseNode,
        id: baseNode.id ? `${idPrefix || baseNode.id}_fragment_${index + 1}` : undefined,
        name: baseNode.name ? `${baseNode.name} fragment ${index + 1}` : undefined,
        compiledGeometry: buildBufferGeometryFromConvexSolid(solid),
        compiledWorldSpace: true,
        _compiledFromStructuralCsg: true
    }));
}

const POST_BUILD_TYPES = new Set([
    'terrain_hole_cutout',
    'shrinkwrap_modifier_primitive',
    'auto_fillet_boolean_primitive',
    'volumetric_csg_blender',
    'sdf_organic_blend_primitive',
    'sdf_intersection_node',
    'sdf_blend_node',
    'shadow_exclusion_volume'
]);

const FRAME_TYPES = new Set([
    'surface_velocity_primitive',
    'kinematic_translation_primitive',
    'kinematic_rotation_primitive',
    'spline_path_follower_primitive',
    'recursive_fractal_primitive',
    'instanced_recursive_geometry_node',
    'cable_primitive'
]);

const RUNTIME_TYPES = new Set([
    'portal',
    'anti_portal',
    'occlusion_portal',
    'clipping_volume',
    'filtered_collision_volume',
    'camera_blocking_volume',
    'ai_perception_blocker_volume',
    'damage_volume',
    'kill_volume',
    'camera_modifier_volume',
    'safe_zone_volume',
    'reflection_probe_volume',
    'light_importance_volume',
    'probe_grid_bounds',
    'localized_fog_volume',
    'volumetric_fog_blocker',
    'culling_distance_volume',
    'post_process_volume',
    'audio_reverb_volume',
    'audio_occlusion_volume',
    'spatial_query_volume',
    'traversal_link_volume',
    'ladder_volume',
    'climb_volume',
    'physics_volume',
    'custom_gravity_volume',
    'directional_wind_volume',
    'buoyancy_volume',
    'fluid_simulation_volume',
    'navmesh_bounds_volume',
    'navmesh_exclusion_volume',
    'radial_force_volume',
    'physics_constraint_volume',
    'convex_decomposition_generator',
    'automatic_convex_subdivision_modifier',
    'adaptive_lod_tessellator',
    'seamless_adaptive_resolution_primitive',
    'text_3d_primitive',
    'pivot_anchor_primitive',
    'streaming_level_volume',
    'checkpoint_spawn_volume',
    'hint_skip_brush',
    'teleport_volume',
    'launch_volume',
    'ambient_audio_volume',
    'ambient_audio_spline',
    'lod_override_volume',
    'navmesh_modifier_volume',
    'voxel_gi_bounds',
    'lightmap_density_volume',
    'light_portal'
]);

const PHASE_RANK = Object.freeze({
    compile: 0,
    runtime: 1,
    post_build: 2,
    frame: 3
});

function getNodeId(node, index) {
    if (node?.id && typeof node.id === 'string') return node.id;
    return `__structural_graph_${node?.type || 'node'}_${index}`;
}

function classifyPhase(node) {
    const type = typeof node?.type === 'string' ? node.type : '';
    if (FRAME_TYPES.has(type)) return 'frame';
    if (POST_BUILD_TYPES.has(type)) return 'post_build';
    if (RUNTIME_TYPES.has(type)) return 'runtime';
    return 'compile';
}

function getExplicitDependencies(node) {
    const deps = new Set();
    const appendIds = (value) => {
        if (!Array.isArray(value)) return;
        value.forEach((entry) => {
            if (typeof entry === 'string' && entry) deps.add(entry);
        });
    };
    appendIds(node?.targetIds);
    appendIds(node?.childNodeList);
    appendIds(node?.targetAIds);
    appendIds(node?.targetBIds);
    if (typeof node?.pivotAnchorId === 'string' && node.pivotAnchorId) deps.add(node.pivotAnchorId);
    if (typeof node?.anchorId === 'string' && node.anchorId) deps.add(node.anchorId);
    return [...deps];
}

export function buildStructuralDependencyGraph(nodes = []) {
    const entries = (Array.isArray(nodes) ? nodes : [])
        .filter((node) => node && typeof node === 'object')
        .map((node, index) => ({
            node,
            index,
            id: getNodeId(node, index),
            phase: classifyPhase(node),
            deps: getExplicitDependencies(node),
            incoming: 0,
            outgoing: new Set()
        }));

    const byId = new Map(entries.map((entry) => [entry.id, entry]));
    const unresolvedDependencies = [];
    let edgeCount = 0;

    entries.forEach((entry) => {
        entry.deps.forEach((depId) => {
            const dependency = byId.get(depId);
            if (!dependency) {
                unresolvedDependencies.push({ nodeId: entry.id, dependencyId: depId });
                return;
            }
            if (!dependency.outgoing.has(entry.id)) {
                dependency.outgoing.add(entry.id);
                entry.incoming += 1;
                edgeCount += 1;
            }
            if (PHASE_RANK[dependency.phase] > PHASE_RANK[entry.phase]) entry.phase = dependency.phase;
        });
    });

    const queue = entries
        .filter((entry) => entry.incoming === 0)
        .sort((a, b) => (PHASE_RANK[a.phase] - PHASE_RANK[b.phase]) || (a.index - b.index));
    const ordered = [];

    while (queue.length > 0) {
        const entry = queue.shift();
        ordered.push(entry);
        entry.outgoing.forEach((targetId) => {
            const target = byId.get(targetId);
            if (!target) return;
            target.incoming -= 1;
            if (target.incoming === 0) {
                queue.push(target);
                queue.sort((a, b) => (PHASE_RANK[a.phase] - PHASE_RANK[b.phase]) || (a.index - b.index));
            }
        });
    }

    const cyclicEntries = entries.filter((entry) => !ordered.includes(entry));
    if (cyclicEntries.length > 0) {
        cyclicEntries.sort((a, b) => a.index - b.index);
        ordered.push(...cyclicEntries);
    }

    const phaseBuckets = {
        compile: 0,
        runtime: 0,
        post_build: 0,
        frame: 0
    };
    ordered.forEach((entry) => {
        phaseBuckets[entry.phase] = (phaseBuckets[entry.phase] || 0) + 1;
    });

    return {
        orderedNodes: ordered.map((entry) => entry.node),
        summary: {
            nodeCount: ordered.length,
            edgeCount,
            cycleCount: cyclicEntries.length > 0 ? 1 : 0,
            unresolvedDependencyCount: unresolvedDependencies.length,
            unresolvedDependencies,
            phaseBuckets
        }
    };
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

export function createRuntimeEventBus() {
    const emitter = mitt();
    const metrics = {
        emitted: 0,
        listenersAdded: 0,
        listenersRemoved: 0
    };
    return {
        on(type, handler) {
            metrics.listenersAdded += 1;
            return emitter.on(type, handler);
        },
        off(type, handler) {
            metrics.listenersRemoved += 1;
            return emitter.off(type, handler);
        },
        emit(type, event) {
            metrics.emitted += 1;
            return emitter.emit(type, event);
        },
        clear() {
            emitter.all.clear();
        },
        getMetrics() {
            let activeListeners = 0;
            emitter.all.forEach((handlers) => {
                activeListeners += Array.isArray(handlers) ? handlers.length : 0;
            });
            return {
                emitted: metrics.emitted,
                listenersAdded: metrics.listenersAdded,
                listenersRemoved: metrics.listenersRemoved,
                activeListeners
            };
        }
    };
}

const validationMetrics = {
    tuningParses: 0,
    tuningParseFailures: 0,
    levelValidations: 0,
    levelValidationFailures: 0
};

export function parseStoredRuntimeTuning(value) {
    validationMetrics.tuningParses += 1;
    if (value === null || value === undefined || typeof value !== 'object' || Array.isArray(value)) {
        validationMetrics.tuningParseFailures += 1;
        return {};
    }
    const output = {};
    for (const [key, raw] of Object.entries(value)) {
        const type = typeof raw;
        if (type === 'number' && Number.isFinite(raw)) output[key] = raw;
        else if (type === 'string' || type === 'boolean') output[key] = raw;
    }
    return output;
}

export function validateLevelPayload(levelData, levelFilename = 'level') {
    validationMetrics.levelValidations += 1;
    if (levelData === null || levelData === undefined || typeof levelData !== 'object' || Array.isArray(levelData)) {
        validationMetrics.levelValidationFailures += 1;
        return `${levelFilename}: root must be an object.`;
    }
    const data = levelData;
    if (data.geometry !== undefined && !Array.isArray(data.geometry)) {
        validationMetrics.levelValidationFailures += 1;
        return `${levelFilename}: geometry must be an array when present.`;
    }
    if (data.lights !== undefined && !Array.isArray(data.lights)) {
        validationMetrics.levelValidationFailures += 1;
        return `${levelFilename}: lights must be an array when present.`;
    }
    if (data.events !== undefined && !Array.isArray(data.events)) {
        validationMetrics.levelValidationFailures += 1;
        return `${levelFilename}: events must be an array when present.`;
    }
    if (!data.levelName && !(data.geometry?.length > 0) && !(data.lights?.length > 0)) {
        validationMetrics.levelValidationFailures += 1;
        return 'Level must have levelName, geometry, or lights.';
    }
    return null;
}

export function getSchemaValidationMetrics() {
    return { ...validationMetrics };
}

const TUNING_RULES = Object.freeze({
    sensitivity: { min: 0.2, max: 3.0, defaultValue: 1.0 },
    staminaDrain: { min: 1.0, max: 60.0, defaultValue: 25.0 },
    staminaRegen: { min: 1.0, max: 60.0, defaultValue: 15.0 },
    walkSpeed: { min: 0.5, max: 12.0, defaultValue: 5.0 },
    sprintSpeed: { min: 0.5, max: 16.0, defaultValue: 8.0 },
    gravity: { min: 1.0, max: 60.0, defaultValue: 32.0 },
    jumpForce: { min: 0.5, max: 20.0, defaultValue: 9.6 },
    fallGravityMultiplier: { min: 0.5, max: 4.0, defaultValue: 1.4 },
    lowJumpGravityMultiplier: { min: 0.5, max: 4.0, defaultValue: 1.18 },
    maxFallSpeed: { min: 1.0, max: 50.0, defaultValue: 28.0 }
});

export const RUNTIME_TUNING_KEYS = Object.freeze([
    'sensitivity',
    'walkSpeed',
    'sprintSpeed',
    'gravity',
    'jumpForce',
    'fallGravityMultiplier',
    'lowJumpGravityMultiplier',
    'maxFallSpeed'
]);

export const RUNTIME_TUNING_DEFAULTS = Object.freeze(
    Object.fromEntries(RUNTIME_TUNING_KEYS.map((key) => [key, TUNING_RULES[key].defaultValue]))
);

export function sanitizeRuntimeTuningValue(key, rawValue) {
    const rule = TUNING_RULES[key];
    if (!rule) return undefined;
    const numeric = typeof rawValue === 'number' ? rawValue : parseFloat(String(rawValue));
    if (!Number.isFinite(numeric)) return rule.defaultValue;
    return clampFiniteNumber(numeric, rule.defaultValue, rule.min, rule.max);
}

export function roundSnapshotNumber(value, digits = 2) {
    const n = Number(value);
    if (!Number.isFinite(n)) return 0;
    const d = Math.min(15, Math.max(0, Math.trunc(digits)));
    return Number(n.toFixed(d));
}

export function buildRenderGameStateSnapshot(game) {
    const player = game.player;
    const position = player?.camera?.position;
    const velocity = player?.velocity;
    return {
        mode: 'hall',
        level: game.currentLevelFilename,
        objective: game.gameState?.objective || null,
        paused: !!game.gameState?.isPaused,
        player: player
            ? {
                  position: position
                      ? { x: roundSnapshotNumber(position.x, 3), y: roundSnapshotNumber(position.y, 3), z: roundSnapshotNumber(position.z, 3) }
                      : null,
                  velocity: velocity
                      ? { x: roundSnapshotNumber(velocity.x, 3), y: roundSnapshotNumber(velocity.y, 3), z: roundSnapshotNumber(velocity.z, 3) }
                      : null,
                  stance: player.stance,
                  onGround: !!player.onGround
              }
            : null,
        tuning: game.getRuntimeTuningSnapshot(),
        counts: {
            collidables: game.collidableMeshes?.length || 0,
            structuralCollidables: game.structuralCollidableMeshes?.length || 0,
            bvhMeshes: game.bvhMeshCount || 0,
            triggerVolumes: game.triggerVolumes?.length || 0,
            recursiveFractals: game.recursiveFractalPrimitives?.length || 0,
            convexHullAggregates: game.convexHullAggregates?.length || 0,
            decalProjectors: game.decalProjectors?.length || 0
        },
        environments: {
            postProcess: game.activePostProcessState
                ? {
                      label: game.activePostProcessState.label,
                      activeVolumes: [...(game.activePostProcessState.activeVolumes || [])],
                      tintStrength: roundSnapshotNumber(game.activePostProcessState.tintStrength || 0, 3),
                      blurAmount: roundSnapshotNumber(game.activePostProcessState.blurAmount || 0, 4)
                  }
                : null,
            audioReverb: game.activeAudioEnvironmentState
                ? {
                      label: game.activeAudioEnvironmentState.label,
                      activeVolumes: [...(game.activeAudioEnvironmentState.activeVolumes || [])],
                      reverbMix: roundSnapshotNumber(game.activeAudioEnvironmentState.reverbMix || 0, 3),
                      echoDelayMs: roundSnapshotNumber(game.activeAudioEnvironmentState.echoDelayMs || 0, 1)
                  }
                : null
        },
        coordinateSystem: 'threejs world coordinates; +x east/right, +y up, -z forward in most authored lanes'
    };
}

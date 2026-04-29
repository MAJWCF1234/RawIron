import * as THREE from 'three';

class BSPNode {
    constructor(entries, depth = 0, maxLeafSize = 12, maxDepth = 10) {
        this.entries = [];
        this.left = null;
        this.right = null;
        this.bounds = new THREE.Box3();
        this.axis = null;
        this.split = 0;

        if (!Array.isArray(entries) || entries.length === 0) return;
        this.bounds.copy(entries[0].box);
        for (let i = 1; i < entries.length; i++) {
            this.bounds.union(entries[i].box);
        }

        if (entries.length <= maxLeafSize || depth >= maxDepth) {
            this.entries = entries;
            return;
        }

        const size = this.bounds.getSize(new THREE.Vector3());
        this.axis = size.x >= size.z ? 'x' : 'z';
        const centers = entries.map((entry) => entry.center[this.axis]).sort((a, b) => a - b);
        this.split = centers[Math.floor(centers.length / 2)];

        const leftEntries = [];
        const rightEntries = [];
        for (const entry of entries) {
            if (entry.center[this.axis] <= this.split) leftEntries.push(entry);
            else rightEntries.push(entry);
        }

        if (leftEntries.length === 0 || rightEntries.length === 0) {
            this.entries = entries;
            this.axis = null;
            return;
        }

        this.left = new BSPNode(leftEntries, depth + 1, maxLeafSize, maxDepth);
        this.right = new BSPNode(rightEntries, depth + 1, maxLeafSize, maxDepth);
    }

    queryBox(box, out, seen) {
        if (!this.bounds || !this.bounds.intersectsBox(box)) return;
        if (!this.left && !this.right) {
            for (const entry of this.entries) {
                if (!entry.box.intersectsBox(box)) continue;
                if (seen.has(entry.object)) continue;
                seen.add(entry.object);
                out.push(entry.object);
            }
            return;
        }
        this.left?.queryBox(box, out, seen);
        this.right?.queryBox(box, out, seen);
    }

    queryRay(ray, far, segmentBox, hitPoint, out, seen) {
        if (!this.bounds || !this.bounds.intersectsBox(segmentBox)) return;
        if (!this.left && !this.right) {
            for (const entry of this.entries) {
                if (seen.has(entry.object)) continue;
                const hit = ray.intersectBox(entry.box, hitPoint);
                if (!hit) continue;
                if (hit.distanceTo(ray.origin) > far) continue;
                seen.add(entry.object);
                out.push(entry.object);
            }
            return;
        }
        this.left?.queryRay(ray, far, segmentBox, hitPoint, out, seen);
        this.right?.queryRay(ray, far, segmentBox, hitPoint, out, seen);
    }
}

export class BSPSpatialIndex {
    constructor(objects = []) {
        const entries = [];
        const center = new THREE.Vector3();
        for (const object of objects) {
            if (!object) continue;
            if (!object.userData.boundingBox || object.userData.dynamicCollider) {
                object.userData.boundingBox = new THREE.Box3().setFromObject(object);
            }
            const box = object.userData.boundingBox?.clone?.() ?? new THREE.Box3().setFromObject(object);
            if (!box || box.isEmpty()) continue;
            box.getCenter(center);
            entries.push({ object, box, center: center.clone() });
        }
        this.root = entries.length > 0 ? new BSPNode(entries) : null;
        this._ray = new THREE.Ray();
        this._segmentEnd = new THREE.Vector3();
        this._segmentBox = new THREE.Box3();
        this._hitPoint = new THREE.Vector3();
    }

    queryBox(box) {
        if (!this.root || !box) return [];
        const out = [];
        this.root.queryBox(box, out, new Set());
        return out;
    }

    queryRay(origin, direction, far) {
        if (!this.root || !origin || !direction || !Number.isFinite(far) || far <= 0) return [];
        if (!Number.isFinite(direction.x) || !Number.isFinite(direction.y) || !Number.isFinite(direction.z) || direction.lengthSq() < 1e-20) return [];
        this._ray.origin.copy(origin);
        this._ray.direction.copy(direction).normalize();
        this._segmentEnd.copy(direction).normalize().multiplyScalar(far).add(origin);
        this._segmentBox.makeEmpty().expandByPoint(origin).expandByPoint(this._segmentEnd);
        const out = [];
        this.root.queryRay(this._ray, far, this._segmentBox, this._hitPoint, out, new Set());
        return out;
    }
}

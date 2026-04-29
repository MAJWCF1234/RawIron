
import * as THREE from 'three';
import { PointerLockControls } from 'three/examples/jsm/controls/PointerLockControls.js';
 
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js';
import { ConvexGeometry } from 'three/examples/jsm/geometries/ConvexGeometry.js';
import { RoundedBoxGeometry } from 'three/examples/jsm/geometries/RoundedBoxGeometry.js';
import { ExtrudeGeometry } from 'three/src/geometries/ExtrudeGeometry.js';
import { acceleratedRaycast, computeBoundsTree, disposeBoundsTree } from 'three-mesh-bvh';
import {
    DEFAULT_KEYS,
    GameAssets,
    STRINGS,
    clampFiniteInteger,
    clampFiniteNumber,
    clampPickupMotion,
    createRuntimeId,
    finiteVec2Components,
    finiteVec3Components,
    finiteQuatComponents,
    finiteScaleComponents,
    pointInsideAuthoringVolume,
    stepYawToward
} from './engine.js';

THREE.BufferGeometry.prototype.computeBoundsTree = computeBoundsTree;
THREE.BufferGeometry.prototype.disposeBoundsTree = disposeBoundsTree;
THREE.Mesh.prototype.raycast = acceleratedRaycast;

const DEFAULT_PLAYER_TUNING = Object.freeze({
    walkSpeed: 5.0,
    sprintSpeed: 8.0,
    crouchSpeed: 2.5,
    proneSpeed: 1.5,
    gravity: 32.0,
    jumpForce: 9.6,
    fallGravityMultiplier: 1.4,
    lowJumpGravityMultiplier: 1.18,
    maxFallSpeed: 28.0,
    maxStepHeight: 0.7
});

/**
 * Player state and movement: position, velocity, stance (stand/crouch/prone), collision.
 * @param {PointerLockControls} controls - Pointer lock camera controls.
 * @param {AnomalousEchoGame} game - Game instance for collision and state.
 */
class Player {
    constructor(controls, game) {
        this.game = game;
        this.controls = controls;
        this.camera = controls.object;
        this.position = this.camera.position;
        this.velocity = new THREE.Vector3();
        this.direction = new THREE.Vector3();
        this.stance = 'standing';
        this.eyeHeights = { standing: 1.8, crouching: 1.2, prone: 0.6 };
        this.hullHeights = { standing: 1.62, crouching: 1.02, prone: 0.4 };
        this.hullRadius = { standing: 0.24, crouching: 0.26, prone: 0.3 };
        this.speeds = {
            walk: DEFAULT_PLAYER_TUNING.walkSpeed,
            sprint: DEFAULT_PLAYER_TUNING.sprintSpeed,
            crouch: DEFAULT_PLAYER_TUNING.crouchSpeed,
            prone: DEFAULT_PLAYER_TUNING.proneSpeed
        };
        this.onGround = false;
        this.targetHeight = this.eyeHeights.standing;
        this.gravity = DEFAULT_PLAYER_TUNING.gravity;
        this.jumpForce = DEFAULT_PLAYER_TUNING.jumpForce;
        this.fallGravityMultiplier = DEFAULT_PLAYER_TUNING.fallGravityMultiplier;
        this.lowJumpGravityMultiplier = DEFAULT_PLAYER_TUNING.lowJumpGravityMultiplier;
        this.maxFallSpeed = DEFAULT_PLAYER_TUNING.maxFallSpeed;
        this.maxStepHeight = DEFAULT_PLAYER_TUNING.maxStepHeight;
        this.groundSnapDistance = 0.14;
        this.groundContactOffset = 0.03;
        this.maxWalkableSlopeY = 0.45;
        this.collisionBox = new THREE.Box3();
        this.tempVector = new THREE.Vector3();
        this._collisionCenter = new THREE.Vector3();
        this.collisionSize = new THREE.Vector3(0.64, 0, 0.64);
        this.feetPosition = new THREE.Vector3();
        this.eyeOffset = new THREE.Vector3(0, this.targetHeight, 0);
        this.hullBottom = new THREE.Vector3();
        this.hullTop = new THREE.Vector3();
        this.groundRaycaster = new THREE.Raycaster();
        this.groundDirection = new THREE.Vector3(0, -1, 0);
        this.forwardVector = new THREE.Vector3();
        this.rightVector = new THREE.Vector3();
        this.moveDirection = new THREE.Vector3();
        this.surfaceNormal = new THREE.Vector3(0, 1, 0);
        this.surfacePoint = new THREE.Vector3();
        this.horizontalVelocity = new THREE.Vector3();
        this.projectedVelocity = new THREE.Vector3();
        this.surfaceMoveDirection = new THREE.Vector3();
        this.stepProbeOffset = new THREE.Vector3();
        this.groundProbeOrigin = new THREE.Vector3();
        this.groundProbePosition = new THREE.Vector3();
        this.slideDelta = new THREE.Vector3();
        this._sweepDelta = new THREE.Vector3();
        this._hullSampleSize = new THREE.Vector3();
        this._hullSampleBox = new THREE.Box3();
        this._hullTraceHit = null;
        this._hullMoveStep = new THREE.Vector3();
        this._hullMoveRemaining = new THREE.Vector3();
        this._hullMoveClip = new THREE.Vector3();
        this._hullMoveNudge = new THREE.Vector3();
        this._hullMoveDelta = new THREE.Vector3();
        this._hullSampleOffsets = [
            new THREE.Vector3(),
            new THREE.Vector3(),
            new THREE.Vector3(),
            new THREE.Vector3(),
            new THREE.Vector3(),
            new THREE.Vector3(),
            new THREE.Vector3()
        ];
        this._hullSampleBoxes = Array.from({ length: 7 }, () => new THREE.Box3());
        this._supportCheckBox = new THREE.Box3();
        this.lastGroundedAt = 0;
        this.jumpHeldLastFrame = false;

        this.baseFov = this.camera.fov;
        this.bobTimer = 0;
        this.syncCameraToBody();
    }

    applyTuning(tuning = {}) {
        if (Number.isFinite(tuning.walkSpeed)) this.speeds.walk = tuning.walkSpeed;
        if (Number.isFinite(tuning.sprintSpeed)) this.speeds.sprint = tuning.sprintSpeed;
        if (Number.isFinite(tuning.crouchSpeed)) this.speeds.crouch = tuning.crouchSpeed;
        if (Number.isFinite(tuning.proneSpeed)) this.speeds.prone = tuning.proneSpeed;
        if (Number.isFinite(tuning.gravity)) this.gravity = tuning.gravity;
        if (Number.isFinite(tuning.jumpForce)) this.jumpForce = tuning.jumpForce;
        if (Number.isFinite(tuning.fallGravityMultiplier)) this.fallGravityMultiplier = tuning.fallGravityMultiplier;
        if (Number.isFinite(tuning.lowJumpGravityMultiplier)) this.lowJumpGravityMultiplier = tuning.lowJumpGravityMultiplier;
        if (Number.isFinite(tuning.maxFallSpeed)) this.maxFallSpeed = tuning.maxFallSpeed;
        if (Number.isFinite(tuning.maxStepHeight)) this.maxStepHeight = tuning.maxStepHeight;
    }

    getTuningSnapshot() {
        return {
            walkSpeed: this.speeds.walk,
            sprintSpeed: this.speeds.sprint,
            crouchSpeed: this.speeds.crouch,
            proneSpeed: this.speeds.prone,
            gravity: this.gravity,
            jumpForce: this.jumpForce,
            fallGravityMultiplier: this.fallGravityMultiplier,
            lowJumpGravityMultiplier: this.lowJumpGravityMultiplier,
            maxFallSpeed: this.maxFallSpeed,
            maxStepHeight: this.maxStepHeight
        };
    }

    setStance(newStance) {
        const preservedFeet = this.feetPosition.clone();
        const valid = newStance && Object.prototype.hasOwnProperty.call(this.eyeHeights, newStance);
        if (!valid) {
            this.stance = 'standing';
        } else {
            this.stance = newStance;
        }
        this.targetHeight = this.eyeHeights[this.stance];
        this.feetPosition.copy(preservedFeet);
        this.syncCameraToBody();
    }

    getCurrentEyeHeight() {
        return this.eyeHeights[this.stance] || this.eyeHeights.standing;
    }

    getCurrentHullHeight() {
        return this.hullHeights[this.stance] || this.hullHeights.standing;
    }

    getCurrentHullRadius() {
        return this.hullRadius[this.stance] || this.hullRadius.standing;
    }

    syncCameraToBody() {
        this.eyeOffset.set(0, this.getCurrentEyeHeight(), 0);
        this.camera.position.copy(this.feetPosition).add(this.eyeOffset);
    }

    syncBodyToCamera() {
        this.feetPosition.copy(this.camera.position).sub(this.eyeOffset.set(0, this.getCurrentEyeHeight(), 0));
    }

    setFeetPosition(position) {
        if (!position) return;
        const { x, y, z } = position;
        if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(z)) return;
        this.feetPosition.copy(position);
        this.syncCameraToBody();
    }

    getCurrentSpeed(isSprinting) {
        if (isSprinting && this.stance === 'standing') return this.speeds.sprint;
        if (this.stance === 'crouching') return this.speeds.crouch;
        if (this.stance === 'prone') return this.speeds.prone;
        return this.speeds.walk;
    }

    isSupportSurfaceCandidate(candidate, feetY, tolerance = 0.18) {
        if (!candidate || !this.game?.getCollisionBounds || !Number.isFinite(feetY)) return false;
        const candidateBox = this.game.getCollisionBounds(candidate, this._supportCheckBox);
        if (!candidateBox || candidateBox.isEmpty()) return false;
        return candidateBox.max.y <= feetY + tolerance && candidateBox.min.y <= feetY;
    }

    withSupportSurfaceFilter(options = {}, feetY = this.feetPosition.y, tolerance = 0.18) {
        const basePredicate = typeof options?.predicate === 'function' ? options.predicate : null;
        return {
            ...options,
            predicate: (candidate, hit) => {
                if (this.isSupportSurfaceCandidate(candidate, feetY, tolerance)) return false;
                return basePredicate ? basePredicate(candidate, hit) : true;
            }
        };
    }

    tryResolveStuck(basePosition = this.feetPosition) {
        if (!basePosition || !this.game?.traceCompoundBoxes) return false;
        const candidate = this.tempVector.clone();
        const step = Math.max(0.06, this.getCurrentHullRadius() * 0.45);
        const lift = Math.max(this.groundContactOffset + 0.02, 0.06);
        const offsets = [
            [0, 0, 0],
            [0, lift, 0],
            [0, this.maxStepHeight * 0.5, 0],
            [step, 0, 0], [-step, 0, 0], [0, 0, step], [0, 0, -step],
            [step, lift, 0], [-step, lift, 0], [0, lift, step], [0, lift, -step],
            [step, 0, step], [step, 0, -step], [-step, 0, step], [-step, 0, -step],
            [step * 2, lift, 0], [-step * 2, lift, 0], [0, lift, step * 2], [0, lift, -step * 2]
        ];
        for (const [x, y, z] of offsets) {
            candidate.copy(basePosition).add(this.stepProbeOffset.set(x, y, z));
            const hit = this.traceRoundedHullBox(candidate, this.withSupportSurfaceFilter({}, candidate.y, 0.24));
            if (!hit) {
                this.setFeetPosition(candidate);
                return true;
            }
        }
        return false;
    }

    forceUnstuckRecovery(basePosition = this.feetPosition) {
        if (!basePosition) return false;
        const origin = basePosition.clone();
        if (this.tryResolveStuck(origin)) {
            this.velocity.y = Math.max(this.velocity.y, this.jumpForce * 0.7);
            this.onGround = false;
            this.lastGroundedAt = -Infinity;
            this.syncCameraToBody();
            return true;
        }

        const candidate = new THREE.Vector3();
        const step = Math.max(0.16, this.getCurrentHullRadius() * 1.35);
        const lifts = [
            Math.max(0.12, this.groundContactOffset + 0.06),
            Math.max(0.28, this.maxStepHeight * 0.45),
            Math.max(0.5, this.maxStepHeight * 0.9),
            Math.max(0.85, this.maxStepHeight + 0.22),
            Math.max(1.2, this.getCurrentHullHeight() * 0.8),
            Math.max(1.7, this.getCurrentHullHeight() + 0.25)
        ];
        const radii = [0, step, step * 2, step * 3, step * 4, Math.max(1.8, step * 6)];
        const offsets = [];
        for (const radius of radii) {
            offsets.push([0, 0, 0]);
            if (radius <= 1e-6) continue;
            offsets.push(
                [radius, 0, 0], [-radius, 0, 0], [0, 0, radius], [0, 0, -radius],
                [radius, 0, radius], [radius, 0, -radius], [-radius, 0, radius], [-radius, 0, -radius]
            );
        }

        for (const lift of lifts) {
            for (const [x, y, z] of offsets) {
                candidate.copy(origin).add(this.stepProbeOffset.set(x, y + lift, z));
                const hit = this.traceRoundedHullBox(candidate, this.withSupportSurfaceFilter({}, candidate.y, 0.28));
                if (hit) continue;
                this.setFeetPosition(candidate);
                this.velocity.y = Math.max(this.velocity.y, this.jumpForce * 0.7);
                this.onGround = false;
                this.lastGroundedAt = -Infinity;
                return true;
            }
        }

        candidate.copy(origin);
        candidate.x += Math.max(step * 4, 1.6);
        candidate.y += Math.max(1.6, this.getCurrentHullHeight(), this.maxStepHeight + 1.0);
        this.setFeetPosition(candidate);
        this.velocity.y = Math.max(this.velocity.y, this.jumpForce * 0.9);
        this.onGround = false;
        this.lastGroundedAt = -Infinity;
        return true;
    }

    checkCollision() {
        if (!this.game.collidableMeshes || !Array.isArray(this.game.collidableMeshes)) return false;
        this.updateCollisionBoxAt();
        return !!this.traceRoundedHullBox(this.feetPosition, this.withSupportSurfaceFilter({}, this.feetPosition.y));
    }

    updateCollisionBoxAt(position = this.feetPosition) {
        const collisionHeight = this.getCurrentHullHeight();
        const radius = this.getCurrentHullRadius();
        this.collisionSize.y = collisionHeight;
        this.collisionSize.x = radius * 2;
        this.collisionSize.z = radius * 2;
        this._collisionCenter.copy(position).add(this.tempVector.set(0, collisionHeight * 0.5, 0));
        return this.collisionBox.setFromCenterAndSize(this._collisionCenter, this.collisionSize);
    }

    getRoundedHullSampleRadius() {
        return Math.max(0.12, this.getCurrentHullRadius() * 0.55);
    }

    getRoundedHullRingOffset() {
        return Math.max(0, this.getCurrentHullRadius() - this.getRoundedHullSampleRadius());
    }

    getRoundedHullSampleHeight() {
        const hullHeight = this.getCurrentHullHeight();
        const radius = this.getCurrentHullRadius();
        return Math.min(hullHeight, Math.max(radius * 1.8, hullHeight * 0.42));
    }

    forEachRoundedHullSample(position, callback) {
        const hullHeight = this.getCurrentHullHeight();
        const sampleHeight = this.getRoundedHullSampleHeight();
        const ring = this.getRoundedHullRingOffset();
        const midBandBottom = Math.max(0, (hullHeight - sampleHeight) * 0.5);
        const topBandBottom = Math.max(0, hullHeight - sampleHeight);
        const offsets = this._hullSampleOffsets;
        offsets[0].set(0, midBandBottom, 0);
        offsets[1].set(ring, midBandBottom, 0);
        offsets[2].set(-ring, midBandBottom, 0);
        offsets[3].set(0, midBandBottom, ring);
        offsets[4].set(0, midBandBottom, -ring);
        offsets[5].set(0, 0, 0);
        offsets[6].set(0, topBandBottom, 0);
        callback(offsets[0], 1.0, 1.0);
        callback(offsets[1], 1.0, 1.0);
        callback(offsets[2], 1.0, 1.0);
        callback(offsets[3], 1.0, 1.0);
        callback(offsets[4], 1.0, 1.0);
        callback(offsets[5], 0.58, 1.0);
        callback(offsets[6], 0.58, 1.0);
    }

    buildRoundedHullSampleBox(position, offset, radiusScale = 1.0, heightScale = 1.0, targetBox = this._hullSampleBox) {
        const hullHeight = this.getCurrentHullHeight();
        const sampleHeight = this.getRoundedHullSampleHeight() * heightScale;
        const sampleRadius = this.getRoundedHullSampleRadius() * radiusScale;
        this._hullSampleSize.set(sampleRadius * 2, sampleHeight, sampleRadius * 2);
        this._collisionCenter.copy(position).add(offset);
        this._collisionCenter.y += (sampleHeight * 0.5);
        targetBox.setFromCenterAndSize(this._collisionCenter, this._hullSampleSize);

        const minAllowedY = position.y;
        const maxAllowedY = position.y + hullHeight;
        if (targetBox.min.y < minAllowedY) {
            const lift = minAllowedY - targetBox.min.y;
            targetBox.min.y += lift;
            targetBox.max.y += lift;
        }
        if (targetBox.max.y > maxAllowedY) {
            const drop = targetBox.max.y - maxAllowedY;
            targetBox.min.y -= drop;
            targetBox.max.y -= drop;
        }
        return targetBox;
    }

    getRoundedHullSampleBoxes(position = this.feetPosition) {
        let sampleIndex = 0;
        this.forEachRoundedHullSample(position, (offset, radiusScale, heightScale) => {
            const targetBox = this._hullSampleBoxes[sampleIndex] || new THREE.Box3();
            this._hullSampleBoxes[sampleIndex] = this.buildRoundedHullSampleBox(position, offset, radiusScale, heightScale, targetBox);
            sampleIndex += 1;
        });
        return this._hullSampleBoxes.slice(0, sampleIndex);
    }

    traceRoundedHullBox(position = this.feetPosition, options = {}) {
        if (!this.game.traceCompoundBoxes) return null;
        return this.game.traceCompoundBoxes(this.getRoundedHullSampleBoxes(position), {
            traceTag: 'player',
            ...options
        });
    }

    traceRoundedHullSweep(position, deltaVector, options = {}) {
        if (!this.game.traceCompoundBoxesSweep || !deltaVector || deltaVector.lengthSq() <= 1e-10) return null;
        return this.game.traceCompoundBoxesSweep(this.getRoundedHullSampleBoxes(position), deltaVector, {
            traceTag: 'player',
            ...options
        });
    }

    moveWithSweep(deltaVector, options = {}) {
        if (!deltaVector || deltaVector.lengthSq() <= 1e-10) return null;
        this._sweepDelta.copy(deltaVector);
        this.updateCollisionBoxAt();
        if (!this.game.traceSweptBox) {
            this.feetPosition.add(this._sweepDelta);
            this.syncCameraToBody();
            return null;
        }
        const traceOptions = this._sweepDelta.y >= 0
            ? this.withSupportSurfaceFilter(options, this.feetPosition.y)
            : options;
        const hit = this.traceRoundedHullSweep(this.feetPosition, this._sweepDelta, traceOptions);
        if (!hit) {
            this.feetPosition.add(this._sweepDelta);
            this.syncCameraToBody();
            return null;
        }
        const moveTime = Math.max(0, hit.time - 0.001);
        this.feetPosition.addScaledVector(this._sweepDelta, moveTime);
        this.syncCameraToBody();
        return hit;
    }

    moveWithSlide(deltaVector, options = {}) {
        if (!deltaVector || deltaVector.lengthSq() <= 1e-10) return null;
        this.updateCollisionBoxAt();
        if (!this.game.slideMoveCompoundBoxes) {
            this.feetPosition.add(deltaVector);
            this.syncCameraToBody();
            return null;
        }
        this._hullMoveDelta.copy(this.feetPosition);
        const traceOptions = this.withSupportSurfaceFilter(options, this.feetPosition.y);
        const result = this.game.slideMoveCompoundBoxes(this.getRoundedHullSampleBoxes(this.feetPosition), deltaVector, traceOptions);
        if (result?.positionDelta) {
            this.feetPosition.add(result.positionDelta);
            this.syncCameraToBody();
        }
        return result;
    }

    getGroundProbeAllowAbove() {
        return Math.max(this.groundContactOffset + 0.16, 0.2);
    }

    probeGroundAtFeet(baseFeetPosition = this.feetPosition, maxDistance = this.groundSnapDistance + this.maxStepHeight + 0.35, allowAbove = 0.06) {
        if (!this.game.findGroundHit || !baseFeetPosition) return null;
        const radius = this.getCurrentHullRadius() * 0.55;
        const probes = [
            [0, 0],
            [radius, 0],
            [-radius, 0],
            [0, radius],
            [0, -radius]
        ];
        const probeLift = Math.max(0.2, maxDistance + 0.1);
        let bestHit = null;

        for (const [offsetX, offsetZ] of probes) {
            this.groundProbePosition.copy(baseFeetPosition);
            this.groundProbePosition.x += offsetX;
            this.groundProbePosition.z += offsetZ;
            this.groundProbeOrigin.copy(this.groundProbePosition);
            this.groundProbeOrigin.y += probeLift;
            const hit = this.game.findGroundHit(this.groundProbeOrigin, {
                maxDistance: probeLift + 0.1,
                minNormalY: this.maxWalkableSlopeY,
                traceTag: 'player'
            });
            if (!hit?.point) continue;

            const relativeY = hit.point.y - baseFeetPosition.y;
            if (relativeY > Math.max(allowAbove, this.getGroundProbeAllowAbove())) continue;
            const clearance = Math.max(0, baseFeetPosition.y - hit.point.y);
            const candidate = {
                ...hit,
                clearance,
                supportDelta: relativeY
            };

            if (!bestHit) {
                bestHit = candidate;
                continue;
            }

            const bestGap = Math.abs(bestHit.supportDelta ?? 0);
            const candidateGap = Math.abs(relativeY);
            if (candidateGap < bestGap - 1e-4 || (Math.abs(candidateGap - bestGap) <= 1e-4 && hit.point.y > bestHit.point.y)) {
                bestHit = candidate;
            }
        }

        return bestHit;
    }

    updateGroundState(allCollidables) {
        this.updateCollisionBoxAt();
        const groundHit = this.probeGroundAtFeet(this.feetPosition, this.groundSnapDistance + this.maxStepHeight + 0.35);

        if (!groundHit) {
            this.onGround = false;
            this.surfaceNormal.set(0, 1, 0);
            return null;
        }
        const clearance = Number.isFinite(groundHit.clearance) ? groundHit.clearance : Math.max(0, this.feetPosition.y - groundHit.point.y);
        groundHit.clearance = clearance;

        if (clearance > this.groundSnapDistance) {
            this.onGround = false;
            this.surfaceNormal.set(0, 1, 0);
            return groundHit;
        }

        this.onGround = true;
        const rawNormal = groundHit.normal || groundHit.worldNormal || groundHit.face?.normal || this.surfaceNormal;
        this.surfaceNormal.copy(rawNormal);
        if (!Number.isFinite(this.surfaceNormal.x) || !Number.isFinite(this.surfaceNormal.y) || !Number.isFinite(this.surfaceNormal.z) || this.surfaceNormal.lengthSq() < 1e-12) {
            this.surfaceNormal.set(0, 1, 0);
        } else {
            this.surfaceNormal.normalize();
        }
        this.surfacePoint.copy(groundHit.point);
        return groundHit;
    }

    tryStepUp(previousPosition) {
        if (!this.onGround) return false;
        const originalY = this.feetPosition.y;
        this.feetPosition.y = previousPosition.y + this.maxStepHeight;
        this.syncCameraToBody();
        if (this.checkCollision()) {
            this.feetPosition.y = originalY;
            this.syncCameraToBody();
            return false;
        }
        if (this.game.findGroundHit) {
            const steppedGroundHit = this.probeGroundAtFeet(this.feetPosition, this.maxStepHeight + 0.3, 0.08);
            if (!steppedGroundHit?.point) {
                this.feetPosition.y = originalY;
                this.syncCameraToBody();
                return false;
            }
            const supportGap = Math.abs(steppedGroundHit.point.y - this.feetPosition.y);
            const maxAcceptedGap = Math.max(this.groundSnapDistance + 0.02, this.maxStepHeight * 0.55);
            if (supportGap > maxAcceptedGap) {
                this.feetPosition.y = originalY;
                this.syncCameraToBody();
                return false;
            }
            this.feetPosition.y = steppedGroundHit.point.y + this.groundContactOffset;
            this.syncCameraToBody();
        }
        return true;
    }

    applyStepDown(maxDistance) {
        if (!(maxDistance > 0)) return null;
        const groundHit = this.probeGroundAtFeet(this.feetPosition, maxDistance, 0.04);
        if (!groundHit?.point) return null;
        const clearance = Number.isFinite(groundHit.clearance) ? groundHit.clearance : Math.max(0, this.feetPosition.y - groundHit.point.y);
        if (clearance > maxDistance) return null;
        this.feetPosition.y = groundHit.point.y + this.groundContactOffset;
        this.syncCameraToBody();
        this.onGround = true;
        this.surfacePoint.copy(groundHit.point);
        const rawNormal = groundHit.normal || groundHit.worldNormal || groundHit.face?.normal || this.surfaceNormal;
        this.surfaceNormal.copy(rawNormal);
        if (!Number.isFinite(this.surfaceNormal.x) || !Number.isFinite(this.surfaceNormal.y) || !Number.isFinite(this.surfaceNormal.z) || this.surfaceNormal.lengthSq() < 1e-12) {
            this.surfaceNormal.set(0, 1, 0);
        } else {
            this.surfaceNormal.normalize();
        }
        return groundHit;
    }

    update(delta, moveState) {
        if (this.controls.isLocked === true) {
            const dt = (!Number.isFinite(delta) || delta <= 0) ? 1 / 60 : Math.min(0.2, delta);
            const now = typeof this.game.getElapsedTime === 'function'
                ? this.game.getElapsedTime()
                : performance.now() * 0.001;
            const allCollidables = this.game.collidableMeshes && Array.isArray(this.game.collidableMeshes) ? this.game.collidableMeshes : [];
            const isMoving = moveState.forward || moveState.backward || moveState.left || moveState.right;
            const jumpHeld = !!(moveState.jump || moveState.up);
            this.targetHeight = this.eyeHeights[this.stance];
            if (this.checkCollision()) {
                this.tryResolveStuck(this.feetPosition);
            }
            const wasGrounded = this.onGround;
            const groundHit = this.updateGroundState(allCollidables);
            const volumeModifiers = this.game?.getPhysicsVolumeModifiersAt?.(this.camera.position) || {
                gravityScale: 1,
                jumpScale: 1,
                drag: 0,
                buoyancy: 0,
                flow: this.tempVector.set(0, 0, 0),
                activeVolumes: []
            };
            const traversalLink = this.game?.getTraversalLinkAt?.(this.camera.position || this.feetPosition) || null;
            const traversalActive = !!traversalLink;
            const climbInput = traversalActive
                ? ((jumpHeld || moveState.forward) ? 1 : 0) - ((moveState.down || moveState.backward) ? 1 : 0)
                : 0;
            if (traversalActive) {
                this.onGround = false;
                this.velocity.y = climbInput * traversalLink.climbSpeed;
            } else if (this.onGround) {
                this.lastGroundedAt = now;
                this.velocity.y = Math.max(0, this.velocity.y);
                if (groundHit?.point) {
                    this.feetPosition.y = groundHit.point.y + this.groundContactOffset;
                    this.syncCameraToBody();
                }
            } else {
                const gravityMultiplier = this.velocity.y > 0
                    ? (jumpHeld ? 1.0 : this.lowJumpGravityMultiplier)
                    : this.fallGravityMultiplier;
                const netGravityScale = THREE.MathUtils.clamp(volumeModifiers.gravityScale - volumeModifiers.buoyancy, -2, 4);
                this.velocity.y = Math.max(
                    this.velocity.y - (this.gravity * gravityMultiplier * netGravityScale * dt),
                    -this.maxFallSpeed
                );
            }

            if (!traversalActive && jumpHeld && !this.jumpHeldLastFrame && this.onGround) {
                this.velocity.y = this.jumpForce * volumeModifiers.jumpScale;
                this.onGround = false;
                this.lastGroundedAt = -Infinity;
            }

            const currentSpeed = this.getCurrentSpeed(moveState.sprint) * (traversalActive ? 0.58 : 1);
            this.direction.set(
                Number(moveState.right) - Number(moveState.left),
                0,
                Number(moveState.forward) - Number(moveState.backward)
            );
            let targetVelocityX = 0, targetVelocityZ = 0;
            if (this.direction.lengthSq() > 0.001) {
                this.direction.normalize();
                this.camera.getWorldDirection(this.forwardVector);
                this.forwardVector.y = 0;
                if (this.forwardVector.lengthSq() < 1e-10) {
                    this.forwardVector.set(0, 0, -1);
                } else {
                    this.forwardVector.normalize();
                }
                this.rightVector.crossVectors(this.camera.up, this.forwardVector).negate();
                if (this.rightVector.lengthSq() < 1e-10) {
                    this.moveDirection.set(
                        this.forwardVector.x * this.direction.z,
                        0,
                        this.forwardVector.z * this.direction.z
                    );
                } else {
                    this.rightVector.normalize();
                    this.moveDirection.copy(this.forwardVector)
                        .multiplyScalar(this.direction.z)
                        .add(this.rightVector.clone().multiplyScalar(this.direction.x));
                }
                this.moveDirection.normalize();
                this.surfaceMoveDirection.copy(this.moveDirection);
                if (this.onGround && this.surfaceNormal.y < 0.999) {
                    this.surfaceMoveDirection.projectOnPlane(this.surfaceNormal);
                    if (this.surfaceMoveDirection.lengthSq() > 1e-10) {
                        this.surfaceMoveDirection.normalize();
                    } else {
                        this.surfaceMoveDirection.set(0, 0, 0);
                    }
                }
                targetVelocityX = this.surfaceMoveDirection.x * currentSpeed;
                targetVelocityZ = this.surfaceMoveDirection.z * currentSpeed;
            }
            const acceleration = traversalActive ? 18.0 : (this.onGround ? 26.0 : 10.0);
            this.velocity.x += (targetVelocityX - this.velocity.x) * acceleration * dt;
            this.velocity.z += (targetVelocityZ - this.velocity.z) * acceleration * dt;
            if (volumeModifiers.flow?.lengthSq?.() > 0) {
                this.velocity.addScaledVector(volumeModifiers.flow, dt);
            }
            if (volumeModifiers.drag > 0) {
                const dragFactor = Math.max(0, 1 - (volumeModifiers.drag * dt * 0.35));
                this.velocity.multiplyScalar(dragFactor);
            }
            
            const prevPosition = this.feetPosition.clone();

            const verticalHit = this.moveWithSweep(this.tempVector.set(0, this.velocity.y * dt, 0));
            if (verticalHit) {
                this.feetPosition.y = prevPosition.y;
                this.syncCameraToBody();
                this.velocity.y = 0;
            }

            const horizontalMotion = this.slideDelta.set(this.velocity.x * dt, 0, this.velocity.z * dt);
            const postVerticalY = this.feetPosition.y;
            const horizontalResult = this.moveWithSlide(horizontalMotion);
            if (Math.abs(this.feetPosition.y - postVerticalY) > 1e-5) {
                this.feetPosition.y = postVerticalY;
                this.syncCameraToBody();
            }
            if (horizontalResult?.blocked) {
                const canStepUp = horizontalResult.hits?.some((hit) => {
                    const normal = hit?.normal;
                    if (!normal) return false;
                    const lateral = Math.hypot(normal.x || 0, normal.z || 0);
                    return lateral > 0.45 && Math.abs(normal.y || 0) < 0.35;
                });
                if (!canStepUp || !this.tryStepUp(prevPosition) || this.checkCollision()) {
                    this.feetPosition.y = prevPosition.y;
                    this.syncCameraToBody();
                    if (horizontalResult.hits?.some((hit) => Math.abs(hit.normal?.x || 0) > 0.3)) {
                        this.velocity.x = 0;
                    }
                    if (horizontalResult.hits?.some((hit) => Math.abs(hit.normal?.z || 0) > 0.3)) {
                        this.velocity.z = 0;
                    }
                } else {
                    this.feetPosition.x = prevPosition.x;
                    this.feetPosition.z = prevPosition.z;
                    this.syncCameraToBody();
                    const steppedResult = this.moveWithSlide(horizontalMotion);
                    if (steppedResult?.blocked) {
                        if (steppedResult.hits?.some((hit) => Math.abs(hit.normal?.x || 0) > 0.3)) {
                            this.velocity.x = 0;
                        }
                        if (steppedResult.hits?.some((hit) => Math.abs(hit.normal?.z || 0) > 0.3)) {
                            this.velocity.z = 0;
                        }
                    }
                }
            }

            let postGroundHit = null;
            if (!traversalActive && this.velocity.y <= 0) {
                const stepDownDistance = wasGrounded ? this.maxStepHeight + this.groundSnapDistance : this.groundSnapDistance;
                postGroundHit = this.applyStepDown(stepDownDistance);
            }
            if (!postGroundHit && !traversalActive) {
                postGroundHit = this.updateGroundState(allCollidables);
            }
            if (this.onGround && this.velocity.y <= 0 && postGroundHit?.point) {
                this.feetPosition.y = postGroundHit.point.y + this.groundContactOffset;
                this.syncCameraToBody();
                this.lastGroundedAt = now;
            } else if (traversalActive) {
                this.onGround = false;
                this.surfaceNormal.set(0, 1, 0);
            }

            const speed = new THREE.Vector2(this.velocity.x, this.velocity.z).length();
            const targetFov = this.baseFov + (speed * 1.5);
            this.camera.fov = THREE.MathUtils.lerp(this.camera.fov, targetFov, dt * 5.0);
            this.camera.updateProjectionMatrix();

            if (this.onGround && speed > 0.5) {
                this.bobTimer += dt * (speed * 1.2);
                this.camera.rotation.z = Math.sin(this.bobTimer) * 0.002 * Math.min(speed, 10);
            } else {
                this.camera.rotation.z = THREE.MathUtils.lerp(this.camera.rotation.z, 0, dt * 10);
            }
            if (!Number.isFinite(this.velocity.x)) this.velocity.x = 0;
            if (!Number.isFinite(this.velocity.y)) this.velocity.y = 0;
            if (!Number.isFinite(this.velocity.z)) this.velocity.z = 0;
            this.jumpHeldLastFrame = jumpHeld;
        }
    }
}

/**
 * Main game controller: engine, levels, spawners, HUD, and input (browser audio disabled; use RawIron.Audio).
 * Construct once, then loadAssets() and animate() run after LoadingManager finishes.
 */
export class AnomalousEchoGame {
    constructor() {
        const params = new URLSearchParams(window.location.search);
        this.quality = (params.get('quality') || (typeof localStorage !== 'undefined' && localStorage.getItem('anomalous-echo-quality')) || 'medium').toLowerCase();
        if (!['low', 'medium', 'high'].includes(this.quality)) this.quality = 'medium';
        if (typeof localStorage !== 'undefined' && params.get('quality')) {
            try {
                localStorage.setItem('anomalous-echo-quality', this.quality);
            } catch (_) {}
        }
        this.reducedMotion = typeof window !== 'undefined' && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
        this.mouseSensitivity = 0.0022;
        this.muted = params.get('mute') === '1';
        this.runtimeEvents = { emit() {} };
        this.keyBindings = { ...DEFAULT_KEYS };
        this.gameState = { isPaused: true, isLoaded: false, isGameOver: false, invulnerableUntil: 0 };
        this.loadedAssets = { textures: {}, models: {} };
        this.animationSources = {};
        this.clock = new THREE.Clock();
        this.flickerIntervals = [];
        this.levelGeometries = [];
        this.animationFrameId = null;
        this.gameStarted = false;
        this.visitedLevels = new Set();
        this._levelLoadInProgress = false;
        this.currentLevelFilename = null;
        this.currentLevelData = null;
        this.hudDismissTimers = new Map();
        this._activeGuidanceHint = null;
        this.activeAudioEnvironmentState = null;
        this.currentShaderIntensityLevel = 'menu';
        this.lightSafetyState = { playerInside: false, known: false, lastMessageAt: -Infinity };
        this.levelLightsById = new Map();
        this.levelNodesById = new Map();
        this.levelTimeouts = [];
        this.levelIntervals = [];
        this.triggerVolumes = [];
        this.completedEventIds = new Set();
        this.bvhMeshCount = 0;
        this.triggerVolumeIndex = null;
        this._spectatorDirection = new THREE.Vector3();
        this._spectatorRight = new THREE.Vector3();
        this._movementScratch = new THREE.Vector3();
        this._movementTestBox = new THREE.Box3();
        this._triggerQueryBox = new THREE.Box3();
        this._triggerQueryPadding = new THREE.Vector3(0.05, 0.05, 0.05);
        this._traceCandidateBox = new THREE.Box3();
        
        this._slideRemaining = new THREE.Vector3();
        this._slideMoved = new THREE.Vector3();
        this._slideStep = new THREE.Vector3();
        this._slideClip = new THREE.Vector3();
        this._slideNudge = new THREE.Vector3();
        this._slideWorkingBox = new THREE.Box3();
        this._structuralTempBox = new THREE.Box3();
        this._structuralTempMatrix = new THREE.Matrix4();
        this._structuralTempQuaternion = new THREE.Quaternion();
        this._structuralTempScale = new THREE.Vector3();
        this._structuralTempPosition = new THREE.Vector3();
        this._runtimeProjectorBounds = new THREE.Box3();
        this._runtimeProjectorTargetBounds = new THREE.Box3();
        this._pendingGameplayEnterTimeout = null;
        this.soundscape = {};
        this._elapsedTimeOverride = null;
        this.initEngine(); 
        this._tempEuler = new THREE.Euler(0, 0, 0, 'YXZ'); // For camera pitch clamping
        this.initUI(); 
        this.loadAssets();
    }

    initEngine() {
        this.scene = new THREE.Scene();
        this.camera = new THREE.PerspectiveCamera(70, window.innerWidth / window.innerHeight, 0.1, 1000);
        this.camera.rotation.order = 'YXZ'; // Fix rotation order for FPS camera to prevent gimbal lock
        const dpr = Math.min(window.devicePixelRatio || 1, 2);
        this.renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
        this.renderer.setPixelRatio(dpr);
        this.renderer.setSize(window.innerWidth, window.innerHeight);
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        this.renderer.shadowMap.autoUpdate = true;
        this.renderer.outputColorSpace = THREE.SRGBColorSpace;
        this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
        this.renderer.toneMappingExposure = 1.65;
        this.pixelRatio = dpr;
        
        this._onWebglContextLost = (event) => {
            event.preventDefault();
            if (this.animationFrameId) {
                cancelAnimationFrame(this.animationFrameId);
                this.animationFrameId = null;
            }
        };
        this._onWebglContextRestored = () => {
            this.animate();
        };
        this.renderer.domElement.addEventListener('webglcontextlost', this._onWebglContextLost, false);
        this.renderer.domElement.addEventListener('webglcontextrestored', this._onWebglContextRestored, false);

        document.body.appendChild(this.renderer.domElement);
        this.controls = new PointerLockControls(this.camera, document.body); 
        this.player = new Player(this.controls, this); 
        this.scene.add(this.controls.object);
        this.moveState = { forward: false, backward: false, left: false, right: false, sprint: false, up: false, down: false }; 
        this.collidableMeshes = []; 
        this.staticCollidableMeshes = [];
        this.structuralCollidableMeshes = [];
        this.dynamicCollidableMeshes = [];
        this.staticCollisionIndex = null;
        this.structuralCollisionIndex = null;
        this._toObject = new THREE.Vector3();
        this._kickSpinScratch = new THREE.Vector3();

        this.composer = {
            render: () => this.renderer.render(this.scene, this.camera)
        };
    }

    initUI() {
        this.ui = { 
            loadingScreen: document.getElementById('loading-screen'),
            loadingProgress: document.getElementById('loading-progress'),
            loadingProgressBar: document.getElementById('loading-progress-bar'),
            loadingProgressFill: document.getElementById('loading-progress-fill'),
            startMenu: document.getElementById('start-menu'), 
            pauseMenu: document.getElementById('pause-menu'),
            gameOverScreen: document.getElementById('game-over-screen'),
            levelTransitionOverlay: document.getElementById('level-transition-overlay'),
            hud: document.getElementById('hud')
        };

        this.ui.startMenu?.addEventListener('click', () => {
            this.requestGameplayEnter();
        });
        this.ui.pauseMenu?.addEventListener('click', () => {
            this.requestGameplayEnter();
        });
        this.ui.gameOverScreen?.addEventListener('click', () => {
            this.reloadGameFromBestAvailableStart();
        });
        const retryBtn = document.getElementById('retry-btn');
        if (retryBtn) retryBtn.addEventListener('click', (e) => { e.stopPropagation(); this.reloadGameFromBestAvailableStart(); });
        const continueBtn = document.getElementById('continue-btn');
        if (continueBtn) continueBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            this.controls.lock();
        });

        this.controls.addEventListener('lock', () => this.handleLock()); 
        this.controls.addEventListener('unlock', () => this.handleUnlock());
        
        this.setupInputListeners();
    }

    

    getElapsedTime() {
        return this._elapsedTimeOverride ?? this.clock.getElapsedTime();
    }

    clearPendingGameplayEnter() {
        if (this._pendingGameplayEnterTimeout != null) {
            window.clearTimeout(this._pendingGameplayEnterTimeout);
            this._pendingGameplayEnterTimeout = null;
        }
    }

    emitRuntimeEvent(eventName, payload = {}) {
        void eventName;
        return payload && typeof payload === 'object' ? payload : {};
    }

    requestGameplayEnter() {
        this.clearPendingGameplayEnter();
        try {
            if (!this.controls.isLocked && document.pointerLockElement !== this.controls.domElement) {
                this.controls.lock();
            }
        } catch (_) {}
        this._pendingGameplayEnterTimeout = window.setTimeout(() => {
            this._pendingGameplayEnterTimeout = null;
            if (this.controls.isLocked || this.gameState.isGameOver) return;
            this.handleLock().catch((error) => {
                console.warn('Gameplay enter fallback failed', error);
            });
        }, 180);
    }

    async handleLock() {
        this.clearPendingGameplayEnter();
        if (this.gameState.isGameOver) return;
        if (this.ui.startMenu) this.ui.startMenu.style.display = 'none';
        if (this.ui.pauseMenu) this.ui.pauseMenu.style.display = 'none';

        if (!this.gameStarted) await this.startGame();
        this.ui.hud?.classList.add('visible');
        this.gameState.isPaused = false;
    }

    handleUnlock() {
        this.clearPendingGameplayEnter();
        if (!this.gameState.isLoaded || this.gameState.isGameOver) return;

        this.ui.hud?.classList.remove('visible');
        if (this.ui.pauseMenu) this.ui.pauseMenu.style.display = 'flex';
        this.gameState.isPaused = true; 

        this.setShaderIntensity('menu'); 
        this.emitRuntimeEvent('stateChanged', { id: createRuntimeId('state'), key: 'isPaused', value: true, reason: 'unlock' });
    }

    

    setShaderIntensity(level) {
        this.currentShaderIntensityLevel = level;
    }

    setupInputListeners() {
        this.contextMenuHandler = (e) => e.preventDefault();
        document.addEventListener('contextmenu', this.contextMenuHandler);
        this.keyDownHandler = this.handleKeyDown.bind(this);
        this.keyUpHandler = this.handleKeyUp.bind(this);
        this.resizeHandler = this.onWindowResize.bind(this);
        this.visibilityChangeHandler = this.handleVisibilityChange.bind(this);
        this.pointerLockErrorHandler = () => {
            if (!this.controls.isLocked) {
            }
        };

        document.addEventListener('keydown', this.keyDownHandler);
        document.addEventListener('keyup', this.keyUpHandler);
        window.addEventListener('resize', this.resizeHandler);
        document.addEventListener('pointerlockerror', this.pointerLockErrorHandler, false);
        document.addEventListener('visibilitychange', this.visibilityChangeHandler, false);
        this.wheelHandler = this.handleWheel.bind(this);
        window.addEventListener('wheel', this.wheelHandler, { passive: false });
    }

    handleSystemKeys(e) {
        return false;
    }

    handleUIKeys(e) {
        void e;
        return false;
    }

    handleActionKeys(e) {
        const keys = this.keyBindings;
        if (e.code === keys.unstuck && !e.repeat) {
            e.preventDefault();
            const recovered = typeof this.player?.forceUnstuckRecovery === 'function'
                ? this.player.forceUnstuckRecovery()
                : false;
            return;
        }
    }

    handleMovementKeys(e, isKeyDown) {
        const keys = this.keyBindings;
        switch (e.code) {
            case keys.forward: this.moveState.forward = isKeyDown; break;
            case keys.backward: this.moveState.backward = isKeyDown; break;
            case keys.left: this.moveState.left = isKeyDown; break;
            case keys.right: this.moveState.right = isKeyDown; break;
            case keys.sprint:
            case 'ShiftRight': this.moveState.sprint = isKeyDown; break;
            case keys.jump:
                if (isKeyDown) {
                    this.moveState.jump = true;
                } else {
                    this.moveState.jump = false;
                }
                break;
            case keys.crouch: if (isKeyDown && !e.repeat) this.player.setStance('crouching'); break;
            case keys.prone: if (isKeyDown && !e.repeat) this.player.setStance('prone'); break;
        }
    }

    handleVisibilityChange() {
        this.clock.getDelta();
        if (document.hidden) {
            if (this.gameState.isLoaded && !this.gameState.isGameOver && !this.gameState.isPaused) {
                this.gameState.isPaused = true;
                if (this.controls.isLocked) this.controls.unlock();
            }
        }
    }

    handleWheel(event) {
        if (!this.gameState.isLoaded || this.gameState.isGameOver) return;

        if (!this.controls?.isLocked || this.gameState.isPaused) return;
    }

    handleKeyDown(e) {
        if (this.handleSystemKeys(e)) return;
        if (this.handleUIKeys(e)) return;

        if (this.gameState.isPaused) {
            return;
        }

        this.handleActionKeys(e);
        this.handleMovementKeys(e, true);
    }

    handleKeyUp(e) {
        this.handleMovementKeys(e, false);
    }

    clearDebugHelpers() {}

    createPhysicsPrimitiveFromData(data, position) {
        const primitiveType = data.primitiveType || 'sphere';
        let geometry;
        switch (primitiveType) {
            case 'sphere': {
                const r = Number(data.radius);
                geometry = new THREE.SphereGeometry(Number.isFinite(r) && r > 0 ? r : 0.55, 24, 16);
                break;
            }
            case 'box': {
                const sz = finiteVec3Components(data.size, [1, 1, 1]);
                geometry = new THREE.BoxGeometry(
                    Math.max(1e-4, Math.abs(sz[0])),
                    Math.max(1e-4, Math.abs(sz[1])),
                    Math.max(1e-4, Math.abs(sz[2]))
                );
                break;
            }
            default:
                console.warn(`Unknown physics primitive "${primitiveType}"`);
                return null;
        }

        const material = new THREE.MeshStandardMaterial({
            color: data.color || 0xffffff,
            emissive: new THREE.Color(data.emissiveColor || data.color || 0x000000),
            emissiveIntensity: data.emissiveIntensity ?? 0.8,
            metalness: data.metalness ?? 0.08,
            roughness: data.roughnessFactor ?? 0.42
        });
        const mesh = new THREE.Mesh(geometry, material);
        mesh.position.copy(position);
        mesh.rotation.fromArray(finiteVec3Components(data.rotation, [0, 0, 0]));
        mesh.scale.fromArray(finiteScaleComponents(data.scale, [1, 1, 1]));
        mesh.name = data.name || data.id || 'Physics Primitive';
        mesh.userData.dynamicCollider = true;
        mesh.userData.isPhysicsProp = true;
        mesh.castShadow = true;
        mesh.receiveShadow = true;

        if (data.glowLight !== false) {
            const light = new THREE.PointLight(
                new THREE.Color(data.lightColor || data.emissiveColor || data.color || '#88ddff'),
                data.lightIntensity ?? 1.6,
                data.lightDistance ?? 8,
                2
            );
            light.position.set(0, 0.35, 0);
            mesh.add(light);
        }

        return mesh;
    }

    buildPhysicsPropBounds(object, data = {}, targetBox = new THREE.Box3()) {
        if (!object) return targetBox.makeEmpty();
        if (Array.isArray(data.collisionBox) && data.collisionBox.length >= 3) {
            const center = new THREE.Vector3();
            const quaternion = new THREE.Quaternion();
            const off = finiteVec3Components(data.collisionOffset, [0, 0, 0]);
            const offset = new THREE.Vector3(off[0], off[1], off[2]);
            const szBox = finiteVec3Components(data.collisionBox, [1, 1, 1]);
            const size = new THREE.Vector3(
                Math.max(1e-4, Math.abs(szBox[0])),
                Math.max(1e-4, Math.abs(szBox[1])),
                Math.max(1e-4, Math.abs(szBox[2]))
            );
            object.getWorldPosition(center);
            object.getWorldQuaternion(quaternion);
            offset.applyQuaternion(quaternion);
            center.add(offset);
            targetBox.setFromCenterAndSize(center, size);
            return targetBox;
        }
        return targetBox.setFromObject(object);
    }

    registerDynamicObjectColliders(root) {
        root?.traverse?.((child) => {
            if (!child.isMesh) return;
            child.userData.dynamicCollider = true;
            this.registerCollidable(child);
        });
    }

    unregisterDynamicObjectColliders(root) {
        root?.traverse?.((child) => {
            if (!child.isMesh || !child.userData?.dynamicCollider) return;
            this.unregisterCollidable(child);
        });
    }

    createBloodDecalAt(position, velocity, type = 'drag', options = {}) {
        if (!position || !velocity) return null;
        if (!Number.isFinite(position.x) || !Number.isFinite(position.y) || !Number.isFinite(position.z)) return null;
        if (!Number.isFinite(velocity.x) || !Number.isFinite(velocity.y) || !Number.isFinite(velocity.z)) return null;
        const id = `blood_decal_${Date.now()}_${Math.floor(Math.random() * 10000)}`;
        const speed = velocity.length();
        if (!Number.isFinite(speed)) return null;
        if (type === 'splatter') {
            const normal = velocity.clone();
            if (normal.lengthSq() < 1e-10) normal.set(0, 1, 0);
            else normal.normalize();
            const plane = new THREE.Mesh(
                new THREE.PlaneGeometry(1.2, 1.0),
                this.createMaterialFromData({
                    baseColorMapRef: 'blood_splatter_detail.png',
                    decalPreset: 'blood',
                    emissiveColor: '#220404',
                    emissiveIntensity: 0.05
                })
            );
            plane.position.copy(position).add(normal.clone().multiplyScalar(0.006));
            plane.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), normal.clone().normalize());
            plane.renderOrder = 20;
            plane.name = id;
            plane.castShadow = false;
            plane.receiveShadow = false;
            this.levelObjects.add(plane);
            this.levelNodesById.set(id, plane);
            return plane;
        }

        let surfaceNormal = null;
        if (options.surfaceNormal instanceof THREE.Vector3) {
            const sn = options.surfaceNormal.clone();
            const snOk = Number.isFinite(sn.x) && Number.isFinite(sn.y) && Number.isFinite(sn.z) && sn.lengthSq() > 1e-12;
            if (snOk) surfaceNormal = sn.normalize();
        }
        const wallAligned = surfaceNormal && Math.abs(surfaceNormal.y) < 0.35;
        const direction = velocity.clone();
        if (wallAligned) {
            const projected = surfaceNormal.clone().multiplyScalar(direction.dot(surfaceNormal));
            direction.sub(projected);
        } else {
            direction.y = 0;
        }
        if (direction.lengthSq() < 0.001) {
            if (wallAligned) {
                direction.set(0, -1, 0);
            } else {
                direction.set(0, 0, -1);
            }
        }
        direction.normalize();
        const length = THREE.MathUtils.clamp(1.4 + speed * 0.16, 1.4, 4.2);
        const plane = new THREE.Mesh(
            new THREE.PlaneGeometry(0.9, length),
            this.createMaterialFromData({
                baseColorMapRef: 'blood_drag_detail.png',
                decalPreset: 'blood',
                emissiveColor: '#220404',
                emissiveIntensity: 0.06
            })
        );
        if (wallAligned) {
            const tangent = direction.clone().normalize();
            let binormal = new THREE.Vector3().crossVectors(surfaceNormal, tangent);
            if (binormal.lengthSq() < 1e-12) {
                binormal.crossVectors(surfaceNormal, new THREE.Vector3(0, 1, 0));
            }
            if (binormal.lengthSq() < 1e-12) {
                binormal.set(1, 0, 0);
            } else {
                binormal.normalize();
            }
            const basis = new THREE.Matrix4().makeBasis(binormal, tangent, surfaceNormal);
            plane.quaternion.setFromRotationMatrix(basis);
            plane.position.copy(position).add(surfaceNormal.clone().multiplyScalar(0.006));
        } else {
            const yaw = Math.atan2(direction.x, direction.z);
            plane.position.set(position.x, position.y + 0.015, position.z);
            plane.rotation.set(-Math.PI * 0.5, yaw, 0);
        }
        plane.renderOrder = 20;
        plane.name = id;
        plane.castShadow = false;
        plane.receiveShadow = false;
        this.levelObjects.add(plane);
        this.levelNodesById.set(id, plane);
        return plane;
    }

    normalizeHumanoidBoneName(name) {
        if (!name) return '';
        const compact = String(name)
            .toLowerCase()
            .replace(/^mixamorig[:_]?/i, '')
            .replace(/^armature\|/i, '')
            .replace(/[^a-z0-9]/g, '');
        const aliases = {
            hip: 'hips',
            hips: 'hips',
            pelvis: 'hips',
            spine: 'spine',
            spine01: 'spine',
            spine1: 'spine',
            spine02: 'chest',
            spine2: 'chest',
            spine03: 'chest',
            spine3: 'chest',
            chest: 'chest',
            upperchest: 'chest',
            neck: 'neck',
            neck01: 'neck',
            head: 'head',
            claviclel: 'leftshoulder',
            leftshoulder: 'leftshoulder',
            clavicler: 'rightshoulder',
            rightshoulder: 'rightshoulder',
            upperarml: 'leftarm',
            leftarm: 'leftarm',
            upperarmr: 'rightarm',
            rightarm: 'rightarm',
            lowerarml: 'leftforearm',
            leftforearm: 'leftforearm',
            lowerarmr: 'rightforearm',
            rightforearm: 'rightforearm',
            handl: 'lefthand',
            lefthand: 'lefthand',
            handr: 'righthand',
            righthand: 'righthand',
            thighl: 'leftupleg',
            leftupleg: 'leftupleg',
            upperlegl: 'leftupleg',
            thighr: 'rightupleg',
            rightupleg: 'rightupleg',
            upperlegr: 'rightupleg',
            calfl: 'leftleg',
            leftleg: 'leftleg',
            lowerlegl: 'leftleg',
            calfr: 'rightleg',
            rightleg: 'rightleg',
            lowerlegr: 'rightleg',
            footl: 'leftfoot',
            leftfoot: 'leftfoot',
            footr: 'rightfoot',
            rightfoot: 'rightfoot',
            balll: 'lefttoebase',
            lefttoebase: 'lefttoebase',
            ballr: 'righttoebase',
            righttoebase: 'righttoebase'
        };
        return aliases[compact] || compact;
    }

    normalizeLoadedModel(modelName, object3d, modelConfig = null) {
        const root = object3d.scene ? object3d.scene : object3d;
        root.name = modelName;
        if (Array.isArray(modelConfig?.importScale) && modelConfig.importScale.length >= 3) {
            root.scale.fromArray(finiteScaleComponents(modelConfig.importScale, [1, 1, 1]));
            root.updateMatrixWorld(true);
        }
        const embeddedAnimationClips = Array.isArray(object3d?.animations) && object3d.animations.length > 0
            ? object3d.animations
                .map((clip) => this.sanitizeSourceAnimationClip(clip))
                .map((clip) => clip?.clone ? clip.clone() : clip)
            : [];
        if (embeddedAnimationClips.length > 0) {
            root.animations = embeddedAnimationClips;
        }
        const explicitTexturePath = modelConfig?.explicitTexturePath || null;
        const explicitTexture = explicitTexturePath ? this._explicitModelTextures?.get(explicitTexturePath) || null : null;
        root.traverse((child) => {
            if (!child.isMesh) return;
            child.castShadow = true;
            child.receiveShadow = true;
            if (child.material) {
                const materials = Array.isArray(child.material) ? child.material : [child.material];
                materials.forEach((material) => {
                    if (explicitTexture) {
                        material.map = explicitTexture;
                        material.color?.setHex?.(0xffffff);
                        material.needsUpdate = true;
                    }
                    if (material.map) {
                        material.map.colorSpace = THREE.SRGBColorSpace;
                        material.map.needsUpdate = true;
                    } else if (material.color) {
                        const c = material.color;
                        if (c.r > 0.9 && c.g > 0.9 && c.b > 0.9) {
                            material.color.setHex(0x666666);
                        }
                    }
                });
            }
        });
        if (modelConfig?.animationProfile === 'hazmat_survivor') {
            const bounds = new THREE.Box3().setFromObject(root);
            if (Number.isFinite(bounds.min.y) && Number.isFinite(bounds.max.y)) {
                root.position.y -= bounds.min.y;
                root.userData.lookHeight = Math.max(1.3, bounds.max.y - bounds.min.y - 0.18);
            }
        }
        if (modelConfig?.npcArchetype) {
            root.userData.npcArchetype = modelConfig.npcArchetype;
        }
        root.userData.importTransform = {
            position: root.position.toArray(),
            quaternion: root.quaternion.toArray(),
            scale: root.scale.toArray()
        };
        return root;
    }

    cloneAnimationClipMap(clipMap) {
        if (!clipMap || typeof clipMap !== 'object') return null;
        const clonedClipMap = {};
        for (const [name, clip] of Object.entries(clipMap)) {
            if (!(clip instanceof THREE.AnimationClip)) continue;
            clonedClipMap[name] = clip.clone();
        }
        return Object.keys(clonedClipMap).length > 0 ? clonedClipMap : null;
    }

    applyAuthoredScale(object, scaleData) {
        if (!object) return;
        const authoredScale = finiteScaleComponents(scaleData, [1, 1, 1]);
        object.scale.multiply(this._toObject.set(authoredScale[0], authoredScale[1], authoredScale[2]));
    }

    applyAuthoredPlacement(object, positionData, rotationData = null, scaleData = null) {
        if (!object) return;
        const importTransform = object.userData?.importTransform || null;
        const finalScale = Array.isArray(importTransform?.scale) && importTransform.scale.length >= 3
            ? finiteScaleComponents(importTransform.scale, [1, 1, 1])
            : [1, 1, 1];
        const authoredScale = finiteScaleComponents(scaleData, [1, 1, 1]);
        finalScale[0] *= authoredScale[0];
        finalScale[1] *= authoredScale[1];
        finalScale[2] *= authoredScale[2];
        const basePosition = Array.isArray(positionData) && positionData.length >= 3
            ? new THREE.Vector3().fromArray(finiteVec3Components(positionData, [0, 0, 0]))
            : new THREE.Vector3();
        if (Array.isArray(importTransform?.position) && importTransform.position.length >= 3) {
            const importPosition = new THREE.Vector3().fromArray(finiteVec3Components(importTransform.position, [0, 0, 0]));
            importPosition.x *= finalScale[0];
            importPosition.y *= finalScale[1];
            importPosition.z *= finalScale[2];
            basePosition.add(importPosition);
        }
        object.position.copy(basePosition);
        if (Array.isArray(rotationData) && rotationData.length >= 3) {
            object.rotation.fromArray(finiteVec3Components(rotationData, [0, 0, 0]));
        }
        object.scale.fromArray(finalScale);
    }

    getHumanoidAnimationCacheOwner(root) {
        return root?.userData?.sourceModelRef || root;
    }

    getCachedHumanoidAnimationLibrary(root, profileName) {
        const owner = this.getHumanoidAnimationCacheOwner(root);
        const cache = owner?.userData?.humanoidAnimationCache;
        const entry = cache?.[profileName];
        if (!entry || typeof entry !== 'object') return null;
        const clipMap = this.cloneAnimationClipMap(entry.clipMap);
        if (!clipMap) return null;
        return {
            clipMap,
            animationProfileData: entry.animationProfileData
                ? JSON.parse(JSON.stringify(entry.animationProfileData))
                : this.getAnimationProfile(profileName)
        };
    }

    storeCachedHumanoidAnimationLibrary(root, profileName, clipMap, animationProfileData) {
        const owner = this.getHumanoidAnimationCacheOwner(root);
        if (!owner?.userData) return;
        owner.userData.humanoidAnimationCache ||= {};
        owner.userData.humanoidAnimationCache[profileName] = {
            clipMap: this.cloneAnimationClipMap(clipMap),
            animationProfileData: JSON.parse(JSON.stringify(animationProfileData))
        };
    }

    applyHumanoidAnimationLibrary(root, profileName, clipMap, animationProfileData) {
        root.userData.animationClips = clipMap;
        root.userData.animationProfile = profileName;
        root.userData.animationProfileData = animationProfileData;
        root.animations = Object.values(clipMap)
            .filter((clip) => clip instanceof THREE.AnimationClip)
            .map((clip) => clip.clone());
        return root;
    }

    cloneLoadedModel(sourceModel) {
        const instance = sourceModel.clone(true);
        instance.userData.sourceModelRef = sourceModel;
        const importTransform = sourceModel?.userData?.importTransform;
        if (importTransform) {
            if (Array.isArray(importTransform.position) && importTransform.position.length >= 3) {
                instance.position.fromArray(finiteVec3Components(importTransform.position, [0, 0, 0]));
            }
            if (Array.isArray(importTransform.quaternion) && importTransform.quaternion.length >= 4) {
                instance.quaternion.fromArray(finiteQuatComponents(importTransform.quaternion));
            }
            if (Array.isArray(importTransform.scale) && importTransform.scale.length >= 3) {
                instance.scale.fromArray(finiteScaleComponents(importTransform.scale, [1, 1, 1]));
            }
        }
        if (!sourceModel?.userData?.animationProfile && Array.isArray(sourceModel?.animations) && sourceModel.animations.length > 0) {
            instance.animations = sourceModel.animations.map((clip) => clip?.clone ? clip.clone() : clip);
        }
        const clonedClipMap = this.cloneAnimationClipMap(sourceModel?.userData?.animationClips);
        if (clonedClipMap) {
            instance.userData.animationClips = clonedClipMap;
            instance.animations = Object.values(clonedClipMap)
                .filter((clip) => clip instanceof THREE.AnimationClip)
                .map((clip) => clip.clone());
        }
        if (sourceModel?.userData?.animationProfile) {
            instance.userData.animationProfile = sourceModel.userData.animationProfile;
        }
        if (sourceModel?.userData?.animationProfileData) {
            instance.userData.animationProfileData = JSON.parse(JSON.stringify(sourceModel.userData.animationProfileData));
        }
        if (sourceModel?.userData?.npcArchetype) {
            instance.userData.npcArchetype = sourceModel.userData.npcArchetype;
        }
        if (importTransform) {
            instance.userData.importTransform = JSON.parse(JSON.stringify(importTransform));
        }
        return instance;
    }

    getPrimarySkinnedMesh(root) {
        let skinnedMesh = null;
        let bestScore = -1;
        root.traverse((child) => {
            if (!child.isSkinnedMesh) return;
            const boneCount = child.skeleton?.bones?.length || 0;
            const vertexCount = child.geometry?.attributes?.position?.count || 0;
            const score = boneCount * 100000 + vertexCount;
            if (score > bestScore) {
                skinnedMesh = child;
                bestScore = score;
            }
        });
        return skinnedMesh;
    }

    getMixamoToAnimationLibraryMap() {
        return {
            'mixamorig:Hips': 'pelvis',
            'mixamorig:Spine': 'spine_01',
            'mixamorig:Spine1': 'spine_02',
            'mixamorig:Spine2': 'spine_03',
            'mixamorig:Neck': 'neck_01',
            'mixamorig:Head': 'Head',
            'mixamorig:LeftShoulder': 'clavicle_l',
            'mixamorig:LeftArm': 'upperarm_l',
            'mixamorig:LeftForeArm': 'lowerarm_l',
            'mixamorig:LeftHand': 'hand_l',
            'mixamorig:LeftHandThumb1': 'thumb_01_l',
            'mixamorig:LeftHandThumb2': 'thumb_02_l',
            'mixamorig:LeftHandThumb3': 'thumb_03_l',
            'mixamorig:LeftHandIndex1': 'index_01_l',
            'mixamorig:LeftHandIndex2': 'index_02_l',
            'mixamorig:LeftHandIndex3': 'index_03_l',
            'mixamorig:LeftHandMiddle1': 'middle_01_l',
            'mixamorig:LeftHandMiddle2': 'middle_02_l',
            'mixamorig:LeftHandMiddle3': 'middle_03_l',
            'mixamorig:LeftHandRing1': 'ring_01_l',
            'mixamorig:LeftHandRing2': 'ring_02_l',
            'mixamorig:LeftHandRing3': 'ring_03_l',
            'mixamorig:LeftHandPinky1': 'pinky_01_l',
            'mixamorig:LeftHandPinky2': 'pinky_02_l',
            'mixamorig:LeftHandPinky3': 'pinky_03_l',
            'mixamorig:RightShoulder': 'clavicle_r',
            'mixamorig:RightArm': 'upperarm_r',
            'mixamorig:RightForeArm': 'lowerarm_r',
            'mixamorig:RightHand': 'hand_r',
            'mixamorig:RightHandThumb1': 'thumb_01_r',
            'mixamorig:RightHandThumb2': 'thumb_02_r',
            'mixamorig:RightHandThumb3': 'thumb_03_r',
            'mixamorig:RightHandIndex1': 'index_01_r',
            'mixamorig:RightHandIndex2': 'index_02_r',
            'mixamorig:RightHandIndex3': 'index_03_r',
            'mixamorig:RightHandMiddle1': 'middle_01_r',
            'mixamorig:RightHandMiddle2': 'middle_02_r',
            'mixamorig:RightHandMiddle3': 'middle_03_r',
            'mixamorig:RightHandRing1': 'ring_01_r',
            'mixamorig:RightHandRing2': 'ring_02_r',
            'mixamorig:RightHandRing3': 'ring_03_r',
            'mixamorig:RightHandPinky1': 'pinky_01_r',
            'mixamorig:RightHandPinky2': 'pinky_02_r',
            'mixamorig:RightHandPinky3': 'pinky_03_r',
            'mixamorig:LeftUpLeg': 'thigh_l',
            'mixamorig:LeftLeg': 'calf_l',
            'mixamorig:LeftFoot': 'foot_l',
            'mixamorig:LeftToeBase': 'ball_l',
            'mixamorig:RightUpLeg': 'thigh_r',
            'mixamorig:RightLeg': 'calf_r',
            'mixamorig:RightFoot': 'foot_r',
            'mixamorig:RightToeBase': 'ball_r'
        };
    }

    buildHumanoidBoneAliasLookup(root) {
        const lookup = new Map();
        if (!root) return lookup;
        const registerBone = (bone) => {
            if (!bone?.name) return;
            const normalized = this.normalizeHumanoidBoneName(bone.name);
            if (!normalized || lookup.has(normalized)) return;
            lookup.set(normalized, bone.name);
        };
        if (root.isSkinnedMesh && Array.isArray(root.skeleton?.bones)) {
            root.skeleton.bones.forEach(registerBone);
            return lookup;
        }
        root.traverse?.((child) => {
            if (child.type === 'Bone') registerBone(child);
            if (child.isSkinnedMesh && Array.isArray(child.skeleton?.bones)) {
                child.skeleton.bones.forEach(registerBone);
            }
        });
        return lookup;
    }

    createHumanoidRetargetNameMap(sourceRoot, targetRoot) {
        const sourceLookup = this.buildHumanoidBoneAliasLookup(sourceRoot);
        const targetLookup = this.buildHumanoidBoneAliasLookup(targetRoot);
        const names = {};
        for (const [normalizedAlias, sourceBoneName] of sourceLookup.entries()) {
            const targetBoneName = targetLookup.get(normalizedAlias);
            if (!targetBoneName) continue;
            names[sourceBoneName] = targetBoneName;
        }
        if (Object.keys(names).length === 0) {
            return this.getMixamoToAnimationLibraryMap();
        }
        return names;
    }

    getAnimationProfile(profileName = 'hazmat_survivor') {
        const profiles = {
            hazmat_survivor: {
                clips: {
                    idle: ['Armature|Idle_Loop', 'Armature|Crouch_Idle_Loop'],
                    alert: ['Armature|Idle_Alert_Loop', 'Armature|Idle_Loop'],
                    talk: ['Armature|Idle_Talking_Loop', 'Armature|Idle_Loop'],
                    radio: ['Armature|Radio_Loop', 'Armature|Idle_Talking_Loop'],
                    walk: ['Armature|Walk_Formal_Loop', 'Armature|Walk_Loop'],
                    run: ['Armature|Sprint_Loop'],
                    interact: ['Armature|Interact'],
                    use: ['Armature|Use_Terminal', 'Armature|Interact'],
                    hit: ['Armature|Hit_Chest', 'Armature|Hit_Head'],
                    death: ['Armature|Death01']
                },
                defaultAction: ['alert', 'idle'],
                resumeAction: ['alert', 'idle'],
                interactAction: ['radio', 'talk', 'interact'],
                timeScale: {
                    alert: 0.9,
                    talk: 0.94,
                    radio: 0.92,
                    walk: 0.92
                },
                loopOnce: ['interact', 'hit', 'death'],
                clampWhenFinished: ['interact', 'hit', 'death']
            },
            police_survivor: {
                clips: {
                    idle: ['Armature|Idle_Loop'],
                    alert: ['Armature|Idle_Alert_Loop', 'Armature|Idle_Loop'],
                    talk: ['Armature|Idle_Talking_Loop', 'Armature|Idle_Loop'],
                    radio: ['Armature|Radio_Loop', 'Armature|Idle_Talking_Loop'],
                    walk: ['Armature|Walk_Formal_Loop', 'Armature|Walk_Loop'],
                    run: ['Armature|Sprint_Loop'],
                    interact: ['Armature|Interact'],
                    use: ['Armature|Use_Terminal', 'Armature|Interact'],
                    hit: ['Armature|Hit_Head', 'Armature|Hit_Chest'],
                    death: ['Armature|Death01']
                },
                defaultAction: ['alert', 'idle'],
                resumeAction: ['alert', 'idle'],
                interactAction: ['radio', 'talk', 'interact'],
                timeScale: {
                    alert: 0.86,
                    talk: 0.96,
                    radio: 0.9,
                    walk: 0.9,
                    run: 1.04
                },
                loopOnce: ['interact', 'hit', 'death'],
                clampWhenFinished: ['interact', 'hit', 'death']
            },
            firefighter_survivor: {
                clips: {
                    idle: ['Armature|Idle_Loop'],
                    alert: ['Armature|Idle_Alert_Loop', 'Armature|Idle_Loop'],
                    talk: ['Armature|Idle_Talking_Loop', 'Armature|Idle_Loop'],
                    radio: ['Armature|Radio_Loop', 'Armature|Idle_Talking_Loop'],
                    walk: ['Armature|Walk_Loop', 'Armature|Walk_Formal_Loop'],
                    run: ['Armature|Sprint_Loop'],
                    interact: ['Armature|Interact'],
                    use: ['Armature|Use_Terminal', 'Armature|Interact'],
                    hit: ['Armature|Hit_Chest', 'Armature|Hit_Head'],
                    death: ['Armature|Death01']
                },
                defaultAction: ['idle', 'alert'],
                resumeAction: ['idle', 'alert'],
                interactAction: ['talk', 'radio', 'interact'],
                timeScale: {
                    talk: 0.95,
                    radio: 0.92,
                    walk: 1.0,
                    run: 1.08
                },
                loopOnce: ['interact', 'hit', 'death'],
                clampWhenFinished: ['interact', 'hit', 'death']
            },
            doctor_survivor: {
                clips: {
                    idle: ['Armature|Idle_Loop'],
                    alert: ['Armature|Idle_Alert_Loop', 'Armature|Idle_Loop'],
                    talk: ['Armature|Idle_Talking_Loop', 'Armature|Idle_Loop'],
                    briefing: ['Armature|Briefing_Loop', 'Armature|Idle_Talking_Loop'],
                    walk: ['Armature|Walk_Loop', 'Armature|Walk_Formal_Loop'],
                    run: ['Armature|Sprint_Loop'],
                    interact: ['Armature|Interact'],
                    use: ['Armature|Use_Terminal', 'Armature|Interact'],
                    hit: ['Armature|Hit_Chest', 'Armature|Hit_Head'],
                    death: ['Armature|Death01']
                },
                defaultAction: ['idle', 'use'],
                resumeAction: ['idle', 'use'],
                interactAction: ['briefing', 'talk', 'use', 'interact'],
                timeScale: {
                    talk: 0.93,
                    briefing: 0.94,
                    walk: 0.98,
                    run: 1.03
                },
                loopOnce: ['interact', 'hit', 'death'],
                clampWhenFinished: ['interact', 'hit', 'death']
            },
            civilian_survivor: {
                clips: {
                    idle: ['Armature|Idle_Loop'],
                    alert: ['Armature|Idle_Alert_Loop', 'Armature|Idle_Loop'],
                    talk: ['Armature|Idle_Talking_Loop', 'Armature|Idle_Loop'],
                    walk: ['Armature|Walk_Formal_Loop', 'Armature|Walk_Loop'],
                    run: ['Armature|Sprint_Loop'],
                    interact: ['Armature|Interact'],
                    cower: ['Armature|Crouch_Idle_Loop', 'Armature|Idle_Loop'],
                    hit: ['Armature|Hit_Chest', 'Armature|Hit_Head'],
                    death: ['Armature|Death01']
                },
                defaultAction: ['idle', 'alert'],
                resumeAction: ['idle', 'alert'],
                interactAction: ['talk', 'alert', 'cower', 'interact'],
                timeScale: {
                    talk: 0.98,
                    cower: 0.9,
                    walk: 0.94,
                    run: 1.02
                },
                loopOnce: ['interact', 'hit', 'death'],
                clampWhenFinished: ['interact', 'hit', 'death']
            }
        };

        return profiles[profileName] || profiles.hazmat_survivor;
    }

    sanitizeRetargetedClip(clip, profileName = '') {
        if (!clip?.tracks?.length) return clip;
        const normalizedProfile = String(profileName || '').toLowerCase();
        const filteredTracks = clip.tracks.filter((track) => {
            const trackName = String(track?.name || '');
            if (/(^|[.\]])Armature\d*\.(position|quaternion|scale)$/i.test(trackName)) return false;
            if (/^(light_hazmat_suit|heavy_hazmat_suit|Character_Monster(?:_\d+)?|Character_[^.\]]+)\.(position|quaternion|scale)$/i.test(trackName)) return false;
            if (/\.scale$/i.test(trackName)) return false;
            return true;
        });
        if (filteredTracks.length === clip.tracks.length) return clip;
        const sanitizedClip = clip.clone();
        sanitizedClip.tracks = filteredTracks;
        sanitizedClip.resetDuration();
        return sanitizedClip;
    }

    sanitizeSourceAnimationClip(clip) {
        if (!(clip instanceof THREE.AnimationClip) || !Array.isArray(clip.tracks) || clip.tracks.length === 0) return clip;
        const filteredTracks = clip.tracks.filter((track) => {
            const trackName = String(track?.name || '');
            if (/(^|[.\]])Armature\d*\.(position|quaternion|scale)$/i.test(trackName)) return false;
            if (/^(light_hazmat_suit|heavy_hazmat_suit|Character_Monster(?:_\d+)?|Character_[^.\]]+)\.(position|quaternion|scale)$/i.test(trackName)) return false;
            return true;
        });
        if (filteredTracks.length === clip.tracks.length) return clip;
        const sanitizedClip = clip.clone();
        sanitizedClip.tracks = filteredTracks;
        sanitizedClip.resetDuration();
        return sanitizedClip;
    }

    buildAnimationClipMapFromAnimations(animations = [], animationProfile = {}) {
        if (!Array.isArray(animations) || animations.length === 0) return {};
        const clips = animations
            .filter((clip) => clip instanceof THREE.AnimationClip)
            .map((clip) => this.sanitizeSourceAnimationClip(clip));
        if (clips.length === 0) return {};
        const clipMap = {};
        const profileClips = animationProfile?.clips || {};
        for (const [alias, clipName] of Object.entries(profileClips)) {
            const clipCandidates = Array.isArray(clipName) ? clipName : [clipName];
            const match = clipCandidates
                .map((candidate) => clips.find((clip) => clip.name === alias || clip.name === candidate))
                .find(Boolean);
            if (match) clipMap[alias] = match;
        }
        const inferAlias = (clipName) => {
            const normalized = String(clipName || '').toLowerCase();
            if (/(radio|walkie|transceiver|dispatch|call\s?in|over\s?radio)/.test(normalized)) return 'radio';
            if (/(brief|explain|lecture|present|discussion|gesture\s?talk|pointing)/.test(normalized)) return 'briefing';
            if (/(alert|guard|ready|combat[\s_-]?idle|aim(?!ed)|watch)/.test(normalized)) return 'alert';
            if (/(terminal|computer|console|panel|keypad|typing|operate|repair|fix|use)/.test(normalized)) return 'use';
            if (/(cower|fear|scared|terrified|hide|ducking)/.test(normalized)) return 'cower';
            if (/(idle|breath|stand)/.test(normalized)) return 'idle';
            if (/(talk|speak|voice|radio)/.test(normalized)) return 'talk';
            if (/(walk|stride)/.test(normalized)) return 'walk';
            if (/(run|sprint)/.test(normalized)) return 'run';
            if (/(attack|slash|bite|punch|strike)/.test(normalized)) return 'attack';
            if (/(hit|hurt|flinch|react)/.test(normalized)) return 'hit';
            if (/(death|die|dead)/.test(normalized)) return 'death';
            if (/(interact|use|button|pickup)/.test(normalized)) return 'interact';
            return null;
        };
        clips.forEach((clip, index) => {
            const alias = inferAlias(clip.name) || (index === 0 ? 'idle' : null);
            if (alias && !clipMap[alias]) {
                clipMap[alias] = clip;
            }
        });
        return clipMap;
    }

    resolveNpcArchetype(spawnerData = {}, modelName = '', animationProfile = '') {
        if (spawnerData?.npcArchetype) return String(spawnerData.npcArchetype);
        if (spawnerData?.archetype) return String(spawnerData.archetype);
        const source = `${modelName || ''} ${animationProfile || ''}`.toLowerCase();
        if (/(doctor|medic|medical)/.test(source)) return 'doctor';
        if (/firefighter/.test(source)) return 'firefighter';
        if (/(police|officer|guard|security)/.test(source)) return 'police';
        if (/(hazmat|hm)/.test(source)) return 'hazmat';
        return 'civilian';
    }

    resolveNpcAnimationProfile(spawnerData = {}, modelName = '', sourceProfile = '') {
        void spawnerData;
        void modelName;
        void sourceProfile;
        return '';
    }

    getCharacterPreferredAction(controller, requestedAction = '', fallbackAction = 'idle') {
        if (!controller?.actions) return fallbackAction;
        const requestedActions = Array.isArray(requestedAction) ? requestedAction : [requestedAction];
        const fallbackActions = Array.isArray(fallbackAction) ? fallbackAction : [fallbackAction];
        const candidateActions = [...requestedActions, ...fallbackActions, 'idle'].filter(Boolean);
        for (const candidate of candidateActions) {
            if (controller.actions[candidate]) return candidate;
        }
        if (controller.actions.idle) return 'idle';
        const firstAvailable = Object.keys(controller.actions)[0];
        return firstAvailable || (Array.isArray(fallbackAction) ? fallbackAction[0] : fallbackAction);
    }

    playCharacterAnimationAction(controller, requestedAction, options = {}) {
        if (!controller?.actions) return null;
        const actionName = this.getCharacterPreferredAction(controller, requestedAction, options.fallbackAction || 'idle');
        const nextAction = controller.actions[actionName];
        if (!nextAction) return null;
        const fadeDuration = Number.isFinite(options.fadeDuration) ? Math.max(0, options.fadeDuration) : 0.2;
        const previousAction = controller.currentAction ? controller.actions[controller.currentAction] : null;
        if (previousAction && previousAction !== nextAction) {
            previousAction.fadeOut(fadeDuration);
        }
        nextAction.reset();
        nextAction.enabled = true;
        nextAction.setEffectiveWeight(1);
        nextAction.setEffectiveTimeScale(1);
        nextAction.fadeIn(options.immediate ? 0 : fadeDuration);
        nextAction.play();
        controller.currentAction = actionName;
        this.emitRuntimeEvent('animationActionAssigned', {
            id: controller.id || createRuntimeId('animctrl'),
            action: actionName
        });
        return nextAction;
    }

    registerHumanoidAnimationSource(name, object, profileName = 'hazmat_survivor', priority = 0) {
        const sourceMesh = this.getPrimarySkinnedMesh(object);
        if (!sourceMesh) return false;
        const animationProfile = this.getAnimationProfile(profileName);
        const clipMap = this.buildAnimationClipMapFromAnimations(object.animations || [], animationProfile);
        const aliasLookup = this.buildHumanoidBoneAliasLookup(sourceMesh);
        const sourceData = {
            name,
            profileName,
            priority,
            root: object,
            skinnedMesh: sourceMesh,
            clips: object.animations || [],
            clipMap,
            aliasLookup
        };
        this.animationSources.humanoidLibraries ||= [];
        this.animationSources.humanoidLibraries = this.animationSources.humanoidLibraries.filter((entry) => entry.name !== name);
        this.animationSources.humanoidLibraries.push(sourceData);
        if (!this.animationSources.humanoidStandard) {
            this.animationSources.humanoidStandard = sourceData;
        }
        return true;
    }

    getHumanoidAnimationSources(profileName = 'hazmat_survivor') {
        if (Array.isArray(this.animationSources.humanoidLibraries) && this.animationSources.humanoidLibraries.length > 0) {
            const filtered = this.animationSources.humanoidLibraries
                .filter((entry) => (entry.profileName || 'hazmat_survivor') === profileName)
                .sort((a, b) => (b.priority || 0) - (a.priority || 0));
            if (filtered.length > 0) return filtered;
        }
        if (profileName === 'hazmat_survivor' && this.animationSources.humanoidStandard) {
            return [this.animationSources.humanoidStandard];
        }
        return [];
    }

    getProfileAnimationLibraryDefinitions() {
        return GameAssets.animationLibraryProfiles || {};
    }

    loadAnimationLibrarySource(definition = {}) {
        if (!definition?.name || !definition?.path || !this.animationLibraryLoader) {
            return Promise.resolve(false);
        }
        return new Promise((resolve) => {
            const finishWithObject = (object) => {
                if (definition.clipAlias && Array.isArray(object?.animations) && object.animations.length > 0) {
                    object.animations = object.animations.map((clip, index) => {
                        const nextClip = this.sanitizeSourceAnimationClip(clip)?.clone?.() || clip?.clone?.() || clip;
                        if (index === 0 && nextClip) nextClip.name = definition.clipAlias;
                        return nextClip;
                    });
                }
                resolve(this.registerHumanoidAnimationSource(definition.name, object, definition.profileName, definition.priority || 0));
            };
            this.animationLibraryLoader.load(
                definition.path,
                finishWithObject,
                undefined,
                (error) => {
                    console.warn(`Failed to load humanoid animation library "${definition.name}".`, error);
                    resolve(false);
                }
            );
        });
    }

    async ensureAnimationProfilesLoaded(profileNames = []) {
        const requestedProfiles = Array.isArray(profileNames) && profileNames.length > 0
            ? [...new Set(profileNames.filter(Boolean))]
            : ['hazmat_survivor'];
        const definitionsByProfile = this.getProfileAnimationLibraryDefinitions();
        const results = {};
        for (const profileName of requestedProfiles) {
            const definitions = Array.isArray(definitionsByProfile?.[profileName]) ? definitionsByProfile[profileName] : [];
            if (definitions.length === 0) {
                results[profileName] = false;
                continue;
            }
            const loadResults = await Promise.all(definitions.map((definition) => this.loadAnimationLibrarySource(definition)));
            results[profileName] = loadResults.some(Boolean);
            this.emitRuntimeEvent('animationSourcesLoaded', {
                id: createRuntimeId('animsrc'),
                profileName,
                requested: definitions.length,
                loaded: loadResults.filter(Boolean).length
            });
        }
        return results;
    }

    resolveExternalAssetUrl(url) {
        if (!url) return url;
        const normalized = String(url).replace(/\\/g, '/');
        const filename = normalized.split('/').pop();
        if (/^Character_.*\.(png|jpe?g)$/i.test(filename)) {
            return `./models/unpacked/Characters_psx/Characters_psx/Textures/${filename}`;
        }
        const radiationWorkerTextures = {
            'body radition guy (1).png': './models/unpacked/Radiation Workers – Retro PSX Character Pack/Radiation Workers – Retro PSX Character Pack/Textures/Light radiation suit/body radition guy (1).png',
            'mask radition (1).png': './models/unpacked/Radiation Workers – Retro PSX Character Pack/Radiation Workers – Retro PSX Character Pack/Textures/Light radiation suit/mask radition (1).png',
            'body radition guy.png': './models/unpacked/Radiation Workers – Retro PSX Character Pack/Radiation Workers – Retro PSX Character Pack/Textures/Heavy hazmat suit/body radition guy.png',
            'shoes radiation guy.png': './models/unpacked/Radiation Workers – Retro PSX Character Pack/Radiation Workers – Retro PSX Character Pack/Textures/Heavy hazmat suit/shoes radiation guy.png',
            'guantelets radiation.png': './models/unpacked/Radiation Workers – Retro PSX Character Pack/Radiation Workers – Retro PSX Character Pack/Textures/Heavy hazmat suit/guantelets radiation.png'
        };
        if (filename && radiationWorkerTextures[filename]) {
            return radiationWorkerTextures[filename];
        }
        return url;
    }

    attachHumanoidAnimationLibrary(root, profileName = 'hazmat_survivor') {
        void profileName;
        return root;
    }

    createCharacterAnimationController(instance, preferredAction = 'idle', trackGlobally = true) {
        void instance;
        void preferredAction;
        void trackGlobally;
        return null;
    }

    loadExternalModelAsset(modelName, modelConfig) {
        const manager = new THREE.LoadingManager();
        manager.setURLModifier((url) => this.resolveExternalAssetUrl(url));
        const path = typeof modelConfig?.path === 'string' ? modelConfig.path.toLowerCase() : '';
        const candidates = path.endsWith('.glb') || path.endsWith('.gltf')
            ? ['glb', 'gltf']
            : path.endsWith('.fbx')
                ? ['fbx']
                : ['obj'];
        const attempted = [];
        const tryLoad = async (type) => {
            attempted.push(type);
            if ((type === 'glb' || type === 'gltf') && typeof GLTFLoader === 'function') {
                const loader = new GLTFLoader(manager);
                return new Promise((resolve, reject) => loader.load(modelConfig.path, resolve, undefined, reject));
            }
            if (type === 'fbx' && typeof FBXLoader === 'function') {
                const loader = new FBXLoader(manager);
                return new Promise((resolve, reject) => loader.load(modelConfig.path, resolve, undefined, reject));
            }
            if (type === 'obj' && typeof OBJLoader === 'function') {
                const loader = new OBJLoader(manager);
                return new Promise((resolve, reject) => loader.load(modelConfig.path, resolve, undefined, reject));
            }
            throw new Error(`Loader unavailable for type "${type}"`);
        };
        return (async () => {
            let lastError = null;
            for (const type of candidates) {
                try {
                    const loaded = await tryLoad(type);
                    const normalized = this.normalizeLoadedModel(modelName, loaded, modelConfig);
                    this.emitRuntimeEvent('modelLoadPathResolved', {
                        id: createRuntimeId('model'),
                        modelName,
                        selectedType: type,
                        attempted
                    });
                    return normalized;
                } catch (error) {
                    lastError = error;
                }
            }
            this.emitRuntimeEvent('modelLoadFailed', {
                id: createRuntimeId('model'),
                modelName,
                attempted,
                error: String(lastError?.message || lastError || 'unknown')
            });
            throw lastError || new Error(`Unable to load external model "${modelName}".`);
        })();
    }

    async loadCachedModel(path) {
        const response = await fetch(path);
        if (!response.ok) throw new Error(`Failed to load cached model: ${response.status}`);
        const json = await response.json();
        const objectLoader = new THREE.ObjectLoader();
        const object = objectLoader.parse(json);
        object.animations = Array.isArray(json.animations)
            ? json.animations.map((clipData) => THREE.AnimationClip.parse(clipData))
            : [];
        return object;
    }

    ensureExplicitModelTexture(texturePath) {
        if (!texturePath) return Promise.resolve(null);
        this._explicitModelTextures ||= new Map();
        this._explicitModelTexturePromises ||= new Map();
        if (this._explicitModelTextures.has(texturePath)) {
            return Promise.resolve(this._explicitModelTextures.get(texturePath));
        }
        if (this._explicitModelTexturePromises.has(texturePath)) {
            return this._explicitModelTexturePromises.get(texturePath);
        }
        const texturePromise = new Promise((resolve, reject) => {
            new THREE.TextureLoader().load(
                texturePath,
                (texture) => {
                    texture.colorSpace = THREE.SRGBColorSpace;
                    this._explicitModelTextures.set(texturePath, texture);
                    this._explicitModelTexturePromises.delete(texturePath);
                    resolve(texture);
                },
                undefined,
                (error) => {
                    this._explicitModelTexturePromises.delete(texturePath);
                    reject(error);
                }
            );
        });
        this._explicitModelTexturePromises.set(texturePath, texturePromise);
        return texturePromise;
    }

    async loadModelAsset(modelName) {
        const externalModel = GameAssets.externalModels?.[modelName];
        if (externalModel) {
            const loadedModel = externalModel.cachePath
                ? await this.loadCachedModel(externalModel.cachePath).catch(() => this.loadExternalModelAsset(modelName, externalModel))
                : await this.loadExternalModelAsset(modelName, externalModel);
            this.loadedAssets.models[modelName] = loadedModel;
            return;
        }

        const modelLoader = new THREE.FileLoader();
        modelLoader.setResponseType('json');

        const modelData = await new Promise((resolve, reject) => {
            modelLoader.load(
                `${GameAssets.MODEL_DATA_PATH_PREFIX}${modelName}.json`,
                resolve,
                undefined,
                reject
            );
        });

        let parsedData;
        try {
            parsedData = typeof modelData === 'string' ? JSON.parse(modelData) : modelData;
        } catch (err) {
            console.warn(`Model JSON parse failed for "${modelName}":`, err);
            return;
        }
        if (!parsedData || typeof parsedData !== 'object') {
            console.warn(`Model data missing or invalid for "${modelName}".`);
            return;
        }
        this.loadedAssets.models[modelName] = this.createModelFromData(parsedData);
    }

    loadAssets() {
        const manager = new THREE.LoadingManager();
        const textureLoader = new THREE.TextureLoader(manager);
        manager.onProgress = (url, loaded, total) => {
            const progress = total > 0 ? Math.round((loaded / total) * 100) : 100;
            if (this.ui.loadingProgress) this.ui.loadingProgress.textContent = `LOADING... ${progress}%`;
            if (this.ui.loadingProgressFill) this.ui.loadingProgressFill.style.width = progress + '%';
            if (this.ui.loadingProgressBar) {
                this.ui.loadingProgressBar.setAttribute('aria-valuenow', String(progress));
            }
        };
        manager.onLoad = async () => {
            this.gameState.isLoaded = true;
            this.setShaderIntensity('high');
            this.emitRuntimeEvent('stateChanged', { id: createRuntimeId('state'), key: 'isLoaded', value: true });

            this.animate();

            await this.autoLaunchDevMode();
        };

        const explicitTextureManifest = GameAssets.textureManifest || {};
        manager.itemStart('asset-bootstrap');
        Object.entries(explicitTextureManifest).forEach(([filename, assetPath]) => {
            textureLoader.load(
                assetPath.startsWith('./') ? assetPath : GameAssets.IMG_PREFIX + assetPath,
                (texture) => {
                    texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
                    texture.name = filename;
                    this.loadedAssets.textures[filename] = texture;
                },
                undefined,
                (error) => {
                    console.warn(`Failed to load texture: ${filename} from ${assetPath}.`, error);
                }
            );
        });
        manager.itemEnd('asset-bootstrap');
    }

    gameOver() {
        if (this.gameState.isGameOver) return;
        this.gameState.isGameOver = true;
        this.gameState.isPaused = true;
        this.emitRuntimeEvent('stateChanged', { id: createRuntimeId('state'), key: 'isGameOver', value: true });
        this.emitRuntimeEvent('stateChanged', { id: createRuntimeId('state'), key: 'isPaused', value: true, reason: 'gameOver' });
        this.levelTimeouts.forEach(timeoutId => clearTimeout(timeoutId));
        this.levelIntervals.forEach(intervalId => clearInterval(intervalId));
        this.levelTimeouts = [];
        this.levelIntervals = [];
        this.stopLoopingAudio('music');
        this.stopLoopingAudio('chase');
        this.controls.unlock();
        this.ui.hud?.classList.remove('visible');
        this.ui.pauseMenu && (this.ui.pauseMenu.style.display = 'none');
        if (this._gameOverUiTimeout) clearTimeout(this._gameOverUiTimeout);
        const gameOverUiDelayMs = Math.min(30000, Math.max(0, 1200));
        this._gameOverUiTimeout = window.setTimeout(() => {
            this._gameOverUiTimeout = null;
            if (!this.ui.gameOverScreen) return;
            this.ui.gameOverScreen.style.display = 'flex';
            this.setShaderIntensity('high');
            const titleEl = document.getElementById('game-over-title');
            if (titleEl && typeof titleEl.focus === 'function') {
                titleEl.setAttribute('tabindex', '-1');
                titleEl.focus();
            }
        }, gameOverUiDelayMs);
    }

    reloadGameFromBestAvailableStart() {
        window.location.reload();
    }

    async startGame() {
        if (!this.gameStarted) {
            this.completedEventIds = new Set();
            await this.loadLevel('dev_level.json');
            this.gameStarted = true;
        }
    }

    async autoLaunchDevMode() {
        this.completedEventIds = new Set();
        await this.startGame();
        if (this.ui.loadingScreen) this.ui.loadingScreen.style.display = 'none';
        if (this.ui.startMenu) this.ui.startMenu.style.display = 'none';
        if (this.ui.pauseMenu) this.ui.pauseMenu.style.display = 'none';
        this.ui.hud?.classList.add('visible');
        this.gameState.isPaused = true;
        if (this.ui.startMenu) {
            this.ui.startMenu.style.display = 'flex';
            const title = this.ui.startMenu.querySelector('h1');
            if (title) title.textContent = 'PROTO ENGINE';
            const copy = this.ui.startMenu.querySelector('p');
            if (copy) copy.textContent = 'Prototype hall ready. Click to lock in and test movement, gravity, and physics.';
            const prompt = this.ui.startMenu.querySelector('.prompt');
            if (prompt) prompt.textContent = '[ CLICK TO ENTER TEST HALL ]';
        }
    }

    completeGame(title = 'ECHO STABILIZED', text = 'You reached the surviving staff and sealed the breach.') {
        this.gameState.isGameOver = true;
        this.gameState.isPaused = true;
        this.emitRuntimeEvent('gameflowCompleted', { id: createRuntimeId('gameflow'), title, text });
        this.levelTimeouts.forEach(timeoutId => clearTimeout(timeoutId));
        this.levelIntervals.forEach(intervalId => clearInterval(intervalId));
        this.levelTimeouts = [];
        this.levelIntervals = [];
        this.stopLoopingAudio('music');
        this.stopLoopingAudio('chase');
        this.controls.unlock();
        if (this.animationFrameId) cancelAnimationFrame(this.animationFrameId);

        const titleEl = document.getElementById('game-over-title');
        const textEl = document.getElementById('game-over-copy');
        const promptEl = document.getElementById('game-over-prompt');
        if (titleEl) titleEl.textContent = title;
        if (textEl) textEl.textContent = text;
        if (promptEl) promptEl.textContent = '[ CLICK TO RESTART ]';

        this.ui.hud?.classList.remove('visible');
        this.ui.pauseMenu && (this.ui.pauseMenu.style.display = 'none');
        this.ui.startMenu && (this.ui.startMenu.style.display = 'none');
        this.ui.gameOverScreen && (this.ui.gameOverScreen.style.display = 'flex');
        this.setShaderIntensity('high');
        if (titleEl && typeof titleEl.focus === 'function') {
            titleEl.setAttribute('tabindex', '-1');
            titleEl.focus();
        }
    }

    onWindowResize() {
        const w = window.innerWidth;
        const h = window.innerHeight;
        const dpr = Math.min(window.devicePixelRatio || 1, 2);
        this.pixelRatio = dpr;
        this.camera.aspect = w / h;
        this.camera.updateProjectionMatrix();
        this.renderer.setPixelRatio(dpr);
        this.renderer.setSize(w, h);
        this.composer?.setPixelRatio?.(dpr);
        this.composer?.setSize?.(w, h);
    }

    clearHudDismissTimer(channel) {
        if (!this.hudDismissTimers) return;
        const existing = this.hudDismissTimers.get(channel);
        if (existing != null) {
            clearTimeout(existing);
            this.hudDismissTimers.delete(channel);
        }
    }

    scheduleHudDismiss(channel, duration, callback) {
        this.clearHudDismissTimer(channel);
        const id = window.setTimeout(() => {
            this.hudDismissTimers?.delete(channel);
            callback?.();
        }, duration);
        this.hudDismissTimers?.set(channel, id);
    }

    stopLoopingAudio(channel) {
        void channel;
    }

    stopAllNonVoiceAudio() {}

    updateSoundscape(delta) {
        void delta;
    }

    registerCollidable(mesh) {
        if (!mesh) return;
        if (this.collidableMeshes.includes(mesh)) return;
        this.collidableMeshes.push(mesh);
        if (mesh.userData.dynamicCollider) this.dynamicCollidableMeshes.push(mesh);
        else {
            this.ensureMeshBVH(mesh);
            this.staticCollidableMeshes.push(mesh);
            if (mesh.userData.structuralCollider !== false) this.structuralCollidableMeshes.push(mesh);
        }
    }

    unregisterCollidable(mesh) {
        if (!mesh) return;
        const remove = (arr) => {
            const index = arr.indexOf(mesh);
            if (index >= 0) arr.splice(index, 1);
        };
        const wasStatic = this.staticCollidableMeshes.includes(mesh);
        remove(this.collidableMeshes);
        remove(this.dynamicCollidableMeshes);
        remove(this.staticCollidableMeshes);
        remove(this.structuralCollidableMeshes);
        this.releaseMeshBVH(mesh);
        if (wasStatic) this.rebuildSpatialIndices();
    }

    rebuildSpatialIndices() {
        this.staticCollisionIndex = null;
        this.structuralCollisionIndex = null;
    }

    queryCollidablesForBox(box) {
        const statics = this.staticCollisionIndex ? this.staticCollisionIndex.queryBox(box) : [...this.staticCollidableMeshes];
        return this.dynamicCollidableMeshes.length > 0 ? [...statics, ...this.dynamicCollidableMeshes] : statics;
    }

    queryCollidablesForRay(origin, direction, far) {
        const statics = this.staticCollisionIndex ? this.staticCollisionIndex.queryRay(origin, direction, far) : [...this.staticCollidableMeshes];
        return this.dynamicCollidableMeshes.length > 0 ? [...statics, ...this.dynamicCollidableMeshes] : statics;
    }

    queryStructuralCollidablesForBox(box) {
        const statics = this.structuralCollisionIndex ? this.structuralCollisionIndex.queryBox(box) : [...this.structuralCollidableMeshes];
        return statics;
    }

    queryStructuralCollidablesForRay(origin, direction, far) {
        const statics = this.structuralCollisionIndex ? this.structuralCollisionIndex.queryRay(origin, direction, far) : [...this.structuralCollidableMeshes];
        return statics;
    }

    objectContainsObject(root, object) {
        if (!root || !object) return false;
        let current = object;
        while (current) {
            if (current === root) return true;
            current = current.parent;
        }
        return false;
    }

    getCollisionBounds(mesh, targetBox = new THREE.Box3()) {
        if (!mesh) return targetBox.makeEmpty();
        if (mesh.userData?.boundingBox instanceof THREE.Box3 && !mesh.userData?.dynamicCollider) {
            return targetBox.copy(mesh.userData.boundingBox);
        }
        return targetBox.setFromObject(mesh);
    }

    ensureMeshBVH(mesh) {
        if (!mesh?.isMesh || mesh.userData?.dynamicCollider) return false;
        const geometry = mesh.geometry;
        if (!geometry || typeof geometry.computeBoundsTree !== 'function') return false;
        if (geometry.boundsTree) {
            if (!mesh.userData.bvhEnabled) {
                mesh.userData.bvhEnabled = true;
                this.bvhMeshCount += 1;
            }
            return true;
        }
        try {
            geometry.computeBoundsTree();
            mesh.userData.bvhEnabled = true;
            this.bvhMeshCount += 1;
            return true;
        } catch {
            mesh.userData.bvhEnabled = false;
            return false;
        }
    }

    releaseMeshBVH(mesh) {
        if (!mesh?.isMesh || !mesh.userData?.bvhEnabled) return;
        mesh.userData.bvhEnabled = false;
        this.bvhMeshCount = Math.max(0, this.bvhMeshCount - 1);
    }

    refineTraceHitSurface(object, queryBox, fallbackPoint, fallbackNormal) {
        if (!object || !queryBox || !fallbackPoint || !fallbackNormal) return null;
        if (!object.isObject3D) return null;
        if (!Number.isFinite(fallbackNormal.x) || !Number.isFinite(fallbackNormal.y) || !Number.isFinite(fallbackNormal.z)) return null;
        if (fallbackNormal.lengthSq() < 1e-10) return null;

        queryBox.getSize(this._traceSize);
        const probeDistance = THREE.MathUtils.clamp(this._traceSize.length() * 0.35 + 0.18, 0.18, 1.4);
        this._traceProbeDirection.copy(fallbackNormal).normalize().multiplyScalar(-1);
        this._traceProbeOrigin.copy(fallbackPoint).addScaledVector(fallbackNormal, probeDistance);
        this._traceRaycaster.set(this._traceProbeOrigin, this._traceProbeDirection);
        this._traceRaycaster.far = probeDistance * 2.2;

        const intersects = this._traceRaycaster.intersectObject(object, true);
        for (const hit of intersects) {
            if (!hit?.face?.normal || !hit.point) continue;
            const worldNormal = hit.face.normal.clone()
                .applyNormalMatrix(this._traceNormalMatrix.getNormalMatrix(hit.object.matrixWorld));
            if (!Number.isFinite(worldNormal.x) || !Number.isFinite(worldNormal.y) || !Number.isFinite(worldNormal.z)) continue;
            if (worldNormal.lengthSq() < 1e-10) continue;
            worldNormal.normalize();
            if (worldNormal.dot(fallbackNormal) < 0.1) continue;
            return {
                point: hit.point.clone(),
                normal: worldNormal
            };
        }

        return null;
    }

    computeTraceBoxHit(queryBox, candidateBox, object) {
        this._traceIntersection.copy(queryBox).intersect(candidateBox);
        const size = this._traceIntersection.max.clone().sub(this._traceIntersection.min);
        if (size.x <= 0 || size.y <= 0 || size.z <= 0 || !Number.isFinite(size.x) || !Number.isFinite(size.y) || !Number.isFinite(size.z)) {
            return null;
        }

        queryBox.getCenter(this._traceCenterA);
        candidateBox.getCenter(this._traceCenterB);
        this._traceDelta.subVectors(this._traceCenterA, this._traceCenterB);

        let axis = 'x';
        let penetration = size.x;
        this._traceNormal.set(Math.sign(this._traceDelta.x) || 1, 0, 0);
        if (size.y < penetration) {
            axis = 'y';
            penetration = size.y;
            this._traceNormal.set(0, Math.sign(this._traceDelta.y) || 1, 0);
        }
        if (size.z < penetration) {
            axis = 'z';
            penetration = size.z;
            this._traceNormal.set(0, 0, Math.sign(this._traceDelta.z) || 1);
        }

        this._tracePoint.copy(this._traceCenterA);
        if (axis === 'x') {
            this._tracePoint.x = this._traceNormal.x > 0 ? candidateBox.max.x : candidateBox.min.x;
            this._tracePoint.y = THREE.MathUtils.clamp(this._traceCenterA.y, candidateBox.min.y, candidateBox.max.y);
            this._tracePoint.z = THREE.MathUtils.clamp(this._traceCenterA.z, candidateBox.min.z, candidateBox.max.z);
        } else if (axis === 'y') {
            this._tracePoint.y = this._traceNormal.y > 0 ? candidateBox.max.y : candidateBox.min.y;
            this._tracePoint.x = THREE.MathUtils.clamp(this._traceCenterA.x, candidateBox.min.x, candidateBox.max.x);
            this._tracePoint.z = THREE.MathUtils.clamp(this._traceCenterA.z, candidateBox.min.z, candidateBox.max.z);
        } else {
            this._tracePoint.z = this._traceNormal.z > 0 ? candidateBox.max.z : candidateBox.min.z;
            this._tracePoint.x = THREE.MathUtils.clamp(this._traceCenterA.x, candidateBox.min.x, candidateBox.max.x);
            this._tracePoint.y = THREE.MathUtils.clamp(this._traceCenterA.y, candidateBox.min.y, candidateBox.max.y);
        }

        const refined = this.refineTraceHitSurface(object, queryBox, this._tracePoint, this._traceNormal);

        return {
            object,
            box: candidateBox.clone(),
            point: refined?.point || this._tracePoint.clone(),
            normal: refined?.normal || this._traceNormal.clone(),
            penetration
        };
    }

    traceBox(queryBox, options = {}) {
        if (!queryBox) return null;
        const {
            structuralOnly = false,
            ignoreObject = null,
            predicate = null
        } = options;
        const candidates = structuralOnly ? this.queryStructuralCollidablesForBox(queryBox) : this.queryCollidablesForBox(queryBox);
        for (const candidate of candidates) {
            if (!candidate) continue;
            if (ignoreObject && (this.objectContainsObject(ignoreObject, candidate) || this.objectContainsObject(candidate, ignoreObject))) continue;
            if (predicate && predicate(candidate) === false) continue;
            const candidateBox = this.getCollisionBounds(candidate, this._traceCandidateBox);
            if (!queryBox.intersectsBox(candidateBox)) continue;
            return {
                object: candidate,
                box: candidateBox.clone(),
                point: queryBox.getCenter(new THREE.Vector3()),
                normal: new THREE.Vector3(0, 1, 0),
                penetration: 0
            };
        }
        return null;
    }

    traceRay(origin, direction, far, options = {}) {
        if (!origin || !direction || !Number.isFinite(far)) return null;
        const {
            structuralOnly = false,
            ignoreObject = null,
            predicate = null
        } = options;
        const candidates = structuralOnly ? this.queryStructuralCollidablesForRay(origin, direction, far) : this.queryCollidablesForRay(origin, direction, far);
        const raycaster = new THREE.Raycaster(origin, direction, 0, far);
        const hits = raycaster.intersectObjects(candidates, true);
        for (const hit of hits) {
            if (ignoreObject && this.objectContainsObject(ignoreObject, hit.object)) continue;
            if (predicate && predicate(hit.object, hit) === false) continue;
            return hit;
        }
        return null;
    }

    traceSweptBox(queryBox, delta, options = {}) {
        if (!queryBox || !delta) return null;
        const {
            structuralOnly = false,
            ignoreObject = null,
            predicate = null
        } = options;
        const sweepBounds = queryBox.clone().union(queryBox.clone().translate(delta));
        const candidates = structuralOnly ? this.queryStructuralCollidablesForBox(sweepBounds) : this.queryCollidablesForBox(sweepBounds);
        let bestHit = null;
        for (const candidate of candidates) {
            if (!candidate) continue;
            if (ignoreObject && (this.objectContainsObject(ignoreObject, candidate) || this.objectContainsObject(candidate, ignoreObject))) continue;
            if (predicate && predicate(candidate) === false) continue;
            const candidateBox = this.getCollisionBounds(candidate, this._traceCandidateBox);
            if (delta.y < -1e-8) {
                const overlapsX = queryBox.max.x > candidateBox.min.x && queryBox.min.x < candidateBox.max.x;
                const overlapsZ = queryBox.max.z > candidateBox.min.z && queryBox.min.z < candidateBox.max.z;
                if (overlapsX && overlapsZ && queryBox.min.y >= candidateBox.max.y) {
                    const endMinY = queryBox.min.y + delta.y;
                    if (endMinY <= candidateBox.max.y) {
                        const time = THREE.MathUtils.clamp((queryBox.min.y - candidateBox.max.y) / (-delta.y), 0, 1);
                        if (!bestHit || time < bestHit.time) {
                            bestHit = {
                                object: candidate,
                                box: candidateBox.clone(),
                                point: new THREE.Vector3(
                                    THREE.MathUtils.clamp((queryBox.min.x + queryBox.max.x) * 0.5, candidateBox.min.x, candidateBox.max.x),
                                    candidateBox.max.y,
                                    THREE.MathUtils.clamp((queryBox.min.z + queryBox.max.z) * 0.5, candidateBox.min.z, candidateBox.max.z)
                                ),
                                normal: new THREE.Vector3(0, 1, 0),
                                time,
                                penetration: 0
                            };
                        }
                    }
                }
            }
        }
        if (bestHit) return bestHit;
        return null;
    }

    traceCompoundBoxes(sampleBoxes, options = {}) {
        if (!Array.isArray(sampleBoxes) || sampleBoxes.length === 0) return null;
        for (const sampleBox of sampleBoxes) {
            const hit = this.traceBox(sampleBox, options);
            if (hit) return hit;
        }
        return null;
    }

    traceCompoundBoxesSweep(sampleBoxes, delta, options = {}) {
        if (!Array.isArray(sampleBoxes) || sampleBoxes.length === 0 || !delta) return null;
        let bestHit = null;
        for (const sampleBox of sampleBoxes) {
            const hit = this.traceSweptBox(sampleBox, delta, options);
            if (hit && (!bestHit || hit.time < bestHit.time)) bestHit = hit;
        }
        return bestHit;
    }

    slideMoveCompoundBoxes(sampleBoxes, delta, options = {}) {
        if (!Array.isArray(sampleBoxes) || sampleBoxes.length === 0) {
            return {
                positionDelta: delta ? delta.clone() : new THREE.Vector3(),
                remainingDelta: new THREE.Vector3(),
                hits: [],
                blocked: false
            };
        }
        const primary = sampleBoxes[0];
        if (!primary) {
            return {
                positionDelta: delta ? delta.clone() : new THREE.Vector3(),
                remainingDelta: new THREE.Vector3(),
                hits: [],
                blocked: false
            };
        }
        const result = this.slideMoveBox(primary, delta || new THREE.Vector3(), options);
        return {
            positionDelta: result.positionDelta,
            remainingDelta: result.remainingDelta,
            hits: result.hits,
            blocked: result.blocked
        };
    }

    slideMoveBox(queryBox, delta, options = {}) {
        if (!queryBox || !delta) {
            return {
                positionDelta: new THREE.Vector3(),
                remainingDelta: new THREE.Vector3(),
                endBox: queryBox ? queryBox.clone() : new THREE.Box3(),
                hits: [],
                blocked: false
            };
        }
        const {
            maxBumps = 4,
            epsilon = 0.001,
            ...traceOptions
        } = options;
        this._slideWorkingBox.copy(queryBox);
        this._slideMoved.set(0, 0, 0);
        this._slideRemaining.copy(delta);
        const hits = [];
        let blocked = false;

        for (let bump = 0; bump < maxBumps; bump += 1) {
            if (this._slideRemaining.lengthSq() <= 1e-10) break;
            const hit = this.traceSweptBox(this._slideWorkingBox, this._slideRemaining, traceOptions);
            if (!hit) {
                this._slideWorkingBox.translate(this._slideRemaining);
                this._slideMoved.add(this._slideRemaining);
                this._slideRemaining.set(0, 0, 0);
                break;
            }

            blocked = true;
            const moveTime = Math.max(0, hit.time - epsilon);
            this._slideStep.copy(this._slideRemaining).multiplyScalar(moveTime);
            if (this._slideStep.lengthSq() > 0) {
                this._slideWorkingBox.translate(this._slideStep);
                this._slideMoved.add(this._slideStep);
            }
            hits.push({
                object: hit.object,
                point: hit.point?.clone?.() || null,
                normal: hit.normal?.clone?.() || null,
                time: hit.time,
                box: hit.box?.clone?.() || null
            });

            const remainingScale = Math.max(0, 1 - hit.time);
            this._slideClip.copy(this._slideRemaining).multiplyScalar(remainingScale);
            const intoSurface = this._slideClip.dot(hit.normal);
            if (intoSurface < 0) {
                this._slideClip.addScaledVector(hit.normal, -intoSurface);
            }
            this._slideRemaining.copy(this._slideClip);
            this._slideNudge.copy(hit.normal).multiplyScalar(epsilon * 2);
            this._slideWorkingBox.translate(this._slideNudge);
            this._slideMoved.add(this._slideNudge);
        }

        return {
            positionDelta: this._slideMoved.clone(),
            remainingDelta: this._slideRemaining.clone(),
            endBox: this._slideWorkingBox.clone(),
            hits,
            blocked
        };
    }

    findGroundHit(origin, options = {}) {
        const {
            maxDistance = 2,
            structuralOnly = false,
            ignoreObject = null,
            minNormalY = 0.5,
            traceTag = null
        } = options;
        return this.traceRay(origin, new THREE.Vector3(0, -1, 0), maxDistance, {
            structuralOnly,
            ignoreObject,
            traceTag,
            predicate: (object, hit) => {
                if (!hit?.face?.normal) return false;
                const worldNormal = hit.face.normal.clone()
                    .applyNormalMatrix(new THREE.Matrix3().getNormalMatrix(object.matrixWorld));
                if (worldNormal.lengthSq() < 1e-12) return false;
                if (!Number.isFinite(worldNormal.x) || !Number.isFinite(worldNormal.y) || !Number.isFinite(worldNormal.z)) return false;
                worldNormal.normalize();
                hit.normal = worldNormal;
                hit.worldNormal = worldNormal;
                return worldNormal.y >= minNormalY;
            }
        });
    }

    updateTriggerVolumeBounds(volume) {
        if (!volume) return null;
        if (!volume.userData) volume.userData = {};
        const box = volume.userData.boundingBox instanceof THREE.Box3 ? volume.userData.boundingBox : new THREE.Box3();
        if (volume.shape === 'box') {
            box.setFromCenterAndSize(volume.position, volume.size);
        } else if (volume.shape === 'cylinder') {
            const radius = Math.max(0.25, Number(volume.radius) || 0.5);
            const height = Math.max(0.25, Number(volume.height) || volume.size?.y || 2);
            box.min.set(volume.position.x - radius, volume.position.y - (height * 0.5), volume.position.z - radius);
            box.max.set(volume.position.x + radius, volume.position.y + (height * 0.5), volume.position.z + radius);
        } else {
            box.min.set(volume.position.x - volume.radius, volume.position.y - volume.radius, volume.position.z - volume.radius);
            box.max.set(volume.position.x + volume.radius, volume.position.y + volume.radius, volume.position.z + volume.radius);
        }
        volume.userData.boundingBox = box;
        return box;
    }

    rebuildTriggerVolumeIndex() {
        this.triggerVolumes.forEach((volume) => this.updateTriggerVolumeBounds(volume));
        this.triggerVolumeIndex = null;
    }

    queryTriggerVolumesForPoint(point) {
        if (!point) return [];
        this._triggerQueryBox.min.copy(point).sub(this._triggerQueryPadding);
        this._triggerQueryBox.max.copy(point).add(this._triggerQueryPadding);
        const candidates = this.triggerVolumeIndex ? this.triggerVolumeIndex.queryBox(this._triggerQueryBox) : [...this.triggerVolumes];
        return candidates;
    }

    disposeMaterial(material) {
        if (!material) return;
        if (material.map) material.map.dispose();
        if (material.normalMap) material.normalMap.dispose();
        if (material.emissiveMap) material.emissiveMap.dispose();
        material.dispose();
    }

    disposeMesh(mesh) {
        if (!mesh) return;
        if (mesh instanceof THREE.Group) {
            while (mesh.children.length > 0) {
                this.disposeMesh(mesh.children[0]);
                mesh.remove(mesh.children[0]);
            }
            return;
        }
        if (mesh instanceof THREE.Mesh) {
            if (mesh.geometry) mesh.geometry.dispose();
            if (mesh.material) {
                if (Array.isArray(mesh.material)) {
                    mesh.material.forEach(m => this.disposeMaterial(m));
                } else {
                    this.disposeMaterial(mesh.material);
                }
            }
        }
    }

    clearLevel() {
        this.flickerIntervals.forEach(interval => clearInterval(interval));
        this.flickerIntervals = [];
        this.levelTimeouts.forEach(timeoutId => clearTimeout(timeoutId));
        this.levelIntervals.forEach(intervalId => clearInterval(intervalId));
        this.levelTimeouts = [];
        this.levelIntervals = [];

        if (this.hudDismissTimers?.size) {
            this.hudDismissTimers.forEach((timeoutId) => clearTimeout(timeoutId));
            this.hudDismissTimers.clear();
        }
        if (this._levelNameToastTimeout) {
            clearTimeout(this._levelNameToastTimeout);
            this._levelNameToastTimeout = null;
        }

        this.lightSafetyState = { playerInside: false, known: false, lastMessageAt: -Infinity };
        this.levelLightsById = new Map();
        this.levelNodesById = new Map();
        this.triggerVolumes = [];
        this.staticCollidableMeshes = [];
        this.structuralCollidableMeshes = [];
        this.dynamicCollidableMeshes = [];
        this.staticCollisionIndex = null;
        this.structuralCollisionIndex = null;
        this.triggerVolumeIndex = null;
        this.currentLevelData = null;
        this.bvhMeshCount = 0;
        this.activeAudioEnvironmentState = null;
        this.clearDebugHelpers();

        if (this.levelObjects) {
            this.scene.remove(this.levelObjects);
            this.levelObjects.traverse(child => {
                this.disposeMesh(child);
            });
            this.levelObjects = null;
        }
        
        this.levelGeometries.forEach(geo => geo.dispose());
        this.levelGeometries = [];

        this.collidableMeshes = [];
        
        this.levelObjects = new THREE.Group();
        this.scene.add(this.levelObjects);
    }

    async loadLevel(levelFilename) {
        this.emitRuntimeEvent('levelLoadRequested', { id: createRuntimeId('level'), levelFilename });
        const overlay = this.ui.levelTransitionOverlay;
        const useFade = overlay && this.gameState.isLoaded;
        if (useFade) {
            overlay.style.opacity = '1';
            await new Promise(r => setTimeout(r, 250));
        }
        this.currentLevelFilename = levelFilename;
        try {
            await this.loadLevelFromFile(levelFilename);
        } catch (err) {
            console.error(`Failed to load level: ${levelFilename}`, err);
            this.emitRuntimeEvent('levelLoadFailed', {
                id: createRuntimeId('level'),
                levelFilename,
                error: String(err?.message || err || 'unknown')
            });
        } finally {
            if (useFade && overlay) overlay.style.opacity = '0';
        }
    }

    async loadLevelFromFile(levelFilename) {
        const response = await fetch('./' + levelFilename);
        if (!response.ok) throw new Error(`Network response was not ok. Status: ${response.status}`);
        const text = await response.text();
        let rawLevelData;
        try {
            rawLevelData = JSON.parse(text);
        } catch {
            throw new Error(STRINGS.msg.levelJsonParse);
        }
        if (!rawLevelData || typeof rawLevelData !== 'object' || Array.isArray(rawLevelData)) {
            throw new Error(STRINGS.msg.levelNotObject);
        }
        const expandedLevelData = rawLevelData;
        const validationError = this.validateLevelData(expandedLevelData, levelFilename);
        if (validationError) throw new Error(validationError);
        const structuralCompilerSettings = expandedLevelData?.settings?.structuralCompiler || {};
        const levelData = {
            ...expandedLevelData,
            geometry: this.compileStructuralGeometryNodes(expandedLevelData.geometry || [], structuralCompilerSettings)
        };

        this.clearLevel();
        this.currentLevelData = levelData;

        const requiredModels = new Set();
        levelData.modelInstances?.forEach((d) => d.modelName && requiredModels.add(d.modelName));
        levelData.spawners?.forEach((s) => s.type === 'friendly_npc' && s.modelName && requiredModels.add(s.modelName));
        levelData.spawners?.forEach((s) => s.type === 'physics_prop' && s.modelName && requiredModels.add(s.modelName));
        const modelsToLoad = [...requiredModels].filter((name) => !this.loadedAssets.models[name]);
    
        if (modelsToLoad.length > 0) {
            if (this.ui.loadingProgress) this.ui.loadingProgress.textContent = `Loading ${levelFilename}...`;
            if (this.ui.loadingProgressFill) this.ui.loadingProgressFill.style.width = '10%';
            if (this.ui.loadingProgressBar) this.ui.loadingProgressBar.setAttribute('aria-valuenow', '10');
            await Promise.all(modelsToLoad.map(async (modelName) => {
                try {
                    await this.loadModelAsset(modelName);
                } catch (error) {
                    console.error(`Failed to load level model: ${modelName}.`, error);
                }
            }));
        }

        if (this.ui.loadingProgressFill) this.ui.loadingProgressFill.style.width = '100%';
        if (this.ui.loadingProgressBar) this.ui.loadingProgressBar.setAttribute('aria-valuenow', '100');
        this.ui.loadingProgress && (this.ui.loadingProgress.textContent = 'Ready.');
        if (levelData.settings && levelData.settings.backgroundColor) this.scene.background = new THREE.Color(levelData.settings.backgroundColor);
        levelData.lights?.forEach((d) => this.createLightFromData(d));
        const geometryNodes = Array.isArray(levelData.geometry) ? levelData.geometry : [];
        geometryNodes.forEach((d) => this.createGeometryFromData(d));
        levelData.modelInstances?.forEach((d) => this.createModelInstanceFromData(d));
        this.rebuildSpatialIndices();
        this.rebuildTriggerVolumeIndex();

        const playerStartData = (Array.isArray(levelData.spawners)
            ? levelData.spawners.find((spawner) => spawner?.type === 'player_start')
            : null);
        if (playerStartData) {
            const spawnStance = typeof playerStartData.stance === 'string' && this.player?.eyeHeights?.[playerStartData.stance]
                ? playerStartData.stance
                : 'standing';
            this.player.stance = spawnStance;
            this.player.targetHeight = this.player.eyeHeights[spawnStance];
            this.placePlayerAtSpawn(this.resolvePlayerStartFeetPosition(playerStartData), playerStartData);
            this.player.velocity.set(0, 0, 0);
            this.finalizePlayerSpawnGrounding();
        }

        this.gameState.invulnerableUntil = this.getElapsedTime() + 1.5;

        this.visitedLevels.add(levelFilename);
        this.emitRuntimeEvent('levelLoaded', {
            id: createRuntimeId('level'),
            levelFilename,
            levelName: levelData.levelName || levelFilename,
            counts: {
                geometry: Array.isArray(levelData.geometry) ? levelData.geometry.length : 0,
                lights: Array.isArray(levelData.lights) ? levelData.lights.length : 0,
                modelInstances: Array.isArray(levelData.modelInstances) ? levelData.modelInstances.length : 0,
                spawners: Array.isArray(levelData.spawners) ? levelData.spawners.length : 0
            }
        });
        this.showLevelNameToast(levelData.levelName || levelFilename);
    }

    dispatchBuiltinEntityInput(targetId, inputName, parameter, context = {}) {
        const normalizedInput = String(inputName || '').toLowerCase();
        switch (normalizedInput) {
            case 'enable':
            case 'turnon': {
                return this.setLevelNodeEnabled(targetId, true);
            }
            case 'disable':
            case 'turnoff': {
                return this.setLevelNodeEnabled(targetId, false);
            }
            case 'toggle': {
                const node = this.getLevelNode(targetId);
                if (node) {
                    return this.setLevelNodeEnabled(targetId, !node.visible);
                }
                return false;
            }
            case 'setemissive':
                return this.setNodeEmissiveIntensity(targetId, Number(parameter) || 1);
            case 'canceltimer':
                return false;
            case 'trigger': {
                const node = this.getLevelNode(targetId);
                if (node?.userData?.onInteract && typeof node.userData.onInteract === 'function') {
                    try {
                        node.userData.onInteract(node);
                        return true;
                    } catch (err) {
                        console.warn('Builtin entity input trigger interact failed', err);
                        return false;
                    }
                }
                return false;
            }
            default:
                return false;
        }
    }

    dispatchEntityInput(targetId, inputName, parameter, context = {}) {
        void targetId;
        void inputName;
        void parameter;
        void context;
        return false;
    }

    showLevelNameToast(levelName) {
        void levelName;
    }

    validateLevelData(levelData, levelFilename) {
        if (!levelData || typeof levelData !== 'object' || Array.isArray(levelData)) {
            const invalidMessage = STRINGS.msg.levelInvalid;
            this.emitRuntimeEvent('schemaValidated', {
                id: createRuntimeId('schema'),
                ok: false,
                target: levelFilename || 'level',
                error: invalidMessage
            });
            return invalidMessage;
        }
        this.emitRuntimeEvent('schemaValidated', {
            id: createRuntimeId('schema'),
            ok: true,
            target: levelFilename || 'level',
            error: null
        });
        return null;
    }

    

    getLevelNode(id) {
        if (!id) return null;
        return this.levelNodesById.get(id) || null;
    }

    getLevelNodeRuntimeObjects(node) {
        if (!node) return [];
        const objects = [];
        node.traverse?.((child) => {
            objects.push(child);
        });
        if (objects.length === 0) objects.push(node);
        return objects;
    }

    setLevelNodeEnabled(nodeId, enabled) {
        const node = this.getLevelNode(nodeId);
        if (!node) return false;
        const runtimeObjects = this.getLevelNodeRuntimeObjects(node);
        let needsSpatialRebuild = false;
        runtimeObjects.forEach((object) => {
            object.visible = enabled;

            if (!enabled) {
                if (this.collidableMeshes.includes(object)) {
                    object.userData._ioWasCollidable = true;
                    this.unregisterCollidable(object);
                    if (!object.userData.dynamicCollider) needsSpatialRebuild = true;
                }
                return;
            }

            if (object.userData?._ioWasCollidable && !this.collidableMeshes.includes(object)) {
                this.registerCollidable(object);
                if (!object.userData.dynamicCollider) needsSpatialRebuild = true;
            }
        });
        if (needsSpatialRebuild) {
            this.rebuildSpatialIndices();
        }
        return true;
    }

    setNodeEmissiveIntensity(nodeId, intensity) {
        const node = this.getLevelNode(nodeId);
        if (!node) return false;
        const applyToMaterial = (material) => {
            if (!material || !('emissiveIntensity' in material)) return;
            material.emissiveIntensity = intensity;
        };
        if (node.isMesh) {
            if (Array.isArray(node.material)) node.material.forEach(applyToMaterial);
            else applyToMaterial(node.material);
            return true;
        }
        node.traverse?.((child) => {
            if (!child.isMesh) return;
            if (Array.isArray(child.material)) child.material.forEach(applyToMaterial);
            else applyToMaterial(child.material);
        });
        return true;
    }

    createDoorMaterial(textureKey, options = {}) {
        const params = {
            color: options.color ?? 0xffffff,
            roughness: options.roughness ?? 0.82,
            metalness: options.metalness ?? 0.72
        };
        const baseTexture = textureKey ? this.loadedAssets.textures[textureKey] : null;
        if (baseTexture) {
            params.map = baseTexture.clone();
            params.map.wrapS = THREE.RepeatWrapping;
            params.map.wrapT = THREE.RepeatWrapping;
            const repeat = finiteVec2Components(options.repeat, [1, 1]);
            params.map.repeat.set(repeat[0], repeat[1]);
        }
        if (options.emissive) {
            params.emissive = new THREE.Color(options.emissive);
            params.emissiveIntensity = options.emissiveIntensity ?? 1;
            if (params.map) params.emissiveMap = params.map;
        }
        return new THREE.MeshStandardMaterial(params);
    }

    isPlayerInsideTriggerVolume(volume) {
        const playerPos = this.player?.camera?.position;
        return this.isPointInsideVolume(playerPos, volume);
    }

    isPointInsideVolume(point, volume) {
        return pointInsideAuthoringVolume(point, volume);
    }

    getPhysicsVolumeModifiersAt(position) {
        const modifiers = {
            gravityScale: 1,
            jumpScale: 1,
            drag: 0,
            buoyancy: 0,
            flow: new THREE.Vector3(),
            activeVolumes: [],
            activeFluids: [],
            activeSurfaceVelocity: [],
            activeRadialForces: []
        };
        if (!position) return modifiers;
        modifiers.drag = THREE.MathUtils.clamp(modifiers.drag, 0, 8);
        modifiers.buoyancy = THREE.MathUtils.clamp(modifiers.buoyancy, 0, 3);
        modifiers.gravityScale = THREE.MathUtils.clamp(modifiers.gravityScale, -2, 4);
        modifiers.jumpScale = THREE.MathUtils.clamp(modifiers.jumpScale, 0, 4);
        return modifiers;
    }

    updateEnvironmentalVolumes() {
        this.activeAudioEnvironmentState = null;
    }

    updateTriggerVolumes() {
        if (!this.player?.camera || !Array.isArray(this.triggerVolumes) || this.triggerVolumes.length === 0) return;
        const now = this.getElapsedTime();
        const candidates = new Set(this.queryTriggerVolumesForPoint(this.player.camera.position));
        for (const volume of this.triggerVolumes) {
            if (volume?.playerInside) candidates.add(volume);
        }
        for (const volume of candidates) {
            if (!volume) continue;
            const inside = this.isPlayerInsideTriggerVolume(volume);
            if (inside && !volume.playerInside) {
                volume.playerInside = true;
                volume.nextBroadcastAt = 0;
                this.emitRuntimeEvent('triggerChanged', { id: createRuntimeId('trigger'), triggerId: volume.id, state: 'enter' });
            } else if (inside && volume.playerInside && volume.broadcastFrequency > 0 && now >= (volume.nextBroadcastAt || 0)) {
                volume.nextBroadcastAt = now + volume.broadcastFrequency;
                
            } else if (!inside && volume.playerInside) {
                volume.playerInside = false;
                this.emitRuntimeEvent('triggerChanged', { id: createRuntimeId('trigger'), triggerId: volume.id, state: 'exit' });
            }
        }
    }

    getGeometryScaleComponents(data) {
        if (Array.isArray(data?.scale) && data.scale.length >= 3) return data.scale;
        return [1, 1, 1];
    }

    getDecalMaterialOverrides(data, scale) {
        if (!data?.decalPreset) return {};
        const [width, height] = this.getGeometryScaleComponents({ scale });
        const base = {
            transparent: true,
            depthWrite: false,
            polygonOffset: true,
            alphaToCoverage: true
        };
        switch (data.decalPreset) {
            case 'blood':
                return {
                    ...base,
                    alphaTest: data.alphaTest ?? 0.45,
                    metalness: data.metalness ?? 0.02,
                    roughnessFactor: data.roughnessFactor ?? 0.98,
                    emissiveIntensity: data.emissiveIntensity ?? 0.05
                };
            case 'cable':
                return {
                    ...base,
                    alphaTest: data.alphaTest ?? 0.35,
                    metalness: data.metalness ?? 0.08,
                    roughnessFactor: data.roughnessFactor ?? 0.92,
                    autoUvScale: data.autoUvScale ?? [Math.max(1, Math.round(width / 0.2)), Math.max(1, Math.round(height / 0.45))]
                };
            case 'hazard':
                return {
                    ...base,
                    alphaTest: data.alphaTest ?? 0.35,
                    metalness: data.metalness ?? 0.18,
                    roughnessFactor: data.roughnessFactor ?? 0.84,
                    emissiveIntensity: data.emissiveIntensity ?? 0.18,
                    autoUvScale: data.autoUvScale ?? [Math.max(1, Math.round(width / 0.8)), Math.max(1, Math.round(height / 0.8))]
                };
            default:
                return {};
        }
    }

    normalizeMaterialData(data, geometryData = null) {
        const materialData = (data && typeof data === 'object') ? { ...data } : {};
        const scale = geometryData ? this.getGeometryScaleComponents(geometryData) : [1, 1, 1];
        Object.assign(materialData, this.getDecalMaterialOverrides(materialData, scale));
        if (!materialData.uvScale && Array.isArray(materialData.autoUvScale)) {
            materialData.uvScale = [...materialData.autoUvScale];
        }
        return materialData;
    }

    createMaterialFromData(data) {
        const materialData = this.normalizeMaterialData(data);
        const texture = materialData.baseColorMapRef != null ? this.loadedAssets.textures[materialData.baseColorMapRef] : undefined;
        const params = {
            metalness: materialData.metalness ?? 0.5,
            roughness: materialData.roughnessFactor ?? 0.8
        };
        if (texture) {
            params.map = texture.clone();
            params.map.wrapS = THREE.RepeatWrapping;
            params.map.wrapT = THREE.RepeatWrapping;
            if (materialData.uvScale) {
                params.map.repeat.fromArray(finiteVec2Components(materialData.uvScale, [1, 1]));
            }
        } else {
            if (materialData.color) {
                params.color = materialData.color;
            } else {
                 console.warn("Texture not found and no color specified:", materialData.baseColorMapRef);
                 params.color = 0xcc00cc;
            }
        }

        if (materialData.emissiveColor) {
            params.emissive = new THREE.Color(materialData.emissiveColor);
            params.emissiveIntensity = materialData.emissiveIntensity || 1.0;
            if (texture) params.emissiveMap = texture;
        }

        if (materialData.transparent) {
            params.transparent = true;
            params.opacity = materialData.opacity ?? 1.0;
            params.alphaTest = materialData.alphaTest ?? 0.08;
            params.depthWrite = materialData.depthWrite ?? false;
            params.side = materialData.side === 'double' ? THREE.DoubleSide : THREE.FrontSide;
        }

        if (materialData.polygonOffset) {
            params.polygonOffset = true;
            params.polygonOffsetFactor = materialData.polygonOffsetFactor ?? -1;
            params.polygonOffsetUnits = materialData.polygonOffsetUnits ?? -2;
        }

        const material = new THREE.MeshStandardMaterial(params);
        if (materialData.transparent) {
            material.alphaToCoverage = materialData.alphaToCoverage ?? true;
        }
        return material;
    }

    getStructuralPrimitiveType(data) {
        if (!data || typeof data !== 'object') return null;
        if (data.type === 'terrain_quad') return 'heightmap_patch';
        return typeof data.type === 'string' ? data.type.toLowerCase() : null;
    }

    parsePrimitiveProfilePoints(points, fallback = [[-0.5, -0.5], [0.5, -0.5], [0.5, 0.5], [-0.5, 0.5]]) {
        const source = Array.isArray(points) && points.length >= 3 ? points : fallback;
        return source.map((point) => new THREE.Vector2(
            clampFiniteNumber(point?.[0], 0, -2, 2),
            clampFiniteNumber(point?.[1], 0, -2, 2)
        ));
    }

    getPrimitiveProfilePreset(profileType = 'square', segments = 24) {
        const normalized = typeof profileType === 'string' ? profileType.toLowerCase() : 'square';
        if (normalized === 'circle') {
            return this.samplePrimitiveArcPoints(0, 0, 0.5, 0, Math.PI * 2, Math.max(segments, 8)).slice(0, -1);
        }
        if (normalized === 'triangle') {
            return [
                new THREE.Vector2(0, 0.58),
                new THREE.Vector2(-0.5, -0.32),
                new THREE.Vector2(0.5, -0.32)
            ];
        }
        return this.parsePrimitiveProfilePoints(null);
    }

    resamplePrimitiveProfileLoop(points, targetCount) {
        const source = Array.isArray(points) ? points.filter(Boolean) : [];
        const safeCount = Math.max(3, clampFiniteInteger(targetCount, Math.max(3, source.length), 3, 96));
        if (source.length === 0) return this.getPrimitiveProfilePreset('square', safeCount);
        if (source.length === safeCount) return source.map((point) => point.clone());
        if (source.length === 1) {
            return Array.from({ length: safeCount }, () => source[0].clone());
        }

        const lengths = [];
        let perimeter = 0;
        for (let index = 0; index < source.length; index += 1) {
            const nextIndex = (index + 1) % source.length;
            const segmentLength = source[index].distanceTo(source[nextIndex]);
            perimeter += segmentLength;
            lengths.push(segmentLength);
        }
        if (perimeter <= 1e-6) {
            return Array.from({ length: safeCount }, (_, index) => source[index % source.length].clone());
        }

        const result = [];
        for (let sampleIndex = 0; sampleIndex < safeCount; sampleIndex += 1) {
            const targetDistance = (sampleIndex / safeCount) * perimeter;
            let walked = 0;
            for (let edgeIndex = 0; edgeIndex < source.length; edgeIndex += 1) {
                const edgeLength = lengths[edgeIndex];
                const nextWalk = walked + edgeLength;
                if (targetDistance <= nextWalk || edgeIndex === source.length - 1) {
                    const nextIndex = (edgeIndex + 1) % source.length;
                    const edgeT = edgeLength <= 1e-6 ? 0 : (targetDistance - walked) / edgeLength;
                    result.push(source[edgeIndex].clone().lerp(source[nextIndex], THREE.MathUtils.clamp(edgeT, 0, 1)));
                    break;
                }
                walked = nextWalk;
            }
        }
        return result;
    }

    getLoftInterpolationValue(t, curve = 'linear') {
        const normalized = typeof curve === 'string' ? curve.toLowerCase() : 'linear';
        const clamped = THREE.MathUtils.clamp(t, 0, 1);
        switch (normalized) {
            case 'ease_in':
            case 'easein':
                return clamped * clamped;
            case 'ease_out':
            case 'easeout':
                return 1 - Math.pow(1 - clamped, 2);
            case 'smoothstep':
            case 'ease_in_out':
            case 'easeinout':
                return clamped * clamped * (3 - (2 * clamped));
            default:
                return clamped;
        }
    }

    buildLoftPrimitiveGeometryFromProfiles(profileA, profileB, {
        curve = 'linear',
        sections = 12,
        distance = 1
    } = {}) {
        const targetCount = Math.max(3, profileA.length, profileB.length);
        const loopA = this.resamplePrimitiveProfileLoop(profileA, targetCount);
        const loopB = this.resamplePrimitiveProfileLoop(profileB, targetCount);
        const safeSections = Math.max(1, clampFiniteInteger(sections, 12, 1, 64));
        const depth = clampFiniteNumber(distance, 1, 0.05, 4);
        const positions = [];
        const indices = [];

        for (let sectionIndex = 0; sectionIndex <= safeSections; sectionIndex += 1) {
            const t = safeSections === 0 ? 0 : sectionIndex / safeSections;
            const shapedT = this.getLoftInterpolationValue(t, curve);
            const z = (-depth * 0.5) + (depth * t);
            for (let pointIndex = 0; pointIndex < targetCount; pointIndex += 1) {
                const point = loopA[pointIndex].clone().lerp(loopB[pointIndex], shapedT);
                positions.push(point.x, point.y, z);
            }
        }

        for (let sectionIndex = 0; sectionIndex < safeSections; sectionIndex += 1) {
            const ringStart = sectionIndex * targetCount;
            const nextRingStart = (sectionIndex + 1) * targetCount;
            for (let pointIndex = 0; pointIndex < targetCount; pointIndex += 1) {
                const nextPoint = (pointIndex + 1) % targetCount;
                const a = ringStart + pointIndex;
                const b = ringStart + nextPoint;
                const c = nextRingStart + pointIndex;
                const d = nextRingStart + nextPoint;
                indices.push(a, b, d, a, d, c);
            }
        }

        const startCenterIndex = positions.length / 3;
        positions.push(
            loopA.reduce((sum, point) => sum + point.x, 0) / loopA.length,
            loopA.reduce((sum, point) => sum + point.y, 0) / loopA.length,
            -depth * 0.5
        );
        const endCenterIndex = positions.length / 3;
        positions.push(
            loopB.reduce((sum, point) => sum + point.x, 0) / loopB.length,
            loopB.reduce((sum, point) => sum + point.y, 0) / loopB.length,
            depth * 0.5
        );

        for (let pointIndex = 0; pointIndex < targetCount; pointIndex += 1) {
            const nextPoint = (pointIndex + 1) % targetCount;
            indices.push(startCenterIndex, nextPoint, pointIndex);
            const endOffset = safeSections * targetCount;
            indices.push(endCenterIndex, endOffset + pointIndex, endOffset + nextPoint);
        }

        const geometry = new THREE.BufferGeometry();
        geometry.setIndex(indices);
        geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
        geometry.computeVertexNormals();
        geometry.center();
        return geometry;
    }

    createConvexHullPrimitiveGeometry(data) {
        const points = Array.isArray(data?.points) && data.points.length >= 4
            ? data.points.map((point) => new THREE.Vector3(...finiteVec3Components(point, [0, 0, 0])))
            : [
                new THREE.Vector3(-0.5, -0.5, -0.5),
                new THREE.Vector3(0.5, -0.5, -0.5),
                new THREE.Vector3(0.5, -0.5, 0.5),
                new THREE.Vector3(-0.5, -0.5, 0.5),
                new THREE.Vector3(0, 0.5, 0)
            ];
        const geometry = new ConvexGeometry(points);
        geometry.center();
        geometry.computeVertexNormals();
        return geometry.index ? geometry.toNonIndexed() : geometry;
    }

    createRoundedBoxPrimitiveGeometry(data) {
        const radius = clampFiniteNumber(data?.bevelRadius ?? data?.radius, 0.08, 0.01, 0.45);
        const segments = clampFiniteInteger(data?.bevelSegments ?? data?.segments, 4, 1, 12);
        return new RoundedBoxGeometry(1, 1, 1, segments, radius);
    }

    signedPow(value, exponent) {
        return Math.sign(value) * Math.pow(Math.abs(value), exponent);
    }

    createSuperellipsoidPrimitiveGeometry(data) {
        const widthSegments = clampFiniteInteger(data?.segments ?? data?.widthSegments, 24, 6, 96);
        const heightSegments = clampFiniteInteger(data?.heightSegments ?? Math.max(8, Math.round(widthSegments * 0.6)), 16, 4, 64);
        const geometry = new THREE.SphereGeometry(0.5, widthSegments, heightSegments);
        const exponent1 = clampFiniteNumber(data?.exponent1, 0.55, 0.1, 8);
        const exponent2 = clampFiniteNumber(data?.exponent2, 0.55, 0.1, 8);
        const position = geometry.getAttribute('position');
        for (let index = 0; index < position.count; index += 1) {
            const x = position.getX(index) / 0.5;
            const y = position.getY(index) / 0.5;
            const z = position.getZ(index) / 0.5;
            position.setXYZ(
                index,
                this.signedPow(x, exponent1) * 0.5,
                this.signedPow(y, exponent2) * 0.5,
                this.signedPow(z, exponent1) * 0.5
            );
        }
        geometry.computeVertexNormals();
        return geometry;
    }

    mergeStructuralPrimitiveGeometryParts(parts, fallbackFactory = () => new THREE.BoxGeometry(1, 1, 1)) {
        const validParts = parts.filter(Boolean);
        if (validParts.length === 0) return fallbackFactory();
        if (validParts.length === 1) return validParts[0];
        const merged = mergeGeometries(validParts, false);
        if (!merged) {
            console.warn('Failed to merge structural primitive geometry parts; falling back to a box.');
            validParts.forEach((part) => part.dispose());
            return fallbackFactory();
        }
        validParts.forEach((part) => part.dispose());
        const normalized = merged.index ? merged.toNonIndexed() : merged;
        if (normalized !== merged) merged.dispose();
        normalized.computeVertexNormals();
        return normalized;
    }

    createRampPrimitiveGeometry() {
        const shape = new THREE.Shape();
        shape.moveTo(-0.5, -0.5);
        shape.lineTo(-0.5, 0.5);
        shape.lineTo(0.5, -0.5);
        shape.lineTo(-0.5, -0.5);
        const geometry = new THREE.ExtrudeGeometry(shape, {
            depth: 1,
            bevelEnabled: false,
            steps: 1,
            curveSegments: 1
        });
        geometry.rotateY(Math.PI / 2);
        geometry.center();
        geometry.computeVertexNormals();
        return geometry;
    }

    createWedgePrimitiveGeometry() {
        const shape = new THREE.Shape();
        shape.moveTo(-0.5, -0.5);
        shape.lineTo(0.5, -0.5);
        shape.lineTo(-0.5, 0.5);
        shape.lineTo(-0.5, -0.5);
        const geometry = new THREE.ExtrudeGeometry(shape, {
            depth: 1,
            bevelEnabled: false,
            steps: 1,
            curveSegments: 1
        });
        geometry.rotateX(-Math.PI / 2);
        geometry.center();
        geometry.computeVertexNormals();
        return geometry;
    }

    createCylinderPrimitiveGeometry(data) {
        const radialSegments = clampFiniteInteger(data?.radialSegments, 16, 6, 48);
        return new THREE.CylinderGeometry(0.5, 0.5, 1, radialSegments, 1, false);
    }

    createGeodesicSpherePrimitiveGeometry(data) {
        const detail = clampFiniteInteger(data?.detail ?? data?.segments, 2, 0, 6);
        return new THREE.IcosahedronGeometry(0.5, detail);
    }

    samplePrimitiveArcPoints(centerX, centerY, radius, startAngle, endAngle, segments) {
        const safeSegments = Math.max(1, clampFiniteInteger(segments, 12, 1, 128));
        const points = [];
        for (let index = 0; index <= safeSegments; index += 1) {
            const t = safeSegments === 0 ? 0 : index / safeSegments;
            const angle = startAngle + ((endAngle - startAngle) * t);
            points.push(new THREE.Vector2(
                centerX + (Math.cos(angle) * radius),
                centerY + (Math.sin(angle) * radius)
            ));
        }
        return points;
    }

    createShapeFromPointLoop(points) {
        const safePoints = Array.isArray(points) ? points.filter(Boolean) : [];
        const shape = new THREE.Shape();
        if (safePoints.length === 0) return shape;
        shape.moveTo(safePoints[0].x, safePoints[0].y);
        for (let index = 1; index < safePoints.length; index += 1) {
            shape.lineTo(safePoints[index].x, safePoints[index].y);
        }
        shape.lineTo(safePoints[0].x, safePoints[0].y);
        return shape;
    }

    createPathFromPointLoop(points) {
        const safePoints = Array.isArray(points) ? points.filter(Boolean) : [];
        const path = new THREE.Path();
        if (safePoints.length === 0) return path;
        path.moveTo(safePoints[0].x, safePoints[0].y);
        for (let index = 1; index < safePoints.length; index += 1) {
            path.lineTo(safePoints[index].x, safePoints[index].y);
        }
        path.lineTo(safePoints[0].x, safePoints[0].y);
        return path;
    }

    createExtrudedStructuralShapeGeometry(shape, {
        depth = 1,
        curveSegments = 12,
        rotateX = 0,
        rotateY = 0,
        rotateZ = 0
    } = {}) {
        const geometry = new THREE.ExtrudeGeometry(shape, {
            depth,
            bevelEnabled: false,
            steps: 1,
            curveSegments: clampFiniteInteger(curveSegments, 12, 1, 128)
        });
        if (rotateX) geometry.rotateX(rotateX);
        if (rotateY) geometry.rotateY(rotateY);
        if (rotateZ) geometry.rotateZ(rotateZ);
        geometry.center();
        geometry.computeVertexNormals();
        return geometry;
    }

    createRoundArchPrimitiveGeometry(data) {
        const thickness = clampFiniteNumber(data?.thickness, 0.16, 0.04, 0.45);
        const segments = clampFiniteInteger(data?.segments, 18, 4, 64);
        const spanDegrees = clampFiniteNumber(data?.spanDegrees, 180, 40, 360);
        const outerRadius = clampFiniteNumber(data?.outerRadius, 0.5, 0.1, 0.5);
        const innerRadius = clampFiniteNumber(
            data?.innerRadius,
            Math.max(0.04, outerRadius - thickness),
            0.02,
            Math.max(0.03, outerRadius - 0.02)
        );
        if (spanDegrees >= 359.5) {
            const outerLoop = this.samplePrimitiveArcPoints(0, 0, outerRadius, 0, Math.PI * 2, segments * 2);
            const innerLoop = this.samplePrimitiveArcPoints(0, 0, innerRadius, 0, Math.PI * 2, segments * 2).reverse();
            const shape = this.createShapeFromPointLoop(outerLoop);
            shape.holes.push(this.createPathFromPointLoop(innerLoop));
            return this.createExtrudedStructuralShapeGeometry(shape, {
                curveSegments: segments,
                rotateX: -Math.PI / 2
            });
        }

        const spanRadians = THREE.MathUtils.degToRad(spanDegrees);
        const startAngle = (Math.PI * 0.5) + (spanRadians * 0.5);
        const endAngle = (Math.PI * 0.5) - (spanRadians * 0.5);
        const outerArc = this.samplePrimitiveArcPoints(0, 0, outerRadius, startAngle, endAngle, segments);
        const innerArc = this.samplePrimitiveArcPoints(0, 0, innerRadius, startAngle, endAngle, segments);
        const outerStart = outerArc[0];
        const outerEnd = outerArc[outerArc.length - 1];
        const innerStart = innerArc[0];
        const innerEnd = innerArc[innerArc.length - 1];
        const outerLoop = [
            new THREE.Vector2(outerStart.x, -0.5),
            outerStart,
            ...outerArc.slice(1, -1),
            outerEnd,
            new THREE.Vector2(outerEnd.x, -0.5)
        ];
        const innerLoop = [
            new THREE.Vector2(innerEnd.x, -0.5),
            innerEnd,
            ...innerArc.slice(1, -1).reverse(),
            innerStart,
            new THREE.Vector2(innerStart.x, -0.5)
        ];
        const shape = this.createShapeFromPointLoop(outerLoop);
        shape.holes.push(this.createPathFromPointLoop(innerLoop));
        return this.createExtrudedStructuralShapeGeometry(shape, {
            curveSegments: segments,
            rotateX: -Math.PI / 2
        });
    }

    createGothicArchPrimitiveGeometry(data) {
        const thickness = clampFiniteNumber(data?.thickness, 0.16, 0.04, 0.38);
        const outerShoulderY = clampFiniteNumber(data?.shoulderY, -0.06, -0.22, 0.18);
        const outerPeakY = 0.5;
        const innerInset = thickness;
        const innerShoulderY = Math.min(outerPeakY - 0.08, outerShoulderY + (thickness * 0.65));
        const innerPeakY = outerPeakY - (thickness * 0.95);

        const shape = new THREE.Shape();
        shape.moveTo(-0.5, -0.5);
        shape.lineTo(-0.5, outerShoulderY);
        shape.quadraticCurveTo(-0.18, 0.34, 0, outerPeakY);
        shape.quadraticCurveTo(0.18, 0.34, 0.5, outerShoulderY);
        shape.lineTo(0.5, -0.5);
        shape.lineTo(-0.5, -0.5);

        const hole = new THREE.Path();
        hole.moveTo(0.5 - innerInset, -0.5);
        hole.lineTo(0.5 - innerInset, innerShoulderY);
        hole.quadraticCurveTo(0.14, 0.26, 0, innerPeakY);
        hole.quadraticCurveTo(-0.14, 0.26, -0.5 + innerInset, innerShoulderY);
        hole.lineTo(-0.5 + innerInset, -0.5);
        hole.lineTo(0.5 - innerInset, -0.5);
        shape.holes.push(hole);

        return this.createExtrudedStructuralShapeGeometry(shape, {
            curveSegments: clampFiniteInteger(data?.segments, 14, 4, 64),
            rotateX: -Math.PI / 2
        });
    }

    createArchPrimitiveGeometry(data) {
        const style = typeof data?.archStyle === 'string' ? data.archStyle.toLowerCase() : 'round';
        return style === 'gothic'
            ? this.createGothicArchPrimitiveGeometry(data)
            : this.createRoundArchPrimitiveGeometry(data);
    }

    createConePrimitiveGeometry(data) {
        const defaultSides = data?.type === 'pyramid' ? 4 : 16;
        const sides = clampFiniteInteger(data?.sides, defaultSides, 3, 64);
        return new THREE.CylinderGeometry(0, 0.5, 1, sides, 1, false);
    }

    createTubePrimitiveGeometry(data) {
        const segments = clampFiniteInteger(data?.segments ?? data?.radialSegments, 18, 6, 64);
        const outerRadius = clampFiniteNumber(data?.outerRadius, 0.5, 0.08, 0.5);
        const innerRadius = clampFiniteNumber(
            data?.innerRadius,
            Math.max(0.04, outerRadius - clampFiniteNumber(data?.thickness, 0.14, 0.02, 0.35)),
            0.02,
            Math.max(0.03, outerRadius - 0.02)
        );
        const outerLoop = this.samplePrimitiveArcPoints(0, 0, outerRadius, 0, Math.PI * 2, segments * 2);
        const innerLoop = this.samplePrimitiveArcPoints(0, 0, innerRadius, 0, Math.PI * 2, segments * 2).reverse();
        const shape = this.createShapeFromPointLoop(outerLoop);
        shape.holes.push(this.createPathFromPointLoop(innerLoop));
        return this.createExtrudedStructuralShapeGeometry(shape, {
            curveSegments: segments,
            rotateX: -Math.PI / 2
        });
    }

    createCapsulePrimitiveGeometry(data) {
        const radialSegments = clampFiniteInteger(data?.radialSegments, 12, 4, 48);
        const capSegments = clampFiniteInteger(data?.segments, 6, 2, 24);
        const radius = clampFiniteNumber(data?.radius ?? data?.outerRadius, 0.25, 0.05, 0.45);
        const length = clampFiniteNumber(data?.length ?? 0.5, 0.5, 0.05, 1.5);
        return new THREE.CapsuleGeometry(radius, length, capSegments, radialSegments);
    }

    createFrustumPrimitiveGeometry(data) {
        const radialSegments = clampFiniteInteger(data?.radialSegments ?? data?.segments, 16, 3, 64);
        const topRadius = clampFiniteNumber(data?.topRadius ?? data?.radiusTop, 0.18, 0, 0.5);
        const bottomRadius = clampFiniteNumber(data?.bottomRadius ?? data?.radiusBottom, 0.5, 0, 0.5);
        return new THREE.CylinderGeometry(topRadius, bottomRadius, 1, radialSegments, 1, false);
    }

    createTorusPrimitiveGeometry(data) {
        const tubularSegments = clampFiniteInteger(data?.segments, 24, 3, 96);
        const radialSegments = clampFiniteInteger(data?.radialSegments, 12, 3, 48);
        const outerRadius = clampFiniteNumber(data?.outerRadius, 0.5, 0.12, 0.8);
        const innerRadius = clampFiniteNumber(data?.innerRadius, 0.22, 0.02, Math.max(0.03, outerRadius - 0.04));
        const majorRadius = Math.max(0.05, (outerRadius + innerRadius) * 0.5);
        const tubeRadius = Math.max(0.02, (outerRadius - innerRadius) * 0.5);
        const arc = THREE.MathUtils.degToRad(clampFiniteNumber(data?.arcDegrees ?? data?.spanDegrees, 360, 1, 360));
        return new THREE.TorusGeometry(majorRadius, tubeRadius, radialSegments, tubularSegments, arc);
    }

    createShapeFromProfileData(data) {
        const profileType = typeof data?.profileType === 'string' ? data.profileType.toLowerCase() : 'circle';
        if (profileType === 'square') {
            const shape = new THREE.Shape();
            shape.moveTo(-0.5, -0.5);
            shape.lineTo(0.5, -0.5);
            shape.lineTo(0.5, 0.5);
            shape.lineTo(-0.5, 0.5);
            shape.lineTo(-0.5, -0.5);
            return shape;
        }
        if (profileType === 'polygon' && Array.isArray(data?.profilePoints) && data.profilePoints.length >= 3) {
            const points = data.profilePoints.map((point) => new THREE.Vector2(
                clampFiniteNumber(point?.[0], 0, -2, 2),
                clampFiniteNumber(point?.[1], 0, -2, 2)
            ));
            return this.createShapeFromPointLoop(points);
        }
        const radius = clampFiniteNumber(data?.radius ?? data?.outerRadius, 0.5, 0.02, 1.5);
        const shape = new THREE.Shape();
        shape.absarc(0, 0, radius, 0, Math.PI * 2, false);
        shape.autoClose = true;
        return shape;
    }

    createBeamGeometryBetweenPoints(start, end, thickness = 0.08) {
        const safeStart = start?.clone?.() || new THREE.Vector3();
        const safeEnd = end?.clone?.() || new THREE.Vector3(0, 0, 1);
        const delta = safeEnd.clone().sub(safeStart);
        const length = delta.length();
        if (!(length > 1e-5)) return null;
        const geometry = new THREE.BoxGeometry(thickness, thickness, length);
        const mid = safeStart.clone().addScaledVector(delta, 0.5);
        const quaternion = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 0, 1), delta.normalize());
        geometry.applyMatrix4(new THREE.Matrix4().compose(mid, quaternion, new THREE.Vector3(1, 1, 1)));
        return geometry;
    }

    getSeededUnitValue(seed) {
        const value = Math.sin((seed * 127.1) + 311.7) * 43758.5453123;
        return value - Math.floor(value);
    }

    buildCatenaryPathPoints(data) {
        const pointA = new THREE.Vector3(...finiteVec3Components(data?.pointA ?? data?.start, [-0.5, 0.22, -0.5]));
        const pointB = new THREE.Vector3(...finiteVec3Components(data?.pointB ?? data?.end, [0.5, 0.22, 0.5]));
        const slack = clampFiniteNumber(data?.slack ?? data?.sagFactor, 0.6, 0, 8);
        const segments = clampFiniteInteger(data?.segments ?? data?.tessellationFactor, 18, 4, 128);
        const points = [];
        const curvature = THREE.MathUtils.clamp(1 + (slack * 0.8), 1, 8);
        const centerShape = 1 - (1 / Math.cosh(curvature * 0.5));
        const sagScale = centerShape > 1e-5 ? slack / centerShape : slack;
        for (let index = 0; index <= segments; index += 1) {
            const t = segments === 0 ? 0 : index / segments;
            const point = pointA.clone().lerp(pointB, t);
            const shape = 1 - (Math.cosh(curvature * (t - 0.5)) / Math.cosh(curvature * 0.5));
            point.y -= shape * sagScale;
            points.push(point);
        }
        return points;
    }

    createManifoldSweepPrimitiveGeometry(data) {
        const pathPoints = Array.isArray(data?.spline) && data.spline.length >= 2
            ? data.spline.map((point) => new THREE.Vector3(...finiteVec3Components(point, [0, 0, 0])))
            : [new THREE.Vector3(-0.5, 0, -0.5), new THREE.Vector3(0, 0.2, 0), new THREE.Vector3(0.5, 0, 0.5)];
        const profileShape = data?.profileShape || data?.crossSectionProfile || data?.profileType || 'circle';
        const shape = this.createShapeFromProfileData({
            ...data,
            profileType: profileShape
        });
        const curve = new THREE.CatmullRomCurve3(pathPoints, false, data?.curveType || 'centripetal', clampFiniteNumber(data?.tension, 0.5, 0, 1));
        const geometry = new THREE.ExtrudeGeometry(shape, {
            steps: clampFiniteInteger(data?.tessellationFactor ?? data?.segments, 28, 2, 256),
            bevelEnabled: false,
            extrudePath: curve,
            curveSegments: clampFiniteInteger(data?.radialSegments, 12, 3, 64)
        });
        geometry.computeVertexNormals();
        return geometry;
    }

    createCatenaryPrimitiveGeometry(data) {
        return this.createManifoldSweepPrimitiveGeometry({
            ...data,
            spline: this.buildCatenaryPathPoints(data),
            profileShape: data?.crossSectionProfile || data?.profileShape || data?.profileType || 'circle'
        });
    }

    createExtrudeAlongNormalPrimitiveGeometry(data) {
        const shape = Array.isArray(data?.profilePoints) && data.profilePoints.length >= 3
            ? this.createShapeFromProfileData({
                ...data,
                profileType: 'polygon'
            })
            : this.createShapeFromProfileData({
                ...data,
                profileType: data?.profileType || data?.crossSectionProfile || 'square'
            });
        return this.createExtrudedStructuralShapeGeometry(shape, {
            depth: clampFiniteNumber(data?.distance ?? data?.depth ?? data?.width ?? data?.thickness, 1, 0.05, 8),
            curveSegments: clampFiniteInteger(data?.segments ?? data?.tessellationFactor, 12, 1, 64)
        });
    }

    createLatticeVolumePrimitiveGeometry(data) {
        const cellSize = clampFiniteNumber(data?.cellSize, 0.25, 0.08, 1);
        const cellsPerAxis = Math.max(1, Math.round(1 / cellSize));
        const patternType = typeof data?.patternType === 'string' ? data.patternType.toLowerCase() : 'x_brace';
        const thickness = clampFiniteNumber(data?.strutThickness ?? data?.thickness, 0.06, 0.01, 0.25);
        const coords = Array.from({ length: cellsPerAxis + 1 }, (_, index) => -0.5 + ((index / cellsPerAxis) * 1));
        const partGeometries = [];
        const segmentKeys = new Set();
        const addSegment = (start, end) => {
            const keyParts = [start, end].map((point) => `${point.x.toFixed(4)},${point.y.toFixed(4)},${point.z.toFixed(4)}`).sort();
            const key = keyParts.join('|');
            if (segmentKeys.has(key)) return;
            segmentKeys.add(key);
            const beam = this.createBeamGeometryBetweenPoints(start, end, thickness);
            if (beam) partGeometries.push(beam);
        };
        for (let ix = 0; ix < cellsPerAxis; ix += 1) {
            for (let iy = 0; iy < cellsPerAxis; iy += 1) {
                for (let iz = 0; iz < cellsPerAxis; iz += 1) {
                    const x0 = coords[ix];
                    const x1 = coords[ix + 1];
                    const y0 = coords[iy];
                    const y1 = coords[iy + 1];
                    const z0 = coords[iz];
                    const z1 = coords[iz + 1];
                    const corners = {
                        a: new THREE.Vector3(x0, y0, z0),
                        b: new THREE.Vector3(x1, y0, z0),
                        c: new THREE.Vector3(x1, y1, z0),
                        d: new THREE.Vector3(x0, y1, z0),
                        e: new THREE.Vector3(x0, y0, z1),
                        f: new THREE.Vector3(x1, y0, z1),
                        g: new THREE.Vector3(x1, y1, z1),
                        h: new THREE.Vector3(x0, y1, z1)
                    };
                    [[corners.a, corners.b], [corners.b, corners.c], [corners.c, corners.d], [corners.d, corners.a],
                     [corners.e, corners.f], [corners.f, corners.g], [corners.g, corners.h], [corners.h, corners.e],
                     [corners.a, corners.e], [corners.b, corners.f], [corners.c, corners.g], [corners.d, corners.h]]
                        .forEach(([start, end]) => addSegment(start, end));
                    if (patternType === 'octet_truss') {
                        const center = new THREE.Vector3((x0 + x1) * 0.5, (y0 + y1) * 0.5, (z0 + z1) * 0.5);
                        Object.values(corners).forEach((corner) => addSegment(center, corner));
                    } else if (patternType === 'k_brace') {
                        const midLeft = new THREE.Vector3(x0, (y0 + y1) * 0.5, (z0 + z1) * 0.5);
                        const midRight = new THREE.Vector3(x1, (y0 + y1) * 0.5, (z0 + z1) * 0.5);
                        const midFront = new THREE.Vector3((x0 + x1) * 0.5, (y0 + y1) * 0.5, z1);
                        addSegment(corners.a, midLeft);
                        addSegment(corners.d, midLeft);
                        addSegment(corners.b, midRight);
                        addSegment(corners.c, midRight);
                        addSegment(corners.e, midFront);
                        addSegment(corners.f, midFront);
                    } else {
                        addSegment(corners.a, corners.c);
                        addSegment(corners.b, corners.d);
                        addSegment(corners.e, corners.g);
                        addSegment(corners.f, corners.h);
                        addSegment(corners.a, corners.g);
                        addSegment(corners.b, corners.h);
                    }
                }
            }
        }
        return this.mergeStructuralPrimitiveGeometryParts(partGeometries);
    }

    createVoronoiFracturePrimitiveGeometry(data) {
        const seedCount = clampFiniteInteger(data?.seedCount, 14, 1, 128);
        const gapThickness = clampFiniteNumber(data?.gapThickness, 0.04, 0, 0.2);
        const jitter = clampFiniteNumber(data?.jitter, 0.18, 0, 0.45);
        const cellsPerAxis = Math.max(1, Math.ceil(Math.cbrt(seedCount)));
        const cellSize = 1 / cellsPerAxis;
        const shardSize = Math.max(0.04, cellSize - gapThickness);
        const partGeometries = [];
        let seedIndex = 0;
        for (let ix = 0; ix < cellsPerAxis && seedIndex < seedCount; ix += 1) {
            for (let iy = 0; iy < cellsPerAxis && seedIndex < seedCount; iy += 1) {
                for (let iz = 0; iz < cellsPerAxis && seedIndex < seedCount; iz += 1) {
                    const center = new THREE.Vector3(
                        -0.5 + ((ix + 0.5) * cellSize),
                        -0.5 + ((iy + 0.5) * cellSize),
                        -0.5 + ((iz + 0.5) * cellSize)
                    );
                    const offset = new THREE.Vector3(
                        (this.getSeededUnitValue(seedIndex + 1) - 0.5) * cellSize * jitter,
                        (this.getSeededUnitValue(seedIndex + 11) - 0.5) * cellSize * jitter,
                        (this.getSeededUnitValue(seedIndex + 21) - 0.5) * cellSize * jitter
                    );
                    const shardScale = new THREE.Vector3(
                        shardSize * (0.74 + (this.getSeededUnitValue(seedIndex + 31) * 0.22)),
                        shardSize * (0.74 + (this.getSeededUnitValue(seedIndex + 41) * 0.22)),
                        shardSize * (0.74 + (this.getSeededUnitValue(seedIndex + 51) * 0.22))
                    );
                    const geometry = new THREE.BoxGeometry(shardScale.x, shardScale.y, shardScale.z);
                    const rotation = new THREE.Euler(
                        (this.getSeededUnitValue(seedIndex + 61) - 0.5) * 0.35,
                        (this.getSeededUnitValue(seedIndex + 71) - 0.5) * 0.35,
                        (this.getSeededUnitValue(seedIndex + 81) - 0.5) * 0.35
                    );
                    geometry.applyMatrix4(new THREE.Matrix4().compose(
                        center.add(offset),
                        new THREE.Quaternion().setFromEuler(rotation),
                        new THREE.Vector3(1, 1, 1)
                    ));
                    partGeometries.push(geometry);
                    seedIndex += 1;
                }
            }
        }
        return this.mergeStructuralPrimitiveGeometryParts(partGeometries);
    }

    createHexahedronPrimitiveGeometry(data) {
        const defaultVertices = [
            [-0.5, -0.5, -0.5],
            [0.5, -0.5, -0.5],
            [0.5, -0.5, 0.5],
            [-0.5, -0.5, 0.5],
            [-0.5, 0.5, -0.5],
            [0.5, 0.5, -0.5],
            [0.5, 0.5, 0.5],
            [-0.5, 0.5, 0.5]
        ];
        const sourceVertices = Array.isArray(data?.vertices) && data.vertices.length >= 8
            ? data.vertices
            : (Array.isArray(data?.points) && data.points.length >= 8 ? data.points : defaultVertices);
        const vertices = sourceVertices.slice(0, 8).map((point) => new THREE.Vector3(...finiteVec3Components(point, [0, 0, 0])));
        const faces = [
            [0, 1, 2], [0, 2, 3],
            [4, 7, 6], [4, 6, 5],
            [0, 4, 5], [0, 5, 1],
            [1, 5, 6], [1, 6, 2],
            [2, 6, 7], [2, 7, 3],
            [3, 7, 4], [3, 4, 0]
        ];
        const positions = [];
        faces.forEach(([a, b, c]) => {
            positions.push(
                vertices[a].x, vertices[a].y, vertices[a].z,
                vertices[b].x, vertices[b].y, vertices[b].z,
                vertices[c].x, vertices[c].y, vertices[c].z
            );
        });
        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
        geometry.computeVertexNormals();
        return geometry;
    }

    createDisplacementPrimitiveGeometry(data) {
        const heightfield = Array.isArray(data?.heightfield)
            ? data.heightfield
            : (Array.isArray(data?.heightData) ? data.heightData : null);
        const rows = Array.isArray(heightfield) ? heightfield.length : 0;
        const cols = rows > 0 && Array.isArray(heightfield[0]) ? heightfield[0].length : 0;
        const widthSegments = clampFiniteInteger(data?.widthSegments, Math.max(1, cols - 1), 1, 128);
        const depthSegments = clampFiniteInteger(data?.depthSegments, Math.max(1, rows - 1), 1, 128);
        const geometry = new THREE.PlaneGeometry(1, 1, widthSegments, depthSegments);
        const position = geometry.getAttribute('position');
        for (let row = 0; row <= depthSegments; row += 1) {
            for (let col = 0; col <= widthSegments; col += 1) {
                const index = (row * (widthSegments + 1)) + col;
                const sampledRow = rows > 0 ? Math.min(rows - 1, Math.round((row / depthSegments) * Math.max(0, rows - 1))) : 0;
                const sampledCol = cols > 0 ? Math.min(cols - 1, Math.round((col / widthSegments) * Math.max(0, cols - 1))) : 0;
                const height = rows > 0 && cols > 0
                    ? clampFiniteNumber(heightfield[sampledRow]?.[sampledCol], 0, -1, 1)
                    : (Math.sin((col / Math.max(1, widthSegments)) * Math.PI * 2) * Math.cos((row / Math.max(1, depthSegments)) * Math.PI * 2) * 0.12);
                position.setZ(index, height);
            }
        }
        geometry.rotateX(-Math.PI / 2);
        geometry.computeVertexNormals();
        return geometry;
    }

    createSplineRibbonPrimitiveGeometry(data) {
        const pathPoints = Array.isArray(data?.spline) && data.spline.length >= 2
            ? data.spline.map((point) => new THREE.Vector3(...finiteVec3Components(point, [0, 0, 0])))
            : [new THREE.Vector3(-0.5, 0, -0.5), new THREE.Vector3(0, 0.04, 0), new THREE.Vector3(0.5, 0, 0.5)];
        const curve = new THREE.CatmullRomCurve3(pathPoints, false, data?.curveType || 'centripetal', clampFiniteNumber(data?.tension, 0.5, 0, 1));
        const steps = clampFiniteInteger(data?.tessellationFactor ?? data?.segments, 24, 2, 256);
        const width = clampFiniteNumber(data?.width ?? data?.radius ?? data?.outerRadius, 0.4, 0.05, 4);
        const positions = [];
        const normals = [];
        const uvs = [];
        const indices = [];
        const up = new THREE.Vector3(0, 1, 0);
        const lateral = new THREE.Vector3();
        const tangent = new THREE.Vector3();

        for (let index = 0; index <= steps; index += 1) {
            const t = steps === 0 ? 0 : index / steps;
            const point = curve.getPointAt(t);
            tangent.copy(curve.getTangentAt(t));
            if (tangent.lengthSq() < 1e-10) tangent.set(0, 0, 1);
            tangent.normalize();
            lateral.crossVectors(up, tangent);
            if (lateral.lengthSq() < 1e-10) lateral.set(1, 0, 0);
            else lateral.normalize();
            const left = point.clone().addScaledVector(lateral, width * 0.5);
            const right = point.clone().addScaledVector(lateral, -width * 0.5);
            positions.push(left.x, left.y, left.z, right.x, right.y, right.z);
            normals.push(0, 1, 0, 0, 1, 0);
            uvs.push(0, t, 1, t);
            if (index < steps) {
                const base = index * 2;
                indices.push(base, base + 2, base + 1, base + 1, base + 2, base + 3);
            }
        }

        const geometry = new THREE.BufferGeometry();
        geometry.setIndex(indices);
        geometry.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
        geometry.setAttribute('normal', new THREE.Float32BufferAttribute(normals, 3));
        geometry.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
        geometry.computeVertexNormals();
        return geometry;
    }

    createTrimSheetSweepPrimitiveGeometry(data) {
        const width = Math.max(0.02, Number(data?.trimWidth ?? data?.width ?? 0.12) || 0.12);
        const height = Math.max(0.02, Number(data?.trimHeight ?? data?.height ?? 0.08) || 0.08);
        const profilePoints = Array.isArray(data?.profilePoints) && data.profilePoints.length >= 3
            ? data.profilePoints
            : [
                [-width * 0.5, 0],
                [width * 0.5, 0],
                [width * 0.5, height],
                [-width * 0.5, height]
            ];
        return this.createManifoldSweepPrimitiveGeometry({
            ...data,
            profilePoints,
            profileShape: 'polygon',
            capStart: data?.capStart !== false,
            capEnd: data?.capEnd !== false
        });
    }

    createLSystemBranchPrimitiveGeometry(data) {
        const iterations = clampFiniteInteger(data?.iterations, 3, 1, 6);
        const branchLength = clampFiniteNumber(data?.branchLength ?? data?.segmentLength, 0.55, 0.08, 4);
        const branchScale = clampFiniteNumber(data?.branchScale ?? data?.recursionScale, 0.72, 0.2, 0.92);
        const branchAngle = THREE.MathUtils.degToRad(clampFiniteNumber(data?.branchAngle ?? data?.angle, 28, 1, 85));
        const radius = clampFiniteNumber(data?.radius ?? data?.thickness, 0.09, 0.01, 1);
        const radialSegments = Math.max(5, clampFiniteInteger(data?.radialSegments, 6, 3, 18));
        const geometries = [];
        const origin = new THREE.Vector3(0, -0.5, 0);
        const up = new THREE.Vector3(0, 1, 0);
        const makeBranch = (start, direction, depth, currentLength, currentRadius) => {
            const end = start.clone().add(direction.clone().multiplyScalar(currentLength));
            const cylinder = new THREE.CylinderGeometry(currentRadius * branchScale, currentRadius, currentLength, radialSegments);
            const midpoint = start.clone().lerp(end, 0.5);
            const quaternion = new THREE.Quaternion().setFromUnitVectors(up, direction.clone().normalize());
            const matrix = new THREE.Matrix4().compose(midpoint, quaternion, new THREE.Vector3(1, 1, 1));
            cylinder.applyMatrix4(matrix);
            geometries.push(cylinder);
            if (depth >= iterations) return;
            const nextLength = currentLength * branchScale;
            const nextRadius = Math.max(0.01, currentRadius * branchScale);
            const left = direction.clone().applyAxisAngle(new THREE.Vector3(0, 0, 1), branchAngle).normalize();
            const right = direction.clone().applyAxisAngle(new THREE.Vector3(0, 0, 1), -branchAngle).normalize();
            makeBranch(end, left, depth + 1, nextLength, nextRadius);
            makeBranch(end, right, depth + 1, nextLength, nextRadius);
        };
        makeBranch(origin, new THREE.Vector3(0, 1, 0), 1, branchLength, radius);
        if (geometries.length === 0) return new THREE.BoxGeometry(1, 1, 1);
        const merged = mergeGeometries(geometries, true);
        geometries.forEach((geometry) => geometry.dispose());
        merged.computeVertexNormals();
        return merged;
    }

    createMetaballPrimitiveGeometry(data) {
        const radius = clampFiniteNumber(data?.radius ?? data?.outerRadius, 0.38, 0.08, 1.5);
        const segments = clampFiniteInteger(data?.segments ?? data?.radialSegments, 22, 8, 64);
        const geometry = new THREE.SphereGeometry(radius, segments, Math.max(6, Math.round(segments * 0.7)));
        const position = geometry.getAttribute('position');
        const influences = Array.isArray(data?.points) && data.points.length > 0
            ? data.points.map((point, index) => ({
                center: new THREE.Vector3(...finiteVec3Components(point, [0, 0, 0])),
                weight: 0.18 + (this.getSeededUnitValue(index + 1) * 0.18)
            }))
            : [
                { center: new THREE.Vector3(-0.24, 0.05, -0.12), weight: 0.26 },
                { center: new THREE.Vector3(0.18, 0.12, 0.08), weight: 0.22 },
                { center: new THREE.Vector3(0.02, -0.14, 0.2), weight: 0.18 }
            ];
        const scratch = new THREE.Vector3();
        const normal = new THREE.Vector3();
        const influenceRadius = Math.max(radius * 1.6, clampFiniteNumber(data?.blendRadius, radius * 1.2, 0.05, 4));
        const influenceRadiusSq = influenceRadius * influenceRadius;
        for (let index = 0; index < position.count; index += 1) {
            scratch.fromBufferAttribute(position, index);
            normal.copy(scratch).normalize();
            let displacement = 0;
            influences.forEach((influence) => {
                const distSq = scratch.distanceToSquared(influence.center);
                displacement += Math.exp(-(distSq / Math.max(1e-4, influenceRadiusSq))) * influence.weight;
            });
            scratch.addScaledVector(normal, displacement);
            position.setXYZ(index, scratch.x, scratch.y, scratch.z);
        }
        geometry.computeVertexNormals();
        return geometry;
    }

    createMiteredCornerPrimitiveGeometry(data) {
        const thickness = clampFiniteNumber(data?.thickness, 0.24, 0.08, 0.46);
        const horizontal = new THREE.BoxGeometry(1, 1, thickness);
        horizontal.translate(0, 0, -0.5 + (thickness * 0.5));
        const vertical = new THREE.BoxGeometry(thickness, 1, 1);
        vertical.translate(-0.5 + (thickness * 0.5), 0, 0);
        return this.mergeStructuralPrimitiveGeometryParts([horizontal, vertical]);
    }

    createRoundedCornerPrimitiveGeometry(data) {
        const segments = clampFiniteInteger(data?.segments, 12, 3, 64);
        const outerRadius = clampFiniteNumber(data?.outerRadius, 1, 0.2, 1);
        const innerRadius = clampFiniteNumber(
            data?.innerRadius,
            Math.max(0.05, outerRadius - clampFiniteNumber(data?.thickness, 0.24, 0.04, 0.45)),
            0.02,
            Math.max(0.03, outerRadius - 0.02)
        );
        const outerArc = this.samplePrimitiveArcPoints(-0.5, -0.5, outerRadius, 0, Math.PI * 0.5, segments);
        const innerArc = this.samplePrimitiveArcPoints(-0.5, -0.5, innerRadius, Math.PI * 0.5, 0, segments);
        const loop = [...outerArc, ...innerArc];
        const shape = this.createShapeFromPointLoop(loop);
        return this.createExtrudedStructuralShapeGeometry(shape, {
            curveSegments: segments,
            rotateX: -Math.PI / 2
        });
    }

    createCornerPrimitiveGeometry(data) {
        const style = typeof data?.cornerStyle === 'string' ? data.cornerStyle.toLowerCase() : 'mitered';
        return style === 'rounded'
            ? this.createRoundedCornerPrimitiveGeometry(data)
            : this.createMiteredCornerPrimitiveGeometry(data);
    }

    buildSpiralStairsPrimitivePartGeometries(data) {
        const steps = clampFiniteInteger(data?.steps, 12, 3, 96);
        const innerRadius = clampFiniteNumber(data?.innerRadius, 0.18, 0.02, 0.75);
        const outerRadius = clampFiniteNumber(data?.outerRadius, 0.5, Math.max(innerRadius + 0.04, 0.06), 1.25);
        const clockwise = data?.clockwise === true;
        const arcDegrees = clampFiniteNumber(data?.arcDegrees ?? data?.spanDegrees, 270, 15, 1080);
        const totalRadians = THREE.MathUtils.degToRad(clockwise ? -arcDegrees : arcDegrees);
        const startRadians = THREE.MathUtils.degToRad(clampFiniteNumber(data?.startDegrees, -135, -1080, 1080));
        const stepRadians = totalRadians / steps;
        const midRadius = (innerRadius + outerRadius) * 0.5;
        const radialThickness = Math.max(0.04, outerRadius - innerRadius);
        const riseHeight = 1 / steps;
        const treadDepth = Math.max(0.08, Math.abs(stepRadians) * midRadius);
        const parts = [];

        for (let index = 0; index < steps; index += 1) {
            const height = (index + 1) * riseHeight;
            const angle = startRadians + ((index + 0.5) * stepRadians);
            const geometry = new THREE.BoxGeometry(radialThickness, height, treadDepth);
            geometry.translate(0, -0.5 + (height * 0.5), 0);
            geometry.rotateY(-angle);
            geometry.translate(Math.cos(angle) * midRadius, 0, Math.sin(angle) * midRadius);
            parts.push(geometry);
        }

        if (data?.centerColumn !== false) {
            const columnRadius = Math.max(0.04, innerRadius * 0.85);
            const column = new THREE.CylinderGeometry(columnRadius, columnRadius, 1, clampFiniteInteger(data?.radialSegments, 12, 6, 32), 1, false);
            parts.push(column);
        }

        return parts;
    }

    createSpiralStairsPrimitiveGeometry(data) {
        return this.mergeStructuralPrimitiveGeometryParts(this.buildSpiralStairsPrimitivePartGeometries(data));
    }

    createGableRoofPrimitiveGeometry() {
        const shape = new THREE.Shape();
        shape.moveTo(-0.5, -0.5);
        shape.lineTo(-0.5, 0.0);
        shape.lineTo(0, 0.5);
        shape.lineTo(0.5, 0.0);
        shape.lineTo(0.5, -0.5);
        shape.lineTo(-0.5, -0.5);
        return this.createExtrudedStructuralShapeGeometry(shape, {
            curveSegments: 1,
            rotateX: -Math.PI / 2
        });
    }

    createHippedRoofPrimitiveGeometry(data) {
        const ridgeRatio = clampFiniteNumber(data?.ridgeRatio, 0.34, 0.08, 0.85);
        const ridgeHalf = THREE.MathUtils.clamp(ridgeRatio * 0.5, 0.04, 0.42);
        const points = [
            new THREE.Vector3(-0.5, -0.5, -0.5),
            new THREE.Vector3(0.5, -0.5, -0.5),
            new THREE.Vector3(0.5, -0.5, 0.5),
            new THREE.Vector3(-0.5, -0.5, 0.5),
            new THREE.Vector3(-0.5, 0.0, -0.5),
            new THREE.Vector3(0.5, 0.0, -0.5),
            new THREE.Vector3(0.5, 0.0, 0.5),
            new THREE.Vector3(-0.5, 0.0, 0.5),
            new THREE.Vector3(0, 0.5, -ridgeHalf),
            new THREE.Vector3(0, 0.5, ridgeHalf)
        ];
        const geometry = new ConvexGeometry(points);
        geometry.center();
        geometry.computeVertexNormals();
        return geometry.index ? geometry.toNonIndexed() : geometry;
    }

    createRoofPrimitiveGeometry(data) {
        const resolvedType = this.getStructuralPrimitiveType(data);
        return resolvedType === 'hipped_roof'
            ? this.createHippedRoofPrimitiveGeometry(data)
            : this.createGableRoofPrimitiveGeometry(data);
    }

    buildStairsPrimitivePartGeometries(data) {
        const steps = clampFiniteInteger(data?.steps, 6, 2, 32);
        const parts = [];
        for (let index = 0; index < steps; index += 1) {
            const height = (index + 1) / steps;
            const depth = 1 / steps;
            const geometry = new THREE.BoxGeometry(1, height, depth);
            geometry.translate(
                0,
                -0.5 + (height * 0.5),
                -0.5 + (index * depth) + (depth * 0.5)
            );
            parts.push(geometry);
        }
        return parts;
    }

    createStairsPrimitiveGeometry(data) {
        const parts = this.buildStairsPrimitivePartGeometries(data);
        return this.mergeStructuralPrimitiveGeometryParts(parts);
    }

    buildHollowBoxPrimitivePartGeometries(data) {
        const openSide = typeof data?.openSide === 'string' ? data.openSide.toLowerCase() : null;
        const wallThickness = clampFiniteNumber(data?.wallThickness, 0.12, 0.04, 0.45);
        const topThickness = openSide === 'top' ? 0 : wallThickness;
        const bottomThickness = openSide === 'bottom' ? 0 : wallThickness;
        const leftThickness = openSide === 'left' ? 0 : wallThickness;
        const rightThickness = openSide === 'right' ? 0 : wallThickness;
        const frontThickness = openSide === 'front' ? 0 : wallThickness;
        const backThickness = openSide === 'back' ? 0 : wallThickness;
        const parts = [];

        if (bottomThickness > 0) {
            const floor = new THREE.BoxGeometry(1, bottomThickness, 1);
            floor.translate(0, -0.5 + (bottomThickness * 0.5), 0);
            parts.push(floor);
        }
        if (topThickness > 0) {
            const ceiling = new THREE.BoxGeometry(1, topThickness, 1);
            ceiling.translate(0, 0.5 - (topThickness * 0.5), 0);
            parts.push(ceiling);
        }

        const innerMinY = -0.5 + bottomThickness;
        const innerMaxY = 0.5 - topThickness;
        const wallHeight = Math.max(innerMaxY - innerMinY, 0.02);
        const wallY = (innerMinY + innerMaxY) * 0.5;

        const innerMinZ = -0.5 + backThickness;
        const innerMaxZ = 0.5 - frontThickness;
        const sideDepth = Math.max(innerMaxZ - innerMinZ, 0.02);
        const sideZ = (innerMinZ + innerMaxZ) * 0.5;

        if (leftThickness > 0) {
            const leftWall = new THREE.BoxGeometry(leftThickness, wallHeight, sideDepth);
            leftWall.translate(-0.5 + (leftThickness * 0.5), wallY, sideZ);
            parts.push(leftWall);
        }
        if (rightThickness > 0) {
            const rightWall = new THREE.BoxGeometry(rightThickness, wallHeight, sideDepth);
            rightWall.translate(0.5 - (rightThickness * 0.5), wallY, sideZ);
            parts.push(rightWall);
        }

        const innerMinX = -0.5 + leftThickness;
        const innerMaxX = 0.5 - rightThickness;
        const frontBackWidth = Math.max(innerMaxX - innerMinX, 0.02);
        const frontBackX = (innerMinX + innerMaxX) * 0.5;

        if (backThickness > 0) {
            const backWall = new THREE.BoxGeometry(frontBackWidth, wallHeight, backThickness);
            backWall.translate(frontBackX, wallY, -0.5 + (backThickness * 0.5));
            parts.push(backWall);
        }
        if (frontThickness > 0) {
            const frontWall = new THREE.BoxGeometry(frontBackWidth, wallHeight, frontThickness);
            frontWall.translate(frontBackX, wallY, 0.5 - (frontThickness * 0.5));
            parts.push(frontWall);
        }

        return parts;
    }

    createHollowBoxPrimitiveGeometry(data) {
        const parts = this.buildHollowBoxPrimitivePartGeometries(data);
        return this.mergeStructuralPrimitiveGeometryParts(parts);
    }

    createCompositeStructuralPrimitiveColliderGeometries(data) {
        switch (data?.type) {
            case 'stairs':
                return this.buildStairsPrimitivePartGeometries(data);
            case 'spiral_stairs':
                return this.buildSpiralStairsPrimitivePartGeometries(data);
            case 'hollow_box':
                return this.buildHollowBoxPrimitivePartGeometries(data);
            default:
                return [];
        }
    }

    getGeometryTransformMatrix(data) {
        const position = new THREE.Vector3(...finiteVec3Components(data?.position, [0, 0, 0]));
        const rotation = new THREE.Euler(...finiteVec3Components(data?.rotation, [0, 0, 0]), 'XYZ');
        const scale = new THREE.Vector3(...finiteScaleComponents(data?.scale, [1, 1, 1]));
        return new THREE.Matrix4().compose(
            position,
            new THREE.Quaternion().setFromEuler(rotation),
            scale
        );
    }

    getOffsetStepMatrix(data = {}) {
        const matrixValues = Array.isArray(data?.offsetStepMatrix) ? data.offsetStepMatrix : (Array.isArray(data?.offsetMatrix) ? data.offsetMatrix : null);
        if (Array.isArray(matrixValues) && matrixValues.length >= 16) {
            return new THREE.Matrix4().fromArray(matrixValues.slice(0, 16).map((value, index) => {
                const numeric = Number(value);
                return Number.isFinite(numeric) ? numeric : (index % 5 === 0 ? 1 : 0);
            }));
        }
        const step = data?.offsetStep && typeof data.offsetStep === 'object' ? data.offsetStep : data;
        const position = new THREE.Vector3(...finiteVec3Components(step?.position ?? step?.positionStep, [0, 0, 0]));
        const rotation = new THREE.Euler(...finiteVec3Components(step?.rotation ?? step?.rotationStep, [0, 0, 0]), 'XYZ');
        const scale = new THREE.Vector3(...finiteScaleComponents(step?.scale ?? step?.scaleStep, [1, 1, 1]));
        return new THREE.Matrix4().compose(position, new THREE.Quaternion().setFromEuler(rotation), scale);
    }

    createTransformedArrayPrimitiveNode(baseNode, transformMatrix, arrayNode, index) {
        const resolvedType = this.getStructuralPrimitiveType(baseNode) || 'box';
        transformMatrix.decompose(this._structuralTempPosition, this._structuralTempQuaternion, this._structuralTempScale);
        const rotation = new THREE.Euler().setFromQuaternion(this._structuralTempQuaternion, 'XYZ');
        return {
            ...baseNode,
            type: resolvedType,
            id: baseNode.id
                ? `${baseNode.id}_array_${index + 1}`
                : (arrayNode.id ? `${arrayNode.id}_${index + 1}` : undefined),
            position: this._structuralTempPosition.toArray(),
            rotation: [rotation.x, rotation.y, rotation.z],
            scale: this._structuralTempScale.toArray(),
            isCollider: baseNode.isCollider ?? arrayNode.isCollider,
            isStructural: baseNode.isStructural ?? arrayNode.isStructural,
            materials: Array.isArray(baseNode.materials) && baseNode.materials.length > 0 ? baseNode.materials : arrayNode.materials
        };
    }

    expandArrayPrimitiveNodes(nodes = []) {
        const expanded = [];
        nodes.forEach((node) => {
            if (!node || typeof node !== 'object') return;
            if (node.type !== 'array_primitive') {
                expanded.push(node);
                return;
            }

            const count = clampFiniteInteger(node.count, 3, 1, 128);
            const basePrimitive = node.basePrimitive && typeof node.basePrimitive === 'object'
                ? { ...node.basePrimitive }
                : { type: typeof node.primitiveType === 'string' ? node.primitiveType : 'box' };
            const baseResolvedType = this.getStructuralPrimitiveType(basePrimitive) || 'box';
            const rootMatrix = this.getGeometryTransformMatrix(node);
            const baseMatrix = this.getGeometryTransformMatrix(basePrimitive);
            const stepMatrix = this.getOffsetStepMatrix(node);

            if (node.mergeHull) {
                const mergedParts = [];
                let runningMatrix = new THREE.Matrix4().identity();
                for (let index = 0; index < count; index += 1) {
                    const partNode = { ...basePrimitive, type: baseResolvedType };
                    const geometry = this.createStructuralPrimitiveGeometryFromData(partNode);
                    if (!geometry) continue;
                    const partMatrix = rootMatrix.clone().multiply(runningMatrix).multiply(baseMatrix);
                    geometry.applyMatrix4(partMatrix);
                    mergedParts.push(geometry);
                    runningMatrix = runningMatrix.clone().multiply(stepMatrix);
                }
                if (mergedParts.length > 0) {
                    expanded.push({
                        ...basePrimitive,
                        id: node.id || basePrimitive.id,
                        type: baseResolvedType,
                        compiledGeometry: this.mergeStructuralPrimitiveGeometryParts(mergedParts),
                        compiledWorldSpace: true,
                        isCollider: basePrimitive.isCollider ?? node.isCollider,
                        isStructural: basePrimitive.isStructural ?? node.isStructural,
                        materials: Array.isArray(basePrimitive.materials) && basePrimitive.materials.length > 0 ? basePrimitive.materials : node.materials
                    });
                }
                return;
            }

            let runningMatrix = new THREE.Matrix4().identity();
            for (let index = 0; index < count; index += 1) {
                const transform = rootMatrix.clone().multiply(runningMatrix).multiply(baseMatrix);
                expanded.push(this.createTransformedArrayPrimitiveNode(basePrimitive, transform, node, index));
                runningMatrix = runningMatrix.clone().multiply(stepMatrix);
            }
        });
        return expanded;
    }

    compileStructuralGeometryNodes(nodes = [], compilerOptions = {}) {
        if (!Array.isArray(nodes) || nodes.length === 0) return [];
        void compilerOptions;
        return this.expandArrayPrimitiveNodes(nodes).filter((node) => node && typeof node === 'object');
    }

    createStructuralPrimitiveGeometryFromData(data) {
        if (data?.compiledGeometry?.isBufferGeometry) {
            return data.compiledGeometry;
        }
        const resolvedType = this.getStructuralPrimitiveType(data);
        switch (resolvedType) {
            case 'box':
                return data?.bevelRadius > 0
                    ? this.createRoundedBoxPrimitiveGeometry(data)
                    : new THREE.BoxGeometry(1, 1, 1);
            case 'plane':
                return new THREE.PlaneGeometry(1, 1);
            case 'ramp':
                return this.createRampPrimitiveGeometry();
            case 'wedge':
                return this.createWedgePrimitiveGeometry();
            case 'cylinder':
                return this.createCylinderPrimitiveGeometry(data);
            case 'arch':
                return this.createArchPrimitiveGeometry(data);
            case 'cone':
            case 'pyramid':
                return this.createConePrimitiveGeometry(data);
            case 'tube':
                return this.createTubePrimitiveGeometry(data);
            case 'hexahedron':
                return this.createHexahedronPrimitiveGeometry(data);
            case 'catenary_primitive':
                return this.createCatenaryPrimitiveGeometry(data);
            case 'convex_hull':
                return this.createConvexHullPrimitiveGeometry(data);
            case 'capsule':
                return this.createCapsulePrimitiveGeometry(data);
            case 'frustum':
                return this.createFrustumPrimitiveGeometry(data);
            case 'torus':
                return this.createTorusPrimitiveGeometry(data);
            case 'displacement':
            case 'heightmap_patch':
            case 'terrain_quad':
                return this.createDisplacementPrimitiveGeometry(data);
            case 'corner':
                return this.createCornerPrimitiveGeometry(data);
            case 'stairs':
                return this.createStairsPrimitiveGeometry(data);
            case 'spiral_stairs':
                return this.createSpiralStairsPrimitiveGeometry(data);
            case 'hollow_box':
                return this.createHollowBoxPrimitiveGeometry(data);
            case 'roof_gable':
            case 'hipped_roof':
                return this.createRoofPrimitiveGeometry(data);
            default:
                return null;
        }
    }

    createModelFromData(modelData) {
        const group = new THREE.Group();
        group.name = modelData.modelName;

        const modelTextures = modelData.textures || {};
        const geoArray = Array.isArray(modelData.geometry) ? modelData.geometry : [];

        geoArray.forEach((geoData) => {
            const geometry = this.createStructuralPrimitiveGeometryFromData(geoData);
            if (!geometry) {
                console.warn(`Unknown geometry type in model ${modelData.modelName}: ${geoData.type}`);
                return;
            }
            this.levelGeometries.push(geometry);
    
            const createMaterial = (matData) => {
                const textureName = modelTextures[matData.ref];
                const texture = this.loadedAssets.textures[textureName];
                const params = {
                    map: texture,
                    color: texture ? 0xffffff : (matData.color || 0xcc00cc),
                    roughness: matData.roughnessFactor || 0.8,
                    metalness: matData.metalness || 0.2,
                };
    
                if (matData.emissiveColor) {
                    params.emissive = new THREE.Color(matData.emissiveColor);
                    params.emissiveIntensity = matData.emissiveIntensity || 1.0;
                    if (texture) params.emissiveMap = texture;
                }
    
                if (matData.transparent) {
                    params.transparent = true;
                    params.opacity = matData.opacity || 0.5;
                }
    
                return new THREE.MeshStandardMaterial(params);
            };
    
            let mesh;
            const matDataArray = Array.isArray(geoData.materials) ? geoData.materials : [geoData.materials];
    
            if (matDataArray.length > 1) { // Multi-material for different faces
                const materials = matDataArray.map(createMaterial);
                mesh = new THREE.Mesh(geometry, materials);
            } else { // Single material for the whole mesh
                const material = createMaterial(matDataArray[0] || {});
                mesh = new THREE.Mesh(geometry, material);
            }
    
            mesh.position.fromArray(finiteVec3Components(geoData.position, [0, 0, 0]));
            mesh.rotation.fromArray(finiteVec3Components(geoData.rotation, [0, 0, 0]));
            mesh.scale.fromArray(finiteScaleComponents(geoData.scale, [1, 1, 1]));
            group.add(mesh);
        });
        return group;
    }

    getClipVolumeModes(data = {}) {
        const rawModes = Array.isArray(data.clipModes)
            ? data.clipModes
            : (data.clipMode != null ? [data.clipMode] : []);
        const parsed = rawModes
            .map((mode) => String(mode).toLowerCase())
            .filter((mode) => mode === 'physics' || mode === 'ai' || mode === 'visibility');
        return parsed.length > 0 ? [...new Set(parsed)] : ['physics'];
    }

    collectTargetNodeMeshes(ids = []) {
        if (!Array.isArray(ids) || ids.length === 0) return [];
        const seen = new Set();
        const targets = [];
        ids.forEach((id) => {
            const node = this.levelNodesById.get(id);
            if (!node) return;
            const pushMesh = (mesh) => {
                if (!mesh?.isMesh || seen.has(mesh)) return;
                seen.add(mesh);
                targets.push(mesh);
            };
            if (node.isMesh) {
                pushMesh(node);
                return;
            }
            node.traverse?.((child) => pushMesh(child));
        });
        return targets;
    }

    createRenderablePrimitiveMesh(data, options = {}) {
        const geometry = this.createStructuralPrimitiveGeometryFromData(data);
        if (!geometry) return null;
        this.levelGeometries.push(geometry);
        const primaryMaterialData = data.materials && data.materials[0] != null
            ? this.normalizeMaterialData(data.materials[0], data)
            : null;
        const material = primaryMaterialData
            ? this.createMaterialFromData(primaryMaterialData)
            : new THREE.MeshStandardMaterial({ color: options.fallbackColor || 0xcc00cc });
        const mesh = new THREE.Mesh(geometry, material);
        if (!data.compiledWorldSpace) {
            mesh.position.fromArray(finiteVec3Components(data.position, [0, 0, 0]));
            mesh.rotation.fromArray(finiteVec3Components(data.rotation, [0, 0, 0]));
            mesh.scale.fromArray(finiteScaleComponents(data.scale, [1, 1, 1]));
        }
        mesh.name = data.name || data.id || createRuntimeId('primitive');
        mesh.castShadow = options.castShadow !== false;
        mesh.receiveShadow = options.receiveShadow !== false;
        return mesh;
    }

    createAliasedPrimitiveData(data = {}, fallbackType = 'box') {
        const primitiveType = typeof data.primitiveType === 'string' ? data.primitiveType.toLowerCase() : fallbackType;
        return {
            ...data,
            type: primitiveType
        };
    }

    createRuntimeBoxVolume(data = {}, defaults = {}) {
        const position = new THREE.Vector3(...finiteVec3Components(data?.position, [0, 0, 0]));
        const sizeValues = finiteVec3Components(data?.size ?? data?.scale, defaults.size || [4, 4, 4]);
        return {
            id: data.id || createRuntimeId(defaults.runtimeId || 'volume'),
            type: defaults.type || data.type || 'volume',
            shape: data.shape === 'cylinder' ? 'cylinder' : (data.shape === 'sphere' ? 'sphere' : 'box'),
            position,
            size: new THREE.Vector3(Math.abs(sizeValues[0]), Math.abs(sizeValues[1]), Math.abs(sizeValues[2])),
            radius: Math.max(0.25, Number.isFinite(Number(data.radius)) ? Number(data.radius) : Math.max(Math.abs(sizeValues[0]), Math.abs(sizeValues[2])) * 0.5),
            height: Math.max(0.25, Math.abs(sizeValues[1] || 2)),
            helper: null
        };
    }

    registerShrinkwrapModifierPrimitive(data) {
        void data;
    }

    buildRuntimePrimitiveMesh(data, fallbackType = 'box', options = {}) {
        return this.createRenderablePrimitiveMesh(this.createAliasedPrimitiveData(data, fallbackType), options);
    }

    createGeometryFromData(data) {
        if (!data || typeof data !== 'object') return;
        const geometry = this.createStructuralPrimitiveGeometryFromData(data);
        if (!geometry) {
            console.warn("Unknown geometry type:", data.type);
            return;
        }
        this.levelGeometries.push(geometry);

        const primaryMaterialData = data.materials && data.materials[0] != null
            ? this.normalizeMaterialData(data.materials[0], data)
            : null;
        const material = primaryMaterialData
            ? this.createMaterialFromData(primaryMaterialData)
            : new THREE.MeshStandardMaterial({ color: 0xcc00cc });
        const mesh = new THREE.Mesh(geometry, material);
        const isDecalPlane = data.type === 'plane' && material.transparent;

        if (!data.compiledWorldSpace) {
            mesh.position.fromArray(finiteVec3Components(data.position, [0, 0, 0]));
            mesh.rotation.fromArray(finiteVec3Components(data.rotation, [0, 0, 0]));
            mesh.scale.fromArray(finiteScaleComponents(data.scale, [1, 1, 1]));
        }
        mesh.name = data.name || data.id;
        mesh.castShadow = !isDecalPlane;
        mesh.receiveShadow = !isDecalPlane;
        if (isDecalPlane) {
            mesh.renderOrder = data.renderOrder ?? primaryMaterialData?.renderOrder ?? 20;
            const decalOffset = Number.isFinite(data.decalOffset)
                ? data.decalOffset
                : (data.decalPreset ? 0.004 : 0);
            if (decalOffset !== 0) {
                const normal = new THREE.Vector3(0, 0, 1).applyQuaternion(mesh.quaternion);
                if (normal.lengthSq() < 1e-12) normal.set(0, 1, 0);
                else normal.normalize();
                mesh.position.addScaledVector(normal, decalOffset);
            }
        }

        const supportColliderGeometries = data.isCollider ? this.createCompositeStructuralPrimitiveColliderGeometries(data) : [];
        if (supportColliderGeometries.length > 0) {
            const root = new THREE.Group();
            root.name = data.name || data.id || `${data.type}_group`;
            root.position.copy(mesh.position);
            root.rotation.copy(mesh.rotation);
            root.scale.copy(mesh.scale);
            mesh.position.set(0, 0, 0);
            mesh.rotation.set(0, 0, 0);
            mesh.scale.set(1, 1, 1);
            root.add(mesh);

            supportColliderGeometries.forEach((supportGeometry, index) => {
                this.levelGeometries.push(supportGeometry);
                const colliderMesh = new THREE.Mesh(supportGeometry, new THREE.MeshBasicMaterial({
                    transparent: true,
                    opacity: 0,
                    depthWrite: false,
                    colorWrite: false
                }));
                colliderMesh.name = `${data.name || data.id || data.type}_collider_${index + 1}`;
                colliderMesh.userData.structuralCollider = data.isStructural !== false;
                colliderMesh.userData.syntheticStructuralPrimitiveCollider = true;
                colliderMesh.castShadow = false;
                colliderMesh.receiveShadow = false;
                root.add(colliderMesh);
            });
            root.updateMatrixWorld(true);
            root.children.forEach((child) => {
                if (!child.isMesh || !child.userData?.syntheticStructuralPrimitiveCollider) return;
                this.registerCollidable(child);
            });
            if (data.id) this.levelNodesById.set(data.id, mesh);
            this.levelObjects.add(root);
            return;
        }

        if (data.isCollider) {
            mesh.userData.structuralCollider = data.isStructural !== false;
            this.registerCollidable(mesh);
        }
        if (data.id) this.levelNodesById.set(data.id, mesh);
        this.levelObjects.add(mesh);
    }

    createModelInstanceFromData(data) {
        const sourceModel = this.loadedAssets.models[data.modelName];
        if (!sourceModel) {
            console.warn(`Model instance "${data.id}" references an unknown model: "${data.modelName}"`);
            return;
        }

        const instance = this.cloneLoadedModel(sourceModel);
        this.applyAuthoredPlacement(instance, data.position, data.rotation, data.scale);
        instance.name = data.name || data.id;
        const blocksPlayer = data.isCollider === true;

        instance.traverse(child => {
            if (child.isMesh) {
                child.castShadow = true;
                child.receiveShadow = true;
                if (blocksPlayer) {
                    child.userData.structuralCollider = data.isStructural !== false;
                    this.registerCollidable(child);
                }
            }
        });

        if (data.id) this.levelNodesById.set(data.id, instance);
        this.levelObjects.add(instance);
    }

    createLightFromData(data) {
        if (!data || typeof data !== 'object') return;
        const color = new THREE.Color(data.color != null ? data.color : '#333333');
        const intensityRaw = data.intensity ?? 1.0;
        const intensity = Number.isFinite(intensityRaw) && intensityRaw >= 0 ? intensityRaw : 1.0;
        const distRaw = data.distance;
        const pointDistance = Number.isFinite(distRaw) && distRaw >= 0 ? distRaw : 0;
        const decayRaw = data.decay;
        const pointDecay = Number.isFinite(decayRaw) && decayRaw >= 0 ? decayRaw : 1;
        let light;

        switch (data.type) {
            case "ambient": light = new THREE.AmbientLight(color, intensity); break;
            case "point": light = new THREE.PointLight(color, intensity, pointDistance, pointDecay); break;
            default: console.warn("Unknown light type:", data.type); return;
        }
        
        if (Array.isArray(data.position) && data.position.length >= 3) {
            const px = Number(data.position[0]);
            const py = Number(data.position[1]);
            const pz = Number(data.position[2]);
            if (Number.isFinite(px) && Number.isFinite(py) && Number.isFinite(pz)) light.position.set(px, py, pz);
        }
        if (data.castShadow) light.castShadow = true;
        light.userData.baseIntensity = intensity;
        light.userData.baseColor = light.color.clone();
        if (data.id) {
            light.name = data.id;
            this.levelLightsById.set(data.id, light);
            this.levelNodesById.set(data.id, light);
        }
        if (data.userData?.flicker) {
            const interval = window.setInterval(() => {
                if (!this.gameState.isPaused) {
                    const flick = intensity * 0.8 + Math.random() * (intensity * 0.4);
                    light.intensity = Number.isFinite(flick) ? Math.max(0, flick) : intensity;
                }
            }, 200);
            this.flickerIntervals.push(interval);
        }
        this.levelObjects.add(light);
    }

    applyPlayerSpawnOrientation(data, position) {
        if (!this.player?.camera || !data || typeof data !== 'object') return;
        const camera = this.player.camera;
        if (Array.isArray(data.lookAt) && data.lookAt.length >= 3) {
            camera.lookAt(this._toObject.fromArray(finiteVec3Components(data.lookAt, [0, 0, 0])));
            camera.rotation.z = 0;
            return;
        }
        if (Array.isArray(data.rotation) && data.rotation.length >= 3) {
            camera.rotation.fromArray(finiteVec3Components(data.rotation, [0, 0, 0]));
            camera.rotation.z = 0;
            return;
        }
        if (Number.isFinite(data.yaw)) {
            camera.rotation.set(0, data.yaw, 0);
            return;
        }
        if (position) {
            camera.lookAt(position.x, camera.position.y, position.z - 1);
            camera.rotation.z = 0;
        }
    }

    isPlayerSpawnBlocked(feetPosition) {
        if (!this.player || !feetPosition) return false;
        if (typeof this.player.traceRoundedHullBox === 'function' && typeof this.player.withSupportSurfaceFilter === 'function') {
            return !!this.player.traceRoundedHullBox(
                feetPosition,
                this.player.withSupportSurfaceFilter({}, feetPosition.y)
            );
        }
        const spawnBox = this.player.updateCollisionBoxAt(feetPosition).clone();
        const feetY = feetPosition.y;
        const candidates = this.queryCollidablesForBox(spawnBox);
        for (const candidate of candidates) {
            if (!candidate) continue;
            const candidateBox = this.getCollisionBounds(candidate, this._traceCandidateBox);
            if (!spawnBox.intersectsBox(candidateBox)) continue;
            if (candidateBox.max.y <= feetY + 0.18) continue;
            return true;
        }
        return false;
    }

    placePlayerAtSpawn(position, data = null) {
        const candidate = new THREE.Vector3();
        const spawnOffsets = [
            [0, 0, 0],
            [0, 0, -0.75], [0.75, 0, 0], [0, 0, 0.75], [-0.75, 0, 0],
            [1.5, 0, 0], [-1.5, 0, 0], [0, 0, 1.5], [0, 0, -1.5],
            [0.75, 0, 0.75], [0.75, 0, -0.75], [-0.75, 0, 0.75], [-0.75, 0, -0.75],
            [2.25, 0, 0], [-2.25, 0, 0], [0, 0, 2.25], [0, 0, -2.25],
            [1.5, 0, 1.5], [1.5, 0, -1.5], [-1.5, 0, 1.5], [-1.5, 0, -1.5],
            [3.0, 0, 0], [-3.0, 0, 0], [0, 0, 3.0], [0, 0, -3.0],
            [0, 0.35, 0], [0, 0.7, 0], [0, 1.05, 0], [0, 1.4, 0]
        ];

        this.player.targetHeight = this.player.eyeHeights[this.player.stance];

        for (const [x, y, z] of spawnOffsets) {
            candidate.copy(position).add(this._toObject.set(x, y, z));
            this.player.setFeetPosition(candidate);
            if (!this.isPlayerSpawnBlocked(candidate)) {
                if (typeof this.player.probeGroundAtFeet === 'function') {
                    const supportHit = this.player.probeGroundAtFeet(
                        candidate,
                        (this.player.maxStepHeight || 0.45) + (this.player.groundSnapDistance || 0.14) + 0.25,
                        0.12
                    );
                    if (supportHit?.point) {
                        candidate.y = supportHit.point.y + (this.player.groundContactOffset || 0);
                        this.player.setFeetPosition(candidate);
                    }
                }
                if (!candidate.equals(position)) {
                    console.warn(`Adjusted player spawn from ${position.toArray()} to ${candidate.toArray()} to avoid collision.`);
                }
                this.applyPlayerSpawnOrientation(data, candidate);
                return;
            }
        }

        const verticalRecoveryOffsets = [1.4, 2.1, 2.8, 3.5, 4.2];
        for (const y of verticalRecoveryOffsets) {
            candidate.copy(position).add(this._toObject.set(0, y, 0));
            this.player.setFeetPosition(candidate);
            if (!this.isPlayerSpawnBlocked(candidate)) {
                if (typeof this.player.probeGroundAtFeet === 'function') {
                    const supportHit = this.player.probeGroundAtFeet(
                        candidate,
                        (this.player.maxStepHeight || 0.45) + (this.player.groundSnapDistance || 0.14) + 0.25,
                        0.12
                    );
                    if (supportHit?.point) {
                        candidate.y = supportHit.point.y + (this.player.groundContactOffset || 0);
                        this.player.setFeetPosition(candidate);
                    }
                }
                console.warn(`Recovered player spawn above authored start: ${candidate.toArray()}`);
                this.applyPlayerSpawnOrientation(data, candidate);
                return;
            }
        }

        this.player.setFeetPosition(position.clone());
        if (typeof this.player.forceUnstuckRecovery === 'function' && this.player.forceUnstuckRecovery(position.clone())) {
            this.applyPlayerSpawnOrientation(data, this.player.feetPosition.clone());
            return;
        }
        this.applyPlayerSpawnOrientation(data, position);
    }

    finalizePlayerSpawnGrounding() {
        if (!this.player?.probeGroundAtFeet) return;
        const groundHit = this.player.probeGroundAtFeet(
            this.player.feetPosition,
            (this.player.groundSnapDistance || 0.14) + (this.player.maxStepHeight || 0.7) + 0.35,
            this.player.getGroundProbeAllowAbove?.() ?? 0.2
        );
        if (!groundHit?.point) {
            this.player.onGround = false;
            return;
        }
        const clearance = Number.isFinite(groundHit.clearance)
            ? groundHit.clearance
            : Math.max(0, this.player.feetPosition.y - groundHit.point.y);
        if (clearance <= Math.max(this.player.groundSnapDistance || 0.14, (this.player.groundContactOffset || 0.03) + 0.02)) {
            this.player.feetPosition.y = groundHit.point.y + (this.player.groundContactOffset || 0.03);
            this.player.syncCameraToBody?.();
            this.player.onGround = true;
            return;
        }
        this.player.onGround = false;
    }

    resolvePlayerStartFeetPosition(data = {}) {
        const posArr = Array.isArray(data.position) && data.position.length >= 3 ? data.position : [0, 0, 0];
        const position = new THREE.Vector3().fromArray(finiteVec3Components(posArr, [0, 0, 0]));
        const spawnStance = typeof data.stance === 'string' && this.player?.eyeHeights?.[data.stance]
            ? data.stance
            : 'standing';
        const authoredAsFeet = data.positionMode === 'feet' || data.positionIsFeet === true;
        return authoredAsFeet
            ? position
            : position.clone().add(this._toObject.set(0, -(this.player?.eyeHeights?.[spawnStance] || 1.8), 0));
    }

    updateCharacterAnimationControllers(delta) {
        void delta;
    }

    stepFrame(delta, forcedElapsedTime = null) {
        const previousElapsedOverride = this._elapsedTimeOverride;
        if (Number.isFinite(forcedElapsedTime)) this._elapsedTimeOverride = forcedElapsedTime;
        try {
            if (!Number.isFinite(delta) || delta <= 0) delta = 1 / 120;
            else delta = Math.min(0.2, delta);
            if (!this.gameState.isPaused) {
                this.setShaderIntensity('gameplay');
            }

            if (!this.gameState.isPaused) {

                this.player.update(delta, this.moveState);
                this.updateEnvironmentalVolumes();
                this.updateCharacterAnimationControllers(delta);
                this.updateSoundscape(delta);
                this.updateHUD(delta);
            } else {
                this.updateCharacterAnimationControllers(delta);
                this.updateEnvironmentalVolumes();
            }
            
            this.renderFrame();
        } finally {
            this._elapsedTimeOverride = previousElapsedOverride;
        }
    }

    renderFrame() {
        const timeDelta = this.getElapsedTime();
        this.setShaderIntensity(this.currentShaderIntensityLevel || 'gameplay');
        void timeDelta;
        this.renderer.render(this.scene, this.camera);
    }

    animate() {
        this.animationFrameId = requestAnimationFrame(() => this.animate());
        if (document.hidden) {
            this.clock.getDelta();
            return;
        }
        let delta = Math.min(this.clock.getDelta(), 0.05);
        if (!Number.isFinite(delta) || delta <= 0) delta = 1 / 60;
        this.stepFrame(delta);
    }

    updateHUD(delta) {
        void delta;
        this.updateTriggerVolumes();
    }

    turnObjectTowardsDirection(object, direction, delta, turnSpeed = Math.PI * 2.1) {
        if (!object || !direction || direction.lengthSq() < 1e-8) return 1;
        const desiredYaw = Math.atan2(direction.x, direction.z);
        const currentYaw = Number.isFinite(object.rotation?.y) ? object.rotation.y : 0;
        const maxStep = Math.max(0.001, (Number.isFinite(turnSpeed) ? turnSpeed : Math.PI * 2.1) * delta);
        const { newYaw, alignment } = stepYawToward(currentYaw, desiredYaw, maxStep);
        object.rotation.y = newYaw;
        return alignment;
    }

    getTurnLimitedSpeedFactor(alignment, minFactor = 0.06, fullSpeedThreshold = 0.84) {
        const safeMin = THREE.MathUtils.clamp(minFactor, 0, 1);
        const safeThreshold = THREE.MathUtils.clamp(fullSpeedThreshold, 0.05, 1);
        const factor = THREE.MathUtils.mapLinear(alignment, -0.2, safeThreshold, safeMin, 1);
        return THREE.MathUtils.clamp(factor, safeMin, 1);
    }

     cleanup() {
        this.flickerIntervals.forEach((id) => clearInterval(id));
        this.flickerIntervals = [];
        this.levelTimeouts.forEach((id) => clearTimeout(id));
        this.levelIntervals.forEach((id) => clearInterval(id));
        this.levelTimeouts = [];
        this.levelIntervals = [];
        if (this.hudDismissTimers?.size) {
            this.hudDismissTimers.forEach((timeoutId) => clearTimeout(timeoutId));
            this.hudDismissTimers.clear();
        }
        if (this._levelNameToastTimeout) {
            clearTimeout(this._levelNameToastTimeout);
            this._levelNameToastTimeout = null;
        }
        this.stopAllNonVoiceAudio();
        if (this._gameOverUiTimeout) {
            clearTimeout(this._gameOverUiTimeout);
            this._gameOverUiTimeout = null;
        }
        if (this.renderer?.domElement && this._onWebglContextLost) {
            this.renderer.domElement.removeEventListener('webglcontextlost', this._onWebglContextLost, false);
            this.renderer.domElement.removeEventListener('webglcontextrestored', this._onWebglContextRestored, false);
        }
        if (this.contextMenuHandler) document.removeEventListener('contextmenu', this.contextMenuHandler);
        document.removeEventListener('keydown', this.keyDownHandler);
        document.removeEventListener('keyup', this.keyUpHandler);
        window.removeEventListener('resize', this.resizeHandler);
        document.removeEventListener('visibilitychange', this.visibilityChangeHandler, false);
        document.removeEventListener('pointerlockerror', this.pointerLockErrorHandler, false);
        if (this.wheelHandler) {
            window.removeEventListener('wheel', this.wheelHandler);
            this.wheelHandler = null;
        }
        if (this.animationFrameId) {
            cancelAnimationFrame(this.animationFrameId);
            this.animationFrameId = null;
        }
        if (this.characterAnimationControllers?.size) {
            for (const controller of this.characterAnimationControllers) {
                try {
                    controller.mixer?.stopAllAction?.();
                    controller.mixer?.uncacheRoot?.(controller.root);
                } catch (_) {}
            }
            this.characterAnimationControllers.clear();
        }
     }
}

let gameInstance = new AnomalousEchoGame();
window.gameInstance = gameInstance;
let gameTeardownRan = false;
function teardownGame() {
    if (gameTeardownRan) return;
    gameTeardownRan = true;
    gameInstance.cleanup();
}
window.addEventListener('pagehide', (e) => {
    if (!e.persisted) teardownGame();
});
window.addEventListener('beforeunload', () => {
    teardownGame();
});

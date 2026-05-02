
import * as THREE from 'three';
import { PointerLockControls } from 'three/examples/jsm/controls/PointerLockControls.js';
import {
    DEFAULT_KEYS,
    GameAssets,
    STRINGS,
    clampFiniteInteger,
    clampFiniteNumber,
    createRuntimeId,
    finiteVec2Components,
    finiteVec3Components,
    finiteQuatComponents,
    finiteScaleComponents,
    pointInsideAuthoringVolume,
    probeGroundAtFeet,
    resolveAuthoringDecalPreset,
    tryGetPrimaryPlayerStart
} from './engine.js';
import { devLevel } from './dev-level.js';

/** Default FP tuning; C++ equivalent: `ri::trace::FpSandboxMovementOptions` / `DefaultLocomotionTuning`. */
const DEFAULT_PLAYER_TUNING = Object.freeze({
    walkSpeed: 5.0,
    gravity: 32.0,
    fallGravityMultiplier: 1.4,
    maxFallSpeed: 28.0,
    maxStepHeight: 0.7
});

/** Player: locomotion, hull traces, ground probe (see RawIron trace/locomotion). */
class Player {
    constructor(controls, game) {
        this.game = game;
        this.controls = controls;
        this.camera = controls.object;
        this.position = this.camera.position;
        this.velocity = new THREE.Vector3();
        this.direction = new THREE.Vector3();
        this.stance = 'standing';
        this.eyeHeights = { standing: 1.8 };
        this.hullHeights = { standing: 1.62 };
        this.hullRadius = { standing: 0.24 };
        this.speeds = {
            walk: DEFAULT_PLAYER_TUNING.walkSpeed
        };
        this.onGround = false;
        this.targetHeight = this.eyeHeights.standing;
        this.gravity = DEFAULT_PLAYER_TUNING.gravity;
        this.fallGravityMultiplier = DEFAULT_PLAYER_TUNING.fallGravityMultiplier;
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
        this.forwardVector = new THREE.Vector3();
        this.rightVector = new THREE.Vector3();
        this.moveDirection = new THREE.Vector3();
        this.surfaceNormal = new THREE.Vector3(0, 1, 0);
        this.surfacePoint = new THREE.Vector3();
        this.surfaceMoveDirection = new THREE.Vector3();
        this.stepProbeOffset = new THREE.Vector3();
        this.groundProbeOrigin = new THREE.Vector3();
        this.groundProbePosition = new THREE.Vector3();
        this.slideDelta = new THREE.Vector3();
        this._sweepDelta = new THREE.Vector3();
        this._supportCheckBox = new THREE.Box3();
        this.lastGroundedAt = 0;
        this.syncCameraToBody();
    }

    setStance() {
        const preservedFeet = this.feetPosition.clone();
        this.stance = 'standing';
        this.targetHeight = this.eyeHeights.standing;
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

    setFeetPosition(position) {
        if (!position) return;
        const { x, y, z } = position;
        if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(z)) return;
        this.feetPosition.copy(position);
        this.syncCameraToBody();
    }

    getCurrentSpeed() {
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
        if (!basePosition || !this.game?.traceBox) return false;
        const candidate = this.tempVector.clone();
        const step = Math.max(0.06, this.getCurrentHullRadius() * 0.45);
        const lift = Math.max(this.groundContactOffset + 0.02, 0.06);
        const offsets = [
            [0, lift, 0],
            [step, 0, 0], [-step, 0, 0], [0, 0, step], [0, 0, -step],
            [step, lift, 0], [-step, lift, 0], [0, lift, step], [0, lift, -step]
        ];
        for (const [x, y, z] of offsets) {
            candidate.copy(basePosition).add(this.stepProbeOffset.set(x, y, z));
            if (!this.traceHullBox(candidate, this.withSupportSurfaceFilter({}, candidate.y, 0.24))) {
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
            this.velocity.y = 0;
            this.syncCameraToBody();
            return true;
        }
        const candidate = new THREE.Vector3();
        const step = Math.max(0.16, this.getCurrentHullRadius() * 1.35);
        for (const lift of [Math.max(0.28, this.maxStepHeight * 0.45), Math.max(0.85, this.maxStepHeight + 0.22), Math.max(1.7, this.getCurrentHullHeight() + 0.25)]) {
            for (const [x, z] of [[0, 0], [step, 0], [-step, 0], [0, step], [0, -step], [step * 2, 0], [-step * 2, 0]]) {
                candidate.copy(origin).add(this.stepProbeOffset.set(x, lift, z));
                if (!this.traceHullBox(candidate, this.withSupportSurfaceFilter({}, candidate.y, 0.28))) {
                    this.setFeetPosition(candidate);
                    this.velocity.y = 0;
                    return true;
                }
            }
        }
        candidate.copy(origin).add(this.stepProbeOffset.set(Math.max(step * 4, 1.6), Math.max(1.6, this.getCurrentHullHeight()), 0));
        this.setFeetPosition(candidate);
        this.velocity.y = 0;
        return true;
    }

    checkCollision() {
        if (!Array.isArray(this.game.collidableMeshes)) return false;
        this.updateCollisionBoxAt();
        return !!this.traceHullBox(this.feetPosition, this.withSupportSurfaceFilter({}, this.feetPosition.y));
    }

    updateCollisionBoxAt(position = this.feetPosition) {
        const collisionHeight = this.getCurrentHullHeight();
        const radius = this.getCurrentHullRadius();
        this.collisionSize.set(radius * 2, collisionHeight, radius * 2);
        this._collisionCenter.copy(position);
        this._collisionCenter.y += collisionHeight * 0.5;
        return this.collisionBox.setFromCenterAndSize(this._collisionCenter, this.collisionSize);
    }

    traceHullBox(position = this.feetPosition, options = {}) {
        if (!this.game?.traceBox) return null;
        return this.game.traceBox(this.updateCollisionBoxAt(position), { traceTag: 'player', ...options });
    }

    traceHullSweep(position, deltaVector, options = {}) {
        if (!this.game?.traceSweptBox || !deltaVector || deltaVector.lengthSq() <= 1e-10) return null;
        return this.game.traceSweptBox(this.updateCollisionBoxAt(position), deltaVector, { traceTag: 'player', ...options });
    }

    moveWithSweep(deltaVector, options = {}) {
        if (!deltaVector || deltaVector.lengthSq() <= 1e-10) return null;
        this._sweepDelta.copy(deltaVector);
        if (!this.game.traceSweptBox) {
            this.feetPosition.add(this._sweepDelta);
            this.syncCameraToBody();
            return null;
        }
        const traceOptions = this._sweepDelta.y >= 0
            ? this.withSupportSurfaceFilter(options, this.feetPosition.y)
            : options;
        const hit = this.traceHullSweep(this.feetPosition, this._sweepDelta, traceOptions);
        if (!hit) {
            this.feetPosition.add(this._sweepDelta);
            this.syncCameraToBody();
            return null;
        }
        this.feetPosition.addScaledVector(this._sweepDelta, Math.max(0, hit.time - 0.001));
        this.syncCameraToBody();
        return hit;
    }

    moveWithSlide(deltaVector, options = {}) {
        if (!deltaVector || deltaVector.lengthSq() <= 1e-10) return null;
        if (!this.game.slideMoveBox) {
            this.feetPosition.add(deltaVector);
            this.syncCameraToBody();
            return null;
        }
        const result = this.game.slideMoveBox(
            this.updateCollisionBoxAt(),
            deltaVector,
            this.withSupportSurfaceFilter(options, this.feetPosition.y)
        );
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
        return probeGroundAtFeet(this.game, baseFeetPosition, {
            maxDistance,
            allowAbove,
            getHullRadius: () => this.getCurrentHullRadius(),
            getGroundProbeAllowAbove: () => this.getGroundProbeAllowAbove(),
            minNormalY: this.maxWalkableSlopeY,
            groundProbePosition: this.groundProbePosition,
            groundProbeOrigin: this.groundProbeOrigin
        });
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
            this.stance = 'standing';
            this.targetHeight = this.eyeHeights.standing;
            if (this.checkCollision()) {
                this.tryResolveStuck(this.feetPosition);
            }
            const wasGrounded = this.onGround;
            const groundHit = this.updateGroundState(allCollidables);
            const volumeModifiers = this.game?.getPhysicsVolumeModifiersAt?.(this.camera.position) || {
                gravityScale: 1,
                drag: 0,
                buoyancy: 0,
                flow: this.tempVector.set(0, 0, 0),
                activeVolumes: []
            };
            if (this.onGround) {
                this.lastGroundedAt = now;
                this.velocity.y = Math.max(0, this.velocity.y);
                if (groundHit?.point) {
                    this.feetPosition.y = groundHit.point.y + this.groundContactOffset;
                    this.syncCameraToBody();
                }
            } else {
                const netGravityScale = THREE.MathUtils.clamp(volumeModifiers.gravityScale - volumeModifiers.buoyancy, -2, 4);
                this.velocity.y = Math.max(
                    this.velocity.y - (this.gravity * this.fallGravityMultiplier * netGravityScale * dt),
                    -this.maxFallSpeed
                );
            }

            const currentSpeed = this.getCurrentSpeed();
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
            const acceleration = this.onGround ? 26.0 : 10.0;
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
            if (this.velocity.y <= 0) {
                const stepDownDistance = wasGrounded ? this.maxStepHeight + this.groundSnapDistance : this.groundSnapDistance;
                postGroundHit = this.applyStepDown(stepDownDistance);
            }
            if (!postGroundHit) {
                postGroundHit = this.updateGroundState(allCollidables);
            }
            if (this.onGround && this.velocity.y <= 0 && postGroundHit?.point) {
                this.feetPosition.y = postGroundHit.point.y + this.groundContactOffset;
                this.syncCameraToBody();
                this.lastGroundedAt = now;
            }

            this.camera.rotation.z = 0;
            if (!Number.isFinite(this.velocity.x)) this.velocity.x = 0;
            if (!Number.isFinite(this.velocity.y)) this.velocity.y = 0;
            if (!Number.isFinite(this.velocity.z)) this.velocity.z = 0;
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
        this.gameState = { isPaused: true, isLoaded: false, isGameOver: false, missionCompleted: false, invulnerableUntil: 0 };
        this.loadedAssets = { textures: {}, models: {} };
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
        this._sharedTextureLoader = null;
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
        this.moveState = { forward: false, backward: false, left: false, right: false }; 
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
    }

    handleSystemKeys(e) {
        return false;
    }

    handleUIKeys(e) {
        void e;
        return false;
    }

    handleActionKeys(e) {
        void e;
        return false;
    }

    handleMovementKeys(e, isKeyDown) {
        const keys = this.keyBindings;
        switch (e.code) {
            case keys.forward: this.moveState.forward = isKeyDown; break;
            case keys.backward: this.moveState.backward = isKeyDown; break;
            case keys.left: this.moveState.left = isKeyDown; break;
            case keys.right: this.moveState.right = isKeyDown; break;
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
        if (Array.isArray(sourceModel?.animations) && sourceModel.animations.length > 0) {
            instance.animations = sourceModel.animations.map((clip) => (clip?.clone ? clip.clone() : clip));
        }
        if (sourceModel?.userData?.npcArchetype) {
            instance.userData.npcArchetype = sourceModel.userData.npcArchetype;
        }
        if (importTransform) {
            instance.userData.importTransform = JSON.parse(JSON.stringify(importTransform));
        }
        return instance;
    }

    resolveNpcArchetype(spawnData = {}, modelName = '', sourceArchetype = '') {
        const explicit = spawnData?.npcArchetype || spawnData?.archetype || sourceArchetype;
        if (explicit && String(explicit).trim().length > 0) {
            return String(explicit).trim().toLowerCase();
        }
        const hint = `${spawnData?.name || ''} ${spawnData?.id || ''} ${modelName || ''}`.toLowerCase();
        if (/(doctor|medic|surgeon|nurse|medical)/.test(hint)) return 'doctor';
        if (/(firefighter|fireman|rescue|ems)/.test(hint)) return 'firefighter';
        if (/(police|officer|cop|security|guard)/.test(hint)) return 'police';
        if (/(hazmat|radiation|containment)/.test(hint)) return 'hazmat';
        if (/(soldier|military|marine)/.test(hint)) return 'soldier';
        return 'civilian';
    }

    async loadModelAsset(modelName) {
        const modelLoader = new THREE.FileLoader();
        modelLoader.setResponseType('json');
        const raw = await new Promise((resolve, reject) => {
            modelLoader.load(`${GameAssets.MODEL_DATA_PATH_PREFIX}${modelName}.json`, resolve, undefined, reject);
        });
        let parsed;
        try {
            parsed = typeof raw === 'string' ? JSON.parse(raw) : raw;
        } catch (err) {
            console.warn(`Model JSON parse failed for "${modelName}":`, err);
            return;
        }
        if (!parsed || typeof parsed !== 'object') {
            console.warn(`Model data missing or invalid for "${modelName}".`);
            return;
        }
        this.loadedAssets.models[modelName] = this.createModelFromData(parsed);
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
            await this.loadLevel(devLevel, 'dev-level.js');
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
            this.staticCollidableMeshes.push(mesh);
            if (mesh.userData.structuralCollider !== false) this.structuralCollidableMeshes.push(mesh);
        }
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

    /**
     * Load a level. Accepts a level object directly (preferred) or a filename string for legacy
     * `fetch('./<filename>')` use. The protoengine ships only `dev-level.js` and imports it
     * statically; the filename branch is kept for tooling that pre-baked JSON during a previous era.
     */
    async loadLevel(levelInput, levelTag = null) {
        const levelFilename = typeof levelInput === 'string' ? levelInput : (levelTag || 'inline-level');
        this.emitRuntimeEvent('levelLoadRequested', { id: createRuntimeId('level'), levelFilename });
        const overlay = this.ui.levelTransitionOverlay;
        const useFade = overlay && this.gameState.isLoaded;
        if (useFade) {
            overlay.style.opacity = '1';
            await new Promise(r => setTimeout(r, 250));
        }
        this.currentLevelFilename = levelFilename;
        try {
            const rawLevelData = typeof levelInput === 'string'
                ? await this.fetchLevelJson(levelInput)
                : levelInput;
            await this.applyLevelData(rawLevelData, levelFilename);
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

    async fetchLevelJson(levelFilename) {
        const response = await fetch('./' + levelFilename);
        if (!response.ok) throw new Error(`Network response was not ok. Status: ${response.status}`);
        const text = await response.text();
        try {
            return JSON.parse(text);
        } catch {
            throw new Error(STRINGS.msg.levelJsonParse);
        }
    }

    async applyLevelData(rawLevelData, levelFilename) {
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

        const playerStartData = tryGetPrimaryPlayerStart(levelData.spawners);
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

    showLevelNameToast(levelName) {
        void levelName;
    }

    validateLevelData(levelData, levelFilename) {
        const ok = levelData && typeof levelData === 'object' && !Array.isArray(levelData);
        const err = ok ? null : STRINGS.msg.levelInvalid;
        this.emitRuntimeEvent('schemaValidated', { id: createRuntimeId('schema'), ok, target: levelFilename || 'level', error: err });
        return err;
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

    normalizeMaterialData(data, geometryData = null) {
        const materialData = (data && typeof data === 'object') ? { ...data } : {};
        const hints = resolveAuthoringDecalPreset(materialData.decalPreset);
        if (!hints) return materialData;
        const [sx, , sz] = Array.isArray(geometryData?.scale) && geometryData.scale.length >= 3
            ? geometryData.scale
            : [1, 1, 1];
        materialData.transparent = true;
        materialData.depthWrite = false;
        materialData.polygonOffset = true;
        materialData.alphaToCoverage = true;
        materialData.alphaTest ??= hints.alphaTest;
        materialData.metalness ??= hints.metalness;
        materialData.roughnessFactor ??= hints.roughnessFactor;
        materialData.emissiveIntensity ??= hints.emissiveIntensity;
        if (!materialData.uvScale && hints.uvScalePerMeterU > 0 && hints.uvScalePerMeterV > 0) {
            materialData.uvScale = [
                Math.max(1, Math.round(Math.abs(sx) * hints.uvScalePerMeterU)),
                Math.max(1, Math.round(Math.abs(sz) * hints.uvScalePerMeterV))
            ];
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
        return typeof data.type === 'string' ? data.type.toLowerCase() : null;
    }

    createConvexHullPrimitiveGeometry(data) {
        // Native (Raw Iron) compiles real convex hulls; the JS shell only renders an AABB stand-in.
        const points = Array.isArray(data?.points) && data.points.length >= 4
            ? data.points.map((point) => finiteVec3Components(point, [0, 0, 0]))
            : [[-0.5, -0.5, -0.5], [0.5, -0.5, -0.5], [0.5, -0.5, 0.5], [-0.5, -0.5, 0.5], [0, 0.5, 0]];
        let minX = Infinity, minY = Infinity, minZ = Infinity;
        let maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;
        for (const [x, y, z] of points) {
            if (x < minX) minX = x; if (y < minY) minY = y; if (z < minZ) minZ = z;
            if (x > maxX) maxX = x; if (y > maxY) maxY = y; if (z > maxZ) maxZ = z;
        }
        const sx = Math.max(0.01, maxX - minX);
        const sy = Math.max(0.01, maxY - minY);
        const sz = Math.max(0.01, maxZ - minZ);
        const geometry = new THREE.BoxGeometry(sx, sy, sz);
        geometry.translate((minX + maxX) * 0.5, (minY + maxY) * 0.5, (minZ + maxZ) * 0.5);
        geometry.center();
        return geometry.toNonIndexed();
    }

    /** Inline replacement for BufferGeometryUtils.mergeGeometries: concats non-indexed position+normal buffers. */
    mergeStructuralPrimitiveGeometryParts(parts, fallbackFactory = () => new THREE.BoxGeometry(1, 1, 1)) {
        const validParts = parts.filter(Boolean);
        if (validParts.length === 0) return fallbackFactory();
        if (validParts.length === 1) return validParts[0];
        const flattened = validParts.map((part) => (part.index ? part.toNonIndexed() : part));
        let totalVerts = 0;
        for (const part of flattened) {
            const positionAttr = part.getAttribute('position');
            if (!positionAttr) {
                console.warn('Structural part missing position attribute; falling back to a box.');
                validParts.forEach((part) => part.dispose());
                flattened.forEach((part) => part.dispose());
                return fallbackFactory();
            }
            totalVerts += positionAttr.count;
        }
        const positions = new Float32Array(totalVerts * 3);
        let offset = 0;
        for (const part of flattened) {
            const arr = part.getAttribute('position').array;
            positions.set(arr, offset);
            offset += arr.length;
        }
        const merged = new THREE.BufferGeometry();
        merged.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        merged.computeVertexNormals();
        validParts.forEach((part) => part.dispose());
        flattened.forEach((part) => { if (!validParts.includes(part)) part.dispose(); });
        return merged;
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
        const r = THREE.MathUtils.clamp(ridgeRatio * 0.5, 0.04, 0.42);
        const verts = [
            -0.5, -0.5, -0.5,   0.5, -0.5, -0.5,   0.5, -0.5,  0.5,  -0.5, -0.5,  0.5,
            -0.5,  0.0, -0.5,   0.5,  0.0, -0.5,   0.5,  0.0,  0.5,  -0.5,  0.0,  0.5,
             0.0,  0.5, -r,     0.0,  0.5,  r
        ];
        // 16 outward-facing triangles: bottom (2) + 4 walls (8) + 2 hip ends (2) + 2 hip sides (4)
        const triangles = [
            0, 1, 2,  0, 2, 3,                          // bottom (-Y)
            0, 4, 5,  0, 5, 1,                          // south wall (-Z)
            3, 6, 7,  3, 2, 6,                          // north wall (+Z)
            0, 3, 7,  0, 7, 4,                          // west wall (-X)
            1, 5, 6,  1, 6, 2,                          // east wall (+X)
            4, 8, 5,                                    // south hip end (-Z, slope)
            7, 6, 9,                                    // north hip end (+Z, slope)
            4, 7, 9,  4, 9, 8,                          // west hip slope (-X)
            5, 9, 6,  5, 8, 9                           // east hip slope (+X)
        ];
        const positions = new Float32Array(triangles.length * 3);
        for (let i = 0; i < triangles.length; i++) {
            const v = triangles[i] * 3;
            positions[i * 3 + 0] = verts[v + 0];
            positions[i * 3 + 1] = verts[v + 1];
            positions[i * 3 + 2] = verts[v + 2];
        }
        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.computeVertexNormals();
        geometry.center();
        return geometry;
    }

    createRoofPrimitiveGeometry(data) {
        const resolvedType = this.getStructuralPrimitiveType(data);
        return resolvedType === 'hipped_roof'
            ? this.createHippedRoofPrimitiveGeometry(data)
            : this.createGableRoofPrimitiveGeometry(data);
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
        if (data?.compiledGeometry?.isBufferGeometry) return data.compiledGeometry;
        const t = this.getStructuralPrimitiveType(data);
        const s = this;
        const unitBox = () => new THREE.BoxGeometry(1, 1, 1);
        const makers = {
            box: unitBox,
            rounded_box: unitBox,
            plane: () => new THREE.PlaneGeometry(1, 1),
            ramp: () => s.createRampPrimitiveGeometry(),
            wedge: () => s.createWedgePrimitiveGeometry(),
            cylinder: () => s.createCylinderPrimitiveGeometry(data),
            arch: () => s.createArchPrimitiveGeometry(data),
            cone: () => s.createConePrimitiveGeometry(data),
            pyramid: () => s.createConePrimitiveGeometry(data),
            hexahedron: () => s.createHexahedronPrimitiveGeometry(data),
            convex_hull: () => s.createConvexHullPrimitiveGeometry(data),
            capsule: () => s.createCapsulePrimitiveGeometry(data),
            frustum: () => s.createFrustumPrimitiveGeometry(data),
            hollow_box: () => s.createHollowBoxPrimitiveGeometry(data),
            roof_gable: () => s.createRoofPrimitiveGeometry(data),
            hipped_roof: () => s.createRoofPrimitiveGeometry(data)
        };
        const mk = makers[t];
        return mk ? mk() : null;
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
        const archetype = this.resolveNpcArchetype(data, data.modelName, sourceModel.userData?.npcArchetype || '');
        instance.userData.npcArchetype = archetype;
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
        return !!this.player.traceHullBox(feetPosition, this.player.withSupportSurfaceFilter({}, feetPosition.y));
    }

    snapSpawnCandidateToGround(candidate) {
        const supportHit = this.player.probeGroundAtFeet(
            candidate,
            this.player.maxStepHeight + this.player.groundSnapDistance + 0.25,
            0.12
        );
        if (supportHit?.point) {
            candidate.y = supportHit.point.y + this.player.groundContactOffset;
            this.player.setFeetPosition(candidate);
        }
    }

    placePlayerAtSpawn(position, data = null) {
        const candidate = new THREE.Vector3();
        const spawnOffsets = [
            [0, 0, 0],
            [0.75, 0, 0], [-0.75, 0, 0], [0, 0, 0.75], [0, 0, -0.75],
            [1.5, 0, 0], [-1.5, 0, 0], [0, 0, 1.5], [0, 0, -1.5],
            [0, 0.35, 0], [0, 1.05, 0], [0, 2.1, 0], [0, 3.5, 0]
        ];
        this.player.targetHeight = this.player.eyeHeights[this.player.stance];

        for (const [x, y, z] of spawnOffsets) {
            candidate.copy(position).add(this._toObject.set(x, y, z));
            this.player.setFeetPosition(candidate);
            if (this.isPlayerSpawnBlocked(candidate)) continue;
            this.snapSpawnCandidateToGround(candidate);
            this.applyPlayerSpawnOrientation(data, candidate);
            return;
        }
        this.player.setFeetPosition(position.clone());
        if (this.player.forceUnstuckRecovery(position.clone())) {
            this.applyPlayerSpawnOrientation(data, this.player.feetPosition.clone());
            return;
        }
        this.applyPlayerSpawnOrientation(data, position);
    }

    finalizePlayerSpawnGrounding() {
        const groundHit = this.player.probeGroundAtFeet(
            this.player.feetPosition,
            this.player.groundSnapDistance + this.player.maxStepHeight + 0.35,
            this.player.getGroundProbeAllowAbove()
        );
        if (!groundHit?.point) {
            this.player.onGround = false;
            return;
        }
        const clearance = Number.isFinite(groundHit.clearance)
            ? groundHit.clearance
            : Math.max(0, this.player.feetPosition.y - groundHit.point.y);
        if (clearance <= Math.max(this.player.groundSnapDistance, this.player.groundContactOffset + 0.02)) {
            this.player.feetPosition.y = groundHit.point.y + this.player.groundContactOffset;
            this.player.syncCameraToBody();
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
                this.updateSoundscape(delta);
                this.updateHUD(delta);
            } else {
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
        if (this.animationFrameId) {
            cancelAnimationFrame(this.animationFrameId);
            this.animationFrameId = null;
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

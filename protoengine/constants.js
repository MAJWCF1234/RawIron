/**
 * Centralized UI and game strings (locale-ready).
 * Use STRINGS.msg.key or STRINGS.ui.key in code.
 */
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
        unstuckFailed: "UNSTUCK RECOVERY FAILED",
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
        copyFailed: "Copy failed",
    },
};

/** Fixed default key bindings for the legacy prototype shell. */
export const DEFAULT_KEYS = {
    forward: 'KeyW',
    backward: 'KeyS',
    left: 'KeyA',
    right: 'KeyD',
    sprint: 'ShiftLeft',
    jump: 'Space',
    unstuck: 'KeyR',
    crouch: 'KeyC',
    prone: 'KeyZ',
};

// Keyboard code labels: `engine/inputUtils.js` (`FormatInputLabelFromInputId`).

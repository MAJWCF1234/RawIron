import test from 'node:test';
import assert from 'node:assert/strict';
import {
    buildExternalModelCandidateTypes,
    buildRenderGameStateSnapshot,
    buildStructuralDependencyGraph
} from './engine.js';

test('buildRenderGameStateSnapshot exposes new runtime primitive counts', () => {
    const snapshot = buildRenderGameStateSnapshot({
        currentLevelFilename: 'dev_level.json',
        gameState: { isPaused: false, objective: null },
        player: null,
        getRuntimeTuningSnapshot: () => ({}),
        collidableMeshes: [],
        structuralCollidableMeshes: [],
        bvhMeshCount: 0,
        triggerVolumes: [],
        recursiveFractalPrimitives: [{ id: 'rf_1' }],
        convexHullAggregates: [{ id: 'hull_1' }, { id: 'hull_2' }],
        decalProjectors: [{ id: 'decal_1' }],
        activePostProcessState: null,
        activeAudioEnvironmentState: null
    });

    assert.equal(snapshot.counts.recursiveFractals, 1);
    assert.equal(snapshot.counts.convexHullAggregates, 2);
    assert.equal(snapshot.counts.decalProjectors, 1);
});

test('buildExternalModelCandidateTypes prioritizes explicit type and de-duplicates fallbacks', () => {
    const result = buildExternalModelCandidateTypes({
        type: 'GLTF',
        path: './models/worker.glb',
        fallbackTypes: ['fbx', 'obj', 'gltf', 'fbx']
    });
    assert.deepEqual(result, ['gltf', 'fbx', 'obj', 'glb']);
});

test('buildExternalModelCandidateTypes can infer extension without type', () => {
    const result = buildExternalModelCandidateTypes({
        path: './models/worker.fbx'
    });
    assert.deepEqual(result, ['fbx']);
});

test('buildStructuralDependencyGraph treats recursive nodes as frame phase', () => {
    const nodes = [
        { id: 'core_a', type: 'box' },
        { id: 'fractal_runtime', type: 'recursive_fractal_primitive', targetIds: ['core_a'] },
        { id: 'instanced_runtime', type: 'instanced_recursive_geometry_node', targetIds: ['core_a'] }
    ];
    const result = buildStructuralDependencyGraph(nodes);
    assert.equal(result.summary.phaseBuckets.frame, 2);
    assert.equal(result.summary.unresolvedDependencyCount, 0);
    assert.equal(result.orderedNodes.length, 3);
});

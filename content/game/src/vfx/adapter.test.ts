import { describe, expect, it } from 'vitest';
import * as THREE from 'three';
import { EmberVfxAdapter } from './adapter';
import { activeVfxCount, configureVfx, disposeVfx, playSpell, updateVfx } from './index';

describe('preserved Ember VFX boundary', () => {
  it('maps Z elevation to Y and Ember negative Y to Engine positive Z, reversibly', () => {
    const scene = new THREE.Scene(); const adapter = new EmberVfxAdapter(scene);
    const authored = new THREE.Vector3(3, -4, 2.65);
    adapter.root.updateMatrixWorld(true);
    const world = authored.clone().applyMatrix4(adapter.root.matrixWorld);
    expect(world.x).toBeCloseTo(3); expect(world.y).toBeCloseTo(2.65); expect(world.z).toBeCloseTo(4);
    expect(adapter.toEmber(world).distanceTo(authored)).toBeLessThan(1e-12);
    adapter.dispose(); expect(scene.children).toHaveLength(0);
  });
  it('converts scale through the same adapter and keeps a human 2.65 units tall', () => {
    const scene = new THREE.Scene(); const adapter = new EmberVfxAdapter(scene, 64);
    adapter.root.updateMatrixWorld(true);
    const world = new THREE.Vector3(64, 0, 64 * 2.65).applyMatrix4(adapter.root.matrixWorld);
    expect(world.x).toBeCloseTo(1); expect(world.y).toBeCloseTo(2.65);
    expect(adapter.toEmber(world).z).toBeCloseTo(169.6); adapter.dispose();
  });
  it('returns local camera coordinates with projection and reuses its camera object', () => {
    const scene = new THREE.Scene(); const adapter = new EmberVfxAdapter(scene);
    const camera = new THREE.PerspectiveCamera(35, 1.5, 0.1, 80); camera.position.set(7, 5, 9); camera.lookAt(0, 0, 0);
    const local = adapter.cameraToEmber(camera);
    expect(local.position.distanceTo(adapter.toEmber(camera.position))).toBeLessThan(1e-10);
    expect(adapter.cameraToEmber(camera)).toBe(local);
    const projected = new THREE.Vector3(2, 1, -3).project(camera);
    const localProjected = adapter.toEmber(new THREE.Vector3(2, 1, -3)).project(local);
    expect(localProjected.distanceTo(projected)).toBeLessThan(1e-10); adapter.dispose();
  });
  it('cancels a cast without retaining its adapter or visual promise', async () => {
    const scene = new THREE.Scene(); configureVfx(scene, { manualUpdate: true });
    const cast = playSpell('cleave', new THREE.Vector3(), [new THREE.Vector3(1, 0, 0)], scene);
    expect(activeVfxCount(scene)).toBe(1); disposeVfx(scene); await cast;
    expect(activeVfxCount(scene)).toBe(0); expect(scene.children).toHaveLength(0);
  });
  it('explicitly rejects spells without a corresponding original 3D renderer', async () => {
    // every real spell has an effect now (createFoodAndWater plays the holy flipbook); only an
    // unknown kind is rejected
    await expect(playSpell('notARealSpell' as Parameters<typeof playSpell>[0], new THREE.Vector3(), [], new THREE.Scene())).rejects.toThrow('No preserved 3D effect');
  });
  it('uses the distinct original Cleave and Sweep per-target arrival formulas', async () => {
    for (const [kind, beforeFrames] of [['cleave', 15], ['sweep', 9]] as const) {
      const scene = new THREE.Scene(); const impacts: string[] = [];
      configureVfx(scene, { manualUpdate: true, onImpact: event => impacts.push(event.targetId ?? '') });
      const cast = playSpell(kind, new THREE.Vector3(), [{ id: 'unit', position: new THREE.Vector3(1, 0, 0) }], scene);
      for (let frame = 0; frame < beforeFrames; frame++) updateVfx(scene, 1 / 30);
      expect(impacts).toEqual([]); updateVfx(scene, 1 / 30); expect(impacts).toEqual(['unit']);
      disposeVfx(scene); await cast;
    }
  });
});

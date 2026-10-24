import * as THREE from 'three';

/** All Ember effects stay in their original coordinate frame. Only this boundary converts
 * XY ground / Z elevation to Engine2 XZ ground / Y elevation. Rotation is right-handed;
 * Ember negative Y maps to Engine positive Z. One authored hex radius = one Engine2 unit. */
export class EmberVfxAdapter {
  readonly root = new THREE.Scene();
  readonly scale: number;
  private localCamera: THREE.Camera | undefined;
  private sourceCamera: THREE.Camera | undefined;
  private readonly inverseRoot = new THREE.Matrix4();
  constructor(readonly scene: THREE.Scene, authoredHexRadius = 1) {
    this.scale = 1 / authoredHexRadius;
    this.root.name = 'Ember preserved VFX coordinate adapter';
    this.root.rotation.x = -Math.PI / 2;
    this.root.scale.setScalar(this.scale);
    scene.add(this.root);
  }
  toEmber(point: THREE.Vector3): THREE.Vector3 {
    return new THREE.Vector3(point.x, -point.z, point.y).multiplyScalar(1 / this.scale);
  }
  cameraToEmber(camera: THREE.Camera): THREE.Camera {
    this.root.updateWorldMatrix(true, false);
    camera.updateWorldMatrix(true, false);
    if (this.sourceCamera !== camera) { this.sourceCamera = camera; this.localCamera = camera.clone(); }
    const local = this.localCamera!;
    local.projectionMatrix.copy(camera.projectionMatrix);
    local.projectionMatrixInverse.copy(camera.projectionMatrixInverse);
    local.matrixAutoUpdate = false;
    this.inverseRoot.copy(this.root.matrixWorld).invert();
    local.matrixWorld.multiplyMatrices(this.inverseRoot, camera.matrixWorld);
    local.matrixWorld.decompose(local.position, local.quaternion, local.scale);
    local.matrixWorldInverse.copy(local.matrixWorld).invert();
    return local;
  }
  dispose(): void { this.root.removeFromParent(); }
}

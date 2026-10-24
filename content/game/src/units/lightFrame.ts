import type { Group } from 'three';

/** Unit light footprints stay world-fixed under the billboard: undo the card's camera turn, mirror
 * and lean. Titan's multi-hex footprint too — turning with the card slid its red light off its
 * hexes as the view turned. */
export function alignUnitLights(lights: Group, _classId: string, cameraYaw: number, mirror: boolean, lean: number, ground: number, spriteY: number): void {
  lights.scale.x = mirror ? -1 : 1;
  lights.rotation.set(lean, mirror ? cameraYaw : -cameraYaw, 0, 'XYZ');
  lights.position.set(0, (ground - spriteY) * Math.cos(lean), (ground - spriteY) * Math.sin(lean));
}

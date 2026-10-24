export interface PartyCameraPoint { x: number; y: number; z: number }

/** A local opening view around actual party positions, independent of the map's void area. */
export function openingPartyCamera(points: readonly PartyCameraPoint[], aspect: number, yaw: number, pitch: number): { target: PartyCameraPoint; distance: number } | null {
  if (!points.length) return null;
  const target = {
    x: points.reduce((sum,p) => sum + p.x,0) / points.length,
    y: points.reduce((sum,p) => sum + p.y,0) / points.length + 1.2,
    z: points.reduce((sum,p) => sum + p.z,0) / points.length,
  };
  const tan = Math.tan(Math.PI / 12); // the normal camera's 30-degree field of view
  let distance = 20;
  for (const p of points) {
    const dx = p.x - target.x, dz = p.z - target.z;
    const right = dx * Math.cos(yaw) - dz * Math.sin(yaw);
    const depth = dx * Math.sin(yaw) + dz * Math.cos(yaw);
    const vertical = (p.y + 1.2 - target.y) * Math.cos(pitch) - depth * Math.sin(pitch);
    const toward = depth * Math.cos(pitch);
    distance = Math.max(distance, toward + (Math.abs(right) + 2.2) / (tan * Math.max(0.25,aspect)), toward + (Math.abs(vertical) + 2.4) / tan);
  }
  return { target, distance };
}

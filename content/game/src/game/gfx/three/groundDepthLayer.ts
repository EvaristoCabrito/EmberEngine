import type { Material } from "three";

/** In the normal sprite view, elevated ground must remain behind every unit and prop.
 * Reserve a far depth band for terrain, preserving its internal depth order and projected
 * elevation. The spatial tactics view continues using physical depth. */
export function groundDepthLayer(material: Material, enabled: { value: number }): void {
  const compile = material.onBeforeCompile;
  const cacheKey = material.customProgramCacheKey.bind(material);
  material.onBeforeCompile = function (shader, renderer) {
    compile.call(this, shader, renderer);
    shader.uniforms.groundDepthLayerEnabled = enabled;
    shader.vertexShader = "uniform float groundDepthLayerEnabled;\n" + shader.vertexShader;
    shader.vertexShader = shader.vertexShader.replace("#include <project_vertex>", `#include <project_vertex>
      if (groundDepthLayerEnabled > 0.5) {
        float terrainDepth = gl_Position.z / gl_Position.w;
        gl_Position.z = (0.95 + terrainDepth * 0.04) * gl_Position.w;
      }
    `);
  };
  material.customProgramCacheKey = () => cacheKey() + ":ground-depth-layer-v1";
  material.needsUpdate = true;
}

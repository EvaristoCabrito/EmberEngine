// ACES filmic as three.js fits it (Stephen Hill's RRT + ODT fit, input scaled by 1 / 0.6): shared by the tonemap pass
// and the bloom's bright pass, so both judge "bright" on the same display values.
#ifndef EMBER_ACES_HLSLI
#define EMBER_ACES_HLSLI

float3 rrtAndOdtFit(float3 v) {
  const float3 a = v * (v + 0.0245786) - 0.000090537;
  const float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
  return a / b;
}

float3 acesFilmic(float3 color) {
  // sRGB -> AP1-ish input and back (three.js ACESInputMat / ACESOutputMat, column-major there).
  const float3x3 inputMat = float3x3(0.59719, 0.35458, 0.04823, 0.07600, 0.90834, 0.01566, 0.02840, 0.13383, 0.83777);
  const float3x3 outputMat = float3x3(1.60475, -0.53108, -0.07367, -0.10208, 1.10813, -0.00605, -0.00327, -0.07276, 1.07602);
  color = color / 0.6;
  color = mul(inputMat, color);
  color = rrtAndOdtFit(color);
  color = mul(outputMat, color);
  return saturate(color);
}

#endif

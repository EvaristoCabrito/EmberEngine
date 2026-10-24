// Draw order shared by the 3D map's layers (three.js renderOrder; higher draws later).
//
// Ember's mist sheets (ThreeAtmosphere: Névoa 2/3/4 at 1.5-1.7) draw by order with no depth
// test: over the ground, then the unit and decoration cards over them, so a flat mist sheet
// never slices a standing card in two. Terrain fog draws after the grid (6), before cards (7).
// Cards already receive the battle's visibility/shading rules. Drawing them after terrain fog
// prevents their translucent edge pixels writing depth that punches bright holes in the fog.
export const FOG_RENDER_ORDER = 6.5;
export const CARD_RENDER_ORDER = 7;
/** Units draw after every prop card: their soft edges then blend over what is really behind them.
 * At the same order, centre-distance sorting could draw a wall or cart card after the unit, and
 * the unit's see-through edge pixels (already in the depth buffer) blocked it: the ground showed
 * through as a green/red outline around the figure. */
export const UNIT_RENDER_ORDER = 7.5;

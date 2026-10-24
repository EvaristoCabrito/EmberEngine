const d=require('../../../content/maps/ember/wisp-forest-crossing018.json').draft;
const candidates=[[14,10],[18,8],[24,8],[29,12],[32,16],[30,24],[14,26],[27,29],[18,12],[24,12],[31,17],[29,24],[16,27],[23,30],[30,20],[15,14]];
const occupied=new Set(d.decorations.map(x=>`${x.x},${x.y}`));const units=[...d.playerSpawns,...d.enemySpawns,...d.neutralSpawns].map(x=>`${x.x},${x.y}`);
for(const [x,y] of candidates){const i=y*d.cols+x;console.log(`${x},${y} ${d.tiles[i]} decor=${occupied.has(`${x},${y}`)} unit=${units.includes(`${x},${y}`)}`)}

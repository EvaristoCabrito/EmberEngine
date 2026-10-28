const fs=require('fs');
const inPath='content/maps/ember/wisp-forest-crossing017.json', outPath='content/maps/ember/wisp-forest-crossing018.json';
if(fs.existsSync(outPath)) throw Error('Refusing to overwrite '+outPath);
const source=JSON.parse(fs.readFileSync(inPath,'utf8')); const draft=JSON.parse(JSON.stringify(source.draft));
const newTreeAssets=['ancient-dead-oak','weeping-willow','lightning-split','hollow-sentinel'].map(x=>`ember-wisp-crossing-${x}-v02`);
const old3d=draft.decorations.filter(d=>d.id.startsWith('ember-wisp-revisited-') && /-(weeping-willow|giant-dead-oak|hollow-snag|uprooted-leaning|lightning-split|spiral-twisted|tall-burnt-pine|broken-crown|root-arch)-v02$/.test(d.id));
const old2d=draft.decorations.filter(d=>['dead-tree','wilds-dead-oak','wilds-twisted-tree'].includes(d.id));
if(old3d.length!==9||old2d.length!==15) throw Error(`unexpected tree layout old3d=${old3d.length} old2d=${old2d.length}`);
const treePoints=[...old3d,...old2d].map(d=>({x:d.x,y:d.y}));
const replaced=new Set([...old3d,...old2d]);
const clutter=new Set(['wilds-dry-twig-pile','wilds-dry-bush','wilds-bare-branch','wilds-mossy-stones','wilds-root-tangle']);
const before=draft.decorations.length;
draft.decorations=draft.decorations.filter(d=>!replaced.has(d)&&!clutter.has(d.id));
for(let i=0;i<treePoints.length;i++){
 const p=treePoints[i], id=newTreeAssets[i%newTreeAssets.length], idx=p.y*draft.cols+p.x, tile=draft.tiles[idx];
 if(!['plains','woods'].includes(tile)) throw Error(`tree on invalid terrain ${p.x},${p.y}: ${tile}`);
 draft.decorations.push({id,x:p.x,y:p.y,rot:i%6,blocksPath:true});
}
let groundTiles=0;
for(let i=0;i<draft.tiles.length;i++) if(['plains','woods'].includes(draft.tiles[i])&&draft.tileVariants[i]===62){draft.tileVariants[i]=65;groundTiles++;}
source.serial+=1; source.savedAt=Date.now(); source.draft=draft;
fs.writeFileSync(outPath,JSON.stringify(source,null,2)+'\n');
const campaignPath='content/data/campaign.json', campaign=JSON.parse(fs.readFileSync(campaignPath,'utf8'));
const mission=campaign.missions.find(x=>x.id==='wisp-forest-crossing'); if(!mission) throw Error('campaign mission missing');
mission.mapFile='wisp-forest-crossing018.json'; fs.writeFileSync(campaignPath,JSON.stringify(campaign,null,2)+'\n');
console.log(JSON.stringify({from:inPath,to:outPath,serial:source.serial,replacedTrees:old3d.length+old2d.length,removedClutter:before-(draft.decorations.length+treePoints.length),new3dTrees:treePoints.length,decorations:draft.decorations.length,groundCellsUpdated:groundTiles,players:draft.playerSpawns.length,enemies:draft.enemySpawns.length,fog:draft.fog,mission:mission.mapFile}));

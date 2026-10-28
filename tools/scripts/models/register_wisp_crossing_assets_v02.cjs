const fs = require('fs');
const path = require('path');
const root = process.cwd();
const assets = [
  ['ancient-dead-oak','Carvalho morto ancestral 3D · Wisp Crossing'],
  ['weeping-willow','Salgueiro morto 3D · Wisp Crossing'],
  ['lightning-split','Árvore partida por raio 3D · Wisp Crossing'],
  ['hollow-sentinel','Árvore sentinela oca 3D · Wisp Crossing'],
];
const kit = path.join(root,'content/models/wisp-crossing-trees');
function glbInfo(p) {
  const b=fs.readFileSync(p); if(b.toString('ascii',0,4)!=='glTF') throw Error(`bad GLB ${p}`);
  const len=b.readUInt32LE(12), j=JSON.parse(b.toString('utf8',20,20+len));
  const poss=(j.accessors||[]).filter(a=>a.type==='VEC3'&&a.min&&a.max); if(!poss.length) throw Error(`missing bounds ${p}`);
  const low=[0,1,2].map(i=>Math.min(...poss.map(a=>a.min[i]))), high=[0,1,2].map(i=>Math.max(...poss.map(a=>a.max[i]))), dims=high.map((v,i)=>v-low[i]);
  const acc=j.accessors||[]; const triangles=(j.meshes||[]).reduce((sum,m)=>sum+(m.primitives||[]).reduce((n,p)=>n+((p.mode??4)===4?(p.indices!=null?acc[p.indices].count/3:Math.max(0,acc[p.attributes.POSITION].count-2)):0),0),0);
  return {width:+Math.max(dims[0],dims[2]).toFixed(3),height:+dims[1].toFixed(3),triangles:Math.round(triangles)};
}
function readJson(p){return JSON.parse(fs.readFileSync(p,'utf8'));}
function writeJson(p,v,compact=false){fs.writeFileSync(p,JSON.stringify(v,null,compact?0:2)+'\n','utf8');}
const modelDb=readJson('content/models/models.json'), catalog=readJson('content/data/editor-catalog.json');
catalog.editor.decoMeta??={}; catalog.editor.decorationInfo??={}; catalog.editor.architectureOrder??=[];
for(const [asset,name] of assets){
  const file=`${asset}-v02.glb`, glb=path.join(kit,file); if(!fs.existsSync(glb)) throw Error(`missing ${glb}`);
  const key=`wisp-crossing-${asset}-v02`, id=`ember-wisp-crossing-${asset}-v02`, modelPath=`/game/ember/wisp-crossing-trees/${file}`, size=glbInfo(glb);
  const entry={id,name,kit:'wisp-crossing-trees',model:key,file:modelPath,height:size.height,width:size.width,footprint:'one',section:'Natureza · Wisp Forest',blocks:true,triangles:size.triangles};
  let old=modelDb.models.find(x=>x.id===id); if(old&&JSON.stringify(old)!==JSON.stringify(entry)) throw Error(`existing conflict: ${id}`); if(!old) modelDb.models.push(entry);
  const deco={name,footprint:[{dx:0,dy:0}],blockingFootprint:null,architectureSpan:false,architecture:false,model3d:'tree',model:{model:key,file:modelPath,height:size.height},mirrorAlternate:false,exitKind:null,size:{w:size.width,h:size.height},light:null,standOn:false,lockYaw:false,nudge:0};
  old=catalog.decorations[id]; if(old&&JSON.stringify(old)!==JSON.stringify(deco)) throw Error(`catalog conflict: ${id}`); if(!old) catalog.decorations[id]=deco;
  const meta={model3d:'tree',exitKind:null,props3d:true,rockStyle:false,thickWall:false,doorStyle:null,wallTexture:null,solidHouse:false,solidCart:false,solidRock:false,chest:false,manual:true,art:''};
  const info={section:'Natureza · Wisp Forest',tile:null,thumb:'',blocksByDefault:true};
  for(const [key0,val] of [['decoMeta',meta],['decorationInfo',info]]) { const ex=catalog.editor[key0][id]; if(ex&&JSON.stringify(ex)!==JSON.stringify(val)) throw Error(`catalog ${key0} conflict: ${id}`); if(!ex) catalog.editor[key0][id]=val; }
  if(!catalog.editor.architectureOrder.includes(id)) catalog.editor.architectureOrder.push(id);
  for(const dst of ['content/assets/game/models/wisp-crossing-trees','content/assets/game/ember/wisp-crossing-trees']){
    fs.mkdirSync(dst,{recursive:true}); const out=path.join(dst,file); if(fs.existsSync(out)){if(!fs.readFileSync(out).equals(fs.readFileSync(glb))) throw Error(`preserving conflicting file: ${out}`);} else fs.copyFileSync(glb,out);
  }
  const modelEntry=`  '${key}': { file: 'wisp-crossing-trees/${file}', height: ${size.height} },`;
  const modelReg='content/game/src/world/models.ts'; let source=fs.readFileSync(modelReg,'utf8');
  const marker="  'wisp-revisited-weeping-willow-v02':"; if(!source.includes(`'${key}':`)){if(!source.includes(marker)) throw Error('world model insertion marker missing'); source=source.replace(marker,modelEntry+'\n'+marker); fs.writeFileSync(modelReg,source);}
  for(const f of ['content/game/src/ember/data.ts','content/game/src/game/data.ts']){
    source=fs.readFileSync(f,'utf8'); if(source.includes(`"${id}":`)) continue;
    const marker2='  "ember-wisp-revisited-broken-stump":'; if(!source.includes(marker2)) throw Error(`data insertion marker missing ${f}`);
    source=source.replace(marker2,`  "${id}": { id: "${id}", name: "${name}", footprint: DECO_ONE, model3d: "tree", treeModel: "${key}" },\n`+marker2); fs.writeFileSync(f,source);
  }
}
writeJson('content/models/models.json',modelDb);
writeJson('content/data/editor-catalog.json',catalog,true);
for(const f of ['content/game/src/ember/types.ts','content/game/src/game/types.ts']){
 let s=fs.readFileSync(f,'utf8');
 for(const [asset] of assets){const key=`wisp-crossing-${asset}-v02`; if(s.includes(`| "${key}"`)) continue; const marker='| "wisp-revisited-root-arch-v02"'; if(!s.includes(marker)) throw Error(`type marker missing ${f}`); s=s.replace(marker,marker+` | "${key}"`);}
 fs.writeFileSync(f,s);
}
const variants='content/game/src/ember/tileVariants.ts'; let vt=fs.readFileSync(variants,'utf8');
if(!vt.includes("key: 'variant-wisp-crossing-ground-plains-v02'")){
 vt=vt.replace('  woods: 65,','  woods: 66,').replace('  plains: 65,','  plains: 66,');
 const marker='  // Crossing uses the same authored forest floor across open clearings without changing walk terrain.';
 if(!vt.includes(marker)) throw Error('variant insertion marker missing');
 const entries="  { key: 'variant-wisp-crossing-ground-plains-v02', terrain: 'plains', variant: 65, file: 'wisp-crossing-ground-v02.png', label: 'Wisp Crossing · Chão da arte original', relief: 0.035 },\n  { key: 'variant-wisp-crossing-ground-woods-v02', terrain: 'woods', variant: 65, file: 'wisp-crossing-ground-v02.png', label: 'Wisp Crossing · Chão da arte original', relief: 0.035 },\n";
 vt=vt.replace(marker,entries+marker); fs.writeFileSync(variants,vt);
}
const assetsFile='content/game/src/game/assets.ts'; let as=fs.readFileSync(assetsFile,'utf8'); as=as.replace('"wisp-forest-crossing": await loadImage("/game/assets/wisp-forest-bg.jpg")','"wisp-forest-crossing": await loadImage("/game/assets/wisp-forest-crossing-panorama-v01.png")'); fs.writeFileSync(assetsFile,as);
const bg='content/game/src/editor/mapBackgrounds.ts'; let bs=fs.readFileSync(bg,'utf8');
bs=bs.replace("  { id: 'scene-wisp-forest', label: 'Floresta dos Wisps', file: 'wisp-forest-bg.jpg', kind: 'battle', mapIds: ['wisp-forest', 'wisp-forest-2', 'wisp-forest-crossing', 'wisp-forest-crossing-boss'] },", "  { id: 'scene-wisp-forest', label: 'Floresta dos Wisps', file: 'wisp-forest-bg.jpg', kind: 'battle', mapIds: ['wisp-forest', 'wisp-forest-2', 'wisp-forest-crossing-boss'] },\n  { id: 'scene-wisp-crossing-panorama', label: 'Travessia da Floresta dos Wisps · Panorâmica', file: 'wisp-forest-crossing-panorama-v01.png', kind: 'battle', mapIds: ['wisp-forest-crossing'] },"); fs.writeFileSync(bg,bs);
console.log('Registered 4 versioned crossing trees, ground variant 65, and crossing-only panorama routing.');

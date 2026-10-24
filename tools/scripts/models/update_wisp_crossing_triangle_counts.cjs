const fs=require('fs'),path=require('path');
const mf='content/models/models.json',db=JSON.parse(fs.readFileSync(mf,'utf8'));
for(const m of db.models.filter(x=>x.kit==='wisp-crossing-trees')){
 const f=path.join('content/models/wisp-crossing-trees',path.basename(m.file)); const b=fs.readFileSync(f),n=b.readUInt32LE(12),g=JSON.parse(b.toString('utf8',20,20+n)),a=g.accessors||[];
 m.triangles=(g.meshes||[]).reduce((sum,mesh)=>sum+(mesh.primitives||[]).reduce((n,p)=>n+((p.mode??4)===4?(p.indices!=null?a[p.indices].count/3:Math.max(0,a[p.attributes.POSITION].count-2)):0),0),0);
}
fs.writeFileSync(mf,JSON.stringify(db,null,2)+'\n');
console.log(db.models.filter(x=>x.kit==='wisp-crossing-trees').map(x=>`${x.id}: ${x.triangles.toLocaleString()} tris`).join('\n'));

const fs=require('fs'), path=require('path'), sharp=require('sharp');
const root=process.cwd(); const src='C:/Users/evari/.codex/generated_images/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/exec-f3b5ee50-46d3-457b-8ba5-a17043a722ac.png';
const albedo=path.join(root,'content/game/public/game/ground-variant-tiles/wisp-crossing-ground-v02.png');
if(fs.existsSync(albedo)) throw Error('Refusing overwrite: '+albedo);
const targets=['plains','woods'].map(k=>path.join(root,'content/game/public/game/ground-variants-3d','variant-wisp-crossing-ground-'+k+'-v02'));
for(const d of targets) if(fs.existsSync(d)) throw Error('Refusing overwrite directory: '+d);
fs.copyFileSync(src,albedo);
(async()=>{
 const {data,info}=await sharp(albedo).resize(512,512).removeAlpha().raw().toBuffer({resolveWithObject:true});
 const lum=new Float32Array(info.width*info.height);
 for(let i=0;i<lum.length;i++){const k=i*info.channels;lum[i]=.2126*data[k]+.7152*data[k+1]+.0722*data[k+2];}
 const normal=Buffer.alloc(lum.length*3),rough=Buffer.alloc(lum.length);
 const at=(x,y)=>lum[(y&511)*512+(x&511)];
 for(let y=0;y<512;y++)for(let x=0;x<512;x++){
   const i=y*512+x, dx=(-at(x-1,y-1)-2*at(x-1,y)-at(x-1,y+1)+at(x+1,y-1)+2*at(x+1,y)+at(x+1,y+1))/8;
   const dy=(-at(x-1,y-1)-2*at(x,y-1)-at(x+1,y-1)+at(x-1,y+1)+2*at(x,y+1)+at(x+1,y+1))/8;
   const nx=-dx*0.045,ny=-dy*0.045,nz=1,len=Math.hypot(nx,ny,nz),o=i*3;
   normal[o]=Math.round((nx/len*.5+.5)*255);normal[o+1]=Math.round((ny/len*.5+.5)*255);normal[o+2]=Math.round((nz/len*.5+.5)*255);
   rough[i]=Math.max(188,Math.min(248,Math.round(226+(lum[i]-96)*.16)));
 }
 const reliefSize=64, small=await sharp(albedo).resize(reliefSize,reliefSize).removeAlpha().raw().toBuffer({resolveWithObject:true});
 const values=[]; for(let i=0;i<reliefSize*reliefSize;i++){const k=i*small.info.channels;values.push(Math.round(.2126*small.data[k]+.7152*small.data[k+1]+.0722*small.data[k+2]));}
 for(const d of targets){fs.mkdirSync(d,{recursive:true}); await sharp(normal,{raw:{width:512,height:512,channels:3}}).png().toFile(path.join(d,'normal.png')); await sharp(rough,{raw:{width:512,height:512,channels:1}}).png().toFile(path.join(d,'rough.png')); fs.writeFileSync(path.join(d,'relief.json'),JSON.stringify({size:reliefSize,values})+'\n');}
 const raw=await sharp(albedo).removeAlpha().raw().toBuffer({resolveWithObject:true}); const w=raw.info.width,h=raw.info.height,ch=raw.info.channels; let lr=0,tb=0,n=0;
 for(let y=0;y<h;y++)for(let c=0;c<ch;c++){lr+=Math.abs(raw.data[(y*w)*ch+c]-raw.data[(y*w+w-1)*ch+c]);n++;}
 for(let x=0;x<w;x++)for(let c=0;c<ch;c++){tb+=Math.abs(raw.data[x*ch+c]-raw.data[((h-1)*w+x)*ch+c]);}
 console.log(JSON.stringify({albedo:info,width:raw.info.width,height:raw.info.height,meanEdgeDeltaX:+(lr/n).toFixed(2),meanEdgeDeltaY:+(tb/(w*ch)).toFixed(2),bytes:fs.statSync(albedo).size}));
})().catch(e=>{console.error(e);process.exitCode=1});

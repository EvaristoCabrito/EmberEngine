const sharp=require('sharp'),fs=require('fs'),path=require('path');
const source='content/game/public/game/assets/wisp-forest-crossing-panorama-v01.png';
const maskSource='C:/Users/evari/.codex/generated_images/01a16c18-9fe6-7212-a2e8-e2e0c6a23af1/exec-ffe235b2-67cf-46fb-8693-b096887355b5.png';
const out='content/game/public/game/assets/wisp-forest-crossing-panorama-v04-alpha.png';
if(fs.existsSync(out))throw Error('Refusing overwrite: '+out);
(async()=>{
 const sm=await sharp(source).metadata(), mm=await sharp(maskSource).metadata();
 if(sm.width!==mm.width||sm.height!==mm.height)throw Error('mask dimensions do not match panorama');
 const alpha=await sharp(maskSource).ensureAlpha().extractChannel(3).raw().toBuffer();
 for(let i=0;i<alpha.length;i++) if(alpha[i]>=224) alpha[i]=255;
 const output=await sharp(source).joinChannel(alpha,{raw:{width:sm.width,height:sm.height,channels:1}}).png().toBuffer();
 fs.writeFileSync(out,output);
 for(const dest of ['content/game/public/game/map-backgrounds/wisp-forest-crossing-panorama-v04-alpha.png']){
  if(fs.existsSync(dest))throw Error('Refusing overwrite: '+dest);fs.copyFileSync(out,dest);
 }
 const {data,info}=await sharp(out).ensureAlpha().raw().toBuffer({resolveWithObject:true});
 let opaque=0,transparent=0,partial=0;for(let i=3;i<data.length;i+=4){if(data[i]===255)opaque++;else if(data[i]===0)transparent++;else partial++;}
 const a=await sharp(source).removeAlpha().raw().toBuffer(), b=await sharp(out).removeAlpha().raw().toBuffer();let mismatch=0;for(let i=0;i<a.length;i++)if(a[i]!==b[i])mismatch++;
 console.log(JSON.stringify({file:out,width:info.width,height:info.height,alpha:{transparent,partial,opaque},rgbPixelMismatches:mismatch,bytes:output.length}));
})().catch(e=>{console.error(e);process.exitCode=1});

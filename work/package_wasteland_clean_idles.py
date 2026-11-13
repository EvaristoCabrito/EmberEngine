from pathlib import Path
import json
import sys
import numpy as np
from scipy import ndimage
from PIL import Image, ImageDraw

work=Path(__file__).resolve().parent
ids=json.loads(sys.argv[1])
sys.argv=[sys.argv[0],str(work/'wasteland-guilds-staging')]
base=(work/'package_wasteland_guild_art.py').read_text(encoding='utf8')
exec(base[:base.index('for form, id in')])
preview=Image.new('RGB',(1600,6*405),(46,45,42))
draw=ImageDraw.Draw(preview)

def remove_edge_noise(im):
    data=np.array(im)
    rgb=data[:,:,:3].astype('int16')
    alpha=data[:,:,3]
    r,g,b=np.moveaxis(rgb,-1,0)
    spill=((r-g>65)&(b<80))|((g-b>85)&(r-b>105))
    alpha[(alpha<32)|spill]=0
    alpha_mask=alpha>=150
    core=ndimage.binary_erosion(alpha_mask,iterations=2)
    nearest=ndimage.distance_transform_edt(~core,return_distances=False,return_indices=True)
    edge=(alpha>0)&~alpha_mask
    data[edge,:3]=data[nearest[0][edge],nearest[1][edge],:3]
    data[alpha==0,:3]=0
    return Image.fromarray(data)

for j,(name,directions) in enumerate(ids.items()):
    rows=[]
    for direction,id in directions.items():
        im=imagegen(f'{name}-idle-{direction}-source-v03',id)
        top,bottom=split(remove_edge_noise(im),2,2)
        rows.append(top+bottom)
    fitted=register(rows,max_h=334,max_w=354)
    sheet=Image.new('RGBA',(size*4,size*2))
    for y,row in enumerate(fitted):
        draw.text((12,(j*2+y)*405+4),f'{name} / {("frente","costas")[y]}',fill=(240,235,225))
        for x,frame in enumerate(row):
            sheet.paste(frame,(size*x,size*y))
            preview.paste(frame,(x*400+8,(j*2+y)*405+18),frame)
    sheet.save(root/f'{name}-idle-v03.webp',lossless=True,method=4)
preview.save(work/'wasteland-vanguard-idles-v03.png')


for name in ids:
    for clip in ('walk','action'):
        old=Image.open(root/f'{name}-{clip}-v01.webp').convert('RGBA')
        sheet=Image.new('RGBA',old.size)
        for y in range(2):
            for x in range(4):
                box=(x*size,y*size,(x+1)*size,(y+1)*size)
                sheet.paste(remove_edge_noise(old.crop(box)),box[:2])
        sheet.save(root/f'{name}-{clip}-v02.webp',lossless=True,method=4)

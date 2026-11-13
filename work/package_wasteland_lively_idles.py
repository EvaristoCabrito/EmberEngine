from pathlib import Path
import sys
from PIL import Image

work=Path(__file__).resolve().parent
sys.argv = [sys.argv[0], str(work/'wasteland-guilds-staging')]
base=(work/'package_wasteland_guild_art.py').read_text(encoding='utf8')
exec(base[:base.index('for form, id in')])
new={
 'moonwarden-human':'b5f49879-1ce9-42ba-adea-b98888adfacc',
 'moonwarden-werewolf':'3df721eb-2904-4182-aa16-3ee5d8638a1c',
 'vanguard':'cbaaca74-e900-4774-bac0-c5f9762fa3d9',
 'swordsmaster':'157b1777-b733-4e78-a0e4-344a82297cef',
 'warbringer':'e7cbae01-4f45-430b-a997-4988f4a5c4e6',
}
def write(name, rows, max_h, max_w):
    fitted=register(rows,max_h,max_w)
    sheet=Image.new('RGBA',(size*4,size*2))
    for y,row in enumerate(fitted):
        for x,frame in enumerate(row):
            sheet.paste(frame,(size*x,size*y))
    sheet.save(root/f'{name}-idle-v02.webp',lossless=True,method=4)
    return fitted[0]
def row_from_wolf(id):
    im=imagegen('moonwarden-great-wolf-idle-source-v02',id)
    top,bottom=split(im,2,2)
    return top+bottom
preview=Image.new('RGBA',(960,6*190),(39,38,35,255))
from PIL import ImageDraw
draw=ImageDraw.Draw(preview)
for j,(name,id) in enumerate(new.items()):
    im=imagegen(f'{name}-idle-source-v02',id)
    if name.startswith('moonwarden'):
        top,bottom=split(im,2,2)
        row=top+bottom
        frames=write(name,[row,row],302 if name.endswith('human') else 316,350)
    else:
        front,back=split(im,4,2)
        frames=write(name,[front,back],326,350)
    draw.text((12,j*190+5),name,fill=(241,233,216,255))
    for i,frame in enumerate(frames):
        frame.thumbnail((168,168))
        preview.alpha_composite(frame,(i*240+36,j*190+18))
if Path(work/'wolf_idle_id.txt').exists():
    row=row_from_wolf((work/'wolf_idle_id.txt').read_text().strip())
    frames=write('moonwarden-great-wolf',[row,row],262,366)
    draw.text((12,5*190+5),'Grande Lobo',fill=(241,233,216,255))
    for i,frame in enumerate(frames):
        frame.thumbnail((168,168))
        preview.alpha_composite(frame,(i*240+36,5*190+18))
preview.convert('RGB').save(work/'wasteland-guild-idles-v02.jpg',quality=92)


from pathlib import Path
import sys
import json
import shutil
import numpy as np
from scipy import ndimage
from PIL import Image, ImageOps

root = Path(sys.argv[1]) / 'public/art'
generated = Path.home() / '.codex/generated_images/01a1d3b3-ea1e-7a80-8908-3fb1d2fe71ae'
generated = Path('C:/Users/evari') / generated.relative_to(Path.home())
sources = root / 'sources-v01'
portraits = root / 'portraits'
for folder in (sources, portraits):
    folder.mkdir(parents=True, exist_ok=True)
size = 384
foot = 363
ember = Path('C:/EmberEngineB/content/sprites/moonwarden-v01')

def imagegen(name, id):
    src = generated / f'exec-{id}.png'
    shutil.copyfile(src, sources / f'{name}.png')
    return Image.open(src).convert('RGBA')

def clean(im):
    data = np.array(im.convert('RGBA'))
    alpha = data[:, :, 3]
    components, count = ndimage.label(alpha >= 32, np.ones((3, 3)))
    areas = np.bincount(components.ravel())
    areas[0] = 0
    keep = areas[components] >= max(15, areas.max() // 150)
    keep = ndimage.binary_dilation(keep, iterations=2)
    alpha[~keep] = 0
    out = Image.fromarray(data)
    box = out.getbbox()
    assert box, 'empty frame'
    return out.crop(box)

def split(im, columns, rows, cuts=None):
    w, h = im.size
    cuts = cuts or [round(i*h/rows) for i in range(rows+1)]
    return [[clean(im.crop((round(i*w/columns), cuts[j], round((i+1)*w/columns), cuts[j+1])))
             for i in range(columns)] for j in range(rows)]

def register(rows, max_h=322, max_w=350):
    frames = [im for row in rows for im in row]
    scale = min(max_h/max(im.height for im in frames), max_w/max(im.width for im in frames))
    result = []
    for row in rows:
        line = []
        for im in row:
            im = im.resize((round(im.width*scale), round(im.height*scale)), Image.Resampling.LANCZOS)
            frame = Image.new('RGBA', (size,size))
            frame.paste(im, ((size-im.width)//2, foot-im.height))
            line.append(frame)
        result.append(line)
    return result

def atlas(name, clip, rows):
    assert len(rows)==2 and all(len(row)==4 for row in rows)
    canvas = Image.new('RGBA', (size*4,size*2))
    for y, row in enumerate(rows):
        for x, frame in enumerate(row):
            canvas.paste(frame,(size*x,size*y))
    canvas.save(root / f'{name}-{clip}-v01.webp', lossless=True, method=4)

def portrait(name, im):
    im = clean(im)
    face = im.crop((0,0,im.width, min(im.height,round(im.height*0.42))))
    portrait = ImageOps.contain(face,(240,240),Image.Resampling.LANCZOS)
    canvas = Image.new('RGBA',(256,256))
    canvas.paste(portrait,((256-portrait.width)//2,(256-portrait.height)//2))
    canvas.save(portraits / f'{name}-portrait-v01.webp',lossless=True)

for form, id in {
    'human': '22833954-b4c6-4380-a6e4-6b148d6f6a76',
    'werewolf': '363a2aea-9535-43e7-a45f-7bc31b56bf9f',
}.items():
    name = f'moonwarden-{form}'
    source = ember/form
    def frames(clip):
        return [Image.open(path).convert('RGBA') for path in sorted((source/clip).glob('*.png'))]
    for clip, source_clip in [('walk','walk'), ('action','attack')]:
        right = frames(source_clip)
        atlas(name,clip,[right,right])
        left = frames(f'{source_clip}-left')
        atlas(name,clip+'-left',[left,left])
    top,bottom = split(imagegen(f'{name}-idle-v02',id),2,2)
    idle = register([top+bottom], max_h=302 if form=='human' else 316)[0]
    atlas(name,'idle',[idle,idle])
    portrait(name,idle[0])
    for clip in ('block','fall','hit','special'):
        raw = frames(clip)
        atlas(name,clip,[raw,raw])

name = 'moonwarden-great-wolf'
stand = clean(imagegen(f'{name}-reference-v01','7f034720-6946-4d84-b933-529df4d9815d'))
(first,step1),(step2,attack) = split(imagegen(f'{name}-poses-v01','b7f490bc-93c7-4524-b088-921327458804'),2,2)
walk, act, idle = register([[first,step1,first,step2],[first,attack,attack,first],[first,stand,first,stand]],max_h=262,max_w=366)
for clip, row in [('walk',walk),('action',act),('idle',idle)]:
    atlas(name,clip,[row,row])
wolf_head = stand.crop((round(stand.width*.64),0,stand.width,round(stand.height*.72)))
thumb = ImageOps.contain(wolf_head,(256,256),Image.Resampling.LANCZOS)
canvas = Image.new('RGBA',(256,256))
canvas.paste(thumb,((256-thumb.width)//2,(256-thumb.height)//2))
canvas.save(portraits / f'{name}-portrait-v01.webp',lossless=True)

vanguards = {
    'vanguard': ('d90385fc-e837-4638-97ce-2076cd2de3de', [0,295,592,887]),
    'swordsmaster': ('043d827b-e000-4051-9645-a90a904360db', [0,297,587,887]),
    'warbringer': ('96f2f644-dbae-45cc-94e1-00ed4327f23c', [0,362,726,1086]),
}
ids = json.loads(sys.argv[2])
for name,(id,cuts) in vanguards.items():
    im = imagegen(f'{name}-walk-action-source-v01',id)
    front, back, action = split(im,4,3,cuts)
    idle_src = imagegen(f'{name}-idle-source-v01',ids[name])
    idle_front,idle_back = split(idle_src,4,2)
    front,back,action,idle_front,idle_back = register([front,back,action,idle_front,idle_back],max_h=338)
    atlas(name,'walk',[front,back])
    atlas(name,'action',[action,action])
    atlas(name,'idle',[idle_front,idle_back])
    portrait(name,idle_front[0])

if len(sys.argv) > 3:
    guilds = split(imagegen('moonwarden-vanguard-guilds-source-v01',sys.argv[3]),2,1)[0]
    for name, im in zip(('guild-moonwarden', 'guild-vanguard'), guilds):
        canvas = Image.new('RGBA',(512,512))
        im = ImageOps.contain(im,(494,494),Image.Resampling.LANCZOS)
        canvas.paste(im,((512-im.width)//2,506-im.height))
        canvas.save(root / f'{name}-v01.webp',lossless=True)


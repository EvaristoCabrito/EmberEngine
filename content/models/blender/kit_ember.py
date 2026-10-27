"""Wave 2, group 8: Ember Ashes' own objects (content/models/ember): Ember crystals, relics, cult stones, ancient machines."""
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kitlib import *  # noqa

def g(o):
    ground(o)
    return o

    return g(o)

    return g(o)

    return g(o)

    return g(o)

def containment_box():
    body = plank_box('body', (1.0, 0.7, 0.55), (0, 0, 0.275), 'iron', 0.03, 1.0)
    lid = plank_box('lid', (1.04, 0.74, 0.12), (0, 0, 0.61), 'iron-rust', 0.03, 1.0)
    for x in (-0.3, 0, 0.3):
        boolean(lid, box(f'r{x}', (0.12, 0.12, 0.03), loc=(x, 0, 0.67), rot=(0, 0, 0.785)))
        boolean(body, box(f'rf{x}', (0.1, 0.04, 0.18), loc=(x, -0.35, 0.3)))
    parts = [body, lid]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(box(f'corner{sx}{sy}', (0.1, 0.1, 0.62), loc=(sx * 0.48, sy * 0.33, 0.31), mat=M('brass')))
    for x in (-0.3, 0, 0.3):
        parts.append(box(f'rune{x}', (0.1, 0.1, 0.02), loc=(x, 0, 0.67), rot=(0, 0, 0.785), mat=M('ember-glow')))
    parts.append(box('clasp', (0.14, 0.05, 0.16), loc=(0, -0.37, 0.55), mat=M('brass')))
    o = join(parts, 'containment-box')
    smooth(o, 40)
    return g(o)

def ancient_reliquary():
    body = plank_box('body', (0.7, 0.45, 0.4), (0, 0, 0.2), 'bronze', 0.025, 1.0)
    dome = sphere('dome', 1.0, (0, 0, 0.4), subdiv=3, mat=M('bronze'), scale=(0.35, 0.22, 0.2))
    boolean(dome, box('cut', (2, 2, 1), loc=(0, 0, -0.1)))
    parts = [body, dome, box('band', (0.74, 0.49, 0.06), loc=(0, 0, 0.4), mat=M('gold')), box('band2', (0.74, 0.49, 0.04), loc=(0, 0, 0.05), mat=M('gold'))]
    for i in range(5):
        for sy in (-1, 1):
            parts.append(sphere(f'gem{i}{sy}', 0.03, (-0.26 + i * 0.13, sy * 0.235, 0.22), subdiv=2, mat=M('ember-crystal')))
    parts.append(sphere('finial', 0.05, (0, 0, 0.62), subdiv=2, mat=M('gold')))
    o = join(parts, 'ancient-reliquary')
    smooth(o, 40)
    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

    return g(o)

if __name__ == '__main__':
    run('ember', [
          
         ('containment-box', containment_box), ('ancient-reliquary', ancient_reliquary), 
          
           
          
         
    ])

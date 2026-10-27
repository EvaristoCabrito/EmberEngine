import bpy,sys
sys.path.insert(0,r'C:/EmberEngine/content/models/blender')
import emberlib,kit_crypt
original_material=emberlib.material
overrides={'masonry-crypt':('wt-gravestone',0.9),'masonry-mossy':('wt-limestone',0.9)}
def photo_material(key,**kw):
 if key in overrides:
  image,rough=overrides[key]
  return original_material(key+'-review-photo',color=image,roughness=rough)
 return original_material(key,**kw)
emberlib.material=photo_material
bpy.ops.wm.read_factory_settings(use_empty=True)
emberlib._materials.clear(); emberlib._images.clear()
parts=kit_crypt._sarcophagus(607,open_lid=True)
piece=next(o for o in parts if o.name=='piece')
parts.remove(piece)
bpy.data.objects.remove(piece,do_unlink=True)
obj=kit_crypt.join(parts,'sarcophagus-broken-lid')
kit_crypt.clamp_floor(obj,0.0)
kit_crypt.smooth(obj,40)
obj=kit_crypt.g(obj)
print('SCENE_MESH_OBJECTS',[(o.name,len(o.data.vertices)) for o in bpy.context.scene.objects if o.type=='MESH'])
print('BUILT',emberlib.export(obj,'crypt','sarcophagus-broken-lid',version='v03'))

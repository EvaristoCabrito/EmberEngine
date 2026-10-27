import pathlib,difflib,json
base=pathlib.Path(r'C:\EmberEngine')
for src in ['content/engine2/editor-catalog.json','content/models/models.json','content/maps/ember/cemiterio-esquecidos008.json']:
 old=(base/src).read_text(encoding='utf-8'); staged=(base/'work/catalog-audit/stage'/pathlib.Path(src).name).read_text(encoding='utf-8')
 print(src,'lengths',len(old),len(staged),'diff',sum(1 for x in difflib.ndiff(old,staged) if x[0]!=' '))
 sm=difflib.SequenceMatcher(a=old,b=staged,autojunk=False)
 for tag,i,j,k,l in sm.get_opcodes():
  if tag!='equal': print(tag,repr(old[max(0,i-60):min(len(old),j+60)]),'=>',repr(staged[max(0,k-60):min(len(staged),l+60)])); break
 print('targets old/new',old.count('ember-extra-grave-marker-v03'),staged.count('ember-extra-grave-marker-v03'))

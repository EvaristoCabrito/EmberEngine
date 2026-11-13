from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

work = Path(__file__).resolve().parent
stage = work / 'wasteland-guilds-staging'
dest = Path('C:/WastelandFarming')
with tempfile.TemporaryDirectory(dir=work) as temp:
    integration = Path(temp)
    code = ('app.js','index.html')
    originals = {}
    for name in code:
        originals[name] = (dest/name).read_bytes()
        (integration/name).write_bytes(originals[name])
    subprocess.run([sys.executable, str(work/'integrate_wasteland_guilds.py'), str(integration)], check=True)
    subprocess.run(['node','--check',str(integration/'app.js')],check=True)
    subprocess.run(['node',str(work/'check_wasteland_guilds.cjs'),str(integration/'app.js')],check=True)
    assets = [(path, dest/path.relative_to(stage)) for path in (stage/'public/art').rglob('*') if path.is_file()]
    for source, target in assets:
        if target.exists() and target.read_bytes() != source.read_bytes():
            raise FileExistsError(target)
    for source, target in assets:
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            with target.open('xb') as stream:
                stream.write(source.read_bytes())
    for name in code:
        if (dest/name).read_bytes() != originals[name]:
            raise RuntimeError(f'{name} changed while integrating')
    for name in code:
        shutil.copyfile(integration/name, dest/name)
    print(f'Integrated guilds and {len(assets)} assets in {dest}')


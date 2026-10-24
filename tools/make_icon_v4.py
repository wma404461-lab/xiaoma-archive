"""Export the approved full-color imagegen artwork; preserve its alpha and design.
SVG is an editable image-container, not an invented vector trace. App and tray
use the same colorful artwork, which stays readable on light and dark surfaces.
"""
from pathlib import Path
from io import BytesIO
import base64,struct,json
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
ASSETS=ROOT/'assets'
SIZES=(16,24,32,48,64,128,256)
REVISION='2026-10-12-free-app-icon-design-v4'

def write_ico(path,frames):
    blobs=[]
    for size in SIZES:
        stream=BytesIO();frames[size].save(stream,format='PNG',compress_level=9);blobs.append(stream.getvalue())
    offset=6+len(blobs)*16;header=bytearray(struct.pack('<HHH',0,1,len(blobs)))
    for size,blob in zip(SIZES,blobs):
        header.extend(struct.pack('<BBBBHHII',size%256,size%256,0,0,1,32,len(blob),offset));offset+=len(blob)
    path.write_bytes(bytes(header)+b''.join(blobs))

def build():
    master=Image.open(ASSETS/'xiaoma-compress-master-v4.png').convert('RGBA')
    assert master.width==master.height and master.getpixel((0,0))[3]==0
    frames={size:master.resize((size,size),Image.Resampling.LANCZOS) for size in SIZES}
    canonical=master.resize((512,512),Image.Resampling.LANCZOS)
    canonical.save(ASSETS/'xiaoma-compress.png',compress_level=9)
    canonical.save(ASSETS/'xiaoma-compress-inverse.png',compress_level=9)
    data=base64.b64encode((ASSETS/'xiaoma-compress.png').read_bytes()).decode('ascii')
    svg=f'''<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
<title>小马压缩 · 蓝色拉链包与象牙白马</title>
<desc>specRevision={REVISION}; imagegen artwork, embedded PNG. Editable image placement/container; not a pure vector path illustration.</desc>
<image id="horse-archive-artwork" x="0" y="0" width="512" height="512" href="data:image/png;base64,{data}"/>
</svg>'''
    for name in ('xiaoma-compress.svg','xiaoma-compress-inverse.svg'):(ASSETS/name).write_text(svg,encoding='utf-8')
    for size,frame in frames.items():
        frame.save(ASSETS/f'xiaoma-compress-{size}.png',compress_level=9)
        frame.save(ASSETS/f'xiaoma-compress-inverse-{size}.png',compress_level=9)
    write_ico(ROOT/'src/app.ico',frames)
    write_ico(ROOT/'src/app-dark.ico',frames)
    (ASSETS/'xiaoma-compress-design-v4.json').write_text(json.dumps({'specRevision':REVISION,'sourceType':'imagegen-raster','svgType':'embedded-image-container','darkVariant':'same full-color blue/ivory/gold design','master':'xiaoma-compress-master-v4.png','sizes':list(SIZES)},ensure_ascii=False,indent=2),encoding='utf-8')
    print('V4 full-color horse archive artwork exported, alpha preserved')
if __name__=='__main__':build()

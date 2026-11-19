"""Recreate the report's synthetic image and verify the two resulting masks."""
from pathlib import Path
import subprocess
from PIL import Image
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT=Path(__file__).resolve().parents[1]
exe=str(ROOT/'segment.exe')
subprocess.run([exe,'generate','512','512','20261112',str(ROOT/'docs/phantom.pgm')],check=True)
subprocess.run([exe,'segment','seq',str(ROOT/'docs/phantom.pgm'),str(ROOT/'docs/mask.pgm')],check=True)
subprocess.run([exe,'segment','par','4',str(ROOT/'docs/phantom.pgm'),str(ROOT/'docs/mask_parallel.pgm')],check=True)
assert (ROOT/'docs/mask.pgm').read_bytes()==(ROOT/'docs/mask_parallel.pgm').read_bytes()
fig,axes=plt.subplots(1,2,figsize=(8.2,4.4))
for ax,name,title in zip(axes,['phantom.pgm','mask.pgm'],['Исходный фантом','Маска, T = 64']):
    ax.imshow(Image.open(ROOT/'docs'/name),cmap='gray',vmin=0,vmax=255)
    ax.axis('off');ax.set_title(title)
fig.tight_layout();fig.savefig(ROOT/'figures/example.pdf');fig.savefig(ROOT/'figures/example.png',dpi=180)

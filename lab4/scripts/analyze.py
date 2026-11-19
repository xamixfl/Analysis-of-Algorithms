from pathlib import Path
import csv
import json
import statistics as st
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT = Path(__file__).resolve().parents[1]
plt.rcParams.update({'font.family':'DejaVu Sans','font.size':12,'pdf.fonttype':42})
groups = {}
with (ROOT/'results/raw.csv').open(newline='') as f:
    for row in csv.DictReader(f):
        key = int(row['side']), int(row['q'])
        groups.setdefault(key,[]).append(row)
sizes = sorted({s for s,q in groups})
qs = sorted({q for s,q in groups})
summary = []
for (side,q), rows in sorted(groups.items()):
    assert len(rows)==100 and len({r['repeat'] for r in rows})==100
    assert len({(r['threshold'],r['checksum']) for r in rows})==1
    assert {(r['threshold'],r['checksum']) for r in rows} == {(r['threshold'],r['checksum']) for r in groups[side,0]}
    values=[float(r['seconds'])*1000 for r in rows]
    baseline=st.mean(float(r['seconds'])*1000 for r in groups[side,0])
    mean=st.mean(values)
    summary.append(dict(side=side,n=side*side,q=q,count=len(values),mean_ms=mean,
        median_ms=st.median(values),std_ms=st.stdev(values),min_ms=min(values),max_ms=max(values),
        speedup=baseline/mean,efficiency=baseline/mean/q if q else 1))
with (ROOT/'results/summary.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=list(summary[0]));w.writeheader();w.writerows(summary)
lookup={(r['side'],r['q']):r for r in summary}
fig,ax=plt.subplots(figsize=(8.2,5.5))
for q in qs:
    ax.plot(sizes,[lookup[s,q]['mean_ms'] for s in sizes],marker='o',label='Последовательно' if not q else f'q = {q}')
ax.set(xscale='log',yscale='log',xlabel='Сторона квадратного изображения, пиксель',ylabel='Среднее реальное время, мс')
ax.set_xticks(sizes,[str(s) for s in sizes]);ax.grid(alpha=.2);ax.legend(ncol=2);fig.tight_layout()
fig.savefig(ROOT/'figures/time.pdf');fig.savefig(ROOT/'figures/time.png',dpi=180);plt.close(fig)
fig,ax=plt.subplots(figsize=(8.2,5.5))
for side in sizes:
    ax.plot(qs[1:],[lookup[side,q]['speedup'] for q in qs[1:]],marker='o',label=f'{side} × {side}')
ax.axhline(1,color='black',ls='--',lw=.8)
ax.set(xscale='log',xlabel='Число рабочих потоков q',ylabel='Ускорение относительно последовательной версии')
ax.set_xticks(qs[1:],[str(q) for q in qs[1:]]);ax.grid(alpha=.2);ax.legend(ncol=2);fig.tight_layout()
fig.savefig(ROOT/'figures/speedup.pdf');fig.savefig(ROOT/'figures/speedup.png',dpi=180);plt.close(fig)
def ru(n,d=3): return f'{n:.{d}f}'.replace('.',',')
rows=[]
for side in sizes:
    seq,p1=lookup[side,0],lookup[side,1]
    best=min((lookup[side,q] for q in qs[1:]),key=lambda r:r['mean_ms'])
    rows.append(f'{side} & {ru(seq["mean_ms"])} & {ru(p1["mean_ms"])} '+r'\\\hline')
head = r'''\begin{table}[H]\caption{Сравнение реализаций при разных размерах}\label{tab:sizes}
\centering\begin{tabularx}{\textwidth}{|c|X|X|}\hline
Сторона, пиксель & Послед., мс & Паралл., $q=1$, мс\\\hhline{|=:=:=|}
'''
(ROOT/'results/table_sizes.tex').write_text(head+'\n'.join(rows)+'\n'+r'\end{tabularx}\end{table}',encoding='utf-8')
rows=[]
for q in qs[1:]:
    r=lookup[sizes[-1],q]
    rows.append(f'{q} & {ru(r["mean_ms"])} & {ru(r["speedup"],2)} '+r'\\\hline')
head = r'''\begin{table}[H]\caption{Результаты для изображения $4096\times4096$}\label{tab:q}
\centering\begin{tabularx}{\textwidth}{|c|X|X|}\hline
Рабочие потоки & Время, мс & Ускорение\\\hhline{|=:=:=|}
'''
(ROOT/'results/table_q.tex').write_text(head+'\n'.join(rows)+'\n'+r'\end{tabularx}\end{table}',encoding='utf-8')
allrows=[]
for side in sizes:
    allrows.append(f'\\multicolumn{{5}}{{|c|}}{{Изображение ${side} \\times {side}$}}'+r'\\\hline')
    for q in qs:
        r=lookup[side,q]
        name='Послед.' if not q else str(q)
        allrows.append(f'{name} & {ru(r["mean_ms"])} & {ru(r["std_ms"])} & {ru(r["min_ms"])} & {ru(r["max_ms"])} '+r'\\\hline')
header = r'''\begin{longtable}{|p{23mm}|p{29mm}|p{26mm}|p{26mm}|p{26mm}|}
\caption{Полные результаты измерений}\label{tab:all}\\\hline
$q$ & Среднее & Станд. откл. & Минимум & Максимум\\\hhline{|=:=:=:=:=|}
\endfirsthead
\multicolumn{5}{l}{Продолжение таблицы~\thetable}\\\hline
$q$ & Среднее & Станд. откл. & Минимум & Максимум\\\hhline{|=:=:=:=:=|}
\endhead
'''
(ROOT/'results/table_all.tex').write_text(header+'\n'.join(allrows)+'\n'+r'\end{longtable}',encoding='utf-8')
best=min((lookup[sizes[-1],q] for q in qs[1:]),key=lambda r:r['mean_ms'])
analysis=dict(largest_side=sizes[-1],best_q=best['q'],best_speedup=best['speedup'],
    seq_ms=lookup[sizes[-1],0]['mean_ms'],par1_ms=lookup[sizes[-1],1]['mean_ms'],
    par1_ratio=lookup[sizes[-1],1]['mean_ms']/lookup[sizes[-1],0]['mean_ms'])
(ROOT/'results/analysis.json').write_text(json.dumps(analysis,indent=2),encoding='utf-8')
print(json.dumps(analysis,indent=2))

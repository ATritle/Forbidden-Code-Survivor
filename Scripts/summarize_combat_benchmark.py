"""Summarize the native combat benchmark's warmup-excluded CSV; no third-party dependencies."""
import csv,json,re,statistics,sys
from pathlib import Path
source=Path(sys.argv[1])
rows=list(csv.DictReader(source.open(newline='')))
def percentile(values,fraction):
    values=sorted(values);position=(len(values)-1)*fraction;low=int(position);high=min(low+1,len(values)-1)
    return values[low]+(values[high]-values[low])*(position-low)
metrics={}
for key in ['frame_ms','game_ms','render_ms','gpu_ms']:
    values=[float(row[key]) for row in rows]
    metrics[key]={'mean':statistics.mean(values),'median':statistics.median(values),'p95':percentile(values,.95),'max':max(values)}
summary={'samples':len(rows),'resolution':'1600x900','sample_seconds':10,'warmup_seconds':5,
         'scenario':'180 mixed cyborgs including one boss; 15 offensive spells; rain and Lumen; 8 full explosion requests per second; fixed VFX caps enabled',
         'metrics':metrics,'mean_fps':1000/metrics['frame_ms']['mean'],
         'active_enemy_min':min(int(row['active_enemies']) for row in rows),'active_enemy_max':max(int(row['active_enemies']) for row in rows),
         'explosion_totals_at_sample_end':{key:int(rows[-1][key]) for key in ['accepted','dropped','fluid_accepted']}}
log=source.parent.parent/(source.stem+'.log')
if log.exists():
    output=log.read_text(encoding='utf-8',errors='replace')
    summary['spell_feedback']=[{'style':int(style),'hits':int(hits),'actual_damage':float(damage),'impact_bursts':int(impact),'flight_bursts':int(flight)}
        for style,hits,damage,impact,flight in re.findall(r'FEEDBACK_BENCH style=(\d+) hits=(\d+) damage=([\d.]+) impact_bursts=(\d+) flight_bursts=(\d+)',output)]
    totals=re.search(r'FEEDBACK_BENCH_TOTAL pulses=(\d+) gpu_bursts=(\d+) coalesced=(\d+) gpu_admission_drops=(\d+)',output)
    if totals:summary['feedback_totals_at_sample_end']=dict(zip(['pulses','gpu_bursts','coalesced','gpu_admission_drops'],map(int,totals.groups())))
destination=source.with_name(source.stem+'-summary.json');destination.write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))

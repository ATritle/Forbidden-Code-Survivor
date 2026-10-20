"""Summarize unfiltered Shipping frame-time samples, including outliers."""
from pathlib import Path
import csv,json,statistics
root=Path(__file__).resolve().parents[1]
reports=[]
for branch in ['A','B']:
    folder=root/'Saved/QA'/('PackagedBench'+branch)/'Saved/Benchmarks'
    rows=list(csv.DictReader((folder/('Overhaul-'+branch+'.csv')).open()))
    frames=sorted(float(row['frame_ms']) for row in rows)
    if len(frames)<500:raise RuntimeError('Insufficient frame samples')
    def percentile(p):return frames[min(len(frames)-1,int((len(frames)-1)*p))]
    telemetry=json.loads((folder/('Overhaul-'+branch+'.json')).read_text())
    stats={'branch':branch,'frames':len(frames),'mean_ms':statistics.mean(frames),'median_ms':statistics.median(frames),'p95_ms':percentile(.95),'p99_ms':percentile(.99),'max_ms':max(frames),'average_fps':1000/statistics.mean(frames),'frames_over_33ms':sum(t>33.333 for t in frames),'frames_over_100ms':sum(t>100 for t in frames),'telemetry':telemetry}
    for name in ['game_ms','render_ms','gpu_ms']:
        values=[float(row[name]) for row in rows]
        stats[name]=statistics.mean(values) if any(values) else None
    reports.append(stats)
result={'result':'PACKAGED_STRESS_PASS' if all(r['telemetry']['projectileDrops']==r['telemetry']['arsenalDrops']==r['telemetry']['fieldDrops']==0 for r in reports) else 'PACKAGED_STRESS_FAIL','hardware':'Ryzen 9 7900X3D / Radeon RX 7900 XTX / 32GB RAM','configuration':'Windows Shipping; DX12 SM6; High preset, 1600x900; 180 pooled cyborgs; 15 fully evolved attacks; eight additional cinematic explosions per second; ten-second warmup, sixty-second samples per evolution branch','measurement':'Unfiltered wall frame times. Zero native stat counters are reported as unavailable, not zero cost. Cosmetic burst admission drops are separate from gameplay-shot/field admission. This is an extreme opt-in test; ordinary runs permit six programs.','runs':reports}
destination=root/'Docs/WindowsOverhaul/Validation/performance.json';destination.parent.mkdir(parents=True,exist_ok=True);destination.write_text(json.dumps(result,indent=2)+'\n')
for r in reports:print(f"{r['branch']}: {r['average_fps']:.1f} average FPS; {r['p95_ms']:.2f}ms p95; {r['max_ms']:.2f}ms max; gameplay drops {r['telemetry']['projectileDrops']}/{r['telemetry']['arsenalDrops']}/{r['telemetry']['fieldDrops']}")

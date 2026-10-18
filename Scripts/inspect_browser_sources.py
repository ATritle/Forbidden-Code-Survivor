import bpy, json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
bpy.ops.wm.open_mainfile(filepath=str(root/'ContentSource/Meshy/NeonExorcist/assembled/NeonExorcist-RidingRig.blend'))
report={'objects':[{'name':o.name,'type':o.type,'dim':list(o.dimensions),'location':list(o.location),'parent':o.parent.name if o.parent else None,'vertices':len(o.data.vertices) if o.type=='MESH' else 0} for o in bpy.context.scene.objects], 'actions':[{ 'name':a.name,'range':list(a.frame_range),'slots':[(s.identifier,s.target_id_type) for s in a.slots]} for a in bpy.data.actions], 'gltf_props':[p.identifier for p in bpy.ops.export_scene.gltf.get_rna_type().properties]}
(root/'Saved/BrowserSourcesInspect.json').write_text(json.dumps(report,indent=2))
print('BROWSER_INSPECT_READY',flush=True)

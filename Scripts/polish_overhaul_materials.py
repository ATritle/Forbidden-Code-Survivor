"""Keep wetness in small pools and gutters, with aggregate visible between reflections."""
from pathlib import Path
import unreal

lib=unreal.MaterialEditingLibrary
root=Path(unreal.Paths.project_dir())
report=[]
assets=['/Game/Art/City/M_CitySurface','/Game/Art/City/M_NeonGlow','/Game/Art/City/M_WetConcrete','/Game/Art/City/M_StreetSurface']
assets += ['/Game/Art/City/Meshy/'+name+'/M_'+name for name in ['ShoppingBlock','MarketKiosk','RooftopShrine']]
for path in assets:
    material=unreal.load_asset(path)
    if not material:raise RuntimeError('Missing shared city material '+path)
    changed=0
    # MaterialEditingLibrary expressions are editor graph subobjects; use the authoritative object inventory.
    for obj in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
        if obj.get_outer()!=material:continue
        code=obj.get_editor_property('code')
        if 'smoothstep(130,230,r)' in code:code=code.replace('smoothstep(130,230,r)','smoothstep(240,420,r)');changed+=1
        if 'smoothstep(.40,.72,n.fbm(W.xy*.0034))' in code:
            code=code.replace('float puddle=smoothstep(.40,.72,n.fbm(W.xy*.0034));','float gutter=max(exp(-pow((abs(W.y+450)-430)/32,2)),exp(-pow((abs(W.x)-430)/32,2))); float puddle=saturate(smoothstep(.64,.88,n.fbm(W.xy*.0054))*.6+gutter*.75);');changed+=1
        if 'smoothstep(.4,.72,n.fbm(W.xy*.0034))' in code:
            code=code.replace('smoothstep(.4,.72,n.fbm(W.xy*.0034))','smoothstep(.64,.88,n.fbm(W.xy*.0054))');changed+=1
        if 'lerp(0.43,0.11,smoothstep(0.46,0.8,n))' in code:
            code=code.replace('lerp(0.43,0.11,smoothstep(0.46,0.8,n))','lerp(0.68,0.15,smoothstep(0.7,0.94,n))');changed+=1
        obj.set_editor_property('code',code)
    lib.recompile_material(material);unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
    report.append({'material':path,'adjusted_nodes':changed})
(root/'Saved/Review/OverhaulMaterialPolish.json').write_text(__import__('json').dumps(report,indent=2))
unreal.log('OVERHAUL_MATERIAL_POLISH_PASS')

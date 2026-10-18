import unreal
folder='/Game/Art/Characters/NeonExorcist/'
mesh=unreal.load_asset(folder+'SK_NeonExorcist')
unreal.log('NEON_MESH_SKELETON '+str(mesh.get_editor_property('skeleton')))
for state in ['HoverIdle','Cruise','Cast','Hit','Defeat']:
    clip=unreal.load_asset(folder+'A_'+state)
    unreal.log('NEON_CLIP_SKELETON '+state+' '+str(clip.get_editor_property('skeleton')))

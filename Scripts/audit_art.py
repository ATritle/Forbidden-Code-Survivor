import unreal
for id in ['Necromancer','Gravebound']:
    folder='/Game/Art/Characters/'+id
    mesh=unreal.load_asset(folder+'/SK_'+id)
    unreal.log('ART_AUDIT '+id+' '+str(mesh.get_editor_property('materials')))
    mat=unreal.load_asset(folder+'/M_'+id)
    for slot in mesh.get_editor_property('materials'):
        unreal.log('ART_SLOT '+str(slot.material_interface))
    for action in ['Idle','Walk','Run']:
        a=unreal.load_asset(folder+'/A_'+action)
        unreal.log('ART_ANIM '+str(a)+' duration='+str(a.get_play_length()))

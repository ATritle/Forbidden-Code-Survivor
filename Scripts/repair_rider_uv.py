"""Restore original Meshy PBR UVs after Meshy rigging expands vertex data.
Uses matching triangle centers and corners; refuses nonmatching geometry.
Run in Blender with -- Vex|Rook|Zero|Mira. Source outputs remain untouched.
"""
import bpy,sys,json
from pathlib import Path
from mathutils import Vector
from mathutils.kdtree import KDTree

def restore_uv(root,rider):
    before=set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=str(root/'model.fbx'))
    imported=[o for o in bpy.context.scene.objects if o not in before]
    source=next(o for o in imported if o.type=='MESH')
    world=lambda o:[o.matrix_world@v.co for v in o.data.vertices]
    bounds=lambda q:(Vector([min(v[i] for v in q) for i in range(3)]),Vector([max(v[i] for v in q) for i in range(3)]))
    al,ah=bounds(world(source));bl,bh=bounds([v for o in rider for v in world(o)])
    scale=(bh.z-bl.z)/(ah.z-al.z)
    positions=[(p-(al+ah)/2)*scale+(bl+bh)/2 for p in world(source)]
    tree=KDTree(len(source.data.polygons))
    for p in source.data.polygons:tree.insert(sum((positions[v] for v in p.vertices),Vector())/len(p.vertices),p.index)
    tree.balance();worst=0;changed=0
    for obj in rider:
        uv=obj.data.uv_layers.active;srcuv=source.data.uv_layers.active
        source_normals=[n.vector.copy() for n in source.data.corner_normals]
        normal_matrix=obj.matrix_world.to_3x3().transposed() @ source.matrix_world.to_3x3().inverted().transposed()
        normals=[n.vector.copy() for n in obj.data.corner_normals]
        for p in obj.data.polygons:
            p.use_smooth=True
            center=sum((obj.matrix_world@obj.data.vertices[v].co for v in p.vertices),Vector())/len(p.vertices)
            _,j,d=tree.find(center);worst=max(worst,d);sp=source.data.polygons[j]
            if d>.0001:raise RuntimeError('Rig geometry differs from original body: '+str(d))
            for li in p.loop_indices:
                pos=obj.matrix_world@obj.data.vertices[obj.data.loops[li].vertex_index].co
                sl=min(sp.loop_indices,key=lambda k:(positions[source.data.loops[k].vertex_index]-pos).length_squared)
                uv.data[li].uv=srcuv.data[sl].uv
                normals[li]=(normal_matrix@source_normals[sl]).normalized();changed+=1
        obj.data.normals_split_custom_set(normals)
    for o in imported:bpy.data.objects.remove(o,do_unlink=True)
    return dict(corners=changed,maxTriangleDistanceMeters=worst)

if __name__=='__main__':
    rid=sys.argv[sys.argv.index('--')+1]
    if rid not in ['Vex','Rook','Zero','Mira']:raise RuntimeError('Unknown rider')
    root=Path(__file__).resolve().parents[1]/'ContentSource/Meshy'/rid
    out=root/'assembled'
    bpy.ops.wm.open_mainfile(filepath=str(out/(rid+'-RidingRig.blend')))
    rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE' and o.name==rid+'Rig')
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH' and any(m.type=='ARMATURE' and m.object==rig for m in o.modifiers)]
    rider=[o for o in meshes if o.name!='Hoverboard']
    report=restore_uv(root,rider)
    bpy.ops.object.select_all(action='DESELECT');rig.select_set(True)
    for o in meshes:o.select_set(True)
    bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(out/('SK_'+rid+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,axis_forward='-Y',axis_up='Z',path_mode='STRIP',mesh_smooth_type='FACE')
    bpy.ops.wm.save_as_mainfile(filepath=str(out/(rid+'-RidingRig.blend')))
    (out/'uv-validation.json').write_text(json.dumps(report,indent=2))
    print('RIDER_PBR_UV_RESTORED',rid,report)

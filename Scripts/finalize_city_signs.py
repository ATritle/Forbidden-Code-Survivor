"""Correct the standard UE plane's vertical UV orientation without reimporting geometry."""
import unreal
mat=unreal.load_asset('/Game/Art/City/M_NeonSign')
changed=0
for expression in unreal.ObjectIterator(unreal.MaterialExpressionCustom):
    if expression.get_outer()!=mat:continue
    code=expression.get_editor_property('code')
    if code.startswith('float2 tile='):
        expression.set_editor_property('code','float2 tile=float2(fmod(Index,4),floor(Index/4));return (tile+lerp(0.01,0.99,float2(1-UV.x,1-UV.y)))/4;');changed+=1
if changed!=1:raise RuntimeError('Expected exactly one neon sign atlas UV expression')
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('CITY_SIGN_ORIENTATION_READY')

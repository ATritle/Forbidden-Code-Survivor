"""Create editable character/GameMode Blueprints and assign the arena's GameMode.
Run with UnrealEditor-Cmd -run=pythonscript -script=<absolute path>.
Existing Blueprints are retained; this script only updates the documented defaults.
"""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()

def blueprint(name, parent):
    path = '/Game/MVP/' + name
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not asset:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        asset = tools.create_asset(name, '/Game/MVP', unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    return asset

character = blueprint('BP_MortisCharacter', unreal.load_class(None, '/Script/DeckBuilder.MortisCharacter'))
character_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/MVP/BP_MortisCharacter')
defaults = unreal.get_default_object(character_class)
defaults.set_editor_property('max_health', 100.0)
boom = defaults.get_editor_property('camera_boom')
boom.set_editor_property('target_arm_length', 2100.0)
boom.set_editor_property('do_collision_test', False)
art = defaults.get_component_by_class(unreal.NecromancyVisualComponent)
mesh = unreal.load_asset('/Game/Art/Characters/Necromancer/SK_Necromancer')
if not mesh:
    raise RuntimeError('Imported necromancer mesh is missing')
art.set_skinned_asset_and_update(mesh)
# Runtime LoadCharacter validates the matching idle/walk/run skeletons and sets scale.
unreal.BlueprintEditorLibrary.compile_blueprint(character)
unreal.EditorAssetLibrary.save_loaded_asset(character, only_if_is_dirty=False)

mode = blueprint('BP_MortisArena', unreal.load_class(None, '/Script/DeckBuilder.MVPArenaGameMode'))
mode_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/MVP/BP_MortisArena')
unreal.get_default_object(mode_class).set_editor_property('default_pawn_class', character_class)
unreal.BlueprintEditorLibrary.compile_blueprint(mode)
unreal.EditorAssetLibrary.save_loaded_asset(mode, only_if_is_dirty=False)
editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not editor.load_level('/Game/MVP/Arena'):
    raise RuntimeError('Could not load arena')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', mode_class)
editor.save_current_level()
unreal.log('MORTIS_CHARACTER_READY')

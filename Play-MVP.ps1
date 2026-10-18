param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$projectPath = Join-Path $PSScriptRoot 'DeckBuilder.uproject'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
# Neon energy combat, pink colliding sparks and lighting; normal waves.
& $editorPath $projectPath '/Game/Maps/NeonDistrict' -game -dx12 -sm6 -CinematicFX -windowed -ResX=1600 -ResY=900 -ForceRes '-ini:Engine:[/Script/Engine.RendererSettings]:r.GenerateMeshDistanceFields=True,[/Script/Engine.RendererSettings]:r.DynamicGlobalIlluminationMethod=1,[/Script/Engine.RendererSettings]:r.ReflectionMethod=1,[/Script/Engine.RendererSettings]:r.Shadow.Virtual.Enable=1' '-ExecCmds=r.SetRes 1600x900w,t.MaxFPS 120'



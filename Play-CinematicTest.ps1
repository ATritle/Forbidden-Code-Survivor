param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(1,1000)][int]$Enemies = 300,
    [ValidateRange(25,100)][int]$ScreenPercentage = 75,
    [ValidateRange(32,128)][int]$FluidResolution = 64,
    [ValidateRange(0,8)][int]$FluidGrids = 2
)
$projectPath = Join-Path $PSScriptRoot 'DeckBuilder.uproject'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
# Separate launch-only overrides; normal MVP renderer settings remain untouched.
& $editorPath $projectPath '/Game/Maps/NeonDistrict' -game -dx12 -sm6 -windowed -ResX=1920 -ResY=1080 -ForceRes -CinematicTest "-StressEnemies=$Enemies" "-FluidResolution=$FluidResolution" "-FluidGrids=$FluidGrids" '-ini:Engine:[/Script/Engine.RendererSettings]:r.GenerateMeshDistanceFields=True,[/Script/Engine.RendererSettings]:r.DynamicGlobalIlluminationMethod=1,[/Script/Engine.RendererSettings]:r.ReflectionMethod=1,[/Script/Engine.RendererSettings]:r.Shadow.Virtual.Enable=1' "-ExecCmds=r.SetRes 1920x1080w,r.ScreenPercentage $ScreenPercentage,t.MaxFPS 60,stat unit,stat gpu"


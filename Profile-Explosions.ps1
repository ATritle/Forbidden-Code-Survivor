param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateRange(1,1000)][int]$Enemies = 300,
    [ValidateRange(0,60)][float]$ExplosionRate = 2,
    [ValidateRange(1,32)][int]$Burst = 1,
    [ValidateRange(32,128)][int]$FluidResolution = 64,
    [ValidateRange(0,8)][int]$FluidGrids = 2,
    [ValidateRange(25,100)][int]$ScreenPercentage = 75,
    [ValidateRange(5,120)][int]$Warmup = 15,
    [ValidateRange(5,120)][int]$Duration = 30,
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string]$Name = 'ExplosionBenchmark',
    [switch]$DebugEffects
)
$ErrorActionPreference = 'Stop'
$projectPath = Join-Path $PSScriptRoot 'DeckBuilder.uproject'
$editorPath = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$logPath = Join-Path $PSScriptRoot "Saved\$Name.log"
$launchArgs = @($projectPath, '/Game/Maps/NeonDistrict', '-game', '-dx12', '-sm6', '-RenderOffscreen', '-windowed', '-ResX=1920', '-ResY=1080', '-ForceRes', '-unattended', '-nop4', '-nosound', '-CinematicTest', '-ExplosionBenchmark', "-StressEnemies=$Enemies", "-ExplosionRate=$ExplosionRate", "-ExplosionBurst=$Burst", "-FluidResolution=$FluidResolution", "-FluidGrids=$FluidGrids", "-BenchWarmup=$Warmup", "-BenchDuration=$Duration", "-BenchName=$Name", "-abslog=$logPath",
    '-ini:Engine:[/Script/Engine.RendererSettings]:r.GenerateMeshDistanceFields=True,[/Script/Engine.RendererSettings]:r.DynamicGlobalIlluminationMethod=1,[/Script/Engine.RendererSettings]:r.ReflectionMethod=1,[/Script/Engine.RendererSettings]:r.Shadow.Virtual.Enable=1',
    "-ExecCmds=r.SetRes 1920x1080w,r.ScreenPercentage $ScreenPercentage,t.MaxFPS 0,r.VSync 0,stat unit")
if($DebugEffects) { $launchArgs += '-VFXDebug' }
& $editorPath @launchArgs
if ($LASTEXITCODE -ne 0 -or !(Select-String -LiteralPath $logPath -SimpleMatch "EXPLOSION_BENCH_PASS $Name" -Quiet)) { throw "Benchmark failed. See $logPath" }
$csvPath = Join-Path $PSScriptRoot "Saved\Benchmarks\$Name.csv"
$frames = @(Import-Csv -LiteralPath $csvPath)
if ($frames.Count -lt 10) { throw 'Insufficient benchmark samples.' }
function Get-Percentile($Values, [double]$P) { $sorted = @($Values | Sort-Object); return [math]::Round($sorted[[math]::Min($sorted.Count-1,[math]::Ceiling($P*$sorted.Count)-1)],3) }
$frameTimes = @($frames | ForEach-Object {[double]$_.frame_ms})
$gpuTimes = @($frames | ForEach-Object {[double]$_.gpu_ms})
$result = [ordered]@{
    Name=$Name; Enemies=$Enemies; RequestedBlastsPerSecond=$ExplosionRate*$Burst; FluidResolution=$FluidResolution; FluidGrids=$FluidGrids;
    OutputResolution='1920x1080'; ScreenPercentage=$ScreenPercentage; WarmupSeconds=$Warmup; SampleSeconds=$Duration; Samples=$frames.Count;
    FrameMedianMs=(Get-Percentile $frameTimes 0.5); FrameP95Ms=(Get-Percentile $frameTimes 0.95); FrameP99Ms=(Get-Percentile $frameTimes 0.99);
    GPUMedianMs=(Get-Percentile $gpuTimes 0.5); GPUP95Ms=(Get-Percentile $gpuTimes 0.95);
    GameMedianMs=(Get-Percentile @($frames|ForEach-Object {[double]$_.game_ms}) 0.5);
    RenderMedianMs=(Get-Percentile @($frames|ForEach-Object {[double]$_.render_ms}) 0.5);
    MinimumActiveEnemies=($frames|ForEach-Object {[int]$_.active_enemies}|Measure-Object -Minimum).Minimum;
    AcceptedInSample=[int]$frames[-1].accepted-[int]$frames[0].accepted;
    DroppedInSample=[int]$frames[-1].dropped-[int]$frames[0].dropped;
    FluidInSample=[int]$frames[-1].fluid_accepted-[int]$frames[0].fluid_accepted;
}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot "Saved\Benchmarks\$Name.json")
[pscustomobject]$result | Format-List


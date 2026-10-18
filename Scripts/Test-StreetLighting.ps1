param([switch]$IncludeBenchmark)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$cases=@(
    @{Name='StreetLightingBefore';Flags=@('-MVPSmoke','-StreetLightingSmoke','-StreetLightingBefore');Pass='STREET_LIGHTING_PASS'},
    @{Name='StreetLightingAfter';Flags=@('-MVPSmoke','-StreetLightingSmoke');Pass='STREET_LIGHTING_PASS'},
    @{Name='StreetLightingCityRegression';Flags=@('-MVPSmoke','-CitySmoke');Pass='CITY_SMOKE_PASS'}
)
if($IncludeBenchmark){
    foreach($mode in @('Before','After')){
        $flags=@('-ExplosionBenchmark','-BenchAllSpells','-BenchCyberRoster','-StressEnemies=180','-ExplosionRate=4','-ExplosionBurst=2','-BenchWarmup=5','-BenchDuration=10',('-BenchName=StreetLighting180'+$mode),'-ExecCmds="t.MaxFPS 0,r.VSync 0,stat unit"')
        if($mode -eq 'Before'){$flags+='-StreetLightingBefore'}
        $cases+=@{Name=('StreetLighting180'+$mode);Flags=$flags;Pass='EXPLOSION_BENCH_PASS'}
    }
}
foreach($case in $cases){
    $log=Join-Path $projectRoot ('Saved\'+$case.Name+'.log')
    $arguments=@((Join-Path $projectRoot 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game','-dx12','-sm6','-windowed','-ResX=1600','-ResY=900','-ForceRes','-RenderOffscreen','-unattended','-nop4',('-abslog='+$log))+$case.Flags
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw ($case.Name+' timed out')}
    $output=Get-Content -LiteralPath $log -Raw
    if($output -notmatch $case.Pass -or $output -match 'STREET_LIGHTING_FAIL|CITY_SMOKE_FAIL|Fatal error|LogMaterial: Error:'){throw ('Validation failed: '+$log)}
    Write-Output ($case.Name+' PASS')
}

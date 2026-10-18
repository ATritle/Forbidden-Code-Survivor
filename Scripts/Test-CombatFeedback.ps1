param([switch]$IncludeBenchmark,[switch]$IncludeRegressions)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project=Join-Path $projectRoot 'DeckBuilder.uproject'
$cases=@(@{Name='CombatFeedbackSmoke';Flags=@('-MVPSmoke','-CombatFeedbackSmoke');Pass='COMBAT_FEEDBACK_PASS'})
if($IncludeRegressions){
    $cases+=@{Name='FeedbackArsenalRegression';Flags=@('-MVPSmoke','-ArsenalSmoke');Pass='ARSENAL_PASS'}
    $cases+=@{Name='FeedbackSettingsRegression';Flags=@('-MVPSmoke','-SettingsSmoke');Pass='SETTINGS_CARD_PASS'}
    $cases+=@{Name='FeedbackCityRegression';Flags=@('-MVPSmoke','-CitySmoke');Pass='CITY_SMOKE_PASS'}
}
if($IncludeBenchmark){$cases+=@{Name='CombatFeedback180';Flags=@('-ExplosionBenchmark','-BenchAllSpells','-BenchCyberRoster','-StressEnemies=180','-ExplosionRate=4','-ExplosionBurst=2','-BenchWarmup=5','-BenchDuration=10','-BenchName=CombatFeedback180','-ExecCmds="t.MaxFPS 0,r.VSync 0,stat unit"');Pass='EXPLOSION_BENCH_PASS'}}
foreach($case in $cases){
    $log=Join-Path $projectRoot ('Saved\'+$case.Name+'.log')
    $arguments=@($project,'/Game/Maps/NeonDistrict','-game','-dx12','-sm6','-windowed','-ResX=1600','-ResY=900','-ForceRes','-RenderOffscreen','-unattended','-nop4','-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0',('-abslog='+$log))+$case.Flags
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw ($case.Name+' timed out')}
    $output=Get-Content -LiteralPath $log -Raw
    if($output -notmatch $case.Pass -or $output -match 'COMBAT_FEEDBACK_FAIL|ARSENAL_FAIL|CITY_SMOKE_FAIL|SETTINGS_CARD_FAIL|Fatal error|LogMaterial: Error:'){throw ('Validation failed: '+$log)}
    Write-Output ($case.Name+' PASS')
}

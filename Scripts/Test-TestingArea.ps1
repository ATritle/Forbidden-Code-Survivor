param([switch]$IncludeRegressions)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project=Join-Path $projectRoot 'DeckBuilder.uproject'
$cases=@(@{Name='TestingMultiSmoke';Flag='-TestingMultiSmoke';Pass='TESTING_MULTI_PASS'},@{Name='TestingAreaSmoke';Flag='-TestingAreaSmoke';Pass='TESTING_AREA_PASS'})
if($IncludeRegressions){
    $cases+=@{Name='TestingSettingsRegression';Flag='-SettingsSmoke';Pass='SETTINGS_CARD_PASS'}
    $cases+=@{Name='TestingCombatRegression';Flag='-CombatFeedbackSmoke';Pass='COMBAT_FEEDBACK_PASS'}
    $cases+=@{Name='TestingHealthAlarmRegression';Flag='-HealthAlarmSmoke';Pass='HEALTH_ALARM_PASS'}
    $cases+=@{Name='TestingCityRegression';Flag='-CitySmoke';Pass='CITY_SMOKE_PASS'}
}
foreach($case in $cases){
    $log=Join-Path $projectRoot ('Saved\'+$case.Name+'.log')
    $arguments=@($project,'/Game/Maps/NeonDistrict','-game','-dx12','-sm6','-MVPSmoke',$case.Flag,'-windowed','-ResX=1600','-ResY=900','-ForceRes','-RenderOffscreen','-unattended','-nop4','-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0',('-abslog='+$log))
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw ($case.Name+' timed out')}
    $output=Get-Content -LiteralPath $log -Raw
    if($output -notmatch $case.Pass -or $output -match 'TESTING_MULTI_FAIL|TESTING_AREA_FAIL|SETTINGS_CARD_FAIL|COMBAT_FEEDBACK_FAIL|HEALTH_ALARM_FAIL|CITY_SMOKE_FAIL|Fatal error|LogMaterial: Error:|LogNiagara: Error:'){throw ('Validation failed: '+$log)}
    Write-Output ($case.Name+' PASS')
}

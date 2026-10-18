param([switch]$IncludeRegressions)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
$project=Join-Path $projectRoot 'DeckBuilder.uproject'
$cases=@(
    @{Name='SevenChallengeSmoke';Flag='-SevenChallengeSmoke';Pass='SEVEN_CHALLENGES_PASS'},
    @{Name='ChallengeSmoke';Flag='-ChallengeSmoke';Pass='CHALLENGE_PASS'},
    @{Name='ChallengePlacementRepeat';Flag='-ChallengeSmoke';Pass='CHALLENGE_PASS'}
)
if($IncludeRegressions){
    $cases+=@{Name='ChallengeTestingRegression';Flag='-TestingMultiSmoke';Pass='TESTING_MULTI_PASS'}
    $cases+=@{Name='ChallengeCombatRegression';Flag='-CombatFeedbackSmoke';Pass='COMBAT_FEEDBACK_PASS'}
    $cases+=@{Name='ChallengeSettingsRegression';Flag='-SettingsSmoke';Pass='SETTINGS_CARD_PASS'}
    $cases+=@{Name='ChallengeCityRegression';Flag='-CitySmoke';Pass='CITY_SMOKE_PASS'}
}
foreach($case in $cases){
    $log=Join-Path $projectRoot ('Saved\'+$case.Name+'.log')
    $arguments=@($project,'/Game/Maps/NeonDistrict','-game','-dx12','-sm6','-MVPSmoke',$case.Flag,'-windowed','-ResX=1600','-ResY=900','-ForceRes','-RenderOffscreen','-unattended','-nop4',('-abslog='+$log))
    $process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw ($case.Name+' timed out')}
    $output=Get-Content -LiteralPath $log -Raw
    if($output -notmatch $case.Pass -or $output -match 'SEVEN_CHALLENGES_FAIL|CHALLENGE_FAIL|TESTING_MULTI_FAIL|COMBAT_FEEDBACK_FAIL|SETTINGS_CARD_FAIL|CITY_SMOKE_FAIL|Fatal error|LogMaterial: Error:|LogNiagara: Error:'){throw ('Validation failed: '+$log)}
    Write-Output ($case.Name+' PASS')
}
$first=Select-String -LiteralPath (Join-Path $projectRoot 'Saved\ChallengeSmoke.log') -Pattern 'CHALLENGE_STATION '
$second=Select-String -LiteralPath (Join-Path $projectRoot 'Saved\ChallengePlacementRepeat.log') -Pattern 'CHALLENGE_STATION '
if(($first.Line -join "`n") -eq ($second.Line -join "`n")){throw 'Separate runs repeated the same station placement sample'}
Write-Output 'Independent random placement samples PASS'

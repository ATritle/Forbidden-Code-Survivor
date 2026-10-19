param([string]$Executable='',[string]$UserDirectory='Saved/QA/SettingsReview',[switch]$LayoutOnly)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor){$Executable='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$root=[IO.Path]::GetFullPath((Join-Path $projectRoot $UserDirectory))
$cases=@(
    @{Name='Wide';Flag='SettingsReviewSmoke';Width=1600;Height=900;Report='report.json';Hidden=$true},
    @{Name='Compact';Flag='SettingsReviewSmoke';Width=1280;Height=720;Report='report.json';Hidden=$true},
    @{Name='Wide';Flag='SettingsPreferencesReload';Width=1600;Height=900;Report='preferences-reload.json';Hidden=$true},
    @{Name='Display';Flag='SettingsDisplaySmoke';Width=1280;Height=720;Report='report.json';Hidden=$false},
    @{Name='Display';Flag='SettingsDisplayReload';Width=1280;Height=720;Report='display-reload.json';Hidden=$true}
)
if($LayoutOnly){$cases=@($cases|Where-Object {$_.Flag -eq "SettingsReviewSmoke"})}
foreach($case in $cases){
    $caseRoot=Join-Path $root $case.Name
    $report=Join-Path $caseRoot ('Saved/Review/SettingsReview/'+$case.Report)
    if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
    $arguments=@(('-'+$case.Flag),'-ForbiddenQA',('-UserDir='+$caseRoot),'-dx12','-sm6','-windowed',('-ResX='+$case.Width),('-ResY='+$case.Height),'-ForceRes','-unattended','-nosplash')
    if($case.Hidden){$arguments+='-RenderOffscreen'}
    if($case.Flag -eq 'SettingsDisplayReload'){$arguments=$arguments|Where-Object {$_ -ne '-windowed' -and $_ -ne '-ForceRes' -and $_ -notlike '-Res?=*'}}
    if($editor){$arguments=@((Join-Path $projectRoot 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$arguments}
    $test=Start-Process -FilePath $Executable -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -ArgumentList $arguments
    if(-not $test.WaitForExit(180000)){Stop-Process -Id $test.Id -Force;throw ($case.Flag+' timed out')}
    if($test.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $report)){throw ($case.Flag+' failed; inspect '+$caseRoot)}
    $result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
    if($result.result -ne 'SETTINGS_REVIEW_PASS'){throw (Get-Content -LiteralPath $report -Raw)}
    Write-Output ($case.Name+' '+$case.Flag+' '+$result.result)
}

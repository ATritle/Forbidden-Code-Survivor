param([string]$Executable='',[string]$UserDirectory='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor){$Executable='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$userRoot=if($UserDirectory){[IO.Path]::GetFullPath($UserDirectory)}else{Join-Path $projectRoot 'Saved\QA\MusicPlayer'}
$reports=Join-Path $userRoot 'Saved\Review\MusicPlayer'
foreach($reload in @($false,$true)){
    $name=if($reload){'reload.json'}else{'report.json'}
    $report=Join-Path $reports $name
    if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
    $arguments=@('-MusicPlayerSmoke','-ForbiddenQA',('-UserDir='+$userRoot),'-dx12','-sm6','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-unattended','-nosplash')
    if($reload){$arguments+='-MusicPlayerReload'}
    if($editor){$arguments=@((Join-Path $projectRoot 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$arguments}
    $test=Start-Process -FilePath $Executable -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -ArgumentList $arguments
    if(-not $test.WaitForExit(120000)){Stop-Process -Id $test.Id -Force;throw 'Music player verification timed out'}
    if($test.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $report)){throw ('Music player test failed; inspect '+$reports)}
    $result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
    $expected=if($reload){'MUSIC_PLAYER_RELOAD_PASS'}else{'MUSIC_PLAYER_PASS'}
    if($result.result -ne $expected){throw (Get-Content -LiteralPath $report -Raw)}
    Get-Content -LiteralPath $report -Raw
}

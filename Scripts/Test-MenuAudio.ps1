param([string]$Executable='',[string]$UserDirectory='')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor){$Executable='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$userRoot=if($UserDirectory){[IO.Path]::GetFullPath($UserDirectory)}else{Join-Path $projectRoot 'Saved\QA\MenuAudio'}
$reports=Join-Path $userRoot 'Saved\Review\MenuAudio'
$report=Join-Path $reports 'report.json'
if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
$arguments=@('-MenuAudioSmoke','-ForbiddenQA',('-UserDir='+$userRoot),'-dx12','-sm6','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-unattended','-nosplash')
if($editor){$arguments=@((Join-Path $projectRoot 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$arguments}
$test=Start-Process -FilePath $Executable -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -ArgumentList $arguments
if(-not $test.WaitForExit(120000)){Stop-Process -Id $test.Id -Force;throw 'Menu audio verification timed out'}
if($test.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $report)){throw ('Menu audio test failed; inspect '+$reports)}
$result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
if($result.result -ne 'MENU_AUDIO_PASS'){throw (Get-Content -LiteralPath $report -Raw)}
Get-Content -LiteralPath $report -Raw

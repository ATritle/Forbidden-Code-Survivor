param([string]$Executable='',[string]$UserDirectory='Saved/QA/ScoreProfiles',[switch]$Compact)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor){$Executable='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'}
$user=[IO.Path]::GetFullPath($UserDirectory)
$argsList=@('-ForbiddenQA','-LeaderboardSmoke',('-UserDir='+$user),'-dx12','-sm6','-RenderOffscreen','-windowed',('-ResX='+$(if($Compact){1280}else{1600})),('-ResY='+$(if($Compact){720}else{900})),'-ForceRes','-unattended','-nosplash')
if($editor){$argsList=@((Join-Path $root 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$argsList}
$report=Join-Path $user 'Saved/Review/ScoreProfiles/report.json'
if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
$process=Start-Process -FilePath $Executable -WorkingDirectory $root -WindowStyle Hidden -PassThru -ArgumentList $argsList
if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw 'Leaderboard validation timed out'}
if(-not(Test-Path -LiteralPath $report)){throw ('No leaderboard report; inspect '+$user)}
$data=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
if($process.ExitCode -ne 0 -or $data.result -ne 'SCORE_PROFILES_PASS'){throw (Get-Content -LiteralPath $report -Raw)}
Get-Content -LiteralPath $report -Raw

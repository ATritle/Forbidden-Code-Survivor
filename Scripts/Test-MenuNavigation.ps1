param([string]$Executable='',[string]$UserDirectory='Saved/QA/MenuNavigation',[switch]$Compact)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor){$Executable='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'}
$user=[IO.Path]::GetFullPath($UserDirectory)
$report=Join-Path $user 'Saved/Review/MenuNavigation/report.json'
if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
$width=if($Compact){1280}else{1600};$height=if($Compact){720}else{900}
$arguments=@('-MenuNavigationSmoke','-ForbiddenQA','-ExecCmds=DisableAllScreenMessages',('-UserDir='+$user),'-dx12','-sm6','-RenderOffscreen','-windowed',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended','-nosplash')
if($editor){$arguments=@((Join-Path $root 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$arguments}
$process=Start-Process -FilePath $Executable -WorkingDirectory $root -WindowStyle Hidden -PassThru -ArgumentList $arguments
if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw 'Menu navigation validation timed out'}
if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $report)){throw ('Menu navigation validation failed; inspect '+$user)}
$result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
if($result.result -ne 'MENU_NAVIGATION_PASS'){throw (Get-Content -LiteralPath $report -Raw)}
Get-Content -LiteralPath $report -Raw

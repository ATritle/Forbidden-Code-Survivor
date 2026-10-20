param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[switch]$SkipBuild,[string]$Executable='',[string]$UserDirectory='Saved/QA/OverhaulFunctional')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor -and -not $SkipBuild){
    & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') DeckBuilderEditor Win64 Development ('-Project='+(Join-Path $projectRoot 'DeckBuilder.uproject')) -WaitMutex -MaxParallelActions=4
    if($LASTEXITCODE -ne 0){throw 'Windows overhaul compilation failed.'}
}
if($editor){$Executable=Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'}
$user=[IO.Path]::GetFullPath($UserDirectory)
$report=Join-Path $user 'Saved/Review/OverhaulFunctional/report.json'
if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
$arguments=@('-OverhaulSmoke','-ForbiddenQA',('-UserDir='+$user),'-dx12','-sm6','-windowed','-ResX=1280','-ResY=720','-ForceRes','-RenderOffscreen','-unattended','-nosplash')
if($editor){$arguments=@((Join-Path $projectRoot 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$arguments}
$process=Start-Process -FilePath $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru
if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw 'Combat playback timed out'}
if(-not(Test-Path -LiteralPath $report)){throw 'Rendered overhaul validation did not produce a report.'}
$result=Get-Content -Raw -LiteralPath $report|ConvertFrom-Json
if($process.ExitCode -ne 0 -or $result.result -ne 'OVERHAUL_FUNCTIONAL_PASS'){throw (Get-Content -Raw -LiteralPath $report)}
Write-Output ($result.result+' / '+$result.cards.Count+' actual spell damage cases / '+$result.largeEnemyRoutes+' terrain routes')

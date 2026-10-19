param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[switch]$SkipBuild)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if(-not $SkipBuild){
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') DeckBuilderEditor Win64 Development ('-Project='+(Join-Path $projectRoot 'DeckBuilder.uproject')) -WaitMutex -MaxParallelActions=4
    if($LASTEXITCODE -ne 0){throw 'Windows overhaul compilation failed.'}
}
$report=Join-Path $projectRoot 'Saved\Review\OverhaulFunctional\report.json'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
$arguments=@((Join-Path $projectRoot 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game','-OverhaulSmoke','-ForbiddenQA','-dx12','-sm6','-windowed','-ResX=1600','-ResY=900','-ForceRes','-RenderOffscreen','-unattended','-nop4',('-abslog='+(Join-Path $projectRoot 'Saved\Review\OverhaulFunctional.log')))
$process=Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output ('OVERHAUL_VALIDATION_STARTED pid='+$process.Id)
while(-not $process.HasExited){
    Start-Sleep -Seconds 2
    $process.Refresh()
}
if(-not(Test-Path -LiteralPath $report)){throw 'Rendered overhaul validation did not produce a report.'}
$result=Get-Content -Raw -LiteralPath $report|ConvertFrom-Json
if($process.ExitCode -ne 0 -or $result.result -ne 'OVERHAUL_FUNCTIONAL_PASS'){throw (Get-Content -Raw -LiteralPath $report)}
Get-Content -Raw -LiteralPath $report

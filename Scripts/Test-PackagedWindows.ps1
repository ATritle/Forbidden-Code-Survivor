$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$packageRoot=Join-Path $projectRoot 'Artifacts\Windows'
$executable=Join-Path $packageRoot 'ForbiddenCodeSurvivor.exe'
if(-not(Test-Path -LiteralPath $executable)){throw 'Build the Windows package first.'}
$reports=Join-Path $projectRoot 'Saved\ReleaseSmoke'
New-Item -ItemType Directory -Path $reports -Force | Out-Null
foreach($name in @('report.json','ForbiddenCode-Windows-Menu.png','ForbiddenCode-Windows-Gameplay.png')){
    $oldFile=Join-Path $reports $name
    if(Test-Path -LiteralPath $oldFile){Remove-Item -LiteralPath $oldFile}
}
$testProcess=Start-Process -FilePath $executable -WorkingDirectory $packageRoot -WindowStyle Hidden -PassThru -ArgumentList @('-ReleaseSmoke','-MVPSmoke',('-ReleaseSmokeDir='+$reports),'-dx12','-sm6','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-unattended','-nosplash')
if(-not $testProcess.WaitForExit(120000)){Stop-Process -Id $testProcess.Id -Force;throw 'Packaged release validation timed out.'}
$reportFile=Join-Path $reports 'report.json'
if(-not(Test-Path -LiteralPath $reportFile)){throw 'Packaged release did not produce its validation report.'}
$report=Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if($report.result -ne 'PACKAGED_RELEASE_PASS'){throw (Get-Content -LiteralPath $reportFile -Raw)}
Get-Content -LiteralPath $reportFile

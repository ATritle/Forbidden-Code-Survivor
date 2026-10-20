param([string]$Executable='',[string]$UserDirectory='Saved/QA/All16Shipping',[switch]$Compact)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not $Executable){$Executable=Join-Path $root 'Artifacts/Windows-0.4.7/ForbiddenCodeSurvivor.exe'}
$userRoot=[IO.Path]::GetFullPath($UserDirectory)
foreach($mode in @('Normal','Expanded')){
 $user=Join-Path $userRoot $mode
 $arguments=@('-AllCardsSmoke','-ForbiddenQA',('-UserDir='+$user),'-dx12','-sm6','-RenderOffscreen','-windowed',('-ResX='+$(if($Compact){1280}else{1600})),('-ResY='+$(if($Compact){720}else{900})),'-ForceRes','-unattended','-nosplash')
 if($mode -eq 'Expanded'){$arguments+='-AllCardsPlaytest'}
 $report=Join-Path $user 'Saved/Review/All16/report.json'
 foreach($name in @('report.json','All16-Combat.png','All16-Inspect.png')){
  $old=Join-Path $user ('Saved/Review/All16/'+$name)
  if(Test-Path -LiteralPath $old){Remove-Item -LiteralPath $old}
 }
 $process=Start-Process -FilePath $Executable -WorkingDirectory $root -WindowStyle Hidden -PassThru -ArgumentList $arguments
 if(-not $process.WaitForExit(90000)){Stop-Process -Id $process.Id -Force;throw 'All-card validation timed out'}
 if(-not(Test-Path -LiteralPath $report)){throw 'No all-card validation report'}
 $data=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
 if($data.result -ne 'ALL16_PASS'){throw (Get-Content -LiteralPath $report -Raw)}
 Write-Output ($data.result+' '+$mode+' capacity='+$data.capacity+' weapons='+$data.weapons+' hands='+$data.hands)
}

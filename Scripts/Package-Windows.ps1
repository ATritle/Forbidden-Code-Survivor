param([string]$EngineRoot='C:\Program Files\Epic Games\UE_5.8',[ValidateSet('Shipping','Development')][string]$Configuration='Shipping',[switch]$Installer,[string]$OutputDirectory='',[ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version='0.4.5')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$uat=Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$archive=if($OutputDirectory){[IO.Path]::GetFullPath($OutputDirectory)}else{Join-Path $projectRoot ('Artifacts\Windows-'+$Version)}
& $uat BuildCookRun ('-project='+ (Join-Path $projectRoot 'DeckBuilder.uproject')) -target=DeckBuilder -nop4 -platform=Win64 ('-clientconfig='+$Configuration) -build -cook -stage -pak -iostore -compressed -archive ('-archivedirectory='+$archive) -map=/Game/Maps/NeonDistrict -prereqs -nodebuginfo -utf8output '-ubtargs=-MaxParallelActions=4'
if($LASTEXITCODE -ne 0){throw 'UE Windows packaging failed. Review the AutomationTool log.'}
$packageRoot=if(Test-Path -LiteralPath (Join-Path $archive 'DeckBuilder.exe')){$archive}else{Join-Path $archive 'Windows'}
$bootstrap=Join-Path $packageRoot 'DeckBuilder.exe'
$branded=Join-Path $packageRoot 'ForbiddenCodeSurvivor.exe'
if(Test-Path -LiteralPath $bootstrap){Move-Item -LiteralPath $bootstrap -Destination $branded -Force}
if(-not (Test-Path -LiteralPath $branded)){throw ('Packaged game executable missing: '+$branded)}
Write-Output ('WINDOWS_PACKAGE_READY '+$branded)
if($Installer){
    & (Join-Path $PSScriptRoot 'Build-WindowsInstaller.ps1') -PackageDirectory $packageRoot -Version $Version
}

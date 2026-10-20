param([string]$Executable='',[string]$UserDirectory='Saved/QA/RiderSelection',[string[]]$Riders=@('NeonExorcist','Vex','Rook','Zero','Mira'),[switch]$Compact,[switch]$SkipReload)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor){$Executable='C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'}
$directory=[IO.Path]::GetFullPath($UserDirectory)
foreach($rider in $Riders){
    foreach($reload in $(if($SkipReload){@($false)}else{@($false,$true)})){
        $user=Join-Path $directory $rider
        $report=Join-Path $user ('Saved/Review/RiderSelection/'+$(if($reload){'reload.json'}else{'report.json'}))
        if(Test-Path -LiteralPath $report){Remove-Item -LiteralPath $report}
        $width=if($Compact){1280}else{1600};$height=if($Compact){720}else{900}
        $arguments=@('-RiderSelectionSmoke','-ForbiddenQA',('-Rider='+$rider),('-UserDir='+$user),'-dx12','-sm6','-RenderOffscreen','-windowed',('-ResX='+$width),('-ResY='+$height),'-ForceRes','-unattended','-nosplash')
        if($reload){$arguments+='-RiderSelectionReload'}
        if($editor){$arguments=@((Join-Path $root 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$arguments}
        $process=Start-Process -FilePath $Executable -WorkingDirectory $root -WindowStyle Hidden -PassThru -ArgumentList $arguments
        if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw ($rider+' selection check timed out')}
        if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $report)){throw ($rider+' validation failed; inspect '+$user)}
        $result=Get-Content -LiteralPath $report -Raw|ConvertFrom-Json
        if($result.result -ne 'RIDER_SELECTION_PASS'){throw (Get-Content -LiteralPath $report -Raw)}
        Get-Content -LiteralPath $report -Raw
    }
}

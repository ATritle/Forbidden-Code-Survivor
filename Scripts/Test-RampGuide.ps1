param([string]$Executable='',[switch]$QuitOnly,[string]$UserDirectory='',[switch]$SkipQuit)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$editor=-not $Executable
if($editor){$Executable='C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'}
$userRoot=if($UserDirectory){[IO.Path]::GetFullPath($UserDirectory)}else{Join-Path $projectRoot 'Saved\QA\RampGuide'}
$reportRoot=Join-Path $userRoot 'Saved\Review\RampGuide'
$cases=if($QuitOnly){@('QuitMenuSmoke','QuitSettingsSmoke')}elseif($SkipQuit){@('GuideSmoke')}else{@('GuideSmoke','QuitMenuSmoke','QuitSettingsSmoke')}
foreach($case in $cases){
    $argsList=@('-ForbiddenQA',('-'+$case),('-UserDir='+$userRoot),'-dx12','-sm6','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-unattended','-nosplash')
    if($editor){$argsList=@((Join-Path $projectRoot 'DeckBuilder.uproject'),'/Game/Maps/NeonDistrict','-game')+$argsList}
    $artifact=Join-Path $reportRoot $(if($case -eq 'GuideSmoke'){'report.json'}else{'quit-requested.txt'})
    if(Test-Path -LiteralPath $artifact){Remove-Item -LiteralPath $artifact}
    $process=Start-Process -FilePath $Executable -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru -ArgumentList $argsList
    if(-not $process.WaitForExit(180000)){Stop-Process -Id $process.Id -Force;throw ($case+' timed out')}
    if($process.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $artifact)){throw ($case+' failed; check '+$reportRoot)}
    if($case -eq 'GuideSmoke'){
        $report=Get-Content -LiteralPath $artifact -Raw|ConvertFrom-Json
        if($report.result -ne 'RAMP_GUIDE_PASS'){throw (Get-Content -LiteralPath $artifact -Raw)}
        Get-Content -LiteralPath $artifact -Raw
    }else{
        @{result='CLOSE_GAME_PASS';source=$case;exitCode=$process.ExitCode}|ConvertTo-Json|Set-Content (Join-Path $reportRoot ($case+'-report.json'))
        Write-Output 'CLOSE_GAME_PASS'
    }
}

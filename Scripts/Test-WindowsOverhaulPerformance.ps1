param([int]$Seconds=60,[int]$Enemies=180)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
$exe=Join-Path $projectRoot 'Artifacts\Windows-0.3.0\ForbiddenCodeSurvivor.exe'
if(-not(Test-Path -LiteralPath $exe)){throw 'Build the Windows 0.3.0 package first.'}
foreach($branch in @('A','B')){
    $testUser=Join-Path $projectRoot ('Saved\QA\PackagedBench'+$branch)
    $arguments=@('-ExplosionBenchmark','-BenchAllSpells','-BenchEvolved','-BenchCyberRoster',('-StressEnemies='+$Enemies),'-ExplosionRate=4','-ExplosionBurst=2','-BenchWarmup=10',('-BenchDuration='+$Seconds),('-BenchName=Overhaul-'+$branch),('-UserDir='+$testUser),'-dx12','-sm6','-novsync','-RenderOffscreen','-windowed','-ResX=1600','-ResY=900','-ForceRes','-unattended','-nosplash')
    if($branch -eq 'B'){$arguments+='-BenchBranchB'}
    $testProcess=Start-Process -FilePath $exe -WindowStyle Hidden -PassThru -ArgumentList $arguments
    $testProcess.WaitForExit()
    $reportPath=Join-Path $testUser ('Saved\Benchmarks\Overhaul-'+$branch+'.json')
    if(-not(Test-Path -LiteralPath $reportPath)){throw ('No Shipping benchmark telemetry: '+$reportPath)}
    $report=Get-Content -LiteralPath $reportPath -Raw|ConvertFrom-Json
    if($testProcess.ExitCode -ne 0 -or $report.enemies -lt ($Enemies-3) -or $report.weapons -ne 15 -or $report.projectileDrops -ne 0 -or $report.arsenalDrops -ne 0 -or $report.fieldDrops -ne 0){throw ('Benchmark gameplay admission failed: '+(Get-Content $reportPath -Raw))}
    Write-Output ('PACKAGED_STRESS_'+$branch+'_PASS '+(Get-Content $reportPath -Raw))
}

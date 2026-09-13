param(
    [Parameter(Mandatory=$true)] [string]$Url,
    [Parameter(Mandatory=$true)] [string]$HtmlPath,
    [string]$OutDir = ".",
    [int]$Vw = 1024,
    [int]$Vh = 768
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$bin  = Join-Path $root "build\Release"

if (-not (Test-Path $bin)) {
    Write-Error "Cannot find build\Release: $bin"
}

# 关键：把 OutDir 转成绝对路径（相对于调用者的 CWD）
$OutDir = (Resolve-Path $OutDir).Path

$dmLearn  = Join-Path $bin "dm_learn.exe"
$dmRender = Join-Path $bin "dm_render_test.exe"

if (-not (Test-Path $dmLearn))  { Write-Error "Cannot find $dmLearn" }
if (-not (Test-Path $dmRender)) { Write-Error "Cannot find $dmRender" }

$webviewJson = Join-Path $OutDir "webview2.json"
$renderJson  = Join-Path $OutDir "render.json"
$reportMd    = Join-Path $OutDir "report.md"

Write-Host ""
Write-Host "[F-3] Step 1/4: export WebView2 snapshot from learn DB" -ForegroundColor Cyan
Push-Location $bin
& $dmLearn export $Url $webviewJson
$rc1 = $LASTEXITCODE
Pop-Location
if ($rc1 -ne 0) { Write-Error "export failed (EXIT=$rc1)" }

Write-Host ""
Write-Host "[F-3] Step 2/4: render with DM engine" -ForegroundColor Cyan
& $dmRender $HtmlPath $Vw $Vh --dump
if ($LASTEXITCODE -ne 0) { Write-Error "dm_render_test failed (EXIT=$LASTEXITCODE)" }

$dumped = "$HtmlPath.snapshot.json"
if (-not (Test-Path $dumped)) {
    Write-Error "snapshot not generated: $dumped"
}
Copy-Item $dumped $renderJson -Force

Write-Host ""
Write-Host "[F-3] Step 3/4: compare" -ForegroundColor Cyan
Push-Location $bin
& $dmLearn compare $webviewJson $renderJson
Pop-Location

Write-Host ""
Write-Host "[F-3] Step 4/4: generate report" -ForegroundColor Cyan
Push-Location $bin
& $dmLearn report $webviewJson $renderJson $reportMd
Pop-Location

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "  Report: $reportMd" -ForegroundColor Green

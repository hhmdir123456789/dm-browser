# run_iter1_v2.ps1 - ASCII only, no Chinese
$ErrorActionPreference = "Continue"

$root = "C:\Users\86545\PycharmProjects\pythonProject3\venv\DM_LL\dm_browser"
Set-Location $root

Write-Host ""
Write-Host "===== STEP 1: create test_iter1.html =====" -ForegroundColor Cyan

$html = @'
<!DOCTYPE html>
<html>
<head>
<style>
  body { margin: 0; padding: 0; font-size: 14px; color: #333; background: #fff; }
  .item { padding: 8px; margin: 4px; background: #f0f0f0; }
  .item:first-child { background: #ffe0e0; }
  .item:not(.keep) { color: #c00; }
  .rel { position: relative; left: 20px; top: 10px; background: #e0f0ff; padding: 6px; }
  .op { opacity: 0.5; background: #333; color: #fff; padding: 10px; }
  .it { font-style: italic; }
</style>
</head>
<body>
  <div class="item keep">item 1 first keep</div>
  <div class="item">item 2 not keep</div>
  <div class="item">item 3 not keep</div>
  <div class="rel">relative offset</div>
  <div class="op">opacity 0.5</div>
  <div class="it">italic text</div>
</body>
</html>
'@

$htmlPath = "$root\tests\pages\test_iter1.html"
$html | Out-File -Encoding ASCII $htmlPath
Write-Host "  created: $htmlPath"

Write-Host ""
Write-Host "===== STEP 2: build =====" -ForegroundColor Cyan
cmake --build build --config Release --target dm_render_test
if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FAILED" -ForegroundColor Red
    exit 1
}
Write-Host "BUILD OK" -ForegroundColor Green

Write-Host ""
Write-Host "===== STEP 3: dump test_iter1 =====" -ForegroundColor Cyan
& "$root\build\Release\dm_render_test.exe" $htmlPath 1156 753 --dump

$snap = "$htmlPath.snapshot.json"
if (-not (Test-Path $snap)) {
    Write-Host "SNAPSHOT MISSING" -ForegroundColor Red
    exit 1
}

$json = Get-Content $snap -Encoding UTF8 -Raw | ConvertFrom-Json

Write-Host ""
Write-Host "===== STEP 4: node table =====" -ForegroundColor Cyan

$rows = @()
foreach ($n in $json.nodes) {
    if ($n.path -match 'div\.[0-9]$') {
        $rows += [pscustomobject]@{
            path  = $n.path
            bg    = $n.style.backgroundColor
            color = $n.style.color
            opac  = $n.style.opacity
            fsty  = $n.style.fontStyle
            x     = $n.layout.x
            y     = $n.layout.y
        }
    }
}
$rows | Format-Table -AutoSize

Write-Host ""
Write-Host "===== STEP 5: DoD checks =====" -ForegroundColor Cyan

$divs = @{}
foreach ($n in $json.nodes) {
    if ($n.path -match 'div\.([0-9])$') {
        $divs[$matches[1]] = $n
    }
}

function Check {
    param($name, $ok)
    if ($ok) {
        Write-Host ("  PASS  " + $name) -ForegroundColor Green
        return $true
    } else {
        Write-Host ("  FAIL  " + $name) -ForegroundColor Red
        return $false
    }
}

$all = $true

# 5.1 first-child
$bg0 = $divs['0'].style.backgroundColor
$bg1 = $divs['1'].style.backgroundColor
$ok51 = ($bg0 -match '255,\s*224,\s*224') -and ($bg1 -notmatch '255,\s*224,\s*224')
if (-not (Check "first-child hits div0 only" $ok51)) { $all = $false }

# 5.2 not(.keep)
$c0 = $divs['0'].style.color
$c1 = $divs['1'].style.color
$c2 = $divs['2'].style.color
$ok52 = ($c0 -match '51,\s*51,\s*51') -and ($c1 -match '204,\s*0,\s*0') -and ($c2 -match '204,\s*0,\s*0')
if (-not (Check "not(.keep) hits div1 div2 only" $ok52)) { $all = $false }

# 5.3 relative
$x3 = [double]$divs['3'].layout.x
$y3 = [double]$divs['3'].layout.y
$ok53 = ($x3 -ge 20) -and ($y3 -ge 10)
if (-not (Check "position relative offset" $ok53)) { $all = $false }

# 5.4 opacity
$op4 = [double]$divs['4'].style.opacity
$ok54 = ($op4 -gt 0.4) -and ($op4 -lt 0.6)
if (-not (Check "opacity 0.5" $ok54)) { $all = $false }

# 5.5 italic
$fs5 = $divs['5'].style.fontStyle
$ok55 = ($fs5 -eq 'italic')
if (-not (Check "font-style italic" $ok55)) { $all = $false }

Write-Host ""
Write-Host "===== SUMMARY =====" -ForegroundColor Cyan
if ($all) {
    Write-Host "ITER1 PASS" -ForegroundColor Green
} else {
    Write-Host "ITER1 FAIL" -ForegroundColor Yellow
}
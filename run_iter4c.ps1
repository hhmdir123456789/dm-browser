$ErrorActionPreference = "Continue"
$root = "C:\Users\86545\PycharmProjects\pythonProject3\venv\DM_LL\dm_browser"
Set-Location $root

Write-Host ""
Write-Host "===== STEP 1: create test_iter4c.html (ASCII) =====" -ForegroundColor Cyan

$html = @'
<!DOCTYPE html>
<html>
<head>
<style>
  body { margin: 0; padding: 0; font-size: 14px; color: #333; background: #fff; }
  .row { margin: 8px; }
</style>
</head>
<body>
  <div class="row"><input type="text" placeholder="name"></div>
  <div class="row"><input type="text" value="filled"></div>
  <div class="row"><button>OK</button></div>
  <div class="row"><select><option>opt1</option></select></div>
</body>
</html>
'@
$html | Out-File -Encoding ASCII "$root\tests\pages\test_iter4c.html"
Write-Host "  created"

Write-Host ""
Write-Host "===== STEP 2: build =====" -ForegroundColor Cyan
cmake --build build --config Release --target dm_render_test
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED" -ForegroundColor Red; exit 1 }

Write-Host ""
Write-Host "===== STEP 3: dump =====" -ForegroundColor Cyan
& "$root\build\Release\dm_render_test.exe" "$root\tests\pages\test_iter4c.html" 800 600 --dump

$snap = "$root\tests\pages\test_iter4c.html.snapshot.json"
if (-not (Test-Path $snap)) { Write-Host "SNAPSHOT MISSING" -ForegroundColor Red; exit 1 }

$json = Get-Content $snap -Encoding UTF8 -Raw | ConvertFrom-Json

Write-Host ""
Write-Host "===== STEP 4: total nodes =====" -ForegroundColor Cyan
Write-Host "  totalNodes: $($json.totalNodes)"
Write-Host "  maxDepth:   $($json.maxDepth)"

Write-Host ""
Write-Host "===== STEP 5: form nodes =====" -ForegroundColor Cyan

$json.nodes | Where-Object { $_.tag -in @('input','button','select') } |
  Select-Object @{n='tag';e={$_.tag}},
                @{n='display';e={$_.style.display}},
                @{n='w';e={$_.layout.w}},
                @{n='h';e={$_.layout.h}},
                @{n='bg';e={$_.style.backgroundColor}},
                @{n='bW';e={$_.style.borderWidth}} |
  Format-Table -AutoSize

Write-Host ""
Write-Host "===== STEP 6: checks =====" -ForegroundColor Cyan

function Check($name, $ok) {
    if ($ok) { Write-Host "  PASS  $name" -ForegroundColor Green; return $true }
    else { Write-Host "  FAIL  $name" -ForegroundColor Red; return $false }
}

$all = $true

$inputs  = @($json.nodes | Where-Object { $_.tag -eq 'input' })
$buttons = @($json.nodes | Where-Object { $_.tag -eq 'button' })
$selects = @($json.nodes | Where-Object { $_.tag -eq 'select' })

if (-not (Check "2 inputs found (got $($inputs.Count))" ($inputs.Count -eq 2))) { $all = $false }
if (-not (Check "1 button found (got $($buttons.Count))" ($buttons.Count -eq 1))) { $all = $false }
if (-not (Check "1 select found (got $($selects.Count))" ($selects.Count -eq 1))) { $all = $false }

if ($inputs.Count -ge 1) {
    $i0 = $inputs[0]
    if (-not (Check "input display=inline-block" ($i0.style.display -eq 'inline-block'))) { $all = $false }
    if (-not (Check "input w=150" ([double]$i0.layout.w -eq 150))) { $all = $false }
    if (-not (Check "input h=24" ([double]$i0.layout.h -eq 24))) { $all = $false }
    if (-not (Check "input border=1" ([int]$i0.style.borderWidth -eq 1))) { $all = $false }
}

if ($buttons.Count -ge 1) {
    $b0 = $buttons[0]
    if (-not (Check "button w=80" ([double]$b0.layout.w -eq 80))) { $all = $false }
    if (-not (Check "button h=28" ([double]$b0.layout.h -eq 28))) { $all = $false }
}

if ($selects.Count -ge 1) {
    $s0 = $selects[0]
    if (-not (Check "select w=120" ([double]$s0.layout.w -eq 120))) { $all = $false }
}

Write-Host ""
if ($all) { Write-Host "ITER4C PASS" -ForegroundColor Green }
else { Write-Host "ITER4C FAIL" -ForegroundColor Yellow }
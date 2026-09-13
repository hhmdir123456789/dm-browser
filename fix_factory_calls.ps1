# fix_factory_calls.ps1  (English only - no encoding issues)

$root = Get-Location
$exts = @("*.cpp", "*.h", "*.hpp", "*.cc")

$changed = 0
$totalHits = 0
foreach ($ext in $exts) {
    Get-ChildItem -Path $root -Recurse -Filter $ext -File |
        Where-Object { $_.FullName -notmatch '\\build\\' -and $_.FullName -notmatch '\\\.git\\' } |
        ForEach-Object {
            $file = $_.FullName
            $content = Get-Content -Raw -Encoding UTF8 $file
            $orig = $content

            $content = [regex]::Replace($content, '::fail\(', '::Fail(')
            $content = [regex]::Replace($content, '::ok\(',   '::Ok(')

            if ($content -ne $orig) {
                $hits = ([regex]::Matches($orig, '::fail\(|::ok\(')).Count
                $totalHits += $hits
                Set-Content -Path $file -Value $content -Encoding UTF8 -NoNewline
                Write-Host ("Modified: {0}  ({1} hits)" -f $file, $hits) -ForegroundColor Yellow
                $changed++
            }
        }
}
Write-Host ""
Write-Host ("Done. Modified {0} files, {1} call sites." -f $changed, $totalHits) -ForegroundColor Green
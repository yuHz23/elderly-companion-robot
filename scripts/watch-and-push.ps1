# watch-and-push.ps1 — File watcher that auto-commits on changes
# Runs indefinitely, polling every 30 seconds for file changes
# Press Ctrl+C to stop

$ProjectDir = "D:\elderly-companion-robot"
Set-Location $ProjectDir

Write-Host "=== Elderly Companion Robot — Auto-Sync Watcher ===" -ForegroundColor Cyan
Write-Host "Watching: $ProjectDir" -ForegroundColor Gray
Write-Host "Interval:  30 seconds" -ForegroundColor Gray
Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
Write-Host ""

while ($true) {
    $changes = git status --porcelain
    if ($changes) {
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        $count = ($changes | Measure-Object).Count
        Write-Host "[$timestamp] Detected $count change(s)" -ForegroundColor Yellow

        git add -A
        git commit -m "auto-sync: $timestamp" 2>$null

        $branch = git branch --show-current
        git push origin $branch 2>$null

        Write-Host "[$timestamp] Pushed to origin/$branch" -ForegroundColor Green
    }

    Start-Seconds 30
}
# auto-push.ps1 — Auto-sync project changes to GitHub
# Usage: Run this script to auto-commit and push changes
# Or set up a scheduled task to run it periodically

param(
    [string]$CommitMessage = "auto-sync: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')",
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$ProjectDir = "D:\elderly-companion-robot"

Set-Location $ProjectDir

# Check for changes
$changes = git status --porcelain
if (-not $changes -and -not $Force) {
    Write-Host "[OK] No changes to sync." -ForegroundColor Green
    exit 0
}

# Stage all changes
git add -A

# Check if there's anything staged
$staged = git diff --cached --name-only
if (-not $staged) {
    Write-Host "[OK] Nothing staged — already up to date." -ForegroundColor Green
    exit 0
}

# Commit
git commit -m $CommitMessage

# Push
$branch = git branch --show-current
git push origin $branch

Write-Host "[DONE] Pushed to origin/$branch" -ForegroundColor Cyan
Write-Host "  Commit: $CommitMessage" -ForegroundColor Gray
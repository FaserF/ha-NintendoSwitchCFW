# Build Script for Nintendo Switch Components
param(
    [Parameter(Mandatory=$false)]
    [string]$Version = "0.1.6"
)

$RepoPath = (Resolve-Path ".").Path
$OutPath = Join-Path $RepoPath "switch_build_out"

# Cleanup old artifacts
if (Test-Path $OutPath) {
    Write-Host "[CLEAN] Removing old artifacts..." -ForegroundColor Yellow
    Remove-Item -Path $OutPath -Recurse -Force
}
New-Item -ItemType Directory -Path $OutPath | Out-Null

# Dynamic Logo Distribution
Write-Host "[INFO] Synchronizing project logo..." -ForegroundColor Cyan
$LogoFile = if (Test-Path "logo_final.png") { "logo_final.png" } else { "logo.png" }

if (Test-Path $LogoFile) {
    Write-Host "[INFO] Attempting to generate icon from $LogoFile..." -ForegroundColor Cyan
    try {
        # Try logo_final first, then logo.png
        $CurrentLogo = $LogoFile
        $Success = $false
        
        # Function to try resize
        function Try-Resize($file, $out) {
            python -c "from PIL import Image; import sys; 
try:
    img = Image.open(sys.argv[1])
    img = img.resize((256, 256))
    img.save(sys.argv[2], 'PNG')
    sys.exit(0)
except Exception as e:
    print(e)
    sys.exit(1)" $file $out
            return $LASTEXITCODE -eq 0
        }

        if (Try-Resize $LogoFile "switch_app/icon.png") {
            $Success = $true
        } elseif ($LogoFile -eq "logo_final.png" -and (Test-Path "logo.png")) {
            Write-Warning "[WARN] logo_final.png failed, trying logo.png..."
            if (Try-Resize "logo.png" "switch_app/icon.png") {
                $Success = $true
            }
        }

        if ($Success) {
            Write-Host "[SUCCESS] Icon generated successfully." -ForegroundColor Green
        } else {
            Write-Warning "[WARN] All logo files failed to resize. Using default or existing icon."
        }
    } catch {
        Write-Warning "[WARN] Unexpected error during icon generation: $_"
    }
}

if (-not (Test-Path "switch_app/icon.png")) {
    Write-Error "[ERROR] switch_app/icon.png is missing! NRO will build with default icon."
    # We could copy the raw logo as fallback if it's already 256x256
    if (Test-Path $LogoFile) {
        Copy-Item $LogoFile -Destination "switch_app/icon.png" -Force
    }
}

# Set version across all files
python scripts/set_version.py $Version

Write-Host "[BUILD] Building Nintendo Switch Homebrew v$Version..." -ForegroundColor Cyan

# Run Docker build
docker run --rm -v "${RepoPath}:/src" devkitpro/devkita64:latest bash -c "
    export DEVKITPRO=/opt/devkitpro
    export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:/usr/bin:/bin
    set -e
    # Fix clock skew
    find /src -exec touch {} +
    # Generate main.json from template safely
    python3 /src/scripts/gen_npdm_json.py
    # Build sysmodule
    cd /src/switch_sysmodule && sed -i 's/\r$//' Makefile && /usr/bin/make clean && /usr/bin/make APP_VERSION=$Version
    /opt/devkitpro/tools/bin/npdmtool main.json main.npdm
    # Build config app
    cd /src/switch_app && sed -i 's/\r$//' Makefile && /usr/bin/make clean && /usr/bin/make VERSION=$Version
"

# Copy artifacts to output folder
if (Test-Path "switch_sysmodule/homeassistant_sysmodule.nso") {
    Write-Host "[INFO] Copying artifacts..." -ForegroundColor Green
    Copy-Item "switch_sysmodule/homeassistant_sysmodule.nso" -Destination (Join-Path $OutPath "main")
    if (Test-Path "switch_sysmodule/main.npdm") {
        Copy-Item "switch_sysmodule/main.npdm" -Destination $OutPath
    }
    # Copy config app
    if (Test-Path "switch_app/homeassistant.nro") {
        Copy-Item "switch_app/homeassistant.nro" -Destination $OutPath
    }
    Write-Host "[DONE] Done! Files are in $OutPath" -ForegroundColor Green
} else {
    Write-Error "[ERROR] Build failed - Binaries not found!"
    exit 1
}

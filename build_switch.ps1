# Build Script for Nintendo Switch Components
param(
    [Parameter(Mandatory=$false)]
    [string]$Version = "0.2.2",

    [Parameter(Mandatory=$false)]
    [switch]$Deploy,  # Add -Deploy switch for optional MTP copy

    [Parameter(Mandatory=$false)]
    [switch]$DeployEmulator # Add -DeployEmulator for Ryujinx copy
)

$RepoPath = (Resolve-Path ".").Path
$OutPath = Join-Path $RepoPath "switch_build_out"

# Ensure LF line endings
Write-Host "[INFO] Converting source files to LF..." -ForegroundColor Cyan
py -c "import os; 
for root, dirs, files in os.walk('.'):
    for f in files:
        if f.endswith(('.cpp', '.h', '.hpp', 'Makefile', '.json', '.ps1')):
            path = os.path.join(root, f)
            try:
                with open(path, 'rb') as file:
                    content = file.read()
                new_content = content.replace(b'\r\n', b'\n')
                if new_content != content:
                    with open(path, b'wb') as file:
                        file.write(new_content)
            except Exception as e:
                pass"

# Cleanup old artifacts
if (Test-Path $OutPath) { Remove-Item -Path $OutPath -Recurse -Force }
New-Item -ItemType Directory -Path $OutPath | Out-Null

# Set version across all files
py scripts/set_version.py $Version

# Validate NRO icon (must be JPEG for Switch homebrew menu)
Write-Host "[CHECK] Validating NRO icon format..." -ForegroundColor Cyan
$IconPath = Join-Path $RepoPath "switch_app/icon.jpg"
$IconPng  = Join-Path $RepoPath "switch_app/icon.png"

if (Test-Path $IconPng) {
    # Check if icon.png is actually a PNG (not a misnamed JPEG)
    $header = [System.IO.File]::ReadAllBytes($IconPng)[0..3]
    $isPng = ($header[0] -eq 0x89 -and $header[1] -eq 0x50 -and $header[2] -eq 0x4E -and $header[3] -eq 0x47)
    if ($isPng) {
        Write-Host "[WARN] icon.png is a real PNG! NRO requires JPEG. Auto-converting..." -ForegroundColor Yellow
        py -c "from PIL import Image; Image.open('switch_app/icon.png').convert('RGB').resize((256,256)).save('switch_app/icon.jpg','JPEG',quality=95)"
        Write-Host "[OK] Converted to icon.jpg (256x256 JPEG)" -ForegroundColor Green
    }
}

if (-not (Test-Path $IconPath)) {
    Write-Error "[ERROR] No icon.jpg found in switch_app/. NRO will have no icon!"
    exit 1
}

# Verify icon.jpg is actually JPEG
$jheader = [System.IO.File]::ReadAllBytes($IconPath)[0..1]
if ($jheader[0] -ne 0xFF -or $jheader[1] -ne 0xD8) {
    Write-Error "[ERROR] icon.jpg is not a valid JPEG file! Please provide a 256x256 JPEG."
    exit 1
}
Write-Host "[OK] Icon validated: icon.jpg is a valid JPEG" -ForegroundColor Green

Write-Host "[BUILD] Building Nintendo Switch Homebrew v$Version..." -ForegroundColor Cyan

# Run Docker build
docker run --rm -v "${RepoPath}:/src" -w /src devkitpro/devkita64:latest bash -c "
    export DEVKITPRO=/opt/devkitpro
    export PATH=/opt/devkitpro/devkitA64/bin:/opt/devkitpro/tools/bin:/usr/bin:/bin
    set -ex
    python3 /src/scripts/gen_npdm_json.py
    cd /src/switch_sysmodule && /usr/bin/make clean && /usr/bin/make DEVKITPRO=/opt/devkitpro APP_VERSION=$Version
    /opt/devkitpro/tools/bin/npdmtool /src/switch_sysmodule/main.json /src/switch_sysmodule/main.npdm
    cd /src/switch_app && /usr/bin/make clean && /usr/bin/make DEVKITPRO=/opt/devkitpro VERSION=$Version
"

# Copy artifacts to output folder using ABSOLUTE PATHS
Write-Host "[INFO] Copying artifacts to staging..." -ForegroundColor Green
$NSO = Join-Path $RepoPath "switch_sysmodule/homeassistant_sysmodule.nso"
$NPDM = Join-Path $RepoPath "switch_sysmodule/main.npdm"
$NRO = Join-Path $RepoPath "switch_app/homeassistant.nro"

if (Test-Path $NSO) {
    Copy-Item $NSO -Destination (Join-Path $OutPath "main") -Force
    if (Test-Path $NPDM) { Copy-Item $NPDM -Destination $OutPath -Force }
    if (Test-Path $NRO) { Copy-Item $NRO -Destination $OutPath -Force }
    
    # Create boot2.flag for Atmosphere
    $FlagsPath = Join-Path $OutPath "flags"
    New-Item -ItemType Directory -Path $FlagsPath -Force | Out-Null
    New-Item -ItemType File -Path (Join-Path $FlagsPath "boot2.flag") -Force | Out-Null
} else {
    Write-Error "[ERROR] Build failed - Sysmodule NSO not found at $NSO"
    exit 1
}

# Optional MTP Deployment
if ($Deploy) {
    Write-Host "[DEPLOY] Attempting MTP Deployment to Switch..." -ForegroundColor Cyan
    $shell = New-Object -ComObject Shell.Application
    $computer = $shell.NameSpace(0x11)
    $switch = $computer.Items() | Where-Object { $_.Name -like "*Nintendo Switch*" }
    
    if ($null -eq $switch) {
        Write-Warning "[WARN] Switch MTP device not found. Skipping deployment."
    } else {
        $sd = $switch.GetFolder.Items() | Where-Object { $_.Name -like "*SD Card*" }
        if ($null -eq $sd) {
             Write-Warning "[WARN] SD Card not found. Skipping deployment."
        } else {
            $sd_folder = $sd.GetFolder
            $atmo = ($sd_folder.Items() | Where-Object { $_.Name -eq "atmosphere" }).GetFolder
            $contents = ($atmo.Items() | Where-Object { $_.Name -eq "contents" }).GetFolder
            $id = "010000000000CAFE"
            
            # Setup sysmodule dir
            $ha_sys_item = $contents.Items() | Where-Object { $_.Name -eq $id }
            if ($null -eq $ha_sys_item) { $contents.NewFolder($id); Start-Sleep -Seconds 1; $ha_sys_item = $contents.Items() | Where-Object { $_.Name -eq $id } }
            $exefs_item = $ha_sys_item.GetFolder.Items() | Where-Object { $_.Name -eq "exefs" }
            if ($null -eq $exefs_item) { $ha_sys_item.GetFolder.NewFolder("exefs"); Start-Sleep -Seconds 1; $exefs_item = $ha_sys_item.GetFolder.Items() | Where-Object { $_.Name -eq "exefs" } }
            $exefs = $exefs_item.GetFolder

            $flags_item = $ha_sys_item.GetFolder.Items() | Where-Object { $_.Name -eq "flags" }
            if ($null -eq $flags_item) { $ha_sys_item.GetFolder.NewFolder("flags"); Start-Sleep -Seconds 1; $flags_item = $ha_sys_item.GetFolder.Items() | Where-Object { $_.Name -eq "flags" } }
            $flags = $flags_item.GetFolder

            # Copy files
            $src_folder = $shell.NameSpace($OutPath)
            Write-Host "[DEPLOY] Copying main and main.npdm..."
            $exefs.CopyHere($src_folder.ParseName("main"), 16)
            $exefs.CopyHere($src_folder.ParseName("main.npdm"), 16)
            
            Write-Host "[DEPLOY] Copying boot2.flag..."
            $flags.CopyHere((Join-Path $src_folder.Self.Path "flags\boot2.flag"), 16)
            
            # Copy NRO
            $switch_dir = ($sd_folder.Items() | Where-Object { $_.Name -eq "switch" }).GetFolder
            Write-Host "[DEPLOY] Copying homeassistant.nro..."
            $switch_dir.CopyHere($src_folder.ParseName("homeassistant.nro"), 16)
            Write-Host "[SUCCESS] MTP Deployment complete!" -ForegroundColor Green
        }
    }
}

# Optional Emulator Deployment
if ($DeployEmulator) {
    Write-Host "[DEPLOY] Attempting Emulator Deployment to Ryujinx..." -ForegroundColor Cyan
    powershell -ExecutionPolicy Bypass -File .\scripts\deploy_emulator.ps1
}

Write-Host "[DONE] Done! Files are in $OutPath" -ForegroundColor Green

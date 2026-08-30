# Build the Visceral — RE2 VR Fluffy Mod Manager package.
#
# Produces ONE zip the player drags into Fluffy. Inside, each feature is its own
# sub-mod (its own subfolder + modinfo.ini), and they all share
# `NameAsBundle=Visceral - RE2 VR`, so Fluffy groups them under a single
# "Visceral - RE2 VR" menu button with a per-feature enable/disable tick.
# (Fluffy supports multiple mods in one archive via subfolders each with a
# modinfo.ini, and NameAsBundle to collapse them into one button.)
#
#   powershell -ExecutionPolicy Bypass -File tools\build-fluffy.ps1 -Version 0.2.0

param([string]$Version = "0.2.0")

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $repo "dist"
$root = Join-Path $env:TEMP ("visceral_fluffy_" + [guid]::NewGuid().ToString("N"))
$bundle = "Visceral - RE2 VR"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
New-Item -ItemType Directory -Force -Path $root | Out-Null

# feature subfolder -> display name, description, files (repo-relative)
$features = @(
  @{ Id="cutscene-safety"; Name="Cutscene Safety (core, recommended)";
     Desc="Pauses Visceral's body adjustments during cutscenes, camera events and enemy grabs. Recommended alongside any body feature.";
     Files=@("reframework/autorun/visceral_cinematic_gate.lua") }
  @{ Id="spine-straighten"; Name="Spine Straighten (torso twist)";
     Desc="Removes the twisted weapon-hold torso posture so the upper body faces forward; keeps gait/breathing motion.";
     Files=@("reframework/autorun/visceral_spine_straighten.lua") }
  @{ Id="foot-grounding"; Name="Foot Grounding (no aim float)";
     Desc="Plants the feet on the floor while aiming (kills the VR aim-pose float). Hands, gun and bullet impact untouched.";
     Files=@("reframework/autorun/visceral_foot_ground.lua") }
  @{ Id="aim-speed"; Name="Faster Aim-Walk";
     Desc="Speeds up movement while aiming (collision-safe, no clipping, stays analog). Aiming only.";
     Files=@("reframework/autorun/visceral_locomotion.lua") }
  @{ Id="no-crosshair"; Name="No Crosshair";
     Desc="Hides the game's 2D crosshair. Config: reframework/data/visceral/crosshair_config.json.";
     Files=@("reframework/autorun/visceral_crosshair.lua","reframework/data/visceral/crosshair_config.json") }
  @{ Id="vr-settings"; Name="VR Settings (FirstPerson + Flashlight)";
     Desc="REFramework config: FirstPerson on, manual flashlight on (F), camera light disabled. Fresh installs only; if you tuned re2_fw_config.txt yourself, set those keys manually instead.";
     Files=@("re2_fw_config.txt") }
)

foreach ($f in $features) {
    $pkg = Join-Path $root $f.Id
    New-Item -ItemType Directory -Force -Path $pkg | Out-Null
    @(
      "name=$($f.Name)",
      "version=v$Version",
      "description=$($f.Desc)",
      "author=TefMeister, Claude",
      "category=RE Framework",
      "screenshot=visceral.png",
      "NameAsBundle=$bundle"
    ) | Set-Content -Encoding UTF8 (Join-Path $pkg "modinfo.ini")
    # bundle screenshot (shown in Fluffy) — include in every sub-mod so the entry always has it
    $shot = Join-Path $repo "visceral.png"
    if (Test-Path $shot) { Copy-Item $shot (Join-Path $pkg "visceral.png") }
    foreach ($rel in $f.Files) {
        $src = Join-Path $repo $rel
        if (-not (Test-Path $src)) { throw "missing file: $rel" }
        $dst = Join-Path $pkg $rel
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item $src $dst
    }
    "  packed sub-mod: {0}" -f $f.Name
}

$zip = Join-Path $dist ("Visceral-RE2-VR-" + $Version + "-Fluffy.zip")
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $root "*") -DestinationPath $zip
Remove-Item -Recurse -Force $root
"`nOne Fluffy zip (drag this into Fluffy): {0}  ({1:N0} bytes)" -f (Split-Path $zip -Leaf), (Get-Item $zip).Length
"In Fluffy it appears as a single '$bundle' button; tick features on/off inside it."

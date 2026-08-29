# Build per-feature Fluffy Mod Manager packages for Visceral — RE2 VR.
# Each feature becomes its own Fluffy mod (modinfo.ini + files) so players can
# enable/disable them individually in Fluffy. Run from anywhere; paths are relative
# to this repo. Output zips land in dist/.
#
#   powershell -ExecutionPolicy Bypass -File tools\build-fluffy.ps1 -Version 0.2.0

param([string]$Version = "0.2.0")

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $repo "dist"
$staging = Join-Path $env:TEMP ("visceral_fluffy_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $dist | Out-Null

# feature id -> display name, description, and the files it ships (repo-relative)
$features = @(
  @{ Id="cutscene-safety"; Name="Visceral - Cutscene Safety (core)";
     Desc="Shared helper: pauses Visceral's body adjustments during cutscenes, camera events and enemy grabs. Recommended alongside any body feature.";
     Files=@("reframework/autorun/visceral_cinematic_gate.lua") }
  @{ Id="spine-straighten"; Name="Visceral - Spine Straighten";
     Desc="Removes the twisted weapon-hold torso posture so the upper body faces forward; keeps gait/breathing motion.";
     Files=@("reframework/autorun/visceral_spine_straighten.lua") }
  @{ Id="foot-grounding"; Name="Visceral - Foot Grounding";
     Desc="Plants the feet on the floor while aiming (kills the VR aim-pose float). Hands, gun and bullet impact untouched.";
     Files=@("reframework/autorun/visceral_foot_ground.lua") }
  @{ Id="aim-speed"; Name="Visceral - Faster Aim-Walk";
     Desc="Speeds up movement while aiming (collision-safe, no clipping, stays analog). Aiming only; normal movement untouched.";
     Files=@("reframework/autorun/visceral_locomotion.lua") }
  @{ Id="no-crosshair"; Name="Visceral - No Crosshair";
     Desc="Hides the game's 2D crosshair. Toggle + config in reframework/data/visceral/crosshair_config.json.";
     Files=@("reframework/autorun/visceral_crosshair.lua","reframework/data/visceral/crosshair_config.json") }
  @{ Id="vr-settings"; Name="Visceral - VR Settings (FirstPerson/Flashlight)";
     Desc="REFramework config with Visceral defaults: FirstPerson on, manual flashlight on (F), camera light disabled. Fresh installs only; if you tuned re2_fw_config.txt yourself, set those keys manually instead.";
     Files=@("re2_fw_config.txt") }
)

foreach ($f in $features) {
    $pkg = Join-Path $staging $f.Id
    New-Item -ItemType Directory -Force -Path $pkg | Out-Null
    # modinfo.ini (Fluffy)
    @(
      "name=$($f.Name)",
      "version=v$Version",
      "description=$($f.Desc)",
      "author=TefMeister, Claude",
      "category=RE Framework"
    ) | Set-Content -Encoding UTF8 (Join-Path $pkg "modinfo.ini")
    # copy the feature's files preserving their reframework/... layout
    foreach ($rel in $f.Files) {
        $src = Join-Path $repo $rel
        if (-not (Test-Path $src)) { throw "missing file: $rel" }
        $dst = Join-Path $pkg $rel
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dst) | Out-Null
        Copy-Item $src $dst
    }
    $zip = Join-Path $dist ("Visceral-" + $f.Id + "-" + $Version + ".zip")
    if (Test-Path $zip) { Remove-Item $zip }
    Compress-Archive -Path (Join-Path $pkg "*") -DestinationPath $zip
    "built {0,-46} {1,7:N0} bytes" -f (Split-Path $zip -Leaf), (Get-Item $zip).Length
}

Remove-Item -Recurse -Force $staging
"`nFluffy packages in: $dist"

"""face_roster.py -- the roster of NEW zombie faces, and how each one is made.

Tefa, 2026-09-12: "the bigger job would be the different heads and faces, that i would like to add
at least 20 of, at least."

Each entry is one new face the game will be able to give a zombie. A face is made by copying an
existing head's prefab + mesh + material (paths re-pointed) and building a NEW albedo from the
source head's own texture with a tint/wear recipe -- so every face is derived from the player's own
game files at build time and nothing here ships game content.

⭐ WHY THE KEY NAMES ARE NOT LIMITED TO THE ENUM. `EM0000_MONTAGE_PARTS_FACE` lists FACE00..14 and
FACE70..76, and only seven of those have no prefab. But the RUNTIME works on strings, not the enum:
`Em0000MontageData.FaceKeyName` is a `System.String` and `MontageManagerBase.getFacePrefab(KeyName)`
looks the string up in the container's `FacePrefabs` list. The enum is editor-side, used by the
`EM0000_MontageData` struct, which the shipped tables do not go through. So a key like `FACE20`
should resolve exactly as `FACE08` does. `[inferred-static 2026-09-12]` -- the first run with a
beyond-the-enum key is what proves it, and the roster is ordered so that the empty ENUM slots come
first: if beyond-enum keys turn out not to resolve, the first seven faces still work.

Tint recipe fields (all applied to the source head's albedo, alpha untouched):
  hue    hue shift, + is toward green. Small numbers: 0.02 is a sallow cast, 0.08 is unmistakably ill.
  sat    saturation multiplier. Below 1 drains blood from the skin; 0.5 reads as long-dead.
  value  brightness multiplier. Below 1 darkens; above 1 gives the waxy, bloated look.
  grey   lifts dark low-saturation pixels (hair) toward grey. Catches shadowed skin too, so keep small.
  ruddy  reddens the mid-tones only -- fresh kills, still flushed. 0 = off.

⚠️ NOT every shipped head can be a source. Face11 and Face14 have a prefab but NO mdf2 and no mesh of
their own in the archive -- they are assembled from another head's parts `[measured 2026-09-12]`. Only
heads that own their material and mesh are usable here; the eight that do are Face00..Face07.
"""

# (key, source face folder, tint, one-line note for the ledger)
ROSTER = [
    # --- the seven empty ENUM slots first: these work whatever happens with string keys ---
    ("FACE08", "Face00", dict(hue=0.045, sat=0.72, value=0.92), "Face00, sallow and drained"),
    ("FACE09", "Face01", dict(hue=0.030, sat=0.60, value=0.80), "Face01, older kill, darker"),
    ("FACE12", "Face02", dict(hue=0.055, sat=0.55, value=0.98), "Face02, green-grey, waxy"),
    ("FACE13", "Face04", dict(hue=0.015, sat=0.90, value=0.88, ruddy=0.18), "Face04, fresh, still flushed"),
    ("FACE74", "Face05", dict(hue=0.060, sat=0.50, value=0.75), "Face05, long dead"),
    ("FACE75", "Face06", dict(hue=0.025, sat=0.78, value=1.05), "Face06, bloated and pale"),
    ("FACE76", "Face07", dict(hue=0.040, sat=0.65, value=0.85), "Face07, mid-decay"),
    # --- beyond the enum: proves the string lookup, and is where the numbers come from ---
    ("FACE20", "Face00", dict(hue=0.020, sat=0.85, value=1.06, ruddy=0.22), "Face00, fresh kill"),
    ("FACE21", "Face00", dict(hue=0.065, sat=0.48, value=0.78, grey=0.10), "Face00, grey and greying"),
    ("FACE22", "Face01", dict(hue=0.050, sat=0.62, value=1.02), "Face01, pallid"),
    ("FACE23", "Face01", dict(hue=0.010, sat=0.95, value=0.82, ruddy=0.25), "Face01, ruddy and dark"),
    ("FACE24", "Face02", dict(hue=0.020, sat=0.80, value=0.86), "Face02, dulled"),
    ("FACE25", "Face02", dict(hue=0.070, sat=0.45, value=0.72, grey=0.12), "Face02, cadaverous"),
    ("FACE26", "Face03", dict(hue=0.035, sat=0.70, value=0.95), "Face03, sallow"),
    ("FACE27", "Face03", dict(hue=0.015, sat=0.88, value=1.08, ruddy=0.20), "Face03, flushed, bloated"),
    ("FACE28", "Face04", dict(hue=0.058, sat=0.52, value=0.80), "Face04, green-grey"),
    ("FACE29", "Face04", dict(hue=0.030, sat=0.75, value=1.00, grey=0.08), "Face04, greying hair"),
    ("FACE30", "Face05", dict(hue=0.022, sat=0.86, value=0.90, ruddy=0.15), "Face05, recent"),
    ("FACE31", "Face05", dict(hue=0.068, sat=0.47, value=1.04), "Face05, waxy and drained"),
    ("FACE32", "Face06", dict(hue=0.048, sat=0.58, value=0.76), "Face06, dark and dead"),
    ("FACE33", "Face06", dict(hue=0.012, sat=0.92, value=0.98, ruddy=0.24), "Face06, fresh"),
    ("FACE34", "Face07", dict(hue=0.062, sat=0.50, value=1.00, grey=0.10), "Face07, grey"),
    ("FACE35", "Face07", dict(hue=0.028, sat=0.82, value=0.84), "Face07, dulled and dark"),
    ("FACE36", "Face03", dict(hue=0.072, sat=0.44, value=1.02, grey=0.10), "Face03, waxy and greying"),
    ("FACE37", "Face06", dict(hue=0.018, sat=0.90, value=0.86, ruddy=0.20), "Face06, fresh and dark"),
]

# The faces the game already gives ordinary male zombies, which the re-deal also draws from.
# ⚠️ FACE11 and FACE14 are NOT here on purpose. Their prefabs exist but they own no mdf2 and no mesh
# (see the module docstring), and the shipped tables only ever give them to two special outfits. The
# 2026-09-12 RPD stall is the reason this list is conservative: dealing a head whose assets resolve
# somewhere else into ordinary outfits is a way to hang a level load, and the police station is where
# enough zombies spawn at once to show it.
SHIPPED_EVERYDAY = ["FACE00", "FACE01", "FACE02", "FACE03", "FACE04",
                    "FACE05", "FACE06", "FACE07", "FACE10"]


def all_face_keys():
    """Every key the outfit table may deal from: the shipped everyday heads plus the new roster."""
    return SHIPPED_EVERYDAY + [k for k, _s, _t, _n in ROSTER]


def roster_for(limit=None):
    return ROSTER if limit is None else ROSTER[:limit]


if __name__ == "__main__":
    print("%d new faces, from %d source heads" % (len(ROSTER), len({s for _k, s, _t, _n in ROSTER})))
    print("total pool after the build: %d" % len(all_face_keys()))
    for k, src, tint, note in ROSTER:
        print("  %-8s <- %-8s %-48s %s" % (k, src, note, tint))

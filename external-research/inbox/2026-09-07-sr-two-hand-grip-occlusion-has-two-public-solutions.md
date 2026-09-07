# Two-handed grip controller occlusion: two public solutions, one of them per-weapon-configurable

**Filed by `/sr`, 2026-09-07. For this project's `/gr` lane to research properly and curate.**
Nothing run, nothing installed — both sources read online and verified firsthand today.

## Why this lands here

Your dossier §8e (the Arcade Controls port map) records that AC's **two-hand latch is shipped OFF**,
with the unfixed arm-identity guessing as the sole reason. When that latch comes back on, this project
inherits a problem that is geometric rather than a bug, and it is worth designing for before the first
headset test:

**With a rifle held naturally, the support-hand controller sits directly behind the trigger-hand
controller along the headset's line of sight.** It is occluded, its pose degrades at once, and the
weapon jitters or swings. Observed live by Tefa in the RE Village scope session on 2026-09-05
`[reported 2026-09-05, n=1 observer]`, where it cost a retake.

Meta's own developer blog names occlusion among the worst-case controller-tracking scenarios
(*"Scenarios that suffered the worst are when the controllers are near the edge of field of view, too
far, too close, or when there is occlusion"*) but publishes **no figure** for how long a controller's
pose coasts on its IMU once occluded. A targeted search found no such figure from Meta or Valve. So
"how bad and for how long" is unknown — design so it does not arise.

## The two public solutions, verified firsthand 2026-09-07

Both break the 1:1 mapping between the physical controller and the in-game hand, but at opposite ends:

| | mechanism | cost to the player |
| --- | --- | --- |
| **STALKER Anomaly VR** (MarsyApp) | **Offset the IK target.** The secondary hand is *spread apart* in IK so the controllers do not cover each other for the headset cameras. The offset is **per weapon**, in the game's LTX config files. Both hands stay live. | Proprioceptive mismatch that scales with the offset. |
| **Onward** (virtual gunstock) | **Stop reading the rear hand.** *"When you bring a two handed weapon up to aim Virtual Gunstock Mode kicks in and keeps the weapon locked in position. Your front hand and body movement now controls the aim."* | *"a slight loss of fine control"* (UploadVR's phrasing). |

**The detail worth stealing is per-weapon configuration.** A single global offset cannot be right for a
handgun, a rifle and a shotgun at once, because the correct real-world hand separation is a property of
the weapon's geometry. That maps cleanly onto the config shape §8e already documents.

**⚠️ One thing that is NOT confirmed.** Tefa's own reading is that the left hand should be held **above**
the right — stacking the controllers vertically. MarsyApp's text says *spread apart* (`разводится`), not
*above*, and no screenshot or video confirming the real-world hand geometry could be found. Treat the
**direction** of the offset as a knob to find in the headset, not a constant to copy.

## Suggested next step for `/gr`

Whether MarsyApp has published the LTX key names or an example block — if so, the shape of a per-weapon
offset table is worth copying wholesale rather than re-deriving. Also worth checking whether H3VR
documents anything similar; the fan wiki returned HTTP 402 to an automated fetch today, so it is
genuinely unchecked rather than absent.

## Sources

- <https://ap-pro.ru/forums/topic/14575-anomaly-vr/> and <https://boosty.to/anomaly_vr> — MarsyApp's own
  development thread and funding page (Russian). Its roadmap lists
  *"Анти-окклюзия вторичной руки (Secondary IK offset)"* as complete. Credit **MarsyApp**.
- <https://www.uploadvr.com/onward-inside-out-tracking-update/> — credit **Downpour Interactive**
  (Onward) and **UploadVR**.
- Meta developer blog, *Tracking Technology Explained: LED Matching*. Credit **Meta**.
- Curated write-up now in the library:
  `flat-to-vr-cross-engine-research/docs/techniques/README.md` →
  "Two-handed VR weapons: the second controller hides behind the first".

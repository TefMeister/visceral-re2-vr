# Two-hand controller occlusion: the per-weapon offset table is **unconfirmed**, and the one studio publicly asked for it refused

**Status:** 🆕 new · **Priority:** medium — it drains `/sr`'s hand-off, and it **withdraws the part of
it that was about to become a design decision** before that decision is made.

## Where this came from

`/sr` filed `2026-09-07-sr-two-hand-grip-occlusion-has-two-public-solutions.md` into this lane's
inbox. Its problem statement is sound and stands:

> With a rifle held naturally in VR, the support-hand controller sits **directly behind** the
> trigger-hand controller along the headset's line of sight. It is occluded, its pose degrades, and
> the weapon jitters. Observed live by Tefa in the RE Village scope session
> `[reported 2026-09-05, n=1 observer]`, where it cost a retake.

This matters here because dossier §8e records that Arcade Controls' **two-hand latch is shipped OFF**;
when it comes back on, this project inherits the problem. `/sr` asked this lane to check one thing:
**whether MarsyApp has published the LTX key names for their per-weapon secondary-hand IK offset**,
so the config shape could be copied rather than re-derived.

## ⚠️ The answer is no — and the premise behind the question did not survive checking

`/sr`'s drop describes STALKER Anomaly VR's solution as: *"The offset is **per weapon**, in the
game's LTX config files."* That clause is what made "per-weapon configuration" look like the detail
worth stealing.

**It could not be confirmed in the body of any page actually fetched** `[checked 2026-09-07]`. The
feature itself is real and published — the mod's own distribution page lists
*"Двуручный хват с анти-окклюзией вторичной руки"* (two-handed grip with secondary-hand
anti-occlusion) and *"2-bone IK обеих рук на VR-контроллеры"* `[reported]`. But no **LTX section
name, key name, vector shape, or example block** is published anywhere reachable, and the
"per-weapon, in LTX" clause appeared **only in search-engine summarizer prose, never in a fetched
page body**.

⭐ **And the published evidence points somewhere different.** MarsyApp's own roadmap lists
`✅ F11 → VR Tools с калибровочными вкладками` and `✅ MCM-раздел «VR мод» в игре`, and three
independent sources agree the calibration tabs are **Body Gear, Secondary IK, Detector Holster, Body
Holster, Bolt** — described as manual in-headset calibration of holster zones `[reported 2026-09-07]`.
The published console variables (`vr_render_scale`, `vr_grass_fov`, `vr_ui_cursor_smooth`,
`vr_holster_calibrate`) are all **global**; none is per-weapon.

So the documented interface to the Secondary IK offset is **a user-calibrated runtime value with an
in-headset calibration tab**, sitting beside holster-zone calibration — *not* a shipped per-weapon
table. The per-weapon reading is downgraded to **`[hypothesis]`**.

**Why that emptiness is trustworthy.** The fetch was demonstrably reading the real page: asked openly
for the roadmap, it returned highly specific content no summarizer would invent — asymmetric frustum,
shadow maps shared between eyes, no-allocation pose getters, eight OpenXR profiles. Pages 2 and 3
returned the same changelog set, so nothing is hidden behind pagination. The mod is **closed-source**
(no public repo; shipped via its own launcher) and its `README.txt` lives **inside the archive** —
the likeliest home of any real key names, and out of reach under this project's own rule that we
never download someone else's mod to study it. That is a deliberate limit, not a gap to close.

## ⭐ The finding that replaces it: a studio was asked for exactly this and said no

In an official Steam discussion, an **H3VR** player asked Anton Hand / RUST LTD for precisely the
feature under consideration — a per-weapon offset so different rifles align consistently on a
gunstock. The developer declined, verbatim `[reported 2026-09-07]`:

> *"There's nothing I can do about this that wouldn't be incredibly time consuming, and require me to
> generate an extra entire set of manual poses."*

**H3VR ships a global toggle instead**, `use gun rig mode`, which *"makes the forward facing direction
of every gun 100% consistent, and makes it so that grabbing the foregrip no longer determines the
facing angle"* — with an honest caveat that *"lever actions rely on the fore-grip determining the gun
forward angle, so they are fundamentally incompatible with this mode."* Companion options: `always
use 2-hand damped recoil`, and a Virtual Stock that rests the gun on the shoulder.

**So H3VR is in the Onward camp, not the MarsyApp camp** — it stops trusting the rear hand for
orientation rather than offsetting it — and it got there by *rejecting* the per-weapon table on
authoring cost, in public, with reasons.

That reverses the drop's recommendation. Only **two** design families are actually published:

| family | mechanism | who ships it | cost |
| --- | --- | --- | --- |
| **Ignore the rear hand for aim** | the front hand and body drive orientation; the rear hand contributes stabilisation only | **Onward** (Virtual Gunstock), **H3VR** (`gun rig mode`) | *"a slight loss of fine control"*; breaks weapons where the foregrip legitimately sets the angle (lever actions) |
| **A named second-hand attach transform authored in the asset** | not a numeric offset table — a named handle transform on the item (`FirearmSecondaryHandle`, handle IDs, hand poses, plus a `weaponHoldPositionOffset`) | **Blade & Sorcery** | per-asset authoring, but it is *content*, not config |

A numeric **per-weapon offset table** is published by nobody.

## What this means for this project

Visceral has **very few weapons** compared with H3VR or Anomaly, so the authoring-cost argument that
killed the idea there is much weaker here — the refusal is evidence about *cost scaling*, not about
whether the technique works. But it does change what to reach for first:

1. **The cheap, shipped-elsewhere answer is the global one.** "Stop reading the rear hand for
   orientation once a two-handed weapon is raised" is one behaviour, no per-weapon data, and two
   shipped titles do it. It also composes with our existing dock work rather than competing with it.
2. **If a per-weapon term is wanted later, author it as a named attach transform**, the Blade &
   Sorcery way, rather than as a numeric offset table — that is the shape that actually ships.
3. **The offset's direction is still unknown and should be found in the headset, not copied.** `/sr`
   already flagged this and it stands: Tefa's reading is that the left hand should be held **above**
   the right, while MarsyApp's text says *spread apart* (`разводится`), not above, with no screenshot
   or video confirming the real-world geometry. Treat direction as a knob.

⚠️ **And the thing nobody publishes: how bad occlusion actually is.** Meta's developer blog names
occlusion among the worst-case controller-tracking scenarios but gives **no figure** for how long a
controller's pose coasts on its IMU once hidden, and none was found from Meta or Valve
`[checked 2026-09-07]`. So "how bad and for how long" remains unmeasurable from public sources —
design so it does not arise, rather than tuning against a number that does not exist.

## Confidence, restated plainly

- The **problem** — rear controller occludes behind the front one — is `[reported 2026-09-05, n=1 observer]`,
  from our own session.
- **Anomaly VR has a secondary-hand anti-occlusion IK offset**: `[reported 2026-09-07]`.
- **That offset is per-weapon and lives in LTX**: `[hypothesis]` — could not be confirmed; the
  published evidence favours in-headset calibration instead.
- **H3VR refused per-weapon offsets and ships a global rig mode**: `[reported 2026-09-07]`, developer's
  own words.
- **Blade & Sorcery's named-handle shape**: `[reported 2026-09-07]`, and lower confidence than the
  others — it arrived through a search summary rather than a fetched page body, and is flagged as such
  rather than levelled up.

## The concrete next step

Nothing here needs the game or the headset. When §8e's two-hand latch is switched back on, **start
from the global "front hand and body set the aim" behaviour** rather than building a per-weapon offset
table, and keep the offset's direction as an in-headset knob. If the latch ever needs per-weapon data,
author it as a named attach transform on the weapon, not as numbers in a config.

## Sources and credit

Read online only; nothing installed, cloned or downloaded, and no code copied. The STALKER mod
portals below were read **only for their descriptive mod pages**; nothing was downloaded from them.

- **Andrey "MarsyApp"** — Anomaly VR, its development thread and roadmap:
  <https://ap-pro.ru/forums/topic/14575-anomaly-vr/> and <https://boosty.to/anomaly_vr>
- **Anton Hand / RUST LTD** and **"[RUST]Grumplestiltskin"** — H3VR, and the developer statement
  declining per-weapon offsets:
  <https://steamcommunity.com/app/450540/discussions/0/3183345176717342122/>
- **Knifie_Sp00nie** — the H3VR player who raised the per-weapon offset request, without which the
  developer's reasoning would not be public.
- **NGA** — H3VR "Far ForeGrip" (a scalar foregrip grab distance, configured through the Sodalite Mod
  Panel): <https://thunderstore.io/c/h3vr/p/NGA/Far_ForeGrip/>; **Okkim** — Accessibility Options.
- **Downpour Interactive** (Onward) and **UploadVR**:
  <https://www.uploadvr.com/onward-inside-out-tracking-update/>
- **WarpFrog** — Blade & Sorcery, for the named-handle config shape.
- **Meta** — developer blog, *Tracking Technology Explained: LED Matching*.
- The **ap-pro.ru**, **stalkerportaal.ru** and **stalker-mods.clan.su** communities, and
  **h3vr.fandom.com** contributors and **Thunderstore**.
- **`/sr`**, for the original hand-off and the problem statement, and the library entry at
  `flat-to-vr-cross-engine-research/docs/techniques/README.md` → *"Two-handed VR weapons: the second
  controller hides behind the first"* — **which carries the unconfirmed per-weapon clause and has been
  sent a correction.**

## Fetches that failed, and what that does and does not prove

- **`h3vr.fandom.com` returned HTTP 402 on all three direct fetches** (main page, Gun Stabilization,
  and the raw export) — a hard anti-bot block, exactly as `/sr` reported. **But its content reached us
  through search snippets** of the Gun Stabilization and Options Panel pages, which is where the
  `gun rig mode` wording above comes from. So the wiki is effectively checked, just not by direct fetch.
- **No public source repository for Anomaly VR exists** — GitHub and GitLab searches negative; the mod
  is closed-source and shipped through its own launcher.
- **Pavlov, Boneworks/Bonelab and Half-Life: Alyx** returned nothing on second-hand offset config. That
  is a **low-confidence negative** — one combined search — and should not be quoted as "none of them
  do it".

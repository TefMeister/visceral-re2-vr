# Neck plug, first run: the plug spawns and toggles, and the collar was already closed (2026-09-06 01:10–01:20, home PC, VR)

Tefa ran the neck plug in the headset at the end of a long evening. Short session, one result, and a
correction to yesterday's mechanism note.

## What happened `[verified-live 2026-09-06, n=1]`

- Plugin v0.7 loaded, `PLUG CREATED` at 01:09:34 with the plug mesh and Claire's `pl3000.mdf2`
  material set. No failure line of the four.
- The 1 Hz summary carried `plug=on @(x y z)` all session; the plug sat ~10 cm above the left
  clavicle joint and moved with the body.
- **NUM0 toggled the plug four times (log: OFF/ON/OFF/ON at 01:12:58–01:13:00) and Tefa saw no
  change at all: "nothing changes when I press NUM0, the collar looks closed."**
- Mode: REFramework FirstPerson **Enabled** with **Hide Joint Mesh ON** (the head joint scaled to
  zero). FirstPerson had been off at launch and was switched on from the Insert menu.

So in Hide-Joint-Mesh mode the collar is **not** hollow, and the plug is redundant there. Whether it
is drawing but hidden under the neck tube, or not drawing at all (material bound to nothing), the
run cannot tell — both look identical from inside a closed collar.

## The correction — Tefa's read, recorded over yesterday's

Yesterday's note (`2026-09-05e`, §1) inferred statically that scaling the `head` joint leaves the
neck tube as an open pipe under the camera and that this *is* the hollow. Tefa's memory of the
earlier sessions is different and is now backed by this run:

> the hollow body only appeared after you put the head shadow back … I had to turn off Hide Joint
> Mesh for the head shadow to work, and that produced the hollow body.

**The hollow is specific to the head-shadow method** (Arcade Controls' feature: head hidden some
other way so the shadow survives, `HideJointMesh=false`). In that mode the face file (face + neck
tube) is presumably hidden whole, leaving the body's collar ring open. With Hide Joint Mesh on, the
neck tube stays and closes the view. `[reported 2026-09-06, user; consistent with n=1 live]` —
§1 of the 05e note is downgraded to `[hypothesis]` for the *hollow* claim; its mesh measurements
stand.

Visceral currently has **no head-shadow script deployed** (autorun holds cinematic_gate, crosshair,
foot_ground, locomotion, native_bridge, spine_straighten), so the hollow state cannot be reproduced
on this install today.

## What this means for the plug (roadmap H1)

The plug is not wasted: it is the fix for the state we want to end up in — **head shadow on**
(roadmap v1 item 4, v2 phase H). It needs to be tested in that state, which needs a Visceral head
hider that keeps the shadow. Until then, H1 is not verifiable and moves behind the head-shadow item.

Two cheap `[PD]` follow-ups fall out:

1. **Make the plug's own state readable.** Log `get_DrawDefault` after `set_DrawDefault`, and the
   material count / first material name off the plug's `via.render.Mesh` at creation, so "toggles
   with no visible change" can be split into *drawing but occluded* vs *not drawing*. One more log
   line, no launch needed to write it.
2. **A Visceral head hider that keeps the shadow.** Port-map item from Arcade Controls' `head_shadow`
   (knowledge only, own implementation): hide the head/face draw without scaling the joint, so the
   shadow pass still draws it. C++ in the plugin, next to the plug. When this lands, the plug gets its
   real test.

## Side reads from the same log (free, not the purpose of the run)

- `PLAYER BOUND: … (pl1000)` while playing Claire. The GameObject **name** is `pl1000` for the
  player regardless of character; the character is told by the mesh/mdf path (the plug used
  `pl3000.mdf2` and looked right for Claire). Worth printing the mesh path in that line so the log
  says who is being played. `[inferred-static 2026-09-06]`
- Bridge summary: `hmd=1 ctl=0` on 534 of 564 seconds, `ctl=1` on 30. The headset pose reaches the
  plugin; controller poses only intermittently. First time the VR row of the bridge has been seen
  live at all. Relevant to the dock's `[VR]` row, not to the plug. `[measured 2026-09-06, n=1 session]`

# The in-world options book: RE2's notebook, RE3's file folder (requirements, 2026-11-07; the design proper next week)

Tefa, 2026-11-07, after the clean rebuilds: *"the vr options separate menus for both games. re 2 i would like to have this done
in blender if possible, 3D and in the game world, like the officer's notebook from the game, where we put all our options ...
and for re3 it's making this file folder"*, and later: *"if this is a big project, then please write it all up, save it and
we'll do this next week when i got more usage"*. This file holds everything Tefa decided in the conversation (their words where
they matter) and what is still open. It is shared by both games: the engine side lives in RE2's repo with the other shared
code; each game's model and pictures are its own. RE3's repo points here.

## 1. What it is (decided by Tefa)

- **RE2: the officer's notebook from the game**, a leather-bound notebook (reference: the RPD officer's notebook page with the
  goddess statue sketch). **RE3: the UBCS file folder** (reference: the "UBCS suicide note" item: brown leather, ring binder
  with a cord, coloured index tabs, photos clipped on the left page, a strap with a press stud), kept **straight, not at the
  picture's angle**.
- Both **modelled in Blender, 3D, with a presence in the game world**: "not in player hand or anything, like an overlay for the
  game", **big, just in front** of the player, as the Village options hang in the world ("like the duke's tablets"), so the
  player can still look around.
- Blender work is done **together with Tefa, one step at a time, every model in separate parts** (standing rule).
- **First milestone (Tefa): "if you can just create a notebook with leather covers for now and have that show up in the game,
  that would be awesome"** (RE2), and the folder for RE3.

## 2. How it is used (decided by Tefa)

- **X + A held for 0.5 s** brings it out and puts it away (the Village binding).
- **The game freezes while it is open, like the inventory** ("in re8 too once we take away the red background there":
  Village's Emporium gets the same freeze; a row on Village's board).
- **The left stick moves, A switches** an option on or off.
- **Pages scroll** when there is more on them than fits.
- **RE2 notebook:** left page = the category's picture and name, right page = its options. **Left stick left / right steps
  through the four categories**; the left page changes **instantly, no page-turn animation, with a paper swoosh sound**. Up /
  down moves through the options on the right page.
- **RE3 folder:** the **four coloured tabs on the left edge are highlighted**; **up / down** moves between them and changes
  the picture and the category description on the left side; **left stick right jumps into the options**, where up / down
  scrolls; (my guess, not yet confirmed: left stick left goes back to the tabs).

## 3. What is on it (decided by Tefa)

- **Four categories (names for now): Comfort, Graphics, Settings, REFramework settings.**
- **RE2:** the four statues of the game on the left page with their names under them: **Lion, Lady, Unicorn, Clock tower
  bell**, for Comfort, Graphics, Settings, REFramework settings.
- **RE3:** the photos clipped on the left page carry an icon for each category.
- **Tefa hand-writes every option in blue pen and designs the sliders**, and sends pictures; these go on the pages as they are.
  The mod draws only what changes: the on / off mark and the slider knob moving along Tefa's drawing (my reading, said back,
  not objected to).
- **REFramework settings: "list all the settings"**, scrollable. (My guess, said back: too many to hand-write, so a
  handwriting-style font in the same blue for those; and REFramework reads most of its settings only at start, so which ones
  can change live has to be found before it is promised.)
- **The smoothing sliders** belong in it (Comfort, my guess). Until it exists they are in REFramework's own menu
  (`lua/visceral_hand_smooth.lua`, back 2026-11-07 in both games: RE2 b198, RE3 b008, simulator-checked).

## 4. What we already know that helps `[from the notes]`

- **Our own models have been in RE2 before:** the forearm bracelets and the neck plug (2026-09-05/06): Blender ->
  RE Mesh Editor (installed in Blender 5.2, with RE-Asset-Library) -> `.mesh.2109108288`, our own `.mdf2.21` materials and
  `.tex.34` textures as loose files under `natives/stm/visceral/`, spawned by the plugin
  (`modding-notes/2026-09-06-forearm-bracelets-v010-built-and-deployed.md`; the tools in `dev-archive/tools/blender` and
  `dev-archive/tools/re-engine`). **The unsolved part then: giving our meshes a working material** (board: "the resource must
  be requested through the game's own loader, not create_resource alone"). That is the first risk here (the leather).
- **Village's options** (the Duke's shop cards) hang in the world because the game's own GUI screens are 3D objects in front of
  the camera (`visceral-re8-vr/modding-notes/2026-11-04-the-dukes-shop-by-name.md`); REFramework's own imgui window is NOT
  seen in the headset. RE2 / RE3 have no shop, so the book must be our own model.
- The shared code (2026-11-07) is the place for the engine side: one copy for RE2 and RE3 (`src/`, `tools/sync_shared.py`).

## 5. Open, for the Fable design next week (the decisions everything builds on)

1. How the book is placed and drawn: our own mesh spawned in front of the camera (as the bracelets), or one of the game's GUI
   screens carrying our pictures, or REFramework's VR overlay; which one is visible in the headset with free look, and holds
   still while the game is frozen.
2. How the pages show live content (the on / off marks, the slider knobs, the scrolling list, the REFramework settings in a
   font): textures swapped at runtime, small extra meshes for the marks, or GUI elements laid on the page.
3. The freeze: the game's own pause (as the inventory) without its menu showing, and the camera / VR layer kept alive.
4. The material problem from September (the leather must look like leather).
5. REFramework settings: how to read every setting, which can change live, how the rest are applied.
6. The input: X + A held 0.5 s, the left stick and A, while the game is frozen (the bridge reads the buttons; the game must not
   act on them).
7. The paper swoosh: the game's own page sound (the files menu) or our sound pack.

## 6. The order of work (a proposal for next week)

1. Fable: the design (section 5), written and approved.
2. With Tefa in Blender, one part at a time: the RE2 notebook's leather covers and pages; the RE3 folder (covers, rings, cord,
   tabs, strap, stud, clipped photos), straight.
3. Opus: the empty notebook / folder appears in front of the player in the game on X + A, the game frozen, and goes away again
   (Tefa's first milestone).
4. Opus: the categories and navigation, the swoosh, our options and the smoothing sliders in it; then Tefa's hand-written pages.
5. REFramework settings (after the Fable answer to 5.5).

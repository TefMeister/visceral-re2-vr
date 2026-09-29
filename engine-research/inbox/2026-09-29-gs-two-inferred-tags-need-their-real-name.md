# /gs 2026-09-29: two dated tags use a name outside the vocabulary

Found by `gs-scan.sh` check 3b (DATED group). Both are real claims with a real date, wearing `[inferred ...]`,
which reads as a tag to a person and counts as nothing to every tool.

- `modding-notes/2026-09-24-the-aim-hunch-is-a-lookat-profile.md:155` — `[inferred 2026-09-24]` ("about the same
  as its walk ... the shuffle only looked slower")
- `modding-notes/2026-09-24-the-aim-hunch-is-a-lookat-profile.md:171` — `[inferred 2026-09-24]` ("Sunk legs + the
  combat ready leg stance = visceral_foot_ground.lua")

Fix (modding lane, the file's owner): one word each. If the claim came from reading files, `[inferred-static
2026-09-24]`; if it was guessed from what was seen, `[hypothesis]`. Line 414 of the same note has an undated
`[inferred]` too: read it and decide whether it is prose or the same defect.

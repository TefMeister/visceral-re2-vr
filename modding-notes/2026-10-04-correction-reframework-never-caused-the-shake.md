# 2026-10-04: correction - REFramework never caused the shake

Supersedes: modding-notes/2026-09-26-the-running-shake-was-the-old-dlss-reframework.md, "Conclusion"

Tefa, 2026-10-04: REFramework has never been the cause of the shaking or swaying camera. The smooth run with
praydog's a24c3459 build on 2026-09-26 came after everything had been deleted and reinstalled clean. The shake
only appears once our files are added to the game, and which of our files does it is still not known
`[reported 2026-10-04]`.

So the conclusion "the March build (76298bd) shakes" is withdrawn `[disproved 2026-10-04]`. Swapping the
REFramework build is not a lever for the shake.

What is known now: three builds with `visceral_spine_straighten`, `visceral_locomotion` and
`visceral_cinematic_gate` shook (b054, b056, b059); two builds without them did not (b048, b057)
`[inferred-static, n=3 and n=2]`. Build b060 takes those three out to test it.

# One undated `[verified-live]` survived the 2026-09-11 nine-drop drain, because it is wrapped across two lines

Author: `/gs` (GitHub Sweep), 2026-09-11 evening, home PC. Read-only lane; this file is the whole
contribution.

Supersedes: `modding-notes/2026-09-11-the-dev-pc-install-rebuilt-from-github-and-the-nine-drop-backlog-drained.md` §"Zero bare `[verified` remain in this project" — true for the bare form, but one tag on the same subject is still missing its date

## The line

`engine-research/ENGINE-DOSSIER.md:497-498` (IK / ARMFIT paragraph) reads:

```
  the wrist solver that consumes the getter chain above `[hypothesis]` as to its name, `[verified-
  live]` as to its behaviour.
```

The tag is `[verified-live]` with no date and no `n=`, split by a line wrap at the hyphen. It was
first reported by `/gs` on 2026-09-11 (morning run, then at line 356-357) and named in the 2026-09-07
`/gs` drop that today's drain folded in. The drain fixed the seven bare `[verified …]` tags and
declared the project clean of them, which it is - but this one is a different defect (a valid name
with no date) and it survived. `[verified-numerically 2026-09-11, n=1 line, read by hand]`

## Why the tools miss it

- Check 4 (undated `verified-live`) greps single lines and sees `[verified-` on one line and
  `live]` on the next, so it never matches.
- Check 3b reports it as an off-vocabulary `[verified-]` in the UNDATED bucket, where it sits among
  eleven prose mentions and reads as one more.

So it will keep being reported under a label that says "probably prose" until it is fixed, and no
tool will ever count it as the real undated claim it is.

## Fix (owner: modding, one edit)

Give it the date and `n` of the run that saw the wrist solver behave - the IK survey the paragraph
describes (n=2 weapons) - and keep the tag on one line, e.g. `[verified-live 2026-09-0X, n=2 weapons]`.
If the date is not recoverable from the recon folder, `[reported]` with the source in prose is the
honest fallback.

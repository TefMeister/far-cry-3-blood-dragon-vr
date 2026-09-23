# /gs 2026-09-23: this repo's root .gitignore would silently drop recon logs

Supersedes: nothing (new finding)

**What:** `.gitignore:17` is a bare `*.log`, with no exception for recon evidence. `gs-scan.sh`
check 9 staged a test path `dev-archive/recon/2099-01-01-probe/capture.log` and git refused it
`[verified-numerically 2026-09-23, n=1 git check-ignore run]`. A future `/lm` or `/pd` session that
saves a log under `dev-archive/recon/` would commit everything *except* the log, with no warning.

**Why it matters:** the log is usually the evidence. Losing it silently turns a
`[verified-live]` claim into one nobody can re-check.

**Fix (one line, modding lane):** add this AFTER line 17 of the root `.gitignore`:

    !dev-archive/recon/**/*.log

Same fault in six repos created from the same template on or after 2026-09-11: borderlands-goty-vr,
bulletstorm-vr, deus-ex-mankind-divided-vr, far-cry-3-blood-dragon-vr, heavy-rain-vr, prey-2017-vr.
If the template or the lanes plugin's bootstrap writes that `.gitignore`, fix it there too.

## What this changes

<!-- One paragraph. What behaviour is different after this than before it. -->

## Why

<!-- Link the design document or the issue. A changelog entry says what changed; the reason
     lives here or in a document this links. -->

## How it was verified

<!-- Paste the output. `sd test`, `sd check`, and for anything touching a circuit, the
     measurement that shows the new numbers are right. A claim without its output is not a
     verification. -->

```
```

## Checklist

- [ ] `sd test` passes, and the output is pasted above.
- [ ] `sd check` exits 0.
- [ ] Every document this made untrue is updated in this same change: the module README, the
      design status line, `docs/backlog/README.md`.
- [ ] `docs/reference/CHANGELOG.md` carries one line per change, dated, past tense, with the
      path in backticks and no reason.
- [ ] A golden file that moved was regenerated deliberately, and the diff says which node's
      goldens changed and why.
- [ ] Anything left undone or unverified is stated here rather than left for the reviewer to
      discover.

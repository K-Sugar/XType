  You are implementing one sub-session of the XType Qt6/QML Settings app
  plan. The plan lives at ui-ultra-plan/ in this repo.

  ## Sub-session

  I am running sub-session: <U1c>

  ## Read order (do this first, in this exact order)

  1. CLAUDE.md  — project hard rules (venv, threading, no telemetry,
     commit style). These rules override everything except direct
     instructions in this prompt.
  2. fcitx5-engine/CLAUDE.md  — engine-local rules (C++17 hot path, no
     std::thread → eventDispatcher, build commands).
  3. ui-ultra-plan/README.md  — folder map and session prompt template.
  4. ui-ultra-plan/00-shared.md  — architecture, repo layout, §4
     challenges, §6 acceptance, §7 out-of-scope, §8 gotchas. This is
     reference material, cited by § number from the per-step bodies.
  5. ui-ultra-plan/<U1a|U1b|U1c>.md  — the only step bodies you execute
     this session.

  Do NOT read the other two sub-session files. They are out of scope and
  will pollute context. If you think you need them, stop and ask.

  ## Operating contract

  - Execute steps in the order listed in the sub-session file. Do not
    skip ahead, do not reorder, do not bundle commits across steps.
  - After each step's "Checkpoint"/"Acceptance" block, verify the
    checkpoint passes BEFORE committing. Commit messages are quoted
    verbatim in each step — use them exactly, do not paraphrase.
  - One commit per step (the message in that step's "Commit:" line).
    This satisfies CLAUDE.md "one commit per session or major change"
    for each Step. The terminal commit of the sub-session is the
    session's headline commit.
  - After each commit, push to origin (per the "always push" memory in
    MEMORY.md). Do not skip the push.
  - If a step has no commit (e.g. U1a Step 2.5 spike), do not invent
    one — proceed to the next step.

  ## Tooling rules

  - Use the venv if running any Python (`scripts/gen-theme-colors.py`
    in U1a Step 3 needs the `colour` package — `pip install colour
    --quiet` inside the venv).
  - Build commands are in fcitx5-engine/CLAUDE.md and the per-step
    bodies. Do not invent flags.
  - Engine tests: `ninja -C fcitx5-engine/build test`. Qt-app tests:
    `ctest --test-dir settings/qt6-app/build`. Both must be green
    before the headline commit.
  - Never commit with failing tests (CLAUDE.md hard rule).
  - Never use destructive git ops (`reset --hard`, force push,
    `clean -fd`) without asking. If you hit a merge conflict or
    surprise file, investigate — never delete to "make it go away".
  - For UI work: build, run `./build/xtype-settings` on this Plasma 6
    Wayland host, and visually check before declaring a step done.
    Type-check + qmllint are not sufficient.

  ## Decision-support rules
  - Before substantive work in each step (writing the file, not
    exploring), call advisor() once. The advisor sees this prompt and
    the transcript so far.
  - If a §4 challenge's solution doesn't match what you observe (e.g.
    KWin blur fails differently in U1a Step 2.5), STOP, document the
    observation, update 00-shared.md §4 with the new decision, then
    proceed. Do not silently deviate.
  - For "comingSoon" controls (§4 #13): they must still write to
    TOML. Do not gate the writer on `enabled`. Easy to miss.
  - For TOML round-trip (§4 #8/#9): the U1b Step 6 tests are
    mandatory and they must pass before the U1b headline commit.

  ## Hard "do not" list

  - Do NOT modify prototype/index.html or any prototype/*.css/jsx — the
    prototype is the design reference, not a target for edits.
  - Do NOT add features not listed in the active sub-session. Three
    similar lines beats a premature abstraction (per CLAUDE.md).
  - Do NOT add comments explaining what code does. Only add a comment
    when the WHY is non-obvious (per CLAUDE.md).
  - Do NOT create planning, decision, or summary docs unless the step
    body asks for one (e.g. docs/config-migration.md in U1a Step 1).
  - Do NOT add backwards-compatibility shims beyond the
    `tab_accepts_word` → `partial_accept` dual-loader specified in U1a
    Step 1.
  - Do NOT exceed 2000 chars on the engine system prompt (CLAUDE.md
    hard rule — only relevant if you touch prompt_builder.cpp).

  ## Reporting cadence

  - One short sentence before the first tool call per step ("Starting
    Step X — <one-line goal>").
  - Short status updates at branch points: "found X, switching
    approach to Y" or "blocked on Z, calling advisor".
  - End-of-step: one or two sentences confirming the checkpoint passed
    and the commit landed.
  - End of session: list the commits made, confirm tests + smoke
    checks green, state which sub-session is next.

  ## When you are done

  For U1a: end-of-step on Step 4. Do not start U1b.
  For U1b: end-of-step on Step 7. Do not start U1c.
  For U1c: end-of-step on Step 12 — this closes UI-PLAN U1.

  If you finish early or hit ambiguity, stop and ask. Do not improvise
  beyond the step body.

  Begin by reading the five files in the read-order list. Then start
  Step 0 (U1a) / Step 5 (U1b) / Step 8 (U1c) accordingly.
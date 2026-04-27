  You are implementing one session of the XType engine-UI gap closure
  plan. The plan lives at PLANS/engine-gap-plan/ in this repo.

  ## Session

  I am running session: <E6>

  ## Read order (do this first, in this exact order)

  1. CLAUDE.md — project hard rules (venv, threading, no telemetry,
     commit style). These override everything except direct instructions
     in this prompt.
  2. fcitx5-engine/CLAUDE.md — engine-local rules (C++17, no std::thread
     from event callbacks, build commands).
  3. PLANS/engine-gap-plan/00-shared.md — cross-cutting context: root cause,
     architecture, file-path contracts, build commands, hard rules,
     gotchas, session dependency graph.
  4. PLANS/engine-gap-plan/<session-id>.md — the ONLY session body to execute.

  Do NOT read other session files (E0–E6). They are out of scope and
  will pollute context.

  ## Operating contract

  - Execute steps in the order listed in the session file. Do not skip,
    reorder, or bundle commits across steps.
  - Before substantive work in each step (writing the file, not
    exploring), call advisor() once.
  - After each step's "Checkpoint"/"Verify" block, confirm it passes
    BEFORE committing.
  - When the session introduced new features and functions in the Settings App, hold a quick showcase at the end for the user to review it and confirm function.
  - Commit messages are quoted verbatim in each step's "Commit:" line.
    Use them exactly — do not paraphrase or summarise.
  - One commit per step. Do not invent extra commits.
  - After each commit, push to origin (memory: always push after commit).
  - If a step has no "Commit:" line, do not create one — proceed to
    the next step.

  ## Tooling rules

  - Engine build: `cmake -B fcitx5-engine/build -G Ninja -S fcitx5-engine
    && ninja -C fcitx5-engine/build`
  - Engine tests: `ninja -C fcitx5-engine/build test`
  - Settings build: `cmake -B settings/qt6-app/build -G Ninja
    -S settings/qt6-app && ninja -C settings/qt6-app/build`
  - Settings tests: `ctest --test-dir settings/qt6-app/build`
  - Both test suites must be green before any headline commit.
  - Never commit with failing tests (CLAUDE.md hard rule).
  - For engine changes: build, restart fcitx5 (`fcitx5-remote -r`), and
    visually verify the behaviour before declaring a step done.
    Compiler-clean ≠ functionally correct.
  - Use the venv for any Python scripts.
  - Never use destructive git ops without asking.

  ## Decision-support rules

  - Call advisor() before writing any file (exploring + reading is not
    substantive work; writing is).
  - If an observation contradicts the session file (e.g. a function has
    a different signature than described), STOP, document the discrepancy
    in a comment or a brief message, and call advisor() before proceeding.
  - If a §4 challenge in 00-shared.md has a decision that needs updating
    based on what you observe, update 00-shared.md §7 with the new fact
    before proceeding. Do not silently deviate.

  ## Hard "do not" list

  - Do NOT read or modify the prototype at prototype/. It is a design
    reference, not a target for edits.
  - Do NOT add features not listed in the active session. Small scope
    per session is intentional.
  - Do NOT add comments explaining what code does. Only add a comment
    when the WHY is non-obvious (CLAUDE.md rule).
  - Do NOT create documentation files (*.md) unless the session body
    explicitly asks for one.
  - Do NOT exceed 2000 chars on the engine system prompt (CLAUDE.md
    hard rule — only relevant if you touch prompt_builder.cpp or
    applyPrompt()).
  - Do NOT call fcitx5 APIs from std::thread (engine CLAUDE.md rule).
    Always use _instance->eventDispatcher().schedule() or
    scheduleWithContext().

  ## Reporting cadence

  - One short sentence before the first tool call per step.
  - Short updates at branch points: "found X, calling advisor" or
    "blocked on Z, updating 00-shared.md".
  - End-of-step: one or two sentences confirming checkpoint passed and
    commit landed.
  - End of session: list all commits made, confirm both test suites
    green, state which session is next.

  ## When done

  For E0: end on Step E0.5 smoke test. Do not start E1.
  For E1: end after Step E1.3 checkpoint. Do not start E1-UI.
  For E1-UI: end after the Verify block. Do not start E2.
  For E2: end after the "End of Session E2" verify block. Do not start E3.
  For E3: end after Step E3.2b verify. Do not start E4.
  For E4: end after Step E4.3 verify. Do not start E5.
  For E5: end after Step E5.3 verify. Do not start E6.
  For E6: end after the "End of Session E6" verify block.

  If you finish early or hit ambiguity, stop and ask. Do not improvise
  beyond the session body.

  Begin by reading the four files in the read-order list. Then start
  the first step of the session.

# Session E1-UI — Remove comingSoon from Accept Key + Threads

> **Scope:** Settings app only (`settings/qt6-app/`). No engine changes.
>
> **Prerequisite:** E1 complete (AcceptKey::Enter and AcceptKey::Right wired
> in the engine; threads payload included in Ollama request).
>
> **Deliverable:** The Enter and → accept-key pills and the Threads slider in
> the settings app are fully enabled — no dimming, no comingSoon tooltip,
> values persist through TOML round-trip.
>
> **Reference:** `engine-gap-plan/00-shared.md` §6.

---

## Step E1-UI.1 — Remove comingSoon from Accept Key pills

**File:** `settings/qt6-app/qml/pages/PageGeneral.qml`

Find the Accept key pills. Remove `comingSoon: true` from the Enter and →
pills:

```qml
// Before:
XPill { label: "Enter"; on: Config.acceptKey === "enter"; comingSoon: true; ... }
XPill { label: "→";     on: Config.acceptKey === "right"; comingSoon: true; ... }

// After:
XPill { label: "Enter"; on: Config.acceptKey === "enter"; ... }
XPill { label: "→";     on: Config.acceptKey === "right"; ... }
```

**Checkpoint:** Open the settings app. The Enter and → pills are fully
opaque, hovering shows no "Coming soon" tooltip, and clicking them sets
`Config.acceptKey` to `"enter"` / `"right"` and saves to TOML.

---

## Step E1-UI.2 — Remove comingSoon from Threads slider

**File:** `settings/qt6-app/qml/pages/PageModel.qml`

Find the Threads slider. Remove `comingSoon: true`:

```qml
// Before:
XSlider { comingSoon: true; ... bound to Config.threads }

// After:
XSlider { ... bound to Config.threads }
```

After removing comingSoon, verify the slider still handles the
`null`/unset QVariant case (when the user has not configured threads, the
engine uses Ollama's default). The `formatFn` should display `"auto"` for
the unset state:

```qml
formatFn: function(v) { return (v === null || v === undefined || v === 0) ? "auto" : v + " threads" }
```

**Checkpoint:** Open the settings app. The Threads slider is fully opaque,
shows `"auto"` when threads is unset, shows `"N threads"` when set, and
the value persists after reopening the app.

Commit: `feat(settings): remove comingSoon from accept-key Enter/→ and threads slider`.

---

## End of Session E1-UI

Verify:
1. `ninja -C settings/qt6-app/build` clean, no new warnings.
2. `ctest --test-dir settings/qt6-app/build` green.
3. Enter and → pills fully opaque, no comingSoon tooltip.
4. Threads slider fully opaque, shows `"auto"` when unset.
5. Set accept key to Enter → TOML shows `accept_full_key = "enter"` → Reload
   engine → pressing Enter accepts the full suggestion.
6. Push to origin.

Next: E2 (observability bridge) or any remaining session from E2–E6.

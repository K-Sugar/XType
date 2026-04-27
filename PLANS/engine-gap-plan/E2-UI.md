# Session E2-UI — Live Test Input on General Page

> **Scope:** Settings app only (`settings/qt6-app/`).
>
> **Prerequisite:** E2 complete.
>
> **Deliverable:** The "Live preview" card on PageGeneral is replaced by a
> styled multi-line text area where the user can type directly and observe
> XType's ghost-text suggestions appearing as preedit inline. The
> animated typewriter (LiveDemo) is removed.
>
> **Why preedit works here:** `TextArea` in Qt6 QML honours the input-method
> protocol. When fcitx5 + XType is active, any focused `TextArea` in any app
> receives preedit events — the settings app is no special case. XType's
> `setClientPreedit` call renders as underlined inline text in the field.

---

## Step E2-UI.1 — Replace LiveDemo card in PageGeneral.qml

**File:** `settings/qt6-app/qml/pages/PageGeneral.qml`

Replace the existing "Live preview" `XCard` block:

```qml
// ── Live preview ───────────────────────────────────────────────
XSection { title: "Live preview"; width: parent.width }

XCard {
    width: parent.width
    height: demo.implicitHeight + 32

    Text {
        x: 0; y: 0
        text: "Ghost text typewriter"
        ...
    }

    LiveDemo {
        id: demo
        anchors { top: parent.top; topMargin: 22; left: parent.left; right: parent.right }
        paused: !Config.engineEnabled
    }
}
```

with a new card containing a `TextArea`:

```qml
// ── Live preview ───────────────────────────────────────────────
XSection { title: "Live preview"; num: "02"; width: parent.width }

XCard {
    width: parent.width
    // Fixed height tall enough for ~3 lines of input + label
    height: 110

    Text {
        x: 0; y: 0
        text: "Type here to test suggestions"
        font.family: Theme.sansFamily; font.pixelSize: 11
        font.capitalization: Font.AllUppercase; font.letterSpacing: 1.2
        color: Theme.ink45
        renderType: Text.NativeRendering
    }

    TextArea {
        id: liveInput
        anchors {
            top: parent.top; topMargin: 22
            left: parent.left; right: parent.right; bottom: parent.bottom
        }
        placeholderText: "Start typing…"
        font.family: Theme.monoFamily; font.pixelSize: 13
        color: Theme.ink100
        wrapMode: TextArea.Wrap
        background: null          // XCard provides the surface
        renderType: Text.NativeRendering
        enabled: Config.engineEnabled
        opacity: Config.engineEnabled ? 1.0 : 0.45
    }
}
```

**Note:** `XSection` numbering — check the existing section numbers on
PageGeneral and continue the sequence. If "01 Engine" is already numbered,
renumber "Live preview" appropriately (e.g. "03" if there's a "02 Behaviour").

---

## Step E2-UI.2 — Remove LiveDemo.qml

Now that LiveDemo is unused, delete it and remove it from the build:

1. Delete `settings/qt6-app/qml/pages/LiveDemo.qml`.
2. In `settings/qt6-app/CMakeLists.txt`, remove the line:
   ```
   qml/pages/LiveDemo.qml
   ```

The `RecentEventsModel` backend (recent_events.json) stays in place —
it feeds future features (E3 and beyond may surface it differently).
The `_liveEvent` property and `Connections` block that were added in E2
disappear along with LiveDemo.qml.

---

## End of Session E2-UI

Verify:
1. `ninja -C settings/qt6-app/build` clean, no new warnings.
2. `ctest --test-dir settings/qt6-app/build` green.
3. Open xtype-settings → General page → "Live preview" card shows a text
   input area (not the typewriter animation).
4. With fcitx5 + XType active, focus the input, type 10+ chars, pause
   briefly — ghost-text suggestion appears underlined in the field.
5. Tab accepts the suggestion into the field.
6. Push to origin.

**Commit:** `feat(settings): replace ghost-text typewriter with live test input on General page`

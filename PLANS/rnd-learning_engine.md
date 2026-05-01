# XType Learning Engine: Current vs. Ideal

## What It Does Now (End-to-End)

### 1. Corpus Collection

Every printable ASCII character the user types flows through `appendChar()` and is *also* appended
to a separate `_userTypedSinceLastTerminator` buffer. This buffer is intentionally disconnected from
the AI accept paths — accepting a suggestion does **not** add anything to it. When the user types
`.`, `!`, or `?`, `harvestSentence()` fires and passes the buffer to `CorpusCollector::record()`.
`deactivate` (focus-out) also flushes if the buffer is ≥ 12 chars.

At `record()`, the sentence is filtered synchronously:
- Length ≥ `min_sentence_chars` (default 12)
- Contains at least one alpha character
- Does not contain a code-shape pattern (`{}=;` run ≥ 2)

Sentences that pass go into a lock-protected in-memory queue (capacity 1000). The background flush
thread wakes every 60s (or when the queue hits 100 entries) and appends to `corpus.txt`, writing a
parallel timestamp to `corpus_timestamps.txt`. After each flush, `pruneOldEntries()` is called with
`forget_after_days` — currently a no-op because the default is 0 (disabled).

### 2. Style Profiling

`StyleProfile::loadFromCorpus()` reads up to **1000 lines** from the beginning of corpus.txt.
Each line is passed through line-level privacy filters (email-like patterns, digit runs ≥5, keywords
"password/secret/token/auth"), then split into sentences at `.!?\n`. Sentences shorter than 12 chars
or missing alpha are dropped.

From the survivors:
- `avgSentenceChars` — mean length in chars, used for `num_predict` calibration
- `commonOpeners` — top-10 sentence-opening word bigrams
- Exemplars — 3–5 sentences sampled across short/medium/long length buckets, after trimming the
  longest and shortest 5% of the pool

The profile is serialized to `style_profile.json` and injected into the system prompt. The profiler
runs on a background worker every 300s and only rebuilds if the profile is stale (corpus modified
>30 min ago, or corpus has grown >200 new lines).

### 3. Config Fields and Their Real Status

| Field | Config default | Status |
|---|---|---|
| `learning.enabled` | `false` | Opt-in; whole corpus + profiler disabled by default |
| `voice_strength` | `50` | **Wired** — slices exemplar array: `ceil(all.size() * vs/100)`. Real gap is coarse resolution (see Gap §5) |
| `forget_after_days` | `0` | **Wired** — `pruneOldEntries()` is called every flush; gap is default-off + no UI |
| `max_corpus_mb` | `50` | Wired — rotation renames to `.1` when exceeded (see Gap §3) |
| `include_examples_in_prompt` | `true` | Wired |

---

## On Accepted Predictions as a Learning Signal

You asked: *"maybe the accepted predictions should actually get a heavier weighting as the user is
happy with it — or am I wrong?"*

The intuition is right. The corpus is the wrong place to apply it.

There is a hard rule (CLAUDE.md) that AI suggestions must never enter the corpus — only
user-typed characters via `appendChar` may be recorded. This rule exists for a concrete reason: if
AI-generated text enters the corpus, it influences exemplar selection, which shapes future prompts,
which produces more similar suggestions, which are more likely to be accepted. Over many cycles,
the "user's style" in the corpus drifts away from how the user actually writes toward how the model
imitates them. This is model collapse applied to style personalization.

But accepted completions do carry signal — specifically, they tell you which phrasings the user
finds natural enough to keep. The right channel is **retrieval weighting, not corpus content**.

**Implemented mechanism (embedding-based, not bigram):**

The original bigram design was superseded by a stronger embedding approach:

1. **In-session ring**: When the user accepts a suggestion, the engine snapshots the
   `_lastQueryEmbedding` (the embedding vector of the user-typed context at accept time) and
   pushes it into `_acceptEmbedRing` — a main-thread `std::deque` capped at 20 entries.

2. **Retrieval scoring**: `scoreAndRankExemplars()` blends query-to-exemplar cosine similarity
   with the max accept-ring cosine boost:
   `score = 0.7 × dot(query, exemplar) + 0.3 × max(dot(ring[i], exemplar))`.
   Exemplars whose embeddings align with contexts where the user previously accepted suggestions
   are ranked higher.

3. **Persistence**: On each accept event `writeAcceptSignals()` atomically serialises the full
   ring to `~/.local/share/xtype/accept_signals.json` (JSON array of float arrays). On engine
   start `loadAcceptSignals()` seeds `_acceptEmbedRing` from that file, with a dimension-
   consistency check to discard stale files from model changes.

4. The corpus.txt itself stays pure user-typed text. The accept signal steers *which* real
   sentences get shown to the model, not the content of those sentences.

The vectors stored are embeddings of **user-typed context**, not the suggestion text, and are
not invertible to text — the privacy constraint is preserved.

---

## Gap Analysis

### 1. Profiler reads the oldest 1000 sentences, not the most recent

**Current:** `loadFromCorpus()` calls `std::getline` up to `kMaxScannedSentences = 1000` from the
beginning of the file. Sentences are appended, so the beginning is the oldest content.

A corpus that has grown to 50k sentences produces a style profile built entirely from text the
user wrote months or years ago. Recent style evolution is invisible to the model.

**Fix vector:** Read the tail of the file, not the head. The profiler should seek to
`max(0, file_size - N_bytes)` and scan forward from the nearest newline. A separate index file (or
the timestamps file) could make this efficient without full re-reads.

---

### 2. No deduplication — boilerplate sentences dominate exemplar sampling

**Current:** The same sentence written identically multiple times (email greetings, sign-offs,
template phrases) gets stored repeatedly. `pickExemplars()` samples uniformly from the length
buckets, so high-frequency phrases are proportionally more likely to be picked.

A user who sends 50 emails starting with "Thanks for reaching out" will have that sentence appear
in exemplars far more than their actual varied writing.

**Fix vector:** Before profiling, deduplicate corpus lines by exact match and near-duplicate
hash (shingling). Alternatively, inverse-document-frequency weight sentences: sentences that appear
many times carry less style signal than rare, distinctive ones and should be penalized in sampling.

---

### 3. Corpus rotation loses history permanently

**Current:** When `corpus.txt` exceeds `max_corpus_mb` (50MB), `rotateIfNeeded()` renames it to
`corpus.txt.1`. The profiler only knows about `corpus.txt` — `corpus.txt.1` is never consulted
again. After rotation, the profile is rebuilt from a near-empty new corpus and degrades to generic
completions until enough new data accumulates.

**Fix vector:** Either: (a) keep a fixed number of rotation files and have the profiler read all of
them (preferring newer), or (b) maintain a compact "profile corpus" file that holds the best N
exemplars extracted from each rotation, so learned style survives the rotation boundary.

---

### 4. No domain/app segmentation

**Current:** All apps feed one flat corpus. A user who writes formal documentation in one app and
casual chat messages in another gets mixed exemplars regardless of what they're doing right now.
The app name is already tracked at harvest time (`ic->program()`) but is not attached to corpus
sentences.

**Fix vector:** Tag each corpus line with a short app-id at write time
(`thunderbird\tSentence text here\n`). At profile time, if the current app matches, weight those
sentences more heavily in exemplar selection. Requires a corpus format migration but is backwards
compatible (untagged lines keep existing behavior).

---

### 5. `voice_strength` has too-coarse resolution

**Current:** The wired implementation does `ceil(all.size() * voice_strength / 100.0)`, where
`all.size()` is at most 5 (the profiler picks a maximum of 5 exemplars). This means
`voice_strength` only has 5 meaningful levels: 0 exemplars (0%), 1 (1–20%), 2 (21–40%), 3
(41–60%), 4 (61–80%), 5 (81–100%). A setting of 30 or 45 or 70 all behave identically to their
neighboring step value.

**Fix vector:** Increase the exemplar pool the profiler picks before slicing. If the profiler
retained 20 exemplars internally and `voice_strength` sliced that set, you'd get 20 meaningful
levels instead of 5. Alternatively, re-interpret `voice_strength` as a probability-of-inclusion
parameter applied per-sentence at prompt-build time rather than a hard slice of a small set.

---

### 6. ~~No accept/reject signal influences retrieval~~ — IMPLEMENTED

**Was:** `recent_events.json` recorded `accepted: bool` per suggestion event but only for metrics
display, with no path back to corpus or profiler.

**Implemented:** Accept signals now influence exemplar retrieval via the embedding ring approach
described in "On Accepted Predictions" above. On each accept event the engine:
- Stores the context query embedding in `_acceptEmbedRing` (main-thread deque, cap 20)
- Persists the ring to `accept_signals.json` (atomic write)
- Seeds the ring from disk on engine start (`loadAcceptSignals()` in constructor)

`scoreAndRankExemplars()` blends query similarity with accept-ring boost at 70/30 weight.
Dimension-mismatch detection handles embedding model changes gracefully (discards stale file).

---

### 7. Privacy filters are incomplete

**Current filters:**
- Email-like: `@` flanked by non-whitespace + `.` after domain (matches `user@host.com`)
- Long digit run: ≥5 consecutive digits
- Keyword list: "password", "secret", "token", "auth"

**Gaps:**
- No URL detection (`https://`, `www.`, bare domains) — web addresses in copied text would be
  learned verbatim
- Keyword list misses: "credentials", "passphrase", "private key", "wallet", "seed phrase", "ssn",
  "dob", "cvv"
- 5-digit cutoff misses 4-digit PINs and short account numbers
- No phone number pattern (e.g., `XXX-XXX-XXXX` or `+1 XXX XXX XXXX`)
- Filters run only at profile-build time on corpus lines, not at `record()` time — a sentence
  containing sensitive keywords is stored in `corpus.txt` and only filtered during profiling

**Fix vector:** Move a superset of the privacy filters into `CorpusCollector::acceptable()` so
sensitive text is never written to disk. Expand the keyword list. Add URL and phone heuristics.
4-digit digit runs should also be filtered (most benign prose doesn't contain isolated 4-digit
numbers).

---

### 8. Code-shape filter is too narrow for modern codebases

**Current:** `hasCodeShape()` looks for runs of `{`, `}`, `;`, `=` with count ≥ 2. This catches
C/C++/Java style but misses:
- Python (indented blocks, colons, no braces)
- YAML/TOML/INI config snippets (key: value, [section])
- SQL (`SELECT`, `FROM`, `WHERE` patterns)
- Markdown (backtick fences, `##` headers, `- ` list prefixes if they sneak through)
- Shell commands (`$`, `|`, `>`, `&&`)

A user typing in a terminal or editor could accidentally harvest short code fragments that pass
the current filter.

**Fix vector:** Add heuristics for `$()`, `|`, `->`, `=>`, `:=`, multiple backticks, and lines
starting with `#!` or `---`. Also consider a minimum ratio of alpha chars to total chars
(prose is predominantly alpha; code is not).

---

## Summary Scorecard

| Capability                     | Current State                                   | Gap Severity |
|--------------------------------|-------------------------------------------------|--------------|
| Recency weighting              | ✓ Reads 256 KB tail of corpus                   | ~~Critical~~ |
| Accept/reject signal           | ✓ Embedding ring + persist to accept_signals.json | ~~High~~   |
| Deduplication                  | ✓ Exact-match dedup + inverse-freq weighting    | ~~High~~     |
| Corpus rotation continuity     | ✓ corpus_seed.json extracted before rotate      | ~~High~~     |
| Privacy filters (at record)    | ✓ hasPrivateTerm() in acceptable()              | ~~High~~     |
| App/domain segmentation        | ✓ app\ttext format + 2× weight for current app  | ~~Medium~~   |
| `voice_strength` resolution    | ✓ kMaxEmbedExemplars=20, 0–100 → 0–20 levels   | ~~Medium~~   |
| Code-shape filter completeness | ✓ Python, YAML, SQL, Markdown, Shell, C/C++     | ~~Medium~~   |
| `forget_after_days`            | Wired; default-off, no UI exposure              | Low          |

---

## What to Fix First

*All high-priority gaps closed.* The remaining open item is `forget_after_days` UI exposure (low
severity — the engine already honours the config field; only the settings app UI is missing).

Historical priority order (all completed):

1. **Recency** — flip `loadFromCorpus()` to read the tail of the file, not the head. ✓
2. **Accept signal routing** — embed-ring persistence via `accept_signals.json`. ✓
3. **Rotation continuity** — `corpus_seed.json` extracted before rename. ✓
4. **Privacy at record time** — `hasPrivateTerm()` in `CorpusCollector::acceptable()`. ✓
   `CorpusCollector::acceptable()` so nothing sensitive reaches disk.

Items 1–2 are the highest leverage. Item 1 costs ~10 lines of code. Item 2 is new but small and
self-contained. Together they would make the learning engine actually responsive to how the user
writes *now* and what they like *today*, rather than a snapshot of their oldest writing averaged
with their current.

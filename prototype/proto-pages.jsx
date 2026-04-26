/* global React, Toggle, Slider, Pill, Section, Row, Bars, useState, useEffect, useRef */
// Hooks already destructured to globals in proto-primitives.jsx — do not redeclare here.

// ============ LIVE GHOST-TEXT DEMO ============
const DEMO_SEQUENCES = [
  { typed: 'Thanks for the heads up — ', ghost: "I'll review the doc tonight and circle back tomorrow.", app: 'Thunderbird' },
  { typed: 'def calculate_total(items):\n    return ', ghost: 'sum(item.price * item.qty for item in items)', app: 'Kate' },
  { typed: 'Hey! Are you free ', ghost: 'around 7 to grab dinner at that place we talked about?', app: 'Discord' },
  { typed: 'TODO: refactor the auth ', ghost: 'middleware to use the new token validator helper.', app: 'Kate' },
];

const LiveDemo = ({ paused }) => {
  const [seqIdx, setSeqIdx] = useState(0);
  const [typedLen, setTypedLen] = useState(0);
  const [ghostShown, setGhostShown] = useState(false);
  const [accepted, setAccepted] = useState(false);

  const seq = DEMO_SEQUENCES[seqIdx];

  useEffect(() => {
    if (paused) return;
    let timer;
    if (typedLen < seq.typed.length) {
      timer = setTimeout(() => setTypedLen(typedLen + 1), 45 + Math.random() * 60);
    } else if (!ghostShown) {
      timer = setTimeout(() => setGhostShown(true), 380);
    } else if (!accepted) {
      timer = setTimeout(() => setAccepted(true), 1800);
    } else {
      timer = setTimeout(() => {
        setSeqIdx((seqIdx + 1) % DEMO_SEQUENCES.length);
        setTypedLen(0);
        setGhostShown(false);
        setAccepted(false);
      }, 1400);
    }
    return () => clearTimeout(timer);
  }, [seqIdx, typedLen, ghostShown, accepted, seq, paused]);

  const visibleTyped = seq.typed.slice(0, typedLen);
  const finalView = accepted;

  return (
    <div className="demo">
      <div className="demo-label">
        <span className="live-dot"></span>
        <span>live · {seq.app}</span>
      </div>
      <span className="typed" style={{ whiteSpace: 'pre-wrap' }}>{visibleTyped}</span>
      {finalView && <span className="typed" style={{ whiteSpace: 'pre-wrap' }}>{seq.ghost}</span>}
      {!finalView && ghostShown && <span className="ghost" style={{ whiteSpace: 'pre-wrap' }}>{seq.ghost}</span>}
      {typedLen >= seq.typed.length && !accepted && <span className="caret"></span>}
      {!ghostShown && typedLen < seq.typed.length && <span className="caret"></span>}
    </div>
  );
};

// ============ PAGE: GENERAL ============
const PageGeneral = ({ s, set, demoPaused }) => (
  <div className="page">
    <div className="page-header">
      <div>
        <h1 className="page-title">General</h1>
        <div className="page-sub">Core engine behavior and acceptance gestures.</div>
      </div>
      <div className="flex gap-8">
        <span className="kbd">Tab</span>
        <span className="muted" style={{fontSize: 12, alignSelf: 'center'}}>to accept</span>
      </div>
    </div>

    <Section num="01" title="Engine">
      <Row label="Enable XType globally" desc="Inline ghost-text in every Wayland text field">
        <Toggle on={s.enabled} onChange={v => set('enabled', v)} />
      </Row>
      <Row label="Trigger" desc="When suggestions appear">
        <div className="segmented">
          <span className={`seg ${s.trigger === 'pause' ? 'on' : ''}`} onClick={() => set('trigger', 'pause')}>typing pause</span>
          <span className={`seg ${s.trigger === 'manual' ? 'on' : ''}`} onClick={() => set('trigger', 'manual')}>manual ⌘.</span>
        </div>
      </Row>
      <Row label="Trigger delay" desc="Time after you stop typing">
        <Slider value={s.delay} min={100} max={800} step={25} onChange={v => set('delay', v)} format={v => `${v} ms`} />
      </Row>
      <Row label="Max suggestion length" desc="Tokens before XType stops generating">
        <Slider value={s.maxLen} min={4} max={48} step={1} onChange={v => set('maxLen', v)} format={v => `${v} tok`} />
      </Row>
    </Section>

    <Section num="02" title="Acceptance">
      <Row label="Accept key" desc="Confirm the entire ghost text">
        <div className="flex gap-8">
          {['Tab', 'Enter', '→'].map(k => (
            <Pill key={k} on={s.acceptKey === k} onClick={() => set('acceptKey', k)}>{k}</Pill>
          ))}
        </div>
      </Row>
      <Row label="Partial accept (word-by-word)" desc="Use Ctrl+→ to accept one word at a time">
        <Toggle on={s.partial} onChange={v => set('partial', v)} />
      </Row>
      <Row label="Dismiss on Esc" desc="Pressing Escape clears the current suggestion">
        <Toggle on={s.dismissEsc} onChange={v => set('dismissEsc', v)} />
      </Row>
    </Section>

    <Section title="Live preview">
      <LiveDemo paused={demoPaused || !s.enabled} />
    </Section>
  </div>
);

// ============ PAGE: PERSONALISATION ============
const PagePersonalisation = ({ s, set }) => {
  const [animKey, setAnimKey] = useState(0);
  useEffect(() => { const t = setInterval(() => setAnimKey(k => k + 1), 4000); return () => clearInterval(t); }, []);
  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h1 className="page-title">Personalisation</h1>
          <div className="page-sub">XType learns your voice from text you accept.</div>
        </div>
      </div>

      <div className="stat-grid" style={{ marginBottom: 24 }}>
        <div className="card">
          <div className="stat-eyebrow">Words learned</div>
          <div className="stat-value">14,302</div>
          <Bars values={[6, 8, 5, 9, 7, 11, 12]} animateKey={animKey} />
        </div>
        <div className="card">
          <div className="stat-eyebrow">Accept rate · 7d</div>
          <div className="stat-value">62%</div>
          <Bars values={[4, 5, 6, 5, 7, 8, 9]} animateKey={animKey} />
        </div>
        <div className="card">
          <div className="stat-eyebrow">Time saved · 7d</div>
          <div className="stat-value">4h 12m</div>
          <Bars values={[2, 4, 3, 5, 6, 7, 8]} animateKey={animKey} />
        </div>
      </div>

      <Section num="01" title="Style learning">
        <Row label="Learn from accepted text" desc={`${(2481).toLocaleString()} sentences absorbed`}>
          <Toggle on={s.learn} onChange={v => set('learn', v)} />
        </Row>
        <Row label="Voice match strength" desc="How aggressively XType matches your tone">
          <Slider value={s.voice} min={0} max={100} step={5} onChange={v => set('voice', v)} format={v => `${v}%`} />
        </Row>
        <Row label="Forget after 30 days" desc="Auto-prune sentences that haven't been accepted again">
          <Toggle on={s.forget} onChange={v => set('forget', v)} />
        </Row>
      </Section>

      <Section num="02" title="Your voice profile">
        <div className="card">
          <div style={{ fontSize: 13, lineHeight: 1.6, color: 'var(--ink-80)' }}>
            Your writing tends to be <span style={{ color: 'var(--purple-soft)' }}>concise</span>,
            uses <span style={{ color: 'var(--purple-soft)' }}>em-dashes</span> often,
            and avoids corporate openers like "I hope this finds you well."
          </div>
          <div style={{ marginTop: 14, display: 'flex', gap: 8 }}>
            <button className="btn ghost">Reset profile</button>
            <button className="btn">Export profile</button>
          </div>
        </div>
      </Section>
    </div>
  );
};

// ============ PAGE: BLOCK LIST ============
const PageBlockList = ({ s, set, toast }) => {
  const [phraseDraft, setPhraseDraft] = useState('');
  const apps = [
    { id: 'pwd',   label: 'Password fields', sub: 'system-detected' },
    { id: '1pw',   label: '1Password',       sub: 'desktop app' },
    { id: 'kpx',   label: 'KeePassXC',       sub: 'desktop app' },
    { id: 'discord', label: 'Discord (DMs)', sub: 'category match' },
    { id: 'kons',  label: 'Konsole',         sub: 'terminal' },
  ];
  const toggleApp = (id) => {
    const has = s.blockedApps.includes(id);
    set('blockedApps', has ? s.blockedApps.filter(x => x !== id) : [...s.blockedApps, id]);
  };
  const addPhrase = () => {
    if (!phraseDraft.trim()) return;
    set('blockedPhrases', [...s.blockedPhrases, phraseDraft.trim()]);
    setPhraseDraft('');
    toast(`Phrase blocked`);
  };
  const removePhrase = (p) => {
    set('blockedPhrases', s.blockedPhrases.filter(x => x !== p));
    toast(`Phrase removed`);
  };
  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h1 className="page-title">Block list</h1>
          <div className="page-sub">Where XType should stay silent.</div>
        </div>
      </div>

      <Section num="01" title="Never suggest in…">
        <div className="pills" style={{ marginBottom: 8 }}>
          {apps.map(a => (
            <Pill key={a.id} on={s.blockedApps.includes(a.id)} onClick={() => toggleApp(a.id)}>{a.label}</Pill>
          ))}
          <Pill add onClick={() => toast('App picker — demo')}>+ add app</Pill>
        </div>
      </Section>

      <Section num="02" title="Blocked phrases">
        <div style={{ display: 'flex', gap: 8, marginBottom: 12 }}>
          <input
            className="input"
            placeholder="e.g. /home/user/secrets/  or  /\bAPI_KEY=\w+\b/"
            value={phraseDraft}
            onChange={e => setPhraseDraft(e.target.value)}
            onKeyDown={e => e.key === 'Enter' && addPhrase()}
          />
          <button className="btn primary" onClick={addPhrase}>Block</button>
        </div>
        {s.blockedPhrases.length === 0 ? (
          <div className="empty">No phrases blocked yet.</div>
        ) : (
          <div>
            {s.blockedPhrases.map(p => (
              <div key={p} className="phrase-row">
                <span className="text">{p}</span>
                <span className="x" onClick={() => removePhrase(p)}>×</span>
              </div>
            ))}
          </div>
        )}
      </Section>
    </div>
  );
};

// ============ PAGE: PER-APP ============
const APPS = [
  { id: 'kate',    name: 'Kate',        cls: 'kate', initial: 'K', defaultMode: 'Code-aware' },
  { id: 'tb',      name: 'Thunderbird', cls: 'tb',   initial: 'T', defaultMode: 'Email tone' },
  { id: 'kons',    name: 'Konsole',     cls: 'kons', initial: 'K', defaultMode: 'Off' },
  { id: 'firefox', name: 'Firefox',     cls: 'fox',  initial: 'F', defaultMode: 'Default' },
  { id: 'discord', name: 'Discord',     cls: 'disc', initial: 'D', defaultMode: 'Casual' },
];
const MODES = ['Default', 'Code-aware', 'Email tone', 'Casual', 'Off'];

const PagePerApp = ({ s, set }) => {
  const [selected, setSelected] = useState('kate');
  const ap = s.apps[selected] || { on: true, mode: APPS.find(a => a.id === selected)?.defaultMode };
  const updateApp = (key, value) => {
    set('apps', { ...s.apps, [selected]: { ...ap, [key]: value } });
  };
  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h1 className="page-title">Per-app settings</h1>
          <div className="page-sub">Different behavior in different apps.</div>
        </div>
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 18 }}>
        <div>
          {APPS.map(a => {
            const cur = s.apps[a.id] || { on: true, mode: a.defaultMode };
            return (
              <div key={a.id} className={`app-row ${selected === a.id ? 'selected' : ''}`} onClick={() => setSelected(a.id)}>
                <div className="flex gap-12" style={{ alignItems: 'center' }}>
                  <div className={`app-icon ${a.cls}`}>{a.initial}</div>
                  <div>
                    <div style={{ fontSize: 13, fontWeight: 500 }}>{a.name}</div>
                    <div style={{ fontSize: 11, color: 'var(--ink-45)' }}>{cur.mode}</div>
                  </div>
                </div>
                <Toggle on={cur.on} onChange={v => set('apps', { ...s.apps, [a.id]: { ...cur, on: v } })} />
              </div>
            );
          })}
        </div>
        <div className="card" style={{ alignSelf: 'flex-start' }}>
          <div style={{ fontSize: 11, fontWeight: 600, letterSpacing: '0.16em', textTransform: 'uppercase', color: 'var(--ink-45)', marginBottom: 14 }}>
            Configuring · {APPS.find(a => a.id === selected)?.name}
          </div>
          <Row label="Enabled in this app">
            <Toggle on={ap.on} onChange={v => updateApp('on', v)} />
          </Row>
          <Row label="Mode" desc="Profile preset for tone & length">
            <select className="input" style={{ width: 140, fontFamily: 'var(--sans)', padding: '6px 10px' }} value={ap.mode} onChange={e => updateApp('mode', e.target.value)}>
              {MODES.map(m => <option key={m} value={m}>{m}</option>)}
            </select>
          </Row>
          <Row label="Suggestion length" desc="Override global setting">
            <Slider value={ap.len ?? 12} min={4} max={48} step={1} onChange={v => updateApp('len', v)} format={v => `${v} tok`} />
          </Row>
        </div>
      </div>
    </div>
  );
};

// ============ PAGE: MODEL ============
const MODELS = [
  { id: 'qwen3', name: 'Qwen3-1.7B-Instruct', size: '1.1 GB', speed: '~32 tok/s', kind: 'Q4', current: true },
  { id: 'phi35', name: 'Phi-3.5-mini',        size: '2.4 GB', speed: '~22 tok/s', kind: 'Q4' },
  { id: 'l32',   name: 'Llama-3.2-3B',        size: '2.0 GB', speed: '~18 tok/s', kind: 'Q4' },
  { id: 'g2',    name: 'Gemma-2B',            size: '1.4 GB', speed: '~28 tok/s', kind: 'Q4' },
];

const PageModel = ({ s, set, openModal, toast }) => {
  const active = MODELS.find(m => m.id === s.activeModel) || MODELS[0];
  const trySwitch = (m) => {
    if (m.id === s.activeModel) return;
    openModal({
      title: `Switch to ${m.name}?`,
      desc: `XType will pause for ~5 seconds while the new model loads. Existing text won't be lost.`,
      confirm: () => { set('activeModel', m.id); toast(`Loaded ${m.name}`); },
      confirmLabel: 'Switch model',
    });
  };
  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h1 className="page-title">Model</h1>
          <div className="page-sub">Local inference. Nothing leaves your machine.</div>
        </div>
      </div>

      <Section num="01" title="Active model">
        <div className="card accent">
          <div className="flex between" style={{ marginBottom: 14 }}>
            <div>
              <div style={{ fontSize: 16, fontWeight: 600, letterSpacing: '-0.01em', color: 'var(--purple-soft)' }}>{active.name}</div>
              <div className="mono" style={{ fontSize: 11.5, color: 'var(--ink-65)', marginTop: 4 }}>{active.size} · CPU · {active.speed} · {active.kind}</div>
            </div>
            <Pill on>active</Pill>
          </div>
          <div className="stat-grid">
            <div>
              <div className="stat-eyebrow">Latency</div>
              <div className="stat-value stat-mini">120 ms</div>
            </div>
            <div>
              <div className="stat-eyebrow">RAM</div>
              <div className="stat-value stat-mini">1.4 GB</div>
            </div>
            <div>
              <div className="stat-eyebrow">CPU · current</div>
              <div className="stat-value stat-mini">18%</div>
            </div>
          </div>
        </div>
      </Section>

      <Section num="02" title="Inference settings">
        <Row label="Quantisation" desc="Smaller = faster + less RAM, slightly lower quality">
          <div className="segmented">
            {['F16', 'Q8', 'Q4', 'Q3'].map(q => (
              <span key={q} className={`seg ${s.quant === q ? 'on' : ''}`} onClick={() => set('quant', q)}>{q}</span>
            ))}
          </div>
        </Row>
        <Row label="Context window" desc="Tokens of surrounding text the model sees">
          <Slider value={s.ctx} min={512} max={4096} step={256} onChange={v => set('ctx', v)} format={v => `${v} tok`} />
        </Row>
        <Row label="Threads" desc={`${s.threads} of 12 cores`}>
          <Slider value={s.threads} min={1} max={12} step={1} onChange={v => set('threads', v)} format={v => `${v}`} />
        </Row>
      </Section>

      <Section num="03" title="Available models">
        <div>
          {MODELS.filter(m => m.id !== s.activeModel).map(m => (
            <div key={m.id} className="app-row" onClick={() => trySwitch(m)}>
              <div className="flex gap-12" style={{ alignItems: 'center' }}>
                <div className="app-icon kde">M</div>
                <div>
                  <div style={{ fontSize: 13, fontWeight: 500 }}>{m.name}</div>
                  <div className="mono" style={{ fontSize: 11, color: 'var(--ink-45)' }}>{m.size} · {m.speed} · {m.kind}</div>
                </div>
              </div>
              <button className="btn">Load</button>
            </div>
          ))}
          <div className="app-row" style={{ borderStyle: 'dashed', justifyContent: 'center', color: 'var(--ink-45)' }} onClick={() => toast('Import GGUF — demo')}>
            + Import GGUF model
          </div>
        </div>
      </Section>
    </div>
  );
};

// ============ PAGE: ABOUT ============
const PageAbout = ({ logs }) => (
  <div className="page">
    <div className="page-header">
      <div>
        <h1 className="page-title">About</h1>
        <div className="page-sub">Build info, logs, and links.</div>
      </div>
    </div>

    <div className="card" style={{ marginBottom: 24 }}>
      <div className="flex gap-16" style={{ alignItems: 'center' }}>
        <div style={{
          width: 64, height: 64, borderRadius: 16,
          background: 'linear-gradient(135deg, oklch(0.7 0.18 var(--hue)), oklch(0.40 0.15 calc(var(--hue) - 15)))',
          boxShadow: '0 0 28px var(--purple-glow), inset 0 1px 0 rgba(255,255,255,0.25)',
          display: 'flex', alignItems: 'center', justifyContent: 'center',
          fontSize: 28, fontWeight: 700, color: 'white', letterSpacing: '-0.04em',
        }}>X</div>
        <div className="f1">
          <div style={{ fontSize: 22, fontWeight: 600, letterSpacing: '-0.02em' }}>XType</div>
          <div className="mono" style={{ fontSize: 11.5, color: 'var(--ink-45)', marginTop: 4 }}>v0.4.2 · Fcitx5 engine · MIT License</div>
          <div style={{ fontSize: 13, color: 'var(--ink-65)', marginTop: 6 }}>Local-first ghost-text autocomplete for Linux.</div>
        </div>
        <div className="flex gap-8" style={{ flexDirection: 'column' }}>
          <button className="btn">GitHub</button>
          <button className="btn">Release notes</button>
        </div>
      </div>
    </div>

    <Section num="01" title="Live logs">
      <div className="logs">
        {logs.map((l, i) => (
          <div key={i} className={l.kind || ''}>{l.text}</div>
        ))}
      </div>
    </Section>

    <Section num="02" title="Diagnostics">
      <div className="flex gap-8">
        <button className="btn">Copy debug bundle</button>
        <button className="btn">Reset all settings</button>
        <button className="btn ghost">Report a bug</button>
      </div>
    </Section>
  </div>
);

Object.assign(window, { PageGeneral, PagePersonalisation, PageBlockList, PagePerApp, PageModel, PageAbout, LiveDemo });

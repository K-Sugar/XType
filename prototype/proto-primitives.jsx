/* global React */
const { useState, useEffect, useRef, useMemo, useCallback } = React;

// ============ ICONS ============
const Icon = ({ d, size = 14 }) => (
  <svg className="nav-icon" width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
    {d}
  </svg>
);
const ICONS = {
  general: <Icon d={<><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9c.39.16.74.42 1.02.74"/></>} />,
  person:  <Icon d={<><path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/></>} />,
  block:   <Icon d={<><circle cx="12" cy="12" r="10"/><line x1="4.93" y1="4.93" x2="19.07" y2="19.07"/></>} />,
  apps:    <Icon d={<><rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/></>} />,
  cpu:     <Icon d={<><rect x="4" y="4" width="16" height="16" rx="2"/><rect x="9" y="9" width="6" height="6"/><line x1="9" y1="1" x2="9" y2="4"/><line x1="15" y1="1" x2="15" y2="4"/><line x1="9" y1="20" x2="9" y2="23"/><line x1="15" y1="20" x2="15" y2="23"/><line x1="20" y1="9" x2="23" y2="9"/><line x1="20" y1="14" x2="23" y2="14"/><line x1="1" y1="9" x2="4" y2="9"/><line x1="1" y1="14" x2="4" y2="14"/></>} />,
  about:   <Icon d={<><circle cx="12" cy="12" r="10"/><line x1="12" y1="16" x2="12" y2="12"/><line x1="12" y1="8" x2="12.01" y2="8"/></>} />,
};

// ============ PRIMITIVES ============
const Toggle = ({ on, onChange }) => (
  <div className={`toggle ${on ? 'on' : ''}`} onClick={() => onChange(!on)} role="switch" aria-checked={on}></div>
);

const Slider = ({ value, min = 0, max = 100, step = 1, onChange, format }) => {
  const ref = useRef(null);
  const [drag, setDrag] = useState(false);
  const pct = ((value - min) / (max - min)) * 100;
  const update = useCallback((clientX) => {
    if (!ref.current) return;
    const r = ref.current.getBoundingClientRect();
    const p = Math.min(1, Math.max(0, (clientX - r.left) / r.width));
    const raw = min + p * (max - min);
    const stepped = Math.round(raw / step) * step;
    onChange(Math.min(max, Math.max(min, stepped)));
  }, [min, max, step, onChange]);
  useEffect(() => {
    if (!drag) return;
    const move = e => update(e.clientX);
    const up = () => setDrag(false);
    window.addEventListener('mousemove', move);
    window.addEventListener('mouseup', up);
    return () => { window.removeEventListener('mousemove', move); window.removeEventListener('mouseup', up); };
  }, [drag, update]);
  return (
    <div className="slider-wrap">
      <div className="slider" ref={ref} onMouseDown={e => { setDrag(true); update(e.clientX); }}>
        <div className="fill" style={{ width: `${pct}%` }}></div>
        <div className="knob" style={{ left: `${pct}%` }}></div>
      </div>
      <span className="slider-value">{format ? format(value) : value}</span>
    </div>
  );
};

const Pill = ({ on, onClick, children, add }) => (
  <span className={`pill ${on ? 'on' : ''} ${add ? 'add' : ''}`} onClick={onClick}>{children}</span>
);

const Section = ({ num, title, children }) => (
  <div className="section">
    <h3 className="section-title">
      {num && <span className="num">{num}</span>}
      <span>{title}</span>
      <span className="rule"></span>
    </h3>
    {children}
  </div>
);

const Row = ({ label, desc, children }) => (
  <div className="row">
    <div>
      <div className="row-label">{label}</div>
      {desc && <div className="row-desc">{desc}</div>}
    </div>
    <div className="row-control">{children}</div>
  </div>
);

const Bars = ({ values, animateKey }) => {
  const max = Math.max(...values);
  return (
    <div className="bars">
      {values.map((v, i) => (
        <div key={`${animateKey}-${i}`} className={`b ${i === values.length - 1 ? 'last' : ''}`} style={{ height: `${(v/max)*100}%` }}></div>
      ))}
    </div>
  );
};

Object.assign(window, { Toggle, Slider, Pill, Section, Row, Bars, ICONS });

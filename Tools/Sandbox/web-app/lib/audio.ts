let ctx: AudioContext | null = null;

function beep(freq: number, dur: number, type: OscillatorType = "sine", gain = 0.06) {
  try {
    ctx = ctx ?? new AudioContext();
    const o = ctx.createOscillator();
    const g = ctx.createGain();
    o.type = type;
    o.frequency.value = freq;
    g.gain.value = gain;
    o.connect(g);
    g.connect(ctx.destination);
    o.start();
    g.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + dur);
    o.stop(ctx.currentTime + dur);
  } catch {
    /* autoplay / entorno sin audio */
  }
}

export const sfx = {
  entry: (f: number) => beep(330 + f * 120, 0.12, "triangle"),
  fama: () => {
    beep(880, 0.08, "square", 0.04);
    setTimeout(() => beep(1320, 0.12, "square", 0.04), 80);
  },
  alert: () => {
    beep(220, 0.3, "sawtooth", 0.08);
    setTimeout(() => beep(196, 0.4, "sawtooth", 0.08), 300);
  },
  solved: () => {
    [523, 659, 784, 1046].forEach((f, i) => setTimeout(() => beep(f, 0.25, "triangle", 0.07), i * 110));
  },
  bad: () => beep(160, 0.25, "sawtooth", 0.06),
  tick: () => beep(1200, 0.04, "square", 0.03),
};

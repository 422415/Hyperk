'use strict';
(() => {
  const endpoint = '/api/analog_calibration';
  const fields = ['red', 'green', 'blue', 'whiteLevel', 'warmFraction', 'whiteMix'];
  const names = ['Playback color', 'RGB reference', 'White LED pair', 'Red channel', 'Green channel', 'Blue channel', 'Warm white channel', 'Cool white channel', 'Slow shadow ramp'];
  const el = id => document.getElementById(id);
  let saved, enabled = false, active = false, dirty = false, busy = false;
  let pattern = 3, color = 'ffffff', inFlight = null, debounce = 0;

  function message(text, kind = '') { el('status').textContent = text; el('status').className = kind; }
  function profile() { return Object.fromEntries(fields.map(k => [k, Number(el(k).value) / 100])); }
  function showValues() {
    for (const k of [...fields, 'test-level']) el(`${k}-value`).textContent = `${Number(el(k).value).toFixed(1).replace(/\.0$/, '')}%`;
    const changed = saved && fields.some(k => Math.abs(profile()[k] - saved[k]) > 0.00001);
    el('draft-status').textContent = changed ? 'Unsaved changes' : (enabled ? 'Saved profile loaded' : 'New draft · not yet activated');
  }
  function loadProfile(p) { fields.forEach(k => { el(k).value = p[k] * 100; }); showValues(); }
  function updateStatus() {
    el('profile-status').textContent = enabled ? 'The new calibration is active. Changes below stay in the draft until you save.' : 'Your existing calibration is still active. Saving this guide activates the new color engine for this lamp.';
    el('preview').textContent = active ? 'Test running' : 'Start selected test';
    el('preview').disabled = active || busy;
    el('stop').disabled = !active || busy;
  }
  async function post(params) {
    const response = await fetch(endpoint, {method:'POST', body:new URLSearchParams(params), keepalive:true, signal:AbortSignal.timeout(5000)});
    if (!response.ok) throw new Error((await response.text()) || `Device returned ${response.status}`);
    return response.json();
  }
  function previewParams() {
    const rgb = [0, 2, 4].map(i => parseInt(color.slice(i, i + 2), 16) * 257);
    return {action:'preview', ...profile(), pattern, level:Number(el('test-level').value) / 100, r:rgb[0], g:rgb[1], b:rgb[2]};
  }
  async function flushPreview() {
    if (inFlight || !active || busy) return;
    inFlight = (async () => {
      while (active && dirty) {
        dirty = false;
        await post(previewParams());
        if (active) message(`Testing: ${names[pattern]}. Adjustments are temporary.`, 'live');
      }
    })();
    try { await inFlight; }
    catch (error) { active = false; message(`Preview failed: ${error.message}. The lamp will leave test mode within 15 seconds.`, 'error'); }
    finally { inFlight = null; updateStatus(); }
  }
  function requestPreview() {
    if (!active) return;
    dirty = true;
    clearTimeout(debounce);
    debounce = setTimeout(flushPreview, 100);
  }
  function selectPattern(value, start = false) {
    pattern = value;
    el('pattern-name').textContent = names[pattern];
    document.querySelectorAll('[data-pattern]').forEach(button => button.setAttribute('aria-pressed', String(Number(button.dataset.pattern) === pattern)));
    if (start) active = true;
    updateStatus();
    requestPreview();
  }
  function step(index) {
    document.querySelectorAll('[data-panel]').forEach(panel => { panel.hidden = Number(panel.dataset.panel) !== index; });
    document.querySelectorAll('[data-step]').forEach(button => {
      if (Number(button.dataset.step) === index) button.setAttribute('aria-current', 'step');
      else button.removeAttribute('aria-current');
    });
    selectPattern([3, 1, 2, 0][index]);
  }
  async function finish(action) {
    if (busy) return;
    busy = true; active = false; dirty = false; clearTimeout(debounce);
    el('workspace').disabled = true;
    const values = profile();
    try {
      // A failed preview must not prevent a subsequent explicit stop or save.
      if (inFlight) await inFlight.catch(() => {});
      await post(action === 'save' ? {action, ...values} : {action:'stop'});
      if (action === 'save') { saved = values; enabled = true; }
      if (action === 'cancel') loadProfile(saved);
      message(action === 'save' ? 'Calibration saved. Normal operation restored.' : action === 'cancel' ? 'Draft discarded. Saved settings restored.' : 'Test stopped. Normal operation restored.');
      showValues();
    } catch (error) { message(`${action === 'save' ? 'Save' : 'Stop'} failed: ${error.message}. Your draft is still on this page.`, 'error'); }
    finally { busy = false; el('workspace').disabled = false; updateStatus(); }
  }
  document.querySelectorAll('[data-step]').forEach(b => b.addEventListener('click', () => step(Number(b.dataset.step))));
  document.querySelectorAll('[data-next]').forEach(b => b.addEventListener('click', () => step(Number(b.dataset.next))));
  document.querySelectorAll('[data-pattern]').forEach(b => b.addEventListener('click', () => selectPattern(Number(b.dataset.pattern), true)));
  document.querySelectorAll('[data-color]').forEach(b => b.addEventListener('click', () => { color = b.dataset.color; selectPattern(0, true); }));
  for (const k of [...fields, 'test-level']) el(k).addEventListener('input', () => { showValues(); requestPreview(); });
  el('preview').addEventListener('click', () => selectPattern(pattern, true));
  el('stop').addEventListener('click', () => finish('stop'));
  el('cancel').addEventListener('click', () => finish('cancel'));
  el('save').addEventListener('click', () => finish('save'));
  el('ramp').addEventListener('click', () => { el('test-level').value = 3; showValues(); selectPattern(8, true); });
  setInterval(() => { if (active && !busy) { dirty = true; flushPreview(); } }, 4000);
  window.addEventListener('pagehide', () => {
    if (active) { active = false; navigator.sendBeacon(endpoint, new URLSearchParams({action:'stop'})); }
  });
  (async () => {
    try {
      const response = await fetch(endpoint);
      if (!response.ok) throw new Error(`Could not load calibration (${response.status})`);
      const data = await response.json();
      if (!data.supported) throw new Error('Select Analog RGBCCT in Device settings, save, then reopen this guide.');
      el('device-name').textContent = `${data.deviceName} / AN-PENTA-PLUS`;
      saved = data.profile; enabled = data.enabled;
      loadProfile(saved); updateStatus();
      el('workspace').disabled = false;
      message('Ready. Start a test when you are beside this lamp.');
    } catch (error) { message(error.message, 'error'); }
  })();
})();

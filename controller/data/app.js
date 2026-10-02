// Tealight fleet controller — vanilla JS, no build step, no dependencies.
// Talks only to this controller's own /api/* (same origin); the controller
// proxies everything to each tealight by fingerprint.

const POLL_MS = 2000;
const cardNodes = new Map();   // fingerprint -> card element
const modeCache = new Map();   // fingerprint -> [{index,name}]
const sliderTimers = new Map(); // element -> debounce timer id

async function getJson(url) {
  const res = await fetch(url);
  if (!res.ok) throw new Error(`${url} -> ${res.status}`);
  return res.json();
}
async function postJson(url, body) {
  const res = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  let data = {};
  try { data = await res.json(); } catch (_) {}
  return { ok: res.ok, status: res.status, data };
}

function hueToCss(hue255, sat255) {
  const h = Math.round((hue255 / 255) * 360);
  const s = Math.round((sat255 / 255) * 100);
  return `hsl(${h}, ${s}%, 50%)`;
}

// ---------------- Device grid ----------------

function buildCard(fp) {
  const tpl = document.getElementById('cardTemplate').content.cloneNode(true);
  const card = tpl.querySelector('.card');
  card.dataset.fp = fp;

  card.querySelector('.gear-btn').addEventListener('click', () => {
    card.querySelector('.details-panel').hidden = !card.querySelector('.details-panel').hidden;
  });

  card.querySelector('.identify-btn').addEventListener('click', () => {
    postJson('/api/device/identify', { fingerprint: fp });
  });

  card.querySelector('.rename-save-btn').addEventListener('click', async () => {
    const name = card.querySelector('.rename-input').value.trim();
    if (!name) return;
    await postJson('/api/device/rename', { fingerprint: fp, name });
    refreshDevices();
  });

  card.querySelector('.forget-btn').addEventListener('click', async () => {
    if (!confirm('Forget this tealight? You can re-add it later by scanning again.')) return;
    await postJson('/api/device/forget', { fingerprint: fp });
    const el = cardNodes.get(fp);
    if (el) el.remove();
    cardNodes.delete(fp);
    refreshDevices();
  });

  card.querySelector('.mode-select').addEventListener('change', (e) => {
    postJson('/api/device/state', { fingerprint: fp, mode: Number(e.target.value) });
  });

  for (const cls of ['hue-slider', 'sat-slider', 'speed-slider', 'intensity-slider', 'brightness-slider']) {
    const field = { 'hue-slider': 'hue', 'sat-slider': 'sat', 'speed-slider': 'speed', 'intensity-slider': 'intensity', 'brightness-slider': 'brightness' }[cls];
    const el = card.querySelector('.' + cls);
    el.addEventListener('input', () => {
      clearTimeout(sliderTimers.get(el));
      sliderTimers.set(el, setTimeout(() => {
        postJson('/api/device/state', { fingerprint: fp, [field]: Number(el.value) });
        if (field === 'hue' || field === 'sat') updateSwatch(card);
      }, 150));
    });
  }

  document.getElementById('grid').appendChild(card);
  cardNodes.set(fp, card);
  return card;
}

function updateSwatch(card) {
  const hue = Number(card.querySelector('.hue-slider').value);
  const sat = Number(card.querySelector('.sat-slider').value);
  card.querySelector('.swatch').style.background = hueToCss(hue, sat);
}

async function ensureModes(fp, selectEl) {
  if (modeCache.has(fp)) return modeCache.get(fp);
  try {
    const modes = await getJson(`/api/device/modes?fp=${encodeURIComponent(fp)}`);
    modeCache.set(fp, modes);
    selectEl.innerHTML = '';
    for (const m of modes) {
      const opt = document.createElement('option');
      opt.value = m.index;
      opt.textContent = m.name;
      selectEl.appendChild(opt);
    }
    return modes;
  } catch (_) {
    return [];
  }
}

function setIfNotEditing(el, value) {
  if (document.activeElement === el) return; // don't fight the user mid-drag/typing
  el.value = value;
}

function renderDevice(dev) {
  let card = cardNodes.get(dev.fingerprint);
  if (!card) card = buildCard(dev.fingerprint);

  card.querySelector('.device-name').textContent = dev.name || dev.fingerprint;
  card.querySelector('.status-dot').classList.toggle('online', !!dev.reachable);
  card.querySelector('.meta-ip').textContent = dev.ip || '—';
  card.querySelector('.meta-fp').textContent = dev.fingerprint;
  card.querySelector('.meta-lastseen').textContent = dev.reachable ? 'online now' : 'unreachable';

  if (document.activeElement !== card.querySelector('.rename-input')) {
    card.querySelector('.rename-input').value = dev.name || '';
  }

  const selectEl = card.querySelector('.mode-select');
  ensureModes(dev.fingerprint, selectEl).then(() => {
    if (dev.state && document.activeElement !== selectEl) {
      selectEl.value = dev.state.mode;
    }
  });

  if (dev.state) {
    setIfNotEditing(card.querySelector('.hue-slider'), dev.state.hue);
    setIfNotEditing(card.querySelector('.sat-slider'), dev.state.sat);
    setIfNotEditing(card.querySelector('.speed-slider'), dev.state.speed);
    setIfNotEditing(card.querySelector('.intensity-slider'), dev.state.intensity);
    setIfNotEditing(card.querySelector('.brightness-slider'), dev.state.brightness);
    updateSwatch(card);
  }
}

async function refreshDevices() {
  let devices = [];
  try {
    devices = await getJson('/api/devices');
  } catch (_) {
    return; // controller hiccup; try again next tick
  }

  document.getElementById('emptyState').hidden = devices.length > 0;

  const seen = new Set();
  for (const dev of devices) {
    renderDevice(dev);
    seen.add(dev.fingerprint);
  }
  for (const [fp, el] of cardNodes) {
    if (!seen.has(fp)) { el.remove(); cardNodes.delete(fp); }
  }
}

// ---------------- Add-device wizard ----------------

const overlay = document.getElementById('wizardOverlay');
const stepScan = document.getElementById('wizardScan');
const stepForm = document.getElementById('wizardForm');
const stepProgress = document.getElementById('wizardProgress');
let selectedDevice = null;
let scanPollHandle = null;
let progressPollHandle = null;

function showStep(step) {
  for (const s of [stepScan, stepForm, stepProgress]) s.hidden = (s !== step);
}

function openWizard() {
  selectedDevice = null;
  document.getElementById('foundList').innerHTML = '';
  document.getElementById('scanStatus').textContent = '';
  showStep(stepScan);
  overlay.hidden = false;
}

function closeWizard() {
  overlay.hidden = true;
  clearInterval(scanPollHandle);
  clearInterval(progressPollHandle);
}

function renderFoundList(found) {
  const list = document.getElementById('foundList');
  list.innerHTML = '';
  for (const f of found) {
    const li = document.createElement('li');
    li.innerHTML = `<span>${f.name || f.address}</span><span class="rssi">${f.rssi} dBm</span>`;
    li.addEventListener('click', () => {
      selectedDevice = f;
      document.getElementById('formDeviceName').textContent = f.name || f.address;
      document.getElementById('formName').value = f.name || '';
      document.getElementById('formSsid').value = '';
      document.getElementById('formPass').value = '';
      showStep(stepForm);
    });
    list.appendChild(li);
  }
}

async function startScan() {
  document.getElementById('foundList').innerHTML = '';
  document.getElementById('scanStatus').textContent = 'Scanning for 5 seconds…';
  await postJson('/api/discovery/start', {});

  let sawScanning = false;
  clearInterval(scanPollHandle);
  scanPollHandle = setInterval(async () => {
    const found = await getJson('/api/discovery/found').catch(() => []);
    renderFoundList(found);
    const status = await getJson('/api/discovery/status').catch(() => ({ state: 'idle' }));
    if (status.state === 'scanning') sawScanning = true;
    if (sawScanning && status.state !== 'scanning') {
      clearInterval(scanPollHandle);
      document.getElementById('scanStatus').textContent =
        found.length ? `Found ${found.length} device(s) — pick one below.` : 'No unpaired tealights found. Make sure one is powered on and in pairing mode, then try again.';
    }
  }, 700);
}

async function submitProvision() {
  const ssid = document.getElementById('formSsid').value.trim();
  const pass = document.getElementById('formPass').value;
  const name = document.getElementById('formName').value.trim();
  if (!ssid || !selectedDevice) return;

  showStep(stepProgress);
  document.getElementById('progressSpinner').hidden = false;
  document.getElementById('progressDone').hidden = true;
  document.getElementById('progressRetry').hidden = true;
  document.getElementById('progressStatus').textContent = 'Connecting…';

  const start = await postJson('/api/discovery/provision', {
    address: selectedDevice.address,
    addrType: selectedDevice.addrType,
    ssid, pass, name,
  });
  if (!start.ok) {
    document.getElementById('progressStatus').textContent = start.data.error || 'Could not start provisioning.';
    document.getElementById('progressSpinner').hidden = true;
    document.getElementById('progressRetry').hidden = false;
    return;
  }

  clearInterval(progressPollHandle);
  progressPollHandle = setInterval(async () => {
    const status = await getJson('/api/discovery/status').catch(() => null);
    if (!status) return;
    if (status.state === 'success') {
      clearInterval(progressPollHandle);
      document.getElementById('progressStatus').textContent = `Connected! IP ${status.ip}`;
      document.getElementById('progressSpinner').hidden = true;
      document.getElementById('progressDone').hidden = false;
      refreshDevices();
    } else if (status.state === 'failed') {
      clearInterval(progressPollHandle);
      document.getElementById('progressStatus').textContent = status.msg || 'Provisioning failed.';
      document.getElementById('progressSpinner').hidden = true;
      document.getElementById('progressRetry').hidden = false;
    }
  }, 1000);
}

document.getElementById('addDeviceBtn').addEventListener('click', openWizard);
document.getElementById('wizardClose').addEventListener('click', closeWizard);
document.getElementById('scanBtn').addEventListener('click', startScan);
document.getElementById('formBack').addEventListener('click', () => showStep(stepScan));
document.getElementById('formSubmit').addEventListener('click', submitProvision);
document.getElementById('progressDone').addEventListener('click', closeWizard);
document.getElementById('progressRetry').addEventListener('click', () => showStep(stepScan));

refreshDevices();
setInterval(refreshDevices, POLL_MS);

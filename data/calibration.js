function setCalibration(gain, r, g, b) {
    const fields = { 'calGain': gain, 'calRed': r, 'calGreen': g, 'calBlue': b };

    for (const [name, value] of Object.entries(fields)) {
        const el = document.querySelector(`input[name="${name}"]`);
        if (el) {
            el.value = value;                  
        }
    }
}

function isAnalogOutput() {
    return document.getElementById('ledType')?.value === "3";
}

function toggleCalibration() {
    const ledTypeSelect = document.getElementById('ledType');
    const calSection = document.getElementById('whiteCalibration');
    const analogSection = document.getElementById('analogCctCalibration');
    const rgbwOrderControl = document.getElementById('rgbwOrderControl');
    const outputCorrection = document.getElementById('outputCorrection');
    const isRgbw = ledTypeSelect.value === "1";
    const isAnalog = ledTypeSelect.value === "3";
    
    calSection.style.display = isRgbw ? "block" : "none";
    if (analogSection) {
        analogSection.style.display = isAnalog ? "block" : "none";
    }
    if (rgbwOrderControl) {
        rgbwOrderControl.style.display = isRgbw ? "block" : "none";
    }
    if (outputCorrection) {
        outputCorrection.style.display = (isRgbw || isAnalog) ? "block" : "none";
    }

    document.querySelectorAll('[data-analog-only]').forEach((el) => {
        el.style.display = isAnalog ? '' : 'none';
    });

    const whiteTitle = document.getElementById('white-channel-title');
    const whiteLabel = document.getElementById('white-strength-label');
    if (whiteTitle) {
        whiteTitle.textContent = isAnalog ? 'CCT white' : 'White LED';
    }
    const whiteTab = document.querySelector('[data-channel-tab="white"]');
    if (whiteTab) {
        whiteTab.textContent = isAnalog ? 'CCT White' : 'White';
    }
    if (whiteLabel && whiteLabel.firstChild) {
        whiteLabel.firstChild.textContent = isAnalog ? 'CCT white strength ' : 'White strength ';
    }

    const mixerLabel = document.querySelector('.white-mixer-control');
    if (mixerLabel && mixerLabel.lastChild) {
        mixerLabel.lastChild.textContent = isAnalog ? ' Extract near-neutral RGB into CCT white' : ' Convert shared RGB brightness into the white LED';
    }

    const activeTab = document.querySelector('[data-channel-tab].active-channel-tab');
    if (activeTab && activeTab.style.display === 'none') {
        selectChannelTab('red', false);
    }
}

let activePreviewButton = null;
let previewTimer = 0;

const outputTuningDefaults = {
    outputRed: 255,
    outputGreen: 255,
    outputBlue: 255,
    outputWhite: 255,
    outputRedToGreen: 0,
    outputRedToBlue: 0,
    outputGreenToRed: 0,
    outputGreenToBlue: 0,
    outputBlueToRed: 0,
    outputBlueToGreen: 0,
    outputWarmWhite: 0,
    outputColdWhite: 255,
    outputWhite: 255
};

function clampChannelValue(value) {
    const parsed = parseInt(value, 10);
    if (isNaN(parsed)) return 0;
    return Math.max(0, Math.min(255, parsed));
}

function scaledChannel(value, gain) {
    return clampChannelValue((Number(value || 0) * Number(gain || 0)) / 255);
}

function formatChannelValues(values) {
    const [r, g, b, w, ww, cw] = normalizeChannelValues(values);
    if (ww || cw || isAnalogOutput()) {
        return `R ${r} / G ${g} / B ${b} / W ${w} / WW ${ww} / CW ${cw}`;
    }
    return `R ${r} / G ${g} / B ${b} / W ${w}`;
}

function normalizeChannelValues(values) {
    const next = [0, 0, 0, 0, 0, 0];
    values.forEach((value, index) => {
        if (index < next.length) {
            next[index] = clampChannelValue(value);
        }
    });
    return next;
}

function displayColorFromChannels(values) {
    const [r, g, b, w, ww, cw] = normalizeChannelValues(values);
    return [
        clampChannelValue(r + w + ww + (cw * 0.82)),
        clampChannelValue(g + w + (ww * 0.76) + (cw * 0.90)),
        clampChannelValue(b + w + (ww * 0.42) + cw)
    ];
}

function setSwatchColor(swatch, values) {
    const [displayR, displayG, displayB] = displayColorFromChannels(values);
    swatch.style.background = `rgb(${displayR}, ${displayG}, ${displayB})`;
}

function updateOutputPreview(title, values, requestedValues = null) {
    const referenceSwatch = document.getElementById('output-reference-swatch');
    const outputSwatch = document.getElementById('output-preview-swatch');
    const text = document.getElementById('output-preview-text');
    if (!referenceSwatch || !outputSwatch || !text) return;

    const [r, g, b, w, ww, cw] = normalizeChannelValues(values);
    const referenceValues = requestedValues || values;

    setSwatchColor(referenceSwatch, referenceValues);
    setSwatchColor(outputSwatch, [r, g, b, w, ww, cw]);
    text.innerHTML = '';

    const titleLine = document.createElement('div');
    titleLine.className = 'output-preview-label';
    titleLine.textContent = title;
    text.appendChild(titleLine);

    const requestLine = document.createElement('div');
    requestLine.textContent = `Screen reference: ${formatChannelValues(referenceValues)}`;
    text.appendChild(requestLine);

    const outputLine = document.createElement('div');
    outputLine.className = 'output-preview-output';
    outputLine.textContent = `After tuning: ${formatChannelValues([r, g, b, w, ww, cw])}`;
    text.appendChild(outputLine);
}

function getNumberParam(params, name, fallback = 0) {
    const parsed = Number(params.get(name));
    return Number.isFinite(parsed) ? parsed : fallback;
}

function splitWhiteToCctPreview(w, params) {
    const warmKelvin = Math.max(1000, Math.min(10000, getNumberParam(params, 'cctWarmKelvin', 3000)));
    const coldKelvin = Math.max(1000, Math.min(10000, getNumberParam(params, 'cctColdKelvin', 6500)));
    const targetKelvin = Math.max(warmKelvin, Math.min(coldKelvin, getNumberParam(params, 'cctTargetKelvin', 6500)));

    if (warmKelvin >= coldKelvin) {
        return [scaledChannel(w, params.get('outputWarmWhite')), 0];
    }

    const warmWeight = (coldKelvin - targetKelvin) / (coldKelvin - warmKelvin);
    const coldWeight = 1 - warmWeight;
    return [
        scaledChannel(w * warmWeight, params.get('outputWarmWhite')),
        scaledChannel(w * coldWeight, params.get('outputColdWhite'))
    ];
}

function updateCorrectedOutputPreview(params) {
    const requested = [
        params.get('referenceR') ?? params.get('r'),
        params.get('referenceG') ?? params.get('g'),
        params.get('referenceB') ?? params.get('b'),
        params.get('referenceW') ?? params.get('w'),
        params.get('referenceWw') ?? params.get('ww'),
        params.get('referenceCw') ?? params.get('cw')
    ];
    let outR = scaledChannel(requested[0], params.get('outputRed')) +
               scaledChannel(requested[1], params.get('outputGreenToRed')) +
               scaledChannel(requested[2], params.get('outputBlueToRed'));
    let outG = scaledChannel(requested[1], params.get('outputGreen')) +
               scaledChannel(requested[0], params.get('outputRedToGreen')) +
               scaledChannel(requested[2], params.get('outputBlueToGreen'));
    let outB = scaledChannel(requested[2], params.get('outputBlue')) +
               scaledChannel(requested[0], params.get('outputRedToBlue')) +
               scaledChannel(requested[1], params.get('outputGreenToBlue'));
    let outW = scaledChannel(requested[3], params.get('outputWhite'));
    let outWw = scaledChannel(requested[4], params.get('outputWarmWhite'));
    let outCw = scaledChannel(requested[5], params.get('outputColdWhite'));

    if (isAnalogOutput()) {
        if (params.get('rgbToWhiteConversion') !== '0' && outW === 0 && outWw === 0 && outCw === 0) {
            const sharedWhite = Math.min(outR, outG, outB);
            const maxRgb = Math.max(outR, outG, outB);
            const threshold = getNumberParam(params, 'cctNeutralThreshold', 24);
            if ((maxRgb - sharedWhite) <= threshold) {
                outW = scaledChannel(sharedWhite, params.get('outputWhite'));
                outR -= sharedWhite;
                outG -= sharedWhite;
                outB -= sharedWhite;
            }
        }

        const [splitWw, splitCw] = splitWhiteToCctPreview(outW, params);
        outWw = clampChannelValue(outWw + splitWw);
        outCw = clampChannelValue(outCw + splitCw);
        outW = 0;
    }
    else if (params.get('rgbToWhiteConversion') !== '0' && outW === 0) {
        const sharedWhite = Math.min(outR, outG, outB);
        outW = sharedWhite;
        outR -= sharedWhite;
        outG -= sharedWhite;
        outB -= sharedWhite;
    }

    updateOutputPreview('Corrected preview', [outR, outG, outB, outW, outWw, outCw], requested);
}

function setTuningValue(name, value) {
    const nextValue = clampChannelValue(value);
    const number = document.querySelector(`[data-tuning-number="${name}"]`);
    const range = document.querySelector(`[data-tuning-range="${name}"]`);
    const named = document.querySelector(`[name="${name}"]`);

    if (number) number.value = nextValue;
    if (range) range.value = nextValue;
    if (named && named !== number) named.value = nextValue;
}

function getTuningValue(name) {
    const number = document.querySelector(`[data-tuning-number="${name}"]`);
    const named = document.querySelector(`[name="${name}"]`);
    return clampChannelValue(number?.value ?? named?.value ?? 0);
}

function collectTuningParams() {
    const params = new URLSearchParams({
        outputRed: getTuningValue('outputRed'),
        outputGreen: getTuningValue('outputGreen'),
        outputBlue: getTuningValue('outputBlue'),
        outputWhite: getTuningValue('outputWhite'),
        outputWarmWhite: getTuningValue('outputWarmWhite'),
        outputColdWhite: getTuningValue('outputColdWhite'),
        outputRedToGreen: getTuningValue('outputRedToGreen'),
        outputRedToBlue: getTuningValue('outputRedToBlue'),
        outputGreenToRed: getTuningValue('outputGreenToRed'),
        outputGreenToBlue: getTuningValue('outputGreenToBlue'),
        outputBlueToRed: getTuningValue('outputBlueToRed'),
        outputBlueToGreen: getTuningValue('outputBlueToGreen'),
        cctNeutralThreshold: document.querySelector('[name="cctNeutralThreshold"]')?.value || '24',
        cctWarmKelvin: document.querySelector('[name="cctWarmKelvin"]')?.value || '3000',
        cctColdKelvin: document.querySelector('[name="cctColdKelvin"]')?.value || '6500',
        cctTargetKelvin: document.querySelector('[name="cctTargetKelvin"]')?.value || '6500',
        rgbToWhiteConversion: document.querySelector('[name="rgbToWhiteConversion"]')?.checked ? '1' : '0'
    });

    return params;
}

async function sendCorrectedPreview(button) {
    if (!button) return;

    const body = collectTuningParams();
    body.set('r', button.dataset.r || '0');
    body.set('g', button.dataset.g || '0');
    body.set('b', button.dataset.b || '0');
    body.set('w', button.dataset.w || '0');
    body.set('ww', button.dataset.ww || '0');
    body.set('cw', button.dataset.cw || '0');
    body.set('referenceR', button.dataset.referenceR || button.dataset.r || '0');
    body.set('referenceG', button.dataset.referenceG || button.dataset.g || '0');
    body.set('referenceB', button.dataset.referenceB || button.dataset.b || '0');
    body.set('referenceW', button.dataset.referenceW || button.dataset.w || '0');
    body.set('referenceWw', button.dataset.referenceWw || button.dataset.ww || '0');
    body.set('referenceCw', button.dataset.referenceCw || button.dataset.cw || '0');

    updateCorrectedOutputPreview(body);
    button.setAttribute('aria-busy', 'true');
    try {
        await fetch('/api/test_corrected_color', { method: 'POST', body });
    }
    catch (e) {
        console.error('Corrected preview failed:', e);
    }
    button.setAttribute('aria-busy', 'false');
}

function scheduleCorrectedPreview() {
    if (!activePreviewButton) return;
    clearTimeout(previewTimer);
    previewTimer = setTimeout(() => sendCorrectedPreview(activePreviewButton), 90);
}

function selectChannelTab(channel, shouldPreview) {
    document.querySelectorAll('[data-channel-tab]').forEach((button) => {
        const isActive = button.dataset.channelTab === channel;
        button.classList.toggle('active-channel-tab', isActive);
        button.setAttribute('aria-selected', isActive ? 'true' : 'false');
    });

    document.querySelectorAll('[data-channel-panel]').forEach((panel) => {
        panel.hidden = panel.dataset.channelPanel !== channel;
    });

    if (shouldPreview) {
        document.querySelector(`[data-preview-channel="${channel}"]`)?.click();
    }
}

function resetOutputTuning() {
    for (const [name, value] of Object.entries(outputTuningDefaults)) {
        setTuningValue(name, value);
    }

    const whiteMixer = document.querySelector('[name="rgbToWhiteConversion"]');
    if (whiteMixer) {
        whiteMixer.checked = false;
    }

    scheduleCorrectedPreview();
}

function saveSettingsForm() {
    const form = document.querySelector('form[action="/save_config"]');
    if (!form) return;

    if (typeof form.requestSubmit === 'function') {
        form.requestSubmit();
    }
    else {
        form.dispatchEvent(new Event('submit', { bubbles: true, cancelable: true }));
    }
}

function setupTuningControls() {
    const initializedNames = new Set();

    document.querySelectorAll('[data-tuning-range], [data-tuning-number]').forEach((control) => {
        const name = control.dataset.tuningRange || control.dataset.tuningNumber;

        if (!initializedNames.has(name)) {
            initializedNames.add(name);
            const currentValue = document.querySelector(`[data-tuning-number="${name}"]`)?.value ?? control.value;
            setTuningValue(name, currentValue);
        }

        control.addEventListener('input', () => {
            setTuningValue(name, control.value);
            scheduleCorrectedPreview();
        });
    });

    document.querySelector('[name="rgbToWhiteConversion"]')?.addEventListener('change', scheduleCorrectedPreview);
    document.querySelectorAll('[name="cctNeutralThreshold"], [name="cctWarmKelvin"], [name="cctColdKelvin"], [name="cctTargetKelvin"]').forEach((control) => {
        control.addEventListener('input', scheduleCorrectedPreview);
    });
    document.getElementById('reset-output-tuning')?.addEventListener('click', resetOutputTuning);
    document.getElementById('save-output-tuning')?.addEventListener('click', saveSettingsForm);

    document.querySelectorAll('[data-corrected-test]').forEach((button) => {
        button.addEventListener('click', async () => {
            document.querySelectorAll('[data-corrected-test]').forEach((b) => b.classList.remove('active-preview'));
            button.classList.add('active-preview');
            activePreviewButton = button;
            await sendCorrectedPreview(button);
        });
    });

    document.querySelectorAll('[data-channel-tab]').forEach((button) => {
        button.addEventListener('click', () => selectChannelTab(button.dataset.channelTab, true));
    });

    const initialTab = document.querySelector('[data-channel-tab].active-channel-tab')?.dataset.channelTab || 'red';
    selectChannelTab(initialTab, false);
}

function setupCalibration(){
    const ledTypeSelect = document.getElementById('ledType');
    ledTypeSelect.addEventListener('change', toggleCalibration);
    setupTuningControls();

    document.getElementById('cal-cold')?.addEventListener('click', () => {
        setCalibration(255, 160, 160, 160);
    });

    document.getElementById('cal-neutral')?.addEventListener('click', () => {
        setCalibration(255, 176, 176, 112);
    });    

    document.getElementById('cct-screen-white')?.addEventListener('click', () => {
        document.querySelector('[name="cctWarmKelvin"]').value = 3000;
        document.querySelector('[name="cctColdKelvin"]').value = 6500;
        document.querySelector('[name="cctTargetKelvin"]').value = 6500;
        setTuningValue('outputWarmWhite', getTuningValue('outputWarmWhite'));
        setTuningValue('outputColdWhite', 255);
        scheduleCorrectedPreview();
    });

    document.getElementById('cct-cold-only')?.addEventListener('click', () => {
        document.querySelector('[name="cctTargetKelvin"]').value = document.querySelector('[name="cctColdKelvin"]').value || 6500;
        setTuningValue('outputWarmWhite', 0);
        setTuningValue('outputColdWhite', 255);
        scheduleCorrectedPreview();
    });
    
    document.querySelectorAll('[data-raw-test]').forEach((button) => {
        button.addEventListener('click', async () => {
            const body = new URLSearchParams({
                r: button.dataset.r || '0',
                g: button.dataset.g || '0',
                b: button.dataset.b || '0',
                w: button.dataset.w || '0',
                ww: button.dataset.ww || '0',
                cw: button.dataset.cw || '0'
            });

            updateOutputPreview('Raw preview', [
                body.get('r'),
                body.get('g'),
                body.get('b'),
                body.get('w'),
                body.get('ww'),
                body.get('cw')
            ]);
            button.setAttribute('aria-busy', 'true');
            try {
                await fetch('/api/test_color', { method: 'POST', body });
            }
            catch (e) {
                console.error('Raw channel test failed:', e);
            }
            button.setAttribute('aria-busy', 'false');
        });
    });

    toggleCalibration();
};

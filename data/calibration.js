function setCalibration(gain, r, g, b) {
    const fields = { 'calGain': gain, 'calRed': r, 'calGreen': g, 'calBlue': b };

    for (const [name, value] of Object.entries(fields)) {
        const el = document.querySelector(`input[name="${name}"]`);
        if (el) {
            el.value = value;                  
        }
    }
}

function toggleCalibration() {
    const ledTypeSelect = document.getElementById('ledType');
    const calSection = document.getElementById('whiteCalibration');
    const rgbwOrderControl = document.getElementById('rgbwOrderControl');
    const outputCorrection = document.getElementById('outputCorrection');
    const isRgbw = ledTypeSelect.value === "1";
    
    calSection.style.display = isRgbw ? "block" : "none";
    if (rgbwOrderControl) {
        rgbwOrderControl.style.display = isRgbw ? "block" : "none";
    }
    if (outputCorrection) {
        outputCorrection.style.display = isRgbw ? "block" : "none";
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
    outputBlueToGreen: 0
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
    const [r, g, b, w] = values.map(clampChannelValue);
    return `R ${r} / G ${g} / B ${b} / W ${w}`;
}

function displayColorFromChannels(values) {
    const [r, g, b, w] = values.map(clampChannelValue);
    return [
        clampChannelValue(r + w),
        clampChannelValue(g + w),
        clampChannelValue(b + w)
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

    const [r, g, b, w] = values.map(clampChannelValue);
    const referenceValues = requestedValues || values;

    setSwatchColor(referenceSwatch, referenceValues);
    setSwatchColor(outputSwatch, [r, g, b, w]);
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
    outputLine.textContent = `After tuning: ${formatChannelValues([r, g, b, w])}`;
    text.appendChild(outputLine);
}

function updateCorrectedOutputPreview(params) {
    const requested = [
        params.get('r'),
        params.get('g'),
        params.get('b'),
        params.get('w')
    ];
    let outR = scaledChannel(params.get('r'), params.get('outputRed')) +
               scaledChannel(params.get('g'), params.get('outputGreenToRed')) +
               scaledChannel(params.get('b'), params.get('outputBlueToRed'));
    let outG = scaledChannel(params.get('g'), params.get('outputGreen')) +
               scaledChannel(params.get('r'), params.get('outputRedToGreen')) +
               scaledChannel(params.get('b'), params.get('outputBlueToGreen'));
    let outB = scaledChannel(params.get('b'), params.get('outputBlue')) +
               scaledChannel(params.get('r'), params.get('outputRedToBlue')) +
               scaledChannel(params.get('g'), params.get('outputGreenToBlue'));
    let outW = scaledChannel(params.get('w'), params.get('outputWhite'));

    if (params.get('rgbToWhiteConversion') !== '0' && outW === 0) {
        const sharedWhite = Math.min(outR, outG, outB);
        outW = sharedWhite;
        outR -= sharedWhite;
        outG -= sharedWhite;
        outB -= sharedWhite;
    }

    updateOutputPreview('Corrected preview', [outR, outG, outB, outW], requested);
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
        outputRedToGreen: getTuningValue('outputRedToGreen'),
        outputRedToBlue: getTuningValue('outputRedToBlue'),
        outputGreenToRed: getTuningValue('outputGreenToRed'),
        outputGreenToBlue: getTuningValue('outputGreenToBlue'),
        outputBlueToRed: getTuningValue('outputBlueToRed'),
        outputBlueToGreen: getTuningValue('outputBlueToGreen'),
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
    
    document.querySelectorAll('[data-raw-test]').forEach((button) => {
        button.addEventListener('click', async () => {
            const body = new URLSearchParams({
                r: button.dataset.r || '0',
                g: button.dataset.g || '0',
                b: button.dataset.b || '0',
                w: button.dataset.w || '0'
            });

            updateOutputPreview('Raw preview', [
                body.get('r'),
                body.get('g'),
                body.get('b'),
                body.get('w')
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

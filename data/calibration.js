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

function clampChannelValue(value) {
    const parsed = parseInt(value, 10);
    if (isNaN(parsed)) return 0;
    return Math.max(0, Math.min(255, parsed));
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

function setupTuningControls() {
    document.querySelectorAll('[data-tuning-range], [data-tuning-number]').forEach((control) => {
        const name = control.dataset.tuningRange || control.dataset.tuningNumber;
        setTuningValue(name, control.value);

        control.addEventListener('input', () => {
            setTuningValue(name, control.value);
            scheduleCorrectedPreview();
        });
    });

    document.querySelector('[name="rgbToWhiteConversion"]')?.addEventListener('change', scheduleCorrectedPreview);

    document.querySelectorAll('[data-corrected-test]').forEach((button) => {
        button.addEventListener('click', async () => {
            document.querySelectorAll('[data-corrected-test]').forEach((b) => b.classList.remove('active-preview'));
            button.classList.add('active-preview');
            activePreviewButton = button;
            await sendCorrectedPreview(button);
        });
    });

    const presets = {
        anime: { outputRed: 255, outputGreen: 185, outputBlue: 120, outputWhite: 0, outputBlueToGreen: 35, outputBlueToRed: 0, rgbToWhite: false },
        neutral: { outputRed: 255, outputGreen: 215, outputBlue: 150, outputWhite: 0, outputBlueToGreen: 20, outputBlueToRed: 0, rgbToWhite: false },
        reset: { outputRed: 255, outputGreen: 255, outputBlue: 255, outputWhite: 255, outputBlueToGreen: 0, outputBlueToRed: 0, rgbToWhite: false }
    };

    document.querySelectorAll('[data-tuning-preset]').forEach((button) => {
        button.addEventListener('click', () => {
            const preset = presets[button.dataset.tuningPreset];
            if (!preset) return;

            setTuningValue('outputRed', preset.outputRed);
            setTuningValue('outputGreen', preset.outputGreen);
            setTuningValue('outputBlue', preset.outputBlue);
            setTuningValue('outputWhite', preset.outputWhite);
            setTuningValue('outputRedToGreen', 0);
            setTuningValue('outputRedToBlue', 0);
            setTuningValue('outputGreenToRed', 0);
            setTuningValue('outputGreenToBlue', 0);
            setTuningValue('outputBlueToRed', preset.outputBlueToRed);
            setTuningValue('outputBlueToGreen', preset.outputBlueToGreen);
            const mixer = document.querySelector('[name="rgbToWhiteConversion"]');
            if (mixer) mixer.checked = preset.rgbToWhite;

            if (!activePreviewButton) {
                activePreviewButton = document.querySelector('[data-corrected-test][data-r="160"][data-g="160"][data-b="160"]');
                activePreviewButton?.classList.add('active-preview');
            }
            scheduleCorrectedPreview();
        });
    });
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

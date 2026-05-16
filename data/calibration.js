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

function setupCalibration(){
    const ledTypeSelect = document.getElementById('ledType');
    ledTypeSelect.addEventListener('change', toggleCalibration);

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

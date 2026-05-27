// ota.js
let otaFirmwareUrl = "";
let otaSelectedFirmware = "";
let localV = "";
let remoteV = "";
let updateCase = null;
let isUpdating = false;
let resolveConfirm;
const releasesApiUrl = "https://api.github.com/repos/422415/Hyperk/releases";

function customConfirm(msg) {
    const modal = document.getElementById('confirm-modal');
    document.getElementById('confirm-msg').innerText = msg;
    modal.showModal();

    return new Promise((resolve) => {
        resolveConfirm = resolve;
    });
}

window.onbeforeunload = function() {
    if (isUpdating) return "Update in progress. Do not close this page!";
};

function closeConfirm(result) {
    const modal = document.getElementById('confirm-modal');
    modal.close();
    resolveConfirm(result);
}

function normalizeOtaFirmwareName(fileName) {
    return (fileName || "").replace(/\s+\(\d+\)(?=\.bin$)/i, "");
}

function firmwareNameMatchesBoard(fileName, boardName) {
    const lowerFileName = normalizeOtaFirmwareName(fileName).toLowerCase();
    const arch = (boardName || "").toLowerCase();
    return !arch || lowerFileName.indexOf(`${arch}.`) >= 0;
}

async function checkFirmwareUpdates() {
    const statusArea = document.getElementById('ota_status_area');
    const statusText = document.getElementById('ota_status_text');
    const installBtn = document.getElementById('install_update_btn');
    const progress = document.getElementById('ota_progress');
    const channel = document.getElementById('ota_channel').value;
    
    if (!cfgBoardArchitecture) {
        statusText.innerText = "❌ Error: Unknown build architecture. Cannot check for updates.";
        return;
    }

    statusArea.style.display = 'block';
    progress.style.display = 'none';
    statusText.innerText = "Fetching releases...";
    installBtn.style.display = 'none';

    let releases;
    try {
        let res = await fetch(releasesApiUrl);
        if (!res.ok) throw new Error(`GitHub API status: ${res.status}`);
        console.log("✅ Data from Worker Proxy");
        releases = await res.json();
    } 
    catch (err) {
        console.warn("⚠️ Worker failed, falling back to GitHub API...", err);
        try {
            let res = await fetch(releasesApiUrl);
            if (!res.ok) throw new Error(`GitHub API status: ${res.status}`);
            releases = await res.json();
        } catch (e) {
            statusText.innerText = "❌ Failed to fetch releases.";
            return;
        }
    }

    const allowPreRelease = (channel === 'testing');
    const validReleases = releases.filter(r => allowPreRelease || !r.prerelease);
    
    if (validReleases.length === 0) {
        statusText.innerText = "No suitable releases found on this channel.";
        return;
    }

    const latest = validReleases[0];
    
    let archSuffix = cfgBoardArchitecture.toLowerCase();

    const expectedSuffix = `_${archSuffix}.bin`;
    const asset = latest.assets.find(a => a.name.startsWith("OTA_") && a.name.endsWith(expectedSuffix));

    if (!asset) {
        statusText.innerHTML = `❌ No firmware for <strong>${cfgDeviceArchitecture}</strong> (${archSuffix}) in release ${latest.tag_name}.<br><small>Looking for: *${expectedSuffix}</small>`;
        return;
    } else {
        otaSelectedFirmware = asset.name;
        console.log(`ℹ️ Using firmware: ${otaSelectedFirmware}`);
    }          

    function compareVersions(v1, v2) {
        if (v1 === v2 || !v1 || !v2 || v1 === "0.0.0" || v2 === "0.0.0") return 0;

        const parse = v => {
            const [ver, suf] = v.split('-', 2);
            return { n: ver.split('.').map(Number), s: suf || "" };
        };

        const d1 = parse(v1), d2 = parse(v2);

        for (let i = 0; i < 3; i++) {
            const n1 = d1.n[i] || 0, n2 = d2.n[i] || 0;
            if (n1 !== n2) return n1 > n2 ? 1 : -1;
        }

        if (!d1.s || !d2.s) return d1.s.localeCompare(d2.s) * -1;

        return d1.s.localeCompare(d2.s, undefined, { numeric: true });
    }

    localV = typeof cfgDeviceVersion !== 'undefined' ? cfgDeviceVersion : "0.0.0";
    remoteV = latest.tag_name;
    updateCase = compareVersions(localV, remoteV);

    if (updateCase === 0) {
        statusText.innerHTML = `✔️ Up to date (<strong>${remoteV}</strong>)`;
        installBtn.style.display = 'none';
        statusArea.style.borderColor = "var(--pico-form-element-border-color)"; // Powrót do standardu
        return;
    }

    statusArea.style.display = 'block';
    statusArea.style.borderColor = "#eab308"; 

    installBtn.style.display = 'block';
    installBtn.style.backgroundColor = "#eab308";
    installBtn.style.borderColor = "#eab308";
    installBtn.style.color = "#111";

    if (updateCase > 0) {
        statusText.innerHTML = `⚠️ Available: <strong class="yperk">${remoteV}</strong><br><small class="yperk">Warning: This is a downgrade!</small>`;
        installBtn.innerText = "Downgrade Firmware";
    } else {
        statusText.innerHTML = `🆕 New update available:<br><strong style="color: var(--pico-primary-background);">${remoteV}</strong>`;
        installBtn.innerText = "Install Update";
    }

    const proxyBase = "https://hyperhdr-github-proxy.hyperhdr.workers.dev/?url=";
    otaFirmwareUrl = proxyBase + encodeURIComponent(asset.browser_download_url);
};   

async function startOtaUpdate() {
    if (!otaFirmwareUrl || !localV || !remoteV || updateCase === null) return;

    const confirmed = await customConfirm(`Are you sure to ${(updateCase === 1) ? "downgrade" : "upgrade"} Hyperk ${localV} to: ${remoteV}?`);
    if (!confirmed) return;

    isUpdating = true;
    
    const statusText = document.getElementById('ota_status_text');
    const progress = document.getElementById('ota_progress');
    const installBtn = document.getElementById('install_update_btn');
    const checkBtn = document.getElementById('check_update_btn');
    const saveConfigBtn = document.querySelector('form[action="/save_config"] button[type="submit"]');

    if (saveConfigBtn) saveConfigBtn.disabled = true;

    installBtn.style.display = 'none';
    checkBtn.disabled = true;
    progress.style.display = 'block';
    progress.value = 0;
    statusText.innerText = "Downloading firmware...";

    try {
        const res = await fetch(otaFirmwareUrl);
        if (!res.ok) throw new Error(`Proxy error: ${res.status} ${res.statusText}`);
        const blob = await res.blob();
        console.log(`ℹ️ Firmware size: ${blob.size} "bytes`);

        statusText.innerText = "Uploading to device... DO NOT REBOOT.";

        const formData = new FormData();
        formData.append("update", blob, "firmware.bin");

        const xhr = new XMLHttpRequest();
        xhr.open("POST", "/ota", true);
        xhr.setRequestHeader("hyperk-ota-firmware-size", blob.size);
        xhr.setRequestHeader("hyperk-ota-firmware-name", otaSelectedFirmware);

        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) {
                const percent = Math.round((e.loaded / e.total) * 100);
                progress.value = percent;
                statusText.innerText = `Flashing: ${percent}%`;
            }
        };

        xhr.onload = () => {
            isUpdating = false;
            if (xhr.status === 200) {
                statusText.innerText = "✅ Update successful! Rebooting...";
                showToast(true);
            } else {
                statusText.innerText = `❌ Flash failed: ${xhr.responseText || xhr.statusText}`;
                progress.style.display = 'none';                
            }
            checkBtn.disabled = false;
             if (saveConfigBtn) saveConfigBtn.disabled = false;
        };

        xhr.onerror = () => {
            isUpdating = false;
            statusText.innerText = "❌ Network error during upload. Device might have rebooted unexpectedly.";
            progress.style.display = 'none';
            checkBtn.disabled = false;
            if (saveConfigBtn) saveConfigBtn.disabled = false;
        };

        xhr.send(formData);

    } catch (err) {
        isUpdating = false;
        statusText.innerText = `❌ Error: ${err.message}`;
        progress.style.display = 'none';
        checkBtn.disabled = false;
        if (saveConfigBtn) saveConfigBtn.disabled = false;
    }    
};

async function startManualOtaUpload() {
    const fileInput = document.getElementById('manual_ota_file');
    const uploadBtn = document.getElementById('manual_ota_btn');
    const statusArea = document.getElementById('ota_status_area');
    const statusText = document.getElementById('ota_status_text');
    const progress = document.getElementById('ota_progress');
    const installBtn = document.getElementById('install_update_btn');
    const checkBtn = document.getElementById('check_update_btn');
    const saveConfigBtn = document.querySelector('form[action="/save_config"] button[type="submit"]');

    if (!fileInput || !fileInput.files || fileInput.files.length === 0) {
        statusArea.style.display = 'block';
        progress.style.display = 'none';
        installBtn.style.display = 'none';
        statusText.innerText = "Select a firmware .bin file first.";
        return;
    }

    const file = fileInput.files[0];
    const fileName = file.name || "";
    const otaFirmwareName = normalizeOtaFirmwareName(fileName);
    const lowerFileName = otaFirmwareName.toLowerCase();

    statusArea.style.display = 'block';
    statusArea.style.borderColor = "#eab308";
    progress.style.display = 'none';
    installBtn.style.display = 'none';

    if (!lowerFileName.startsWith("ota_hyperk_") || !lowerFileName.endsWith(".bin")) {
        statusText.innerText = "Pick an OTA_Hyperk .bin file, not a factory, bootloader, or partitions file.";
        return;
    }

    if (!firmwareNameMatchesBoard(fileName, cfgBoardArchitecture)) {
        statusText.innerText = `Wrong firmware for this controller. Expected a file for ${cfgBoardArchitecture}.`;
        return;
    }

    const confirmed = await customConfirm(`Upload and flash ${fileName}?`);
    if (!confirmed) return;

    isUpdating = true;

    if (saveConfigBtn) saveConfigBtn.disabled = true;
    if (checkBtn) checkBtn.disabled = true;
    if (uploadBtn) uploadBtn.disabled = true;

    progress.style.display = 'block';
    progress.value = 0;
    statusText.innerText = "Uploading to device... DO NOT REBOOT.";

    const formData = new FormData();
    formData.append("update", file, "firmware.bin");

    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/ota", true);
    xhr.setRequestHeader("hyperk-ota-firmware-size", file.size);
    xhr.setRequestHeader("hyperk-ota-firmware-name", otaFirmwareName);

    xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
            const percent = Math.round((e.loaded / e.total) * 100);
            progress.value = percent;
            statusText.innerText = `Flashing: ${percent}%`;
        }
    };

    xhr.onload = () => {
        isUpdating = false;
        if (xhr.status === 200) {
            statusText.innerText = "Update successful! Rebooting...";
            showToast(true);
        } else {
            statusText.innerText = `Flash failed: ${xhr.responseText || xhr.statusText}`;
            progress.style.display = 'none';
        }
        if (checkBtn) checkBtn.disabled = false;
        if (uploadBtn) uploadBtn.disabled = false;
        if (saveConfigBtn) saveConfigBtn.disabled = false;
    };

    xhr.onerror = () => {
        isUpdating = false;
        statusText.innerText = "Network error during upload. Device might have rebooted unexpectedly.";
        progress.style.display = 'none';
        if (checkBtn) checkBtn.disabled = false;
        if (uploadBtn) uploadBtn.disabled = false;
        if (saveConfigBtn) saveConfigBtn.disabled = false;
    };

    xhr.send(formData);
}

async function startFactoryReset() {
    const confirmed = await customConfirm("Factory reset Hyperk and wipe all saved settings?");
    if (!confirmed) return;

    isUpdating = true;
    const statusArea = document.getElementById('ota_status_area');
    const statusText = document.getElementById('ota_status_text');
    const resetStatus = document.getElementById('factory_reset_status');
    const progress = document.getElementById('ota_progress');
    const installBtn = document.getElementById('install_update_btn');
    const checkBtn = document.getElementById('check_update_btn');
    const resetBtn = document.getElementById('factory_reset_btn');

    if (statusArea) statusArea.style.display = 'block';
    if (progress) progress.style.display = 'none';
    if (installBtn) installBtn.style.display = 'none';
    if (checkBtn) checkBtn.disabled = true;
    if (resetBtn) resetBtn.disabled = true;
    if (statusText) statusText.innerText = "Wiping settings...";
    if (resetStatus) {
        resetStatus.style.display = 'block';
        resetStatus.innerText = "Wiping settings...";
    }

    try {
        const res = await fetch('/api/factory_reset', { method: 'POST' });
        if (!res.ok) throw new Error(`Reset failed: ${res.status}`);
        if (statusText) statusText.innerText = "Factory reset complete. Rebooting...";
        if (resetStatus) resetStatus.innerText = "Factory reset complete. Rebooting...";
        showToast(true);
    }
    catch (err) {
        isUpdating = false;
        if (statusText) statusText.innerText = `Factory reset failed: ${err.message}`;
        if (resetStatus) resetStatus.innerText = `Factory reset failed: ${err.message}`;
        if (checkBtn) checkBtn.disabled = false;
        if (resetBtn) resetBtn.disabled = false;
    }
}

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "data"
DEMO = ROOT / "demo"


DEMO_CONFIG = {
    "ssid": "Theater WiFi",
    "architecture": "ESP32",
    "board": "esp32-wled-esp32-ota-quinled-rgbw",
    "version": "demo",
    "segments": [{"data": 16, "clock": 0, "startIndex": 0}],
    "segmentSupported": 2,
    "apMode": False,
    "type": 1,
    "rgbwOrder": 1,
    "numLeds": 52,
    "relay-gpio": -1,
    "relay-inverted": False,
    "brightness": 255,
    "r": 255,
    "g": 255,
    "b": 0,
    "effect": 0,
    "deviceName": "hyperk-demo",
    "extraMdnsTag": "wled",
    "calGain": 255,
    "calRed": 176,
    "calGreen": 176,
    "calBlue": 112,
    "outputRed": 255,
    "outputGreen": 255,
    "outputBlue": 255,
    "outputWhite": 255,
    "outputRedToGreen": 0,
    "outputRedToBlue": 0,
    "outputGreenToRed": 0,
    "outputGreenToBlue": 0,
    "outputBlueToRed": 0,
    "outputBlueToGreen": 0,
    "rgbToWhiteConversion": False,
}


MOCK_SCRIPT = r"""
<script>
(() => {
  const demoConfig = __DEMO_CONFIG__;
  const nativeFetch = window.fetch ? window.fetch.bind(window) : null;

  function jsonResponse(data, status = 200) {
    return new Response(JSON.stringify(data), {
      status,
      headers: { "Content-Type": "application/json" }
    });
  }

  function textResponse(data, status = 200) {
    return new Response(data, {
      status,
      headers: { "Content-Type": "text/plain" }
    });
  }

  function readPath(input) {
    const raw = typeof input === "string" ? input : (input && input.url) || "";
    try {
      return new URL(raw, window.location.href).pathname;
    }
    catch (_) {
      return raw;
    }
  }

  function isPath(path, target) {
    return path === target || path.endsWith(target);
  }

  function clamp(value) {
    const parsed = Number(value);
    if (!Number.isFinite(parsed)) return 0;
    return Math.max(0, Math.min(255, Math.round(parsed)));
  }

  function scaled(value, gain) {
    return clamp((Number(value || 0) * Number(gain || 0)) / 255);
  }

  async function paramsFromBody(init) {
    const body = init && init.body;
    if (body instanceof URLSearchParams) return body;
    if (typeof body === "string") return new URLSearchParams(body);
    return new URLSearchParams();
  }

  function formatChannelValues(values) {
    const [r, g, b, w] = values.map(clamp);
    return `R ${r} / G ${g} / B ${b} / W ${w}`;
  }

  function displayColor(values) {
    const [r, g, b, w] = values.map(clamp);
    return [clamp(r + w), clamp(g + w), clamp(b + w)];
  }

  function setSwatchColor(swatch, values) {
    const [r, g, b] = displayColor(values);
    swatch.style.background = `rgb(${r}, ${g}, ${b})`;
  }

  function updatePreview(title, values, requestedValues = null) {
    const referenceSwatch = document.getElementById("output-reference-swatch");
    const outputSwatch = document.getElementById("output-preview-swatch");
    const text = document.getElementById("output-preview-text");
    if (!referenceSwatch || !outputSwatch || !text) return;

    const [r, g, b, w] = values.map(clamp);
    const referenceValues = requestedValues || values;

    setSwatchColor(referenceSwatch, referenceValues);
    setSwatchColor(outputSwatch, [r, g, b, w]);
    text.innerHTML = "";

    const titleLine = document.createElement("div");
    titleLine.className = "output-preview-label";
    titleLine.textContent = title;
    text.appendChild(titleLine);

    const requestLine = document.createElement("div");
    requestLine.textContent = `Screen reference: ${formatChannelValues(referenceValues)}`;
    text.appendChild(requestLine);

    const outputLine = document.createElement("div");
    outputLine.className = "output-preview-output";
    outputLine.textContent = `After tuning: ${formatChannelValues([r, g, b, w])}`;
    text.appendChild(outputLine);
  }

  async function handleCorrectedPreview(init) {
    const p = await paramsFromBody(init);
    const r = Number(p.get("r") || 0);
    const g = Number(p.get("g") || 0);
    const b = Number(p.get("b") || 0);
    const w = Number(p.get("w") || 0);

    let outR = scaled(r, p.get("outputRed")) +
               scaled(g, p.get("outputGreenToRed")) +
               scaled(b, p.get("outputBlueToRed"));
    let outG = scaled(g, p.get("outputGreen")) +
               scaled(r, p.get("outputRedToGreen")) +
               scaled(b, p.get("outputBlueToGreen"));
    let outB = scaled(b, p.get("outputBlue")) +
               scaled(r, p.get("outputRedToBlue")) +
               scaled(g, p.get("outputGreenToBlue"));
    let outW = scaled(w, p.get("outputWhite"));

    if ((p.get("rgbToWhiteConversion") || "0") !== "0" && outW === 0) {
      const shared = Math.min(outR, outG, outB);
      outW = shared;
      outR -= shared;
      outG -= shared;
      outB -= shared;
    }

    updatePreview("Corrected preview", [outR, outG, outB, outW], [r, g, b, w]);
    return jsonResponse({ status: "ok" });
  }

  async function handleRawPreview(init) {
    const p = await paramsFromBody(init);
    updatePreview("Raw preview", [
      p.get("r") || 0,
      p.get("g") || 0,
      p.get("b") || 0,
      p.get("w") || 0
    ]);
    return jsonResponse({ status: "ok" });
  }

  async function handleSaveConfig(init) {
    const p = await paramsFromBody(init);
    p.forEach((value, key) => {
      if (key in demoConfig) {
        const numeric = Number(value);
        demoConfig[key] = Number.isFinite(numeric) && value.trim() !== "" ? numeric : value;
      }
    });
    demoConfig.rgbToWhiteConversion = p.has("rgbToWhiteConversion");
    return jsonResponse({ status: "saved" });
  }

  window.fetch = async (input, init = {}) => {
    const raw = typeof input === "string" ? input : (input && input.url) || "";
    const path = readPath(input);

    if (isPath(path, "/api/get_current_config")) {
      return jsonResponse({ config: demoConfig });
    }
    if (isPath(path, "/api/test_corrected_color")) {
      return handleCorrectedPreview(init);
    }
    if (isPath(path, "/api/test_color")) {
      return handleRawPreview(init);
    }
    if (isPath(path, "/save_config")) {
      return handleSaveConfig(init);
    }
    if (isPath(path, "/save_wifi")) {
      return jsonResponse({ status: "reboot" });
    }
    if (isPath(path, "/api/wifi_scan")) {
      return jsonResponse([
        { ssid: "Theater WiFi", rssi: -44 },
        { ssid: "Basement Node", rssi: -52 },
        { ssid: "Middle Floor Node", rssi: -71 }
      ]);
    }
    if (isPath(path, "/api/stats")) {
      return jsonResponse({ fps: 60, clients: 1, uptime: 12345 });
    }
    if (isPath(path, "/ota")) {
      return textResponse("DEMO OTA OK");
    }
    if (raw.includes("hyperk-github-releases-api-proxy") ||
        raw.includes("api.github.com/repos/awawa-dev/Hyperk/releases")) {
      return jsonResponse([{
        tag_name: demoConfig.version,
        prerelease: false,
        assets: [{
          name: "OTA_Hyperk_0.0.4-quinled.11_esp32-wled-esp32-ota-quinled-rgbw.bin",
          browser_download_url: "https://example.invalid/demo.bin"
        }]
      }]);
    }
    if (raw === "https://example.invalid/demo.bin") {
      return textResponse("demo firmware");
    }

    return nativeFetch ? nativeFetch(input, init) : textResponse("Not mocked", 404);
  };

  window.XMLHttpRequest = function DemoXMLHttpRequest() {
    const xhr = {
      upload: {},
      headers: {}
    };
    xhr.open = function(method, url) {
      this.method = method;
      this.url = url;
      this.status = 0;
      this.responseText = "";
    };
    xhr.setRequestHeader = function(name, value) {
      this.headers[name] = value;
    };
    xhr.send = function(body) {
      const total = body && body.size ? body.size : 100;
      setTimeout(() => {
        if (this.upload && typeof this.upload.onprogress === "function") {
          this.upload.onprogress({ lengthComputable: true, loaded: total, total });
        }
        this.status = 200;
        this.responseText = "DEMO OTA OK";
        if (typeof this.onload === "function") this.onload();
      }, 350);
    };
    return xhr;
  };
})();
</script>
"""


DEMO_HELPER_SCRIPT = r"""
<script>
(() => {
  const demoLoadedScripts = {};

  window.loadSubScript = function(scriptName, initFunName) {
    if (demoLoadedScripts[scriptName]) return;
    demoLoadedScripts[scriptName] = true;
    if (initFunName && typeof window[initFunName] === "function") {
      window[initFunName]();
    }
  };

  document.addEventListener("DOMContentLoaded", () => {
    const sections = document.querySelectorAll('form[action="/save_config"] details');
    const hardware = sections[0];
    const calibration = sections[1];
    setTimeout(() => {
      if (hardware && !hardware.open) {
        hardware.open = true;
      }
      if (calibration && !calibration.open) {
        calibration.open = true;
      }
      window.loadSubScript("gpio", "setupPinValidator");
      window.loadSubScript("calibration", "setupCalibration");
    }, 50);
  });
})();
</script>
"""

def style_tag(path: Path) -> str:
    return f"<style>\n{path.read_text(encoding='utf-8')}\n</style>"


def script_tag(path: Path) -> str:
    content = path.read_text(encoding="utf-8").replace("</script", "<\\/script")
    return f"<script>\n{content}\n</script>"


def main() -> None:
    version = (ROOT / "version").read_text(encoding="utf-8").strip()
    demo_config = dict(DEMO_CONFIG)
    demo_config["version"] = f"{version}-demo"
    html = (DATA / "settings.html").read_text(encoding="utf-8")

    html = re.sub(r'\s*<link rel="stylesheet" href="/css/[^"]+">\n?', "\n", html)
    html = re.sub(r'\s*<link id="favicon"[^>]+>\n?', "\n", html)

    inline_styles = "\n".join(
        style_tag(DATA / "css" / name)
        for name in ("pico.min.css", "style.css", "settings.css")
    )
    html = html.replace("</head>", f"{inline_styles}\n{MOCK_SCRIPT.replace('__DEMO_CONFIG__', json.dumps(demo_config))}\n</head>")

    inline_scripts = "\n".join(
        script_tag(DATA / name)
        for name in ("gpio.js", "calibration.js", "network.js", "wifi.js", "ota.js")
    )
    html = html.replace("</body>", f"{inline_scripts}\n{DEMO_HELPER_SCRIPT}\n</body>")
    html = html.replace("{{VERSION}}", version)
    html = "\n".join(line.rstrip() for line in html.splitlines()) + "\n"

    DEMO.mkdir(exist_ok=True)
    (DEMO / "settings-demo.html").write_text(html, encoding="utf-8", newline="\n")
    print(f"Wrote {DEMO / 'settings-demo.html'}")


if __name__ == "__main__":
    main()

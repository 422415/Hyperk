# Hyperk Settings UI Demo

Open `settings-demo.html` directly in a browser to preview the settings UI without an ESP controller.

The demo uses the real firmware HTML/CSS/JavaScript, plus mocked device API responses. Preview buttons update the output preview swatch instead of sending data to LEDs.

To rebuild the demo after editing files in `data/`:

```powershell
python tools/build_ui_demo.py
```

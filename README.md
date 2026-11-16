<div align="center">

# xfce4-clock-plugin

Zero-jitter digital **Clock** plugin for **XFCE 4** with stacked or inline layouts, visual styles, and an interactive calendar popover.

</div>

---

## Highlights

- **6 Layout Modes** — `Time Only`, `Date Only`, `Stacked (Time / Date)`, `Stacked (Date / Time)`, `Inline (Time · Date)`, and `Inline (Date · Time)`.
- **5 Visual Styles** — `Minimal`, `Pill Badge`, `Accent Split`, `Card Outline`, and `Monospace`, plus per-element font size, weight, padding, radius, and colors.
- **Zero-Jitter Tabular Numerals** — `tabular-nums` alignment and second-boundary timer sync prevent neighboring panel items from shifting.
- **Built-in Calendar Popover** — Left-click opens a clean calendar popup with week numbers and a **Today** button (or runs a custom command).
- **Thread-Safe Timezones** — Uses GLib `GTimeZone` (e.g. `Asia/Tehran`, `UTC`, or system local) with live `strftime` format previews.

## Install

Requires `gtk3`, `xfce4-panel`, `libxfce4ui`, and `libxfce4util`.

```bash
cd ~/Downloads/xfce4-clock-plugin
make
sudo make install
xfce4-panel -r
```

Right-click the panel → **Panel** → **Add New Items...** → **Clock** (or run `xfce4-panel --add=xfce4-clock`).

## License

[MIT](LICENSE)

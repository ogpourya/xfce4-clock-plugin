<div align="center">

# xfce4-clock-plugin

Customizable **Clock** plugin for **XFCE 4** with stacked or inline date/time layouts, 6 distinct visual clock styles, and an interactive calendar popup.

</div>

---

## Visual Styles

- **Digital** — Crisp tabular-numeral digital readout.
- **LCD (7-Segment)** — Vector 7-segment display with unlit ghost segments and glowing colons.
- **Fuzzy** — Conversational human-time wording (`5 minutes`, `15 minutes`, or `Part of day`).
- **Analog** — Live circular watch dial with hour, minute, and second hands.
- **Binary (BCD)** — Binary-Coded Decimal LED dot matrix.
- **Nixie Tube** — Retro glowing vacuum-tube digit capsules.

## Install

```bash
cd ~/Downloads/xfce4-clock-plugin
make
sudo make install
xfce4-panel -r
```

Right-click the panel → **Panel** → **Add New Items...** → **Clock** (or run `xfce4-panel --add=xfce4-clock`).

## License

[MIT](LICENSE)

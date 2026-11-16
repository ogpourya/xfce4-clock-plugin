<div align="center">

# xfce4-clock-plugin

Customizable **Clock** plugin for **XFCE 4** with stacked or inline date/time layouts, independent styles for time, date, and separator, and a calendar popup.

</div>

---

## 11 Visual Styles (Independent for Time, Date & Separator)

- **Digital** — Crisp tabular-numeral readout.
- **LCD (7-Segment)** — Vector 7-segment display with ghost segments and glowing colons.
- **Fuzzy** — Conversational wording (`"nearly quarter past twelve"`, `"Tuesday the 17th of November"`).
- **Analog** — Live circular watch dial with hour, minute, and accent second hands.
- **Binary (BCD)** — Binary-Coded Decimal LED dot matrix and binary date encoding.
- **Nixie Tube** — Individual glowing amber vacuum-tube capsules per digit/character.
- **Dot Matrix (LED)** — 5×7 round LED pixel-matrix display.
- **Flip Clock** — Split-flap mechanical airport tiles with center hinge line.
- **Morse Code** — Live Morse dots and dashes (`·` and `—`).
- **Roman Numerals** — Classical Roman numerals (`XXIII:XLV`, `XVII · XI · MMXXVI`).
- **Neon Glow** — Dual-layer futuristic neon text glow.

## Install

```bash
cd ~/Downloads/xfce4-clock-plugin
make
sudo make install
xfce4-panel -r
```

## License

[MIT](LICENSE)

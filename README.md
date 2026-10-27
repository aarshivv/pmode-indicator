# pmode-indicator

Read-only monitor for the embedded controller of the GMKtec NucBox EVO-X2
(Sixunited AXB35-02 board), plus an Omarchy bar widget showing the power mode.

Derived from [cmetz/ec-su_axb35-linux](https://github.com/cmetz/ec-su_axb35-linux)
at commit `f62c2c2` (GPL-2.0), with every write path removed. If you want to
*control* fans or switch power modes from software, use that driver instead.

## Guarantees

- Never calls `ec_write()`/`ec_transaction()`; `make` fails if the source or
  the built module references either (`make verify`).
- All sysfs attributes are `0444`; the hwmon ops have no `.write`.
- No background work; unloading only removes sysfs entries.
- Only loads when DMI says GMKtec `NucBox_EVO-X2`.

The power mode can only be changed with the physical P-mode button.
Fans stay under the EC's automatic control.

## What it exposes

| Path | Content |
|---|---|
| `/sys/devices/platform/pmode_indicator/power_mode` | `quiet` / `balanced` / `performance` / `unknown` |
| `/sys/devices/platform/pmode_indicator/power_mode_raw` | raw EC byte at `0x31` |
| hwmon `pmode_indicator`: `fan1..3_input`, `temp1_input` | fan rpm, EC CPU temperature (also in `sensors`) |

EC `0x31` values (verified on BIOS 1.12): `00` balanced (~85 W), `01`
performance (~120 W, ~140 W boost), `02` quiet (~54 W). Wattages are
measured package power (PPT) under an all-core load.

The system fan (`fan3`) is normally stopped; the EC starts it at roughly
70 °C. Fan labels avoid the word "cpu" on purpose: btop 1.4.7 also parses
`fanN_input` as a temperature and would auto-pick a "CPU fan" as its CPU
sensor, showing rpm/1000 as °C.

## Requirements

- Kernel headers for the running kernel, `dkms`, and a C toolchain
  (`install.sh` installs `dkms` with pacman if missing).
- Secure Boot off, or a DKMS signing key enrolled with MOK; the module is
  out-of-tree and taints the kernel, which is harmless.
- The bar widget needs [Omarchy](https://omarchy.org) 4.x (omarchy-shell);
  the driver and `pmode-status` work on any distro.

## Install / remove

```sh
./install.sh     # builds + verifies, installs via DKMS, adds bar widget (asks for sudo)
./uninstall.sh
pmode-status     # CLI readout (--json for scripts)
```

`install.sh` restarts the Omarchy shell at the end: hot reload does not
recreate a widget that is already on the bar. Do the same
(`omarchy restart shell`) after editing the widget.

## Tested on

| | |
|---|---|
| Hardware | GMKtec NucBox EVO-X2, Ryzen AI Max+ 395, BIOS 1.12 (2025-12-09) |
| Kernel | 7.2.8 (`linux-omarchy`), DKMS 3.4.4 |
| Desktop | Omarchy 4.0.6, Hyprland |

### Other AXB35-02 boards

The upstream driver also lists the Bosgame M5, FEVM FA-EX9, Peladn YO1,
NIMO AI MiniPC and Corsair AI Workstation 300. They likely share the same
register map, but this module only loads on DMI-matched machines. To try
another board, add its `sys_vendor` / `product_name`
(`cat /sys/class/dmi/id/{sys_vendor,product_name}`) to `axb35_dmi_table`
in `src/pmode_indicator.c`, then confirm `power_mode_raw` follows the
button before trusting the labels.

## License

GPL-2.0, see [LICENSE](LICENSE). Register map and fan quirks from
cmetz/ec-su_axb35-linux; power-mode values re-verified on hardware.

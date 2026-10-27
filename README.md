# pmode-indicator

Read-only monitor for the embedded controller of the GMKtec NucBox EVO-X2
(Sixunited AXB35-02 board), plus an Omarchy bar widget showing the power mode.

Derived from [cmetz/ec-su_axb35-linux](https://github.com/cmetz/ec-su_axb35-linux)
at commit `f62c2c2` (GPL-2.0), with every write path removed.

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
performance (~120 W, ~140 W boost), `02` quiet (~54 W).

## Install / remove

```sh
./install.sh     # builds + verifies, installs via DKMS, adds bar widget (asks for sudo)
./uninstall.sh
pmode-status  # CLI readout
```

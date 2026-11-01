# PW3 Phase 4D — Battery-Only Power Characterization

Date: 2026-10-31 local  
Device: Kindle Paperwhite 3 (G090KB), i.MX6 SoloLite, 512 MB RAM  
Kernel: `3.0.35-lab126`  
Root: hardened ext3, `data=ordered`, `errors=remount-ro`, `barrier=0`  
Final boot ID: `9b2a013b-e04b-488f-81ef-2154d3fa7d46`

**Overall result: PARTIAL.**

Battery telemetry and the logger are validated, and one real battery-only
session (Test A) completed. Tests B and C were **not run** because the owner
could not continue the unplug/reconnect sessions. No battery-life improvement
claim is made.

## 1. Telemetry validation: PASS

Observed at the start, with USB attached and the battery full:

| Attribute | Value | Actual unit / interpretation |
|---|---:|---|
| `status` | `Full` | USB attached / charge complete |
| `capacity` | `99` | percent |
| `voltage_now` | `4198` | **mV** |
| `current_now` | `0` | **mA**, valid API; zero while USB/full |
| `charge_now` | `1470` | **mAh** |
| `charge_full` | `1408` | **mAh** |
| `charge_full_design` | `790000` | driver declares µAh (790 mAh); inconsistent with `charge_full` |
| `temp` | `95` | **°F**, about 35 °C |
| `cycle_count` | `25` | cycles |
| `health` | absent | not exposed |
| `manufacturer` / serial | absent | not exposed |

Source verification:

- `voltage_now` is computed as `mV * 1000 / 1000`, so the exposed value is mV.
- `current_now` is computed in mA and is **negative when discharging**.
- `charge_now`, `charge_full` are converted to mAh in the driver.
- `temp` is explicitly stored as Fahrenheit.
- Gauge polling is every **30 s** (`update_interval_ms = 30000`).
- `charge_full_design = 790000` is not dimensionally consistent with the
  observed `charge_full = 1408`; it is not used for health or lifetime claims.

Battery-only current sensing works: Test A recorded `current_now` from
`-49` to `-191` mA. The sign convention is negative = discharge.

`charger_online` is not a reliable connection indicator: it was `0` while USB
was connected/full and `1` while the device was battery-only. The reliable
fields are `usb_connected` (from `/proc/asession`) and `uic_present`.

## 2. Logger validation: PASS

`tools/maintenance/pw3-battery-logger.sh` was updated and validated:

- boot ID and test metadata in every row;
- `epoch`, `uptime`, `boot_id`, `test`, `note`;
- battery status, capacity, voltage, current, charge, temperature, cycles;
- CPU, context switches, interrupts, load, Wi-Fi/Tailscale PIDs;
- frontlight and governor;
- explicit `usb_connected`, `usb_host`, `charger_online`,
  `uic_present`, `power_change`;
- metadata sidecar and power-change event log;
- 60 s sampling, 1,440-row cap;
- tmpfs-only output, no per-row global `sync`;
- stop at capacity ≤ 30 % or temperature ≥ 104 °F;
- auto-stop on USB reconnect after a battery-only interval.

A short USB-connected validation run passed. The Test A metadata recorded
`end_reason=usb_reconnected`.

## 3. Test matrix

| Test | Condition | Status |
|---|---|---|
| A | Home idle, Wi-Fi off, frontlight 14 | **PASS** — 53.35 min battery-only |
| B | Home idle, Wi-Fi + Tailscale connected | **NOT TESTED** |
| C | KOReader EPUB open, Wi-Fi off | **NOT TESTED** |

Test A was run with the Tailscale daemon still running, but disconnected from
the network. That is part of the measured condition, not a clean
“Wi-Fi-and-Tailscale-off” baseline.

## 4. Test A result

### 4.1 Session

| Field | Value |
|---|---|
| Boot ID | `062d91e3-7f5a-4fd9-83b0-44bb8e2dfd34` |
| Battery-only start | epoch `1793421061` |
| Battery-only end | epoch `1793424262` |
| Duration | **3,201 s / 53.35 min** |
| Samples | 54 |
| USB state | connected before; disconnected for the full session; reconnected at end |
| Wi-Fi | off; `wlan=down`, `wpa=0`, `udhcpc=0` |
| Tailscale | daemon PID 715 present, disconnected |
| Frontlight | raw `573` = level 14 |
| Governor | `performance` |
| Reader | not running |

### 4.2 Observed battery trend

| Metric | Start | End | Change |
|---|---:|---:|---:|
| Capacity | 99 % | 94 % | **−5 %** |
| `charge_now` | 1470 mAh | 1390 mAh | **−80 mAh** |
| `voltage_now` | 4183 mV | 4120 mV | **−63 mV** |
| `current_now` | −63 mA | −150 mA | discharge |
| Temperature | 95 °F | 95 °F | 93–96 °F range |

Capacity stepped once every **~604 s / 10.07 min**:

```text
t=0     99%  1470 mAh  4183 mV
t=604   98%  1455 mAh  4172 mV
t=1208  97%  1440 mAh  4161 mV
t=1812  96%  1426 mAh  4140 mV
t=2416  95%  1411 mAh  4137 mV
t=2959  94%  1397 mAh  4120 mV
t=3201  94%  1390 mAh  4120 mV
```

### 4.3 Current, power and rate

| Metric | Observed |
|---|---:|
| Current mean (absolute) | **122.3 mA** |
| Current median | **139.5 mA** |
| Current range | 49–191 mA |
| Current p10 / p90 | 63.5 / 159.0 mA |
| Mean V × I | **≈ 0.508 W** |
| Endpoint charge rate | **90.0 mAh/h** |
| Endpoint capacity rate | **5.6 %/h** |
| Voltage slope | **≈ 71 mV/h** |
| Temperature | 93–96 °F / 33.9–35.6 °C |

The current samples and the charge-counter endpoint disagree somewhat
(122 mA vs 90 mAh/h). This is expected from a filtered gauge, coarse 1 mAh
charge count and 1 % capacity steps. Treat 90 mAh/h as the observed
charge-counter rate and 0.51 W as an approximate instantaneous estimate. Neither
is a calibrated energy-meter measurement.

**No lifetime extrapolation is claimed.** A short 53-minute session cannot
establish a full battery runtime.

### 4.4 CPU, interrupts and EPDC

| Metric | Test A |
|---|---:|
| CPU idle | **70.19 %** |
| CPU busy | **29.81 %** |
| `gw-shell` CPU | ≈ 0.13 % |
| Context switches | 1,250,512 / 3,201 s = **390.7/s** |
| Interrupts | 695,105 / 3,201 s = **217.2/s** |
| Frontlight | level 14 |
| EPDC/PXP counter | **not captured in this first logger build** |
| Reader CPU | none |
| Wi-Fi CPU | no supplicant/DHCP process |

The high total CPU is explained in part by `tailscaled`: a separate 30 s
process sample measured **349 jiffies / 3,003 total = 11.6 % CPU** for
`tailscaled` with Wi-Fi off. `top` showed about 18 % CPU for `tailscaled` and
`gw-shell` at only 0.13 %. Tailscale is spinning/reconnecting while it has no
network path.

The device log also showed several power-button `display off/on` events before
the battery-only session. They occurred before the unplug boundary; the session
itself was battery-only and the display was on at the start. The logger did not
record display/lock state, so the exact screen-on duty cycle is not fully
instrumented. This is a confidence limitation, not a telemetry failure.

## 5. Observed vs estimated

**Observed:**

- 99 → 94 % in 53.35 min
- 1470 → 1390 mAh
- 4183 → 4120 mV
- 93–96 °F
- negative discharge current from −49 to −191 mA
- 29.81 % total CPU busy, with `tailscaled` about 11.6 % of CPU in a 30 s sample

**Estimated:**

- endpoint charge rate ≈ **90 mAh/h**
- endpoint capacity rate ≈ **5.6 %/h**
- mean V × I ≈ **0.51 W**
- voltage slope ≈ **71 mV/h**

**Not estimated:**

- full battery runtime
- battery health / remaining cycles
- energy improvement from filesystem hardening
- Test B or C energy comparison
- exact EPDC energy share

## 6. Safety and regression

Stop criteria were configured but not reached:

- capacity did not fall below 30 %
- temperature stayed at 93–96 °F, below 104 °F
- no unexpected reboot or logger failure
- USB disconnect and reconnect were both recorded
- logger auto-stopped on reconnect

Post-test checks:

| Check | Result |
|---|---|
| Home | PASS after clean reboot |
| Wi-Fi | PASS — connected to `Olak Coffee`, `192.168.100.56` |
| DHCP / DNS / ping | PASS |
| Tailscale | PASS — `100.100.178.75`, Running |
| KOReader launch/normal exit | PASS — sidecar written, test book hash unchanged |
| USB administration | PASS |
| Guardian | `state=00000000` |
| Root mount | `rw,noatime,user_xattr,barrier=0,data=ordered` |
| p1 / kernel / Guardian / filesystem settings | unchanged |

One UI/display artifact was observed after the power-button toggles: the
Reader/Library quick tiles showed stale “Reader unavailable” text. A clean
reboot restored the normal Home screen. This is a display/UI-state issue, not a
filesystem or battery failure, but it should be watched.

## 7. Comparison of conditions

| Condition | Measured? | Key confounders |
|---|---|---|
| A: Home, Wi-Fi off | **Yes, 53.35 min** | Tailscale daemon running disconnected; power-button display events before session |
| B: Home, Wi-Fi + Tailscale connected | **No** | Would need another unplug session |
| C: KOReader open, Wi-Fi off | **No** | Would need another unplug session and a controlled book |

No ranking of Wi-Fi vs Tailscale vs reader energy is possible from one test.

## 8. Recommendations ranked by measured evidence

1. **Stop `tailscaled` when Wi-Fi is intentionally off.**  
   Measured impact: `tailscaled` consumed **11.6 % CPU** in a 30 s sample and
   about **18 %** in `top` while Wi-Fi was off. This is the clearest measured
   energy consumer in Test A. It likely explains much of the 29.8 % total CPU
   and the 90 mAh/h discharge rate. This is a recommendation for a later phase,
   not a change made here.

2. **Repeat Test A with `tailscaled` stopped.**  
   This is required to separate true Home idle from Tailscale reconnect spin.
   Without it, Test A cannot be treated as a clean “Wi-Fi off” baseline.

3. **Run Tests B and C before making any comparative energy claim.**  
   The current data cannot show whether Wi-Fi+Tailscale or KOReader costs more
   or less than the Tailscale-spin condition.

4. **Use `usb_connected` / `uic_present`, not `charger_online`, for power-state
   detection.**  
   `charger_online` was inverted relative to physical USB state.

5. **Use `charge_now` and current trends, not capacity percentage alone.**  
   `capacity` is 1 % coarse; `charge_now` is 1 mAh resolution and showed clear
   monotonic steps.

6. **Do not use `charge_full_design` for health or runtime math.**  
   Its declared µAh value is inconsistent with the observed `charge_full`
   value in mAh.

7. **Add PXP/EPDC and display/lock state to the logger.**  
   The first battery run did not capture EPDC update counts or whether the
   display was on. That limits attribution of the remaining CPU/energy.

## 9. Limitations and confidence

| Item | Confidence |
|---|---|
| Battery units and sign convention | **High** — verified in driver source |
| `current_now` works battery-only | **High** — 54 samples, negative discharge |
| 5 % / 80 mAh drop over 53 min | **High** — monotonic capacity and charge steps |
| 90 mAh/h endpoint rate | **Medium** — short session, filtered gauge |
| 0.51 W mean V × I | **Low–medium** — instantaneous samples, not a power meter |
| Tailscale CPU attribution | **Medium–high** — direct process sample, but separate from the full run |
| Screen-on duty cycle | **Medium** — display was on at start; exact state not logged |
| B/C comparison | **None** — not tested |
| Lifetime or health | **None** — not measured |

No claim is made that the Phase 4C filesystem hardening improved battery life.
The only measured comparison is against the historical idle baseline, and the
Test A condition was dominated by Tailscale reconnect activity.

## 10. Final state

| Check | Result |
|---|---|
| Boot ID | `9b2a013b-e04b-488f-81ef-2154d3fa7d46` |
| Root | ext3, `rw,noatime,user_xattr,barrier=0,data=ordered` |
| Battery | 96 %, USB attached |
| Guardian | `state=00000000` |
| Wi-Fi | connected, `192.168.100.56` |
| Tailscale | online, `100.100.178.75` |
| Home | normal after clean reboot |
| KOReader | launch/normal exit PASS |
| USB administration | PASS |
| Logger | stopped; Test A logs exported |

## 11. Evidence

- `docs/evidence/PW3-PHASE4D-BATTERY.md`
- `docs/evidence/PW3-PHASE4D-BATTERY.json`
- `docs/evidence/raw/PW3-PHASE4D-testA.tsv`
- `docs/evidence/raw/PW3-PHASE4D-testA-meta.txt`
- `docs/evidence/raw/PW3-PHASE4D-testA-events.txt`
- `docs/evidence/raw/PW3-PHASE4D-testA-summary.json`
- `docs/evidence/raw/PW3-PHASE4D-testA-steps.txt`
- `docs/evidence/raw/PW3-PHASE4D-telemetry-initial.txt`
- `docs/evidence/raw/PW3-PHASE4D-logger-validation.txt`
- `docs/evidence/raw/PW3-PHASE4D-cpu-consumer.txt`
- `docs/evidence/raw/PW3-PHASE4D-tailscale-cpu.txt`
- `docs/evidence/raw/PW3-PHASE4D-posttest-safety.txt`
- `docs/evidence/raw/PW3-PHASE4D-koreader-recheck.txt`
- `docs/evidence/raw/PW3-PHASE4D-final-state.txt`

Private host copies:

```text
work/private/evidence/phase4d-20261031/
  testA.tsv
  testA-meta.txt
  testA-events.txt
  cpu-consumer.txt
  tailscale-cpu.txt
  posttest-safety.txt
  koreader-recheck.txt
  final-state.txt
  screens/final-home.png
  screens/home-corrupted-before-reboot.png
  screens/home-after-reboot.png
```

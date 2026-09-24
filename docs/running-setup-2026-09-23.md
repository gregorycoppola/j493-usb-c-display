# Running setup and demo notes — September 23, 2026

The owner reports that the large external monitor connected by HDMI through
the USB-C adapter has been working well during continued use. Read-only checks
at approximately 18:58 EDT confirmed that the machine is running the rebuilt
experimental display drivers for kernel `7.1.13-3-2-ARCH`.

## Setup

- Apple MacBook Pro, 13-inch, M2, 2022 (`j493` / `t8112`).
- Samsung LS27D300G → HDMI cable → USB-C adapter/hub → Mac USB-C port.
- The previously recorded adapter is an ADAM elements CASA Hub A01; its
  physical label and the currently occupied port were not rechecked today.
- Hyprland reports the Samsung as `DP-1`, active at 1920×1080, 60 Hz, scale 1,
  beside the internal 2560×1600 panel at scale 2. The DP connector name is
  consistent with the USB-C DisplayPort path feeding the adapter's HDMI output.

## What was verified

The local rebuild's read-only installer check returned:

```text
/boot/initramfs-linux-asahi.img: candidate
/boot/m1n1/boot.bin: candidate
Loaded appledrm: rebuilt candidate
Loaded tps6598x_core: rebuilt candidate
```

The installed boot files match the September 22 rebuild's SHA-256 manifest,
and both loaded modules' GNU build-ID notes match that rebuild. The running
kernel is `7.1.13-3-2-ARCH`; `appledrm`'s `usb_c_reconnect` parameter is `Y`.
The live device tree enables the external DCP and has DisplayPort targets for
both left USB-C connectors. DRM reports `DP-1` connected and enabled, and
Hyprland reports display power on.

The preserved rebuild source also passes this public repository's
`python3 check.py <source>` patched-file hash check. This ties the rebuild's
source to the published patch series. It is the build-005 implementation
rebuilt for the newer kernel, rather than the original September 21 binaries.
Today's checks complete the previously pending post-reboot verification.

The current boot began at 11:30 EDT. Boot duration is not a measurement of
continuous visible output. The owner's report provides the physical-use
observation; software detection alone cannot establish a lit panel.

## Scope and remaining limitations

No new unplug/replug, port-switch, monitor power-cycle, or suspend test was
performed for this check. The September 21 physical tests remain historical
results on the older kernel; cycle counts and recovery times were not recorded.
Today's report does not specify an exact testing duration or cycle count.

The background recorder is sampling and its kernel journal stream is running.
It reports three fault matches since recorder startup, including its replay
window; this is not an error-free log claim or a count of visible blackouts.
The [September 22 lock/display-wake blackout](idle-wake-blackout-2026-09-22.md)
remains unresolved. Continued successful use does not establish that it is fixed.
Future kernel/initramfs/m1n1 updates can still overwrite the experimental setup.

## Public discussion — September 24, 2026

The owner [added a comment to Omarchy Mac issue #336](https://github.com/omacom/omarchy-mac/issues/336#issuecomment-5818617955), which reports a detected-but-black USB-C display after reconnect on a different M2 model. The comment links the [public j493 source demo PR](https://github.com/gregorycoppola/j493-usb-c-display/pull/2) and reports that HDMI reconnect through the USB-C hub, USB-C reconnect, monitor power reconnect, and use of either USB-C port generally restore the picture on this j493 setup. The owner also reports occasional indeterminate display states at apparently random times. No new cycle counts, timings, failure logs, or repeatable trigger were supplied with this comment; reliable recovery is not established, and the j493 result does not resolve the M2 Air report.

The comment discloses that an AI coding agent wrote the new code and that [Asahi's generative AI policy](https://asahilinux.org/llm-policy/) rules out material LLM-assisted contributions. The owner did not submit this as an Asahi PR and asked for guidance on appropriate practice. No response to that question had been recorded when this note was written.

## Proposed physical demo

1. Show the monitor picture, HDMI cable, adapter label, and occupied Mac port.
2. Show the running kernel and driver verification alongside the visible panel.
3. With work saved, demonstrate HDMI-only reconnect and then movement of the
   hub to the other USB-C port, recording each action and time to visible recovery.
4. Record actual cycle counts and any blackouts. Treat display power/wake and
   whole-system suspend/resume as separate tests.

These demo steps are a plan, not completed test results. Development and this
documentation were assisted by an LLM coding agent; physical observations are
owner-reported. No drivers, boot files, or display settings changed during this check.

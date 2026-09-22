# Black screen after lock and display wake — September 22, 2026

The owner reported that the Samsung monitor was black/off again on the tested j493, CASA Hub A01, build-005 setup. The observed failure followed an automatic lock and display power-off. The kernel logged both display controllers powering down at 11:36:47 EDT and powering up at 11:43:28–29, followed by another brief off/on cycle. Omarchy logged an unlock at 11:43:38.

The external display lost HPD at 11:44:04 and reconnected at 11:44:08 with a reported 1920×1080@60 modeset. During the visible blackout, DRM and Hyprland still reported DP-1 connected, enabled, DPMS on, and 1920×1080@60. This shows why connector detection alone does not establish a visible image.

Another external HPD remove/reconnect and successful modeset were logged at 11:46:13–16. The owner later said they had replugged the monitor, but the precise time and action were not established. A DPMS-on command at 11:47 did not visibly restore the image. A DP-1 off/on command at 11:49 briefly produced a picture; the owner then reported normal operation. The cable and software actions are too close together to identify what provided lasting recovery.

The logs establish a lock/display power-off and wake sequence, **not confirmed whole-system suspend**. They do not establish whether the driver, hub, monitor, or their interaction caused the blackout. The same firmware clock warning appears during successful modesets, so it is not a proven cause. No code or boot files changed during diagnosis. This episode limits the September 21 field-test claim: wake reliability and blackout-free operation remain unvalidated. Private raw logs are retained outside this public repository.

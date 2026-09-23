# Experimental j493 USB-C display recovery

Kernel patches for one external monitor through either USB-C port on the **13-inch M2 MacBook Pro (2022, j493/t8112)**. Adds USB-C reconnect recovery and runtime port routing on top of existing Asahi experimental DisplayPort work. This is an independent, AI-assisted source demo.

## Patch series

Apply in the order in [`patches/series`](patches/series).

| Patches | Origin and purpose |
| --- | --- |
| 0001–0004 | Janne Grunau / Asahi: DP device-tree enablement, ATC power workaround, and TIPD hotplug notifications. Preserved upstream patches; these prerequisites also touch other t8112 device trees. |
| 0005 | avillagran's experimental HDMI recovery from [Omarchy Mac PR #406](https://github.com/omacom/omarchy-mac/pull/406). Preserved prerequisite patch. |
| **0006** | Adapt recovery to the opt-in j493 USB-C path, including HDMI-only disconnect notification. |
| **0007** | Port-tagged notifications, coldplug replay, serialized routing, PHY/crossbar selection, and release/deactivation before handoff. Includes the tested startup and HDMI teardown corrections. |
| **0008** | Wire both j493 ports into the experimental route and retain the ATC power workaround. |

The original changes to review are **0006–0008**. Prerequisites are included so the series can be applied from a public baseline. One controller serves one external display at a time. The new routing path requires j493/t8112, its experimental device-tree properties, and `appledrm.usb_c_reconnect=1`.

## Apply and test

Requires Git, Python 3, and a C compiler with AddressSanitizer/UndefinedBehaviorSanitizer. The exact base is [AsahiLinux/linux `13aba96fb344feb5708d998c54331a719431a3db`](https://github.com/AsahiLinux/linux/commit/13aba96fb344feb5708d998c54331a719431a3db), tagged `asahi-7.1.13-2`. Run from this repository in Bash:

```bash
(
  set -e
  demo_repo="$PWD"
  git clone --depth 1 --branch asahi-7.1.13-2 https://github.com/AsahiLinux/linux.git linux
  test "$(git -C linux rev-parse HEAD)" = 13aba96fb344feb5708d998c54331a719431a3db
  python3 check.py linux --base
  while IFS= read -r patch; do
    git -C linux apply "$demo_repo/patches/$patch"
  done < patches/series
  python3 check.py linux
  python3 tests/run.py linux
)
```

Use a fresh checkout. The manifest verifies every modified file before/after application; it does not validate an entire kernel configuration. Tests compile actual routing/recovery C with mocked kernel/firmware interfaces. They exercise port changes, disconnect ordering, failure handling, recovery generations, and model/opt-in gating; they do not establish firmware or hardware correctness.

The commands prepare source and run tests. Kernel/module building and boot deployment require a matching kernel configuration/toolchain and platform-specific recovery preparation. Both TIPD and DRM must be built together: DRM now uses exported TIPD broker symbols. When building as external modules, build TIPD first and supply its `Module.symvers` to DRM through `KBUILD_EXTRA_SYMBOLS`. This repository contains no installer or prebuilt kernel/modules.

## Evidence and remaining limitations

**September 23:** the owner reports continued successful use. Read-only checks
confirmed the published implementation rebuilt for `7.1.13-3-2-ARCH` is loaded,
both installed boot files match that rebuild, and the Samsung is active at
1080p/60 Hz. See [running setup and demo notes](docs/running-setup-2026-09-23.md)
for verification details and the remaining wake limitation.

On September 21, 2026, the owner reported successful startup on either port, hub movement between ports, HDMI unplug/replug on either port, and monitor power cycling. Hardware: j493, CASA Hub A01, Samsung LS27D300G at 1920×1080/60 Hz, kernel `7.1.13-2-1-ARCH`. Installed artifacts and loaded drivers were verified as build-005. Cycle counts and individual recovery timings were not recorded.

On September 22 the owner reported another black/off screen after an automatic lock and display power-off/wake sequence. Linux reported the output connected/enabled; the cause remains unknown. A later cable replug and software wake tests overlapped, so neither can be credited with recovery. See the [dated incident record](docs/idle-wake-blackout-2026-09-22.md). **Blackout-free operation is not established.** Whole-system suspend/resume on this build, rapid hotplug, other adapters/models, simultaneous sinks, and long-term reliability remain unvalidated. Kernel/package updates do not preserve the original experimental deployment.

Extraction checks: all patched DRM/TIPD source files match the previously tested build-005 sources byte for byte, excluding generated module metadata. Compiling the extracted j493 device tree also reproduced the tested DTB byte for byte. Every modified pre-existing baseline file was verified against the public pinned commit. The focused tests were rerun for this publication. The earlier full module build/hardware results are historical evidence, not a fresh hardware campaign.

Related reports: [Omarchy Mac #336](https://github.com/omacom/omarchy-mac/issues/336) describes detected-but-black reconnects on an M2 Air; [Asahi #571](https://github.com/AsahiLinux/linux/issues/571) describes adapter-dependent link instability. These are context, not issues this demo claims to close.

## Attribution and licensing

Asahi prerequisite commits: [f216a6e7](https://github.com/AsahiLinux/linux/commit/f216a6e787f89b7545e28494f624e0ce997900ea), [1069f56d](https://github.com/AsahiLinux/linux/commit/1069f56d6225fbd16cea99d5e9988163465f207b), [9b65351d](https://github.com/AsahiLinux/linux/commit/9b65351d9cf850ec8273f138f04480dc1e93f15a), [d02d57a6](https://github.com/AsahiLinux/linux/commit/d02d57a6c830ed5f2934490aa6bb35df0db4d376). Their author/sign-off headers are retained.

Recovery patch and upstream tests were taken from avillagran's [PR #406 revision `631e66d3`](https://github.com/omacom/omarchy-mac/commit/631e66d39f69730fd890173f9d3b78d286e2deac). Patch SHA-256: `dbf332ead80ad84d5fb681fe83fc92c89cb88e0d4e416142acbb80565aacedcf`. The test harness has a small output-directory adjustment; the j493 wrapper also exercises USB-C and model gates. The upstream test license is retained in [`tests/upstream/LICENSE`](tests/upstream/LICENSE).

The root MIT license covers original standalone tools, tests, documentation, and original contributions where compatible with the destination file's license. Kernel source and modifications remain subject to their existing SPDX licenses (including GPL-2.0 and GPL-2.0-only OR MIT); the root license does not relicense third-party code or patch context. New driver fragments retain their GPL-2.0-only OR MIT headers. Linux license texts remain available in the pinned source's `LICENSES/` directory.

Development, extraction, and documentation were assisted by an LLM coding agent. Hardware observations are owner-reported. This is not an Asahi submission or an endorsed Omarchy release. [Asahi's LLM policy](https://asahilinux.org/llm-policy/) excludes material LLM-assisted contributions; public availability does not establish upstream eligibility.

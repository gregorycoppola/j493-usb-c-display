"""Run PR #406's real-C seams on native HDMI and the opt-in USB-C route.

The upstream harness is preserved verbatim. Only its mock device layout and
route selection are adapted here; the tested recovery functions come from the
built driver source. These synchronous mocks cannot prove firmware/race behavior.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent


def extract(source, filename, name):
    text = (source / "drivers/gpu/drm/apple" / filename).read_text()
    position = text.index(name + "(")
    start = text.rfind("\n", 0, position) + 1
    brace = text.index("{", position)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def run(source):
    env = dict(os.environ, ASAHI_TEST_SOURCE=str(source))
    subprocess.run(["python3", str(HERE / "upstream/test_reconnect.py")], env=env, check=True)
    harness = (HERE / "upstream/run_c_seam_tests.py").read_text()
    harness = harness.replace("bool hdmi_hpd,valid_mode;", "bool hdmi_hpd,usb_c_reconnect,valid_mode;")
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        for route in ("native-hdmi", "usb-c"):
            script = harness
            if route == "usb-c":
                script = script.replace(".hdmi_hpd=true,", ".hdmi_hpd=false,.usb_c_reconnect=true,")
                script = script.replace("d.hdmi_hpd=false; CHECK", "d.usb_c_reconnect=false; CHECK")
            path = directory / f"{route}.py"
            path.write_text(script)
            print(f"Recovery sequence: {route}", flush=True)
            subprocess.run(["python3", str(path)], env=env, check=True)
        # Exercise the actual eligibility helper and actual firmware HPD callback.
        preamble = r'''
#include <stdbool.h>
#include <assert.h>
#include <string.h>
typedef unsigned long long u64;
struct device { void *of_node; };
struct apple_connector { bool connected; int hotplug_wq; };
struct apple_dcp { struct device *dev; bool main_display,during_modeset,valid_mode;
  bool hdmi_hpd,usb_c_reconnect; int hdmi_generation,vblank_wq;
  struct apple_connector *connector; };
static bool usb_c_reconnect, machine_match, controller_match;
static bool of_machine_is_compatible(const char *s) {
  assert(!strcmp(s,"apple,j493")); return machine_match;
}
static bool of_device_is_compatible(void *node,const char *s) {
  (void)node; assert(!strcmp(s,"apple,t8112-dcpext"));return controller_match;
}
static int events;
static void schedule_work(int *w) { (void)w;events++; }
#define dev_info(...) ((void)0)
#define atomic_inc(p) (++*(p))
'''
        main = r'''
int main(void) {
 struct device dev={0};
 struct apple_connector con={.connected=true};
 struct apple_dcp d={.dev=&dev,.connector=&con,.valid_mode=true};
 for(int opt=0;opt<2;opt++) for(int machine=0;machine<2;machine++)
  for(int controller=0;controller<2;controller++) {
   usb_c_reconnect=opt;machine_match=machine;controller_match=controller;
   assert(dcp_usb_c_reconnect_enabled(&d)==(opt&&machine&&controller));
  }
 d.usb_c_reconnect=true;
 u64 connected=0;
 dcpep_cb_hotplug(&d,&connected);
 assert(d.hdmi_generation==1 && !con.connected && !d.valid_mode && events==2);
 connected=1;dcpep_cb_hotplug(&d,&connected);
 assert(d.hdmi_generation==1 && con.connected && events==3);
 connected=0;d.usb_c_reconnect=false;dcpep_cb_hotplug(&d,&connected);
 assert(d.hdmi_generation==1); /* opt-out leaves generation untouched */
 d.hdmi_hpd=true;dcpep_cb_hotplug(&d,&connected);
 assert(d.hdmi_generation==2); /* native HDMI retained */
 d.main_display=true;dcpep_cb_hotplug(&d,&connected);
 assert(d.hdmi_generation==2); /* internal display excluded */
 d.main_display=false;d.during_modeset=true;dcpep_cb_hotplug(&d,&connected);
 assert(d.hdmi_generation==2); /* retain firmware modeset-callback suppression */
 return 0;
}
'''
        path = directory / "hpd.c"
        path.write_text(preamble + extract(source, "dcp.c", "dcp_usb_c_reconnect_enabled") +
                        extract(source, "iomfb_template.c", "dcpep_cb_hotplug") + main)
        executable = directory / "hpd"
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        str(path), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        print("Model/opt-in gating and HDMI-only firmware HPD: PASS")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    run(parser.parse_args().source.resolve())

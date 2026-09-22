/* Compile and execute the actual routing implementation with a fake firmware. */
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define DPTX_CONNECT_TIMEOUT 2000
#define NOTIFY_OK 1
#define NOTIFY_DONE 0
#define container_of(p,t,m) ((t *)((char *)(p)-offsetof(t,m)))
#define dev_info(...) ((void)0)
#define dev_err(...) ((void)0)
#define dev_warn(...) ((void)0)
struct notifier_block { int (*notifier_call)(struct notifier_block *, unsigned long, void *); };
struct apple_route_event {void *target; unsigned port; bool connected;};
struct phy {int id;};
struct mux_control {int id; bool held;};
struct completion {int kind;};
struct dptx_port {
 bool connected,enabled;
 void *service;
 struct phy *atcphy;
 struct completion linkcfg_completion,inactive_completion;
};
struct apple_dcp {
 void *dev,*connector,*avep;
 struct dptx_port dptxport[2];
 struct phy *phy,*route_phy[2];
 struct mux_control *route_mux[2];
 unsigned dptx_phy,dptx_die;
 bool active,crashed,route_dynamic,route_registered,route_failed,route_present[2];
 int route_active;
 struct notifier_block route_notifier;
};
static int calls, fail_at, selected, releases, hpd, disconnects, hpd_low_error;
static bool inactive_ready, link_ready, link_needs_hpd;
static char history[128];
static int step(char c) {history[strlen(history)]=c; return ++calls == fail_at ? -EIO : 0;}
static void *dev_fwnode(void *dev) {return dev;}
static void disconnected_hpd_event(void *p) {disconnects++;}
static void av_service_disconnect(struct apple_dcp *d) {}
static void av_service_connect(struct apple_dcp *d) {}
static int dptxport_set_hpd(void *p,bool value) {int ret=step(value?'H':'h');if(!value && hpd_low_error)return hpd_low_error;if(!ret){hpd=value;if(value && link_needs_hpd)link_ready=true;}return ret;}
static int dptxport_release_display(void *p) {releases++;return step('R');}
static int mux_control_deselect(struct mux_control *m) {assert(m->held);int r=step('D');m->held=false;return r;}
static int mux_control_try_select(struct mux_control *m,int state) {assert(state==2);assert(!m->held);int r=step('S');if(!r)m->held=true;selected=m->id;return r;}
static int dptxport_connect(void *p,int core,int route,int die) {assert(route==selected);return step('C');}
static int dptxport_request_display(void *p) {return step('Q');}
static int wait_for_completion_timeout(struct completion *c,int timeout) {step(c->kind?'i':'l');return c->kind?inactive_ready:link_ready;}
static void reinit_completion(struct completion *c) {}
static void usleep_range(int a,int b) {}
static int apple_route_register(struct notifier_block *nb) {return step('N');}
static void apple_route_unregister(struct notifier_block *nb) {step('U');}
#include "dcp-route.c"
static struct apple_dcp d;
static struct phy phys[2];
static struct mux_control muxes[2];
static void reset(void) {
 memset(&d,0,sizeof(d));memset(muxes,0,sizeof(muxes));memset(history,0,sizeof(history));
 calls=fail_at=releases=disconnects=hpd=hpd_low_error=0;selected=-1;inactive_ready=link_ready=true;link_needs_hpd=false;
 d.dev=&d;d.active=d.route_dynamic=true;d.route_active=-1;
 d.dptxport[0].enabled=true;d.dptxport[0].inactive_completion.kind=1;
 for(int i=0;i<2;i++){phys[i].id=muxes[i].id=i;d.route_phy[i]=&phys[i];d.route_mux[i]=&muxes[i];}
}
static void event(int port,bool connected) {struct apple_route_event e={d.dev,port,connected};dcp_route_event(&d.route_notifier,0,&e);}
int main(void) {
 /* Physical HDMI removal may precede the driver's HPD-low request.
  * A rejected request must still reach release; only successful teardown
  * permits reconnect or transfer to the other port. */
 for(int first=0;first<2;first++) {
  for(int next=0;next<2;next++) {
   reset();event(first,true);hpd=0;hpd_low_error=-EINVAL;
   event(first,false);
   assert(!d.route_failed && d.route_active==-1 && releases==1);
   assert(!strcmp(history,"SCQlHhRiD"));
   event(next,true);assert(!d.route_failed && d.route_active==next && hpd);
  }
  reset();event(first,true);hpd_low_error=-EINVAL;inactive_ready=false;
  event(first,false);
  assert(d.route_failed && muxes[first].held && releases==1 && !strchr(history,'D'));
  int before=calls;event(1-first,true);assert(calls==before);
  reset();event(first,true);hpd_low_error=-EINVAL;fail_at=calls+2;
  event(first,false);
  assert(d.route_failed && muxes[first].held && !strchr(history,'i'));
  reset();event(first,true);hpd_low_error=-ETIMEDOUT;event(first,false);
  assert(d.route_failed && muxes[first].held && releases==0);
 }
 for(int first=0;first<2;first++) {
  reset();event(first,true);assert(d.route_active==first && hpd && !d.route_failed);
  assert(!strcmp(history,"SCQlH"));int before=calls;
  event(first,true);event(1-first,false);assert(calls==before && !disconnects);
  event(first,false);assert(d.route_active==-1 && !hpd && releases==1);
  assert(!strcmp(history,"SCQlHhRiD"));
  event(1-first,true);assert(d.route_active==1-first && d.phy==&phys[1-first]);
  event(1-first,false);event(first,true);assert(d.route_active==first);
 }
 reset();event(0,true);event(1,true);assert(d.route_active==0);
 event(0,false);assert(d.route_active==1); /* pending sink takeover */
 reset();event(1,true);event(0,true);assert(d.route_active==1);
 event(1,false);assert(d.route_active==0);
 reset();event(2,true);assert(!calls && d.route_active==-1);
 struct apple_route_event foreign={NULL,0,true};
 dcp_route_event(&d.route_notifier,0,&foreign);assert(!calls);
 for(int fail=1;fail<=3;fail++) { /* mux, target, request failures */
  reset();fail_at=fail;event(0,true);assert(d.route_failed);
  int before=calls;event(1,true);event(0,false);assert(calls==before);
 }
 /* This firmware may configure lanes only AFTER HPD. A pre-HPD
  * timeout must not prevent the notification that starts link training. */
 for(int first=0;first<2;first++) {
  reset();link_ready=false;link_needs_hpd=true;event(first,true);
  assert(!d.route_failed && d.route_active==first && hpd && link_ready);
  assert(!strcmp(history,"SCQlH"));
  event(first,false);link_ready=false;event(1-first,true);
  assert(!d.route_failed && d.route_active==1-first && hpd && link_ready);
 }
 /* A missing lane notification alone is not a failed routing RPC. */
 reset();link_ready=false;event(1,true);
 assert(!d.route_failed && d.route_active==1 && muxes[1].held && hpd);
 event(1,false);assert(d.route_active==-1 && !d.route_failed);
 reset();fail_at=5;event(1,true);assert(d.route_failed); /* HPD failure */
 for(int fail=1;fail<=2;fail++) {
  reset();event(0,true);fail_at=calls+fail;event(0,false);
  assert(d.route_failed && d.route_active==0 && muxes[0].held);
  int before=calls;event(1,true);assert(calls==before);
 }
 reset();event(0,true);inactive_ready=false;event(0,false);
 assert(d.route_failed && d.route_active==0 && muxes[0].held && !strchr(history,'D'));
 reset();event(0,true);fail_at=calls+4;event(0,false);assert(d.route_failed);
 reset();d.active=false;event(1,true);assert(d.route_failed && !calls);
 reset();d.dptxport[0].enabled=false;event(1,true);assert(d.route_failed && !calls);
 reset();d.crashed=true;event(1,true);assert(d.route_failed && !calls);
 reset();assert(!dcp_route_start(&d));assert(d.route_registered);int before=calls;
 assert(!dcp_route_start(&d));assert(calls==before);
 event(1,true);dcp_route_stop(&d);assert(!d.route_registered && d.route_active==-1);
 before=calls;dcp_route_stop(&d);assert(calls==before);
 reset();d.route_dynamic=false;assert(!dcp_route_start(&d));assert(!calls);
 reset();fail_at=1;assert(dcp_route_start(&d)==-EIO && !d.route_registered);
 puts("PASS: both routes, repeated reconnects, inactive-port isolation, takeover, invalid events, firmware failures, teardown");
}

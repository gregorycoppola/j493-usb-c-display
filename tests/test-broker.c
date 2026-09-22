#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
typedef uint32_t u32;
struct fwnode_handle {bool present, invalid;u32 port;};
struct notifier_block {int (*notifier_call)(struct notifier_block *,unsigned long,void *);};
struct apple_route_event {struct fwnode_handle *target;u32 port;bool connected;};
struct cd321x {struct fwnode_handle *connector_fwnode;bool route_dynamic,route_connected;u32 route_port;};
#define DEFINE_MUTEX(n) pthread_mutex_t n=PTHREAD_MUTEX_INITIALIZER
#define BLOCKING_NOTIFIER_HEAD(n) struct notifier_block *n
#define EXPORT_SYMBOL_GPL(n)
#define mutex_lock(p) pthread_mutex_lock(p)
#define mutex_unlock(p) pthread_mutex_unlock(p)
#define connector_status_connected 1
#define connector_status_disconnected 2
static int legacy_count,legacy_status,count;
static bool machine=true;
static struct apple_route_event observed[32];
static void drm_connector_oob_hotplug_event(struct fwnode_handle *f,int status){legacy_count++;legacy_status=status;}
static bool of_machine_is_compatible(const char *s){return machine;}
static bool fwnode_property_present(struct fwnode_handle *f,const char *s){return f->present;}
static int fwnode_property_read_u32(struct fwnode_handle *f,const char *s,u32 *p){*p=f->port;return f->invalid?-EINVAL:0;}
static int blocking_notifier_chain_register(struct notifier_block **chain,struct notifier_block *nb){if(*chain)return -EBUSY;*chain=nb;return 0;}
static void blocking_notifier_chain_unregister(struct notifier_block **chain,struct notifier_block *nb){assert(*chain==nb);*chain=NULL;}
static int blocking_notifier_call_chain(struct notifier_block **chain,unsigned long n,void *e){return *chain?(*chain)->notifier_call(*chain,n,e):0;}
#include "route-broker.c"
static int observe(struct notifier_block *n,unsigned long unused,void *e){assert(count<32);observed[count++]=*(struct apple_route_event *)e;return 0;}
int main(void){
 struct fwnode_handle target={0}, front={.present=true,.port=1}, back={.present=true,.port=0};
 struct cd321x a={.connector_fwnode=&target},b={.connector_fwnode=&target},duplicate={.connector_fwnode=&target};
 struct notifier_block nb={.notifier_call=observe};
 assert(!cd321x_route_add(&a,&front));assert(!cd321x_route_add(&b,&back));
 assert(cd321x_route_add(&duplicate,&front)==-EBUSY && !duplicate.route_dynamic);
 cd321x_route_event(&a,true);assert(!count && !legacy_count);
 assert(!apple_route_register(&nb));assert(count==2); /* coldplug replay */
 assert(observed[0].port==0 && !observed[0].connected);
 assert(observed[1].port==1 && observed[1].connected && observed[1].target==&target);
 cd321x_route_event(&b,false);assert(count==3 && observed[2].port==0 && !observed[2].connected);
 cd321x_route_event(&a,false);assert(count==4 && !observed[3].connected);
 cd321x_route_remove(&a);assert(!apple_route_ports[1] && !a.route_dynamic);
 apple_route_unregister(&nb);int before=count;
 cd321x_route_event(&b,true);assert(count==before);
 assert(!apple_route_register(&nb));assert(count==before+1 && observed[before].connected);
 apple_route_unregister(&nb);cd321x_route_remove(&b);
 struct fwnode_handle legacy={0};struct cd321x old={.connector_fwnode=&target};
 assert(!cd321x_route_add(&old,&legacy));cd321x_route_event(&old,true);assert(legacy_count==1 && legacy_status==1);
 cd321x_route_event(&old,false);assert(legacy_count==2 && legacy_status==2);
 old.connector_fwnode=NULL;cd321x_route_event(&old,true);assert(legacy_count==2);
 struct fwnode_handle invalid={.present=true,.port=2};old.connector_fwnode=&target;
 assert(cd321x_route_add(&old,&invalid)==-EINVAL);
 invalid.port=0;invalid.invalid=true;assert(cd321x_route_add(&old,&invalid)==-EINVAL);
 machine=false;assert(cd321x_route_add(&old,&back)==-EINVAL);
 puts("PASS: coldplug replay, port identity, removal, unsubscribe, legacy compatibility, invalid configuration");
}

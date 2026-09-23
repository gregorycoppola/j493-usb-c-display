/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
#ifndef APPLE_EXPERIMENTAL_ROUTE_H
#define APPLE_EXPERIMENTAL_ROUTE_H
#include <linux/notifier.h>
#include <linux/property.h>

/* Private j493 experiment ABI, shared by the matching TIPD and DRM modules.
 * Callbacks run synchronously, serialized by the TIPD route lock. A disconnect
 * callback completes before TIPD resets that PHY. Never call TIPD from it.
 */
struct apple_route_event {
	struct fwnode_handle *target;
	u32 port;
	bool connected;
};
int apple_route_register(struct notifier_block *nb);
void apple_route_unregister(struct notifier_block *nb);
#endif

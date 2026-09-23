/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Included by TIPD core; deliberately restricted to j493 and two ports. */
static DEFINE_MUTEX(apple_route_lock);
static BLOCKING_NOTIFIER_HEAD(apple_route_chain);
static struct cd321x *apple_route_ports[2];

int apple_route_register(struct notifier_block *nb)
{
	int ret, i;

	mutex_lock(&apple_route_lock);
	ret = blocking_notifier_chain_register(&apple_route_chain, nb);
	if (!ret) {
		for (i = 0; i < 2; i++) {
			struct cd321x *cd = apple_route_ports[i];
			struct apple_route_event event;
			if (!cd)
				continue;
			event.target = cd->connector_fwnode;
			event.port = i;
			event.connected = cd->route_connected;
			nb->notifier_call(nb, 0, &event);
		}
	}
	mutex_unlock(&apple_route_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(apple_route_register);

void apple_route_unregister(struct notifier_block *nb)
{
	mutex_lock(&apple_route_lock);
	blocking_notifier_chain_unregister(&apple_route_chain, nb);
	mutex_unlock(&apple_route_lock);
}
EXPORT_SYMBOL_GPL(apple_route_unregister);

static void cd321x_route_event(struct cd321x *cd, bool connected)
{
	struct apple_route_event event = {
		.target = cd->connector_fwnode,
		.port = cd->route_port,
		.connected = connected,
	};
	if (!cd->connector_fwnode)
		return;
	if (!cd->route_dynamic) {
		drm_connector_oob_hotplug_event(cd->connector_fwnode,
			connected ? connector_status_connected : connector_status_disconnected);
		return;
	}
	mutex_lock(&apple_route_lock);
	cd->route_connected = connected;
	blocking_notifier_call_chain(&apple_route_chain, 0, &event);
	mutex_unlock(&apple_route_lock);
}

static int cd321x_route_add(struct cd321x *cd, struct fwnode_handle *fwnode)
{
	u32 port;
	int ret = 0;
	if (!fwnode_property_present(fwnode, "apple,display-route"))
		return 0;
	if (!of_machine_is_compatible("apple,j493") || !cd->connector_fwnode ||
	    fwnode_property_read_u32(fwnode, "apple,display-route", &port) || port > 1)
		return -EINVAL;
	mutex_lock(&apple_route_lock);
	if (apple_route_ports[port]) {
		ret = -EBUSY;
	} else {
		cd->route_port = port;
		cd->route_dynamic = true;
		apple_route_ports[port] = cd;
	}
	mutex_unlock(&apple_route_lock);
	return ret;
}

static void cd321x_route_remove(struct cd321x *cd)
{
	struct apple_route_event event = {
		.target = cd->connector_fwnode,
		.port = cd->route_port,
		.connected = false,
	};
	if (cd->route_dynamic) {
		mutex_lock(&apple_route_lock);
		cd->route_connected = false;
		blocking_notifier_call_chain(&apple_route_chain, 0, &event);
		apple_route_ports[cd->route_port] = NULL;
		cd->route_dynamic = false;
		mutex_unlock(&apple_route_lock);
	}
}

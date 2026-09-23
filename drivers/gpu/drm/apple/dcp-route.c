/* SPDX-License-Identifier: GPL-2.0-only OR MIT */
/* Included by dcp.c. TIPD serializes callbacks across BOTH physical ports. */
static int dcp_route_error(struct apple_dcp *dcp, const char *stage, int ret)
{
	dev_err(dcp->dev, "USB-C route %d: %s failed (%d)\n",
		dcp->route_active, stage, ret);
	return ret;
}
static int dcp_route_release(struct apple_dcp *dcp)
{
	struct dptx_port *port = &dcp->dptxport[0];
	int ret;

	if (dcp->route_active < 0)
		return 0;
	disconnected_hpd_event(dcp->connector);
	if (dcp->avep)
		av_service_disconnect(dcp);
	if (port->connected) {
		ret = dptxport_set_hpd(port->service, false);
		if (ret && ret != -EINVAL)
			return dcp_route_error(dcp, "HPD low", ret);
		/* The fixed-route driver proceeds to release after HPD low is
		 * rejected. Preserve that teardown path, but do not surrender the
		 * route until release AND PHY deactivation are confirmed below.
		 * Transport failures remain fatal; -EINVAL alone is not success.
		 */
		if (ret)
			dev_warn(dcp->dev, "USB-C route %d: HPD low rejected (%d); verifying display release and PHY deactivation\n",
				 dcp->route_active, ret);
		ret = dptxport_release_display(port->service);
		if (ret)
			return dcp_route_error(dcp, "release display", ret);
		/* Do not repoint the PHY while firmware may still deactivate it. */
		if (!wait_for_completion_timeout(&port->inactive_completion,
						DPTX_CONNECT_TIMEOUT))
			return dcp_route_error(dcp, "PHY deactivation", -ETIMEDOUT);
		port->connected = false;
	}
	ret = mux_control_deselect(dcp->route_mux[dcp->route_active]);
	if (ret)
		return dcp_route_error(dcp, "crossbar release", ret);
	dcp->route_active = -1;
	return 0;
}

static int dcp_route_connect(struct apple_dcp *dcp, unsigned int route)
{
	struct dptx_port *port = &dcp->dptxport[0];
	int ret;

	if (!port->enabled || !dcp->active || dcp->crashed)
		return -ENODEV;
	ret = mux_control_try_select(dcp->route_mux[route], 2);
	if (ret)
		return dcp_route_error(dcp, "crossbar select", ret);
	dcp->route_active = route;
	dcp->phy = dcp->route_phy[route];
	dcp->dptx_phy = route;
	port->atcphy = dcp->phy;
	reinit_completion(&port->linkcfg_completion);
	reinit_completion(&port->inactive_completion);
	/* A failed RPC may already have reached firmware. Retain ownership until
	 * release/deactivation succeeds, even when connect reports an error. */
	port->connected = true;
	ret = dptxport_connect(port->service, 0, route, dcp->dptx_die);
	if (ret)
		return dcp_route_error(dcp, "firmware connect", ret);
	ret = dptxport_request_display(port->service);
	if (ret)
		return dcp_route_error(dcp, "request display", ret);
	/* Match the working fixed-route sequence: this pre-HPD wait is
	 * advisory. Firmware can defer lane configuration until HPD is high.
	 * Treating its expiry as fatal prevents us from ever asserting HPD.
	 * RPC errors and PHY-deactivation timeouts remain fatal below/above.
	 */
	if (!wait_for_completion_timeout(&port->linkcfg_completion,
					DPTX_CONNECT_TIMEOUT))
		dev_info(dcp->dev, "USB-C route %u: no pre-HPD lane notification; asserting HPD\n",
			 route);
	usleep_range(5, 10);
	ret = dptxport_set_hpd(port->service, true);
	if (ret)
		return dcp_route_error(dcp, "HPD high", ret);
	if (dcp->avep)
		av_service_connect(dcp);
	dev_info(dcp->dev, "USB-C route configured for %s port; awaiting display hotplug\n",
		 route ? "front" : "back");
	return 0;
}

static int dcp_route_event(struct notifier_block *nb, unsigned long unused,
			   void *data)
{
	struct apple_dcp *dcp = container_of(nb, struct apple_dcp, route_notifier);
	struct apple_route_event *event = data;
	int ret, next;

	if (event->target != dev_fwnode(dcp->dev) || event->port > 1)
		return NOTIFY_DONE;
	dcp->route_present[event->port] = event->connected;
	if (dcp->route_failed)
		return NOTIFY_OK;
	/* Charger/USB events on the unused port must not drop the display. */
	if (!event->connected && dcp->route_active == event->port) {
		ret = dcp_route_release(dcp);
		if (ret)
			goto failed;
	}
	/* Keep the existing display if both ports have sinks. */
	if (dcp->route_active >= 0)
		return NOTIFY_OK;
	next = dcp->route_present[0] ? 0 : dcp->route_present[1] ? 1 : -1;
	if (next < 0)
		return NOTIFY_OK;
	ret = dcp_route_connect(dcp, next);
	if (ret)
		goto failed;
	return NOTIFY_OK;
failed:
	/* Fail closed: never switch PHYs after an uncertain firmware response. */
	dcp->route_failed = true;
	dev_err(dcp->dev, "USB-C route failed (%d); reboot required before another route\n", ret);
	return NOTIFY_OK;
}

static int dcp_route_start(struct apple_dcp *dcp)
{
	int ret;
	if (!dcp->route_dynamic || dcp->route_registered)
		return 0;
	if (!dcp->dptxport[0].enabled || !dcp->active)
		return -ENODEV;
	dcp->route_notifier.notifier_call = dcp_route_event;
	ret = apple_route_register(&dcp->route_notifier);
	if (!ret)
		dcp->route_registered = true;
	return ret;
}

static void dcp_route_stop(struct apple_dcp *dcp)
{
	if (!dcp->route_registered)
		return;
	apple_route_unregister(&dcp->route_notifier);
	dcp->route_registered = false;
	if (dcp_route_release(dcp))
		dcp->route_failed = true;
}

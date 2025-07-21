/*
OBS Output Filter Plugin
Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <QMainWindow>
#include <QMetaObject>
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <obs-websocket-api.h>
#include <util/config-file.h>
#include "plugin-macros.generated.h"

#define MIN_REASON 8

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static void shutdown_callback(obs_data_t *request_data, obs_data_t *response_data, void *priv_data);

bool obs_module_load(void)
{
	blog(LOG_INFO, "plugin loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load()
{
	uint api_version = obs_websocket_get_api_version();
	if (api_version == 0) {
		blog(LOG_ERROR, "Unable to fetch obs-websocket plugin API version.");
		return;
	}
	else if (api_version == 1) {
		blog(LOG_WARNING, "Unsupported obs-websocket plugin API version for calling requests.");
		return;
	}

	obs_websocket_vendor vendor = obs_websocket_register_vendor(PLUGIN_NAME);
	if (!vendor) {
		blog(LOG_ERROR,
		     "Vendor registration failed! (obs-websocket should have logged something if installed properly.)");
		return;
	}

	if (!obs_websocket_vendor_register_request(vendor, "shutdown", shutdown_callback, NULL)) {
		blog(LOG_ERROR, "Failed to register 'shutdown' request with obs-websocket.");
		return;
	}

	blog(LOG_INFO, "Registered 'shutdown' to obs-websocket");
}

void obs_module_unload()
{
	blog(LOG_INFO, "plugin unloaded");
}

static bool can_shutdown(bool try_stop)
{
	bool ret = true;

	if (obs_frontend_recording_active()) {
		if (try_stop) {
			blog(LOG_INFO, "stopping recording...");
			obs_frontend_recording_stop();
		}
		ret = false;
	}

	if (obs_frontend_streaming_active()) {
		if (try_stop) {
			blog(LOG_INFO, "stopping streaming...");
			obs_frontend_streaming_stop();
		}
		ret = false;
	}

	// TODO: Remux

	return ret;
}

static void invoke_shutdown()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		blog(LOG_ERROR, "main_window is %p", main_window);
		return;
	}

	QMetaObject::invokeMethod(main_window, "close", Qt::QueuedConnection);
}

static void force_shutdown_cb(enum obs_frontend_event event, void *data)
{
	UNUSED_PARAMETER(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		blog(LOG_DEBUG, "received event %d", (int)event);
		if (can_shutdown(false))
			invoke_shutdown();
		return;
	case OBS_FRONTEND_EVENT_EXIT:
		obs_frontend_remove_event_callback(force_shutdown_cb, NULL);
		return;
	}
}

static void shutdown_callback(obs_data_t *request_data, obs_data_t *response_data, void *priv_data)
{
	UNUSED_PARAMETER(priv_data);
	UNUSED_PARAMETER(response_data); // TDOO: Implement
	blog(LOG_INFO, "shutdown is called...");

	const char *reason = obs_data_get_string(request_data, "reason");
	if (!reason || strlen(reason) < MIN_REASON) {
		blog(LOG_ERROR, "shutdown requires reason with at least %d characters", MIN_REASON);
		return;
	}

	const char *support_url = obs_data_get_string(request_data, "support_url");

	blog(LOG_INFO, "Shuting down obs-studio... Reason: %s", reason);
	if (support_url && *support_url) {
		blog(LOG_INFO, "If you need support, visit <%s>.", support_url);
	}

	if (obs_data_get_bool(request_data, "force")) {
		if (!can_shutdown(true)) {
			obs_frontend_add_event_callback(force_shutdown_cb, NULL);
			return;
		}
	}

	invoke_shutdown();
}

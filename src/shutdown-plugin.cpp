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
#include <cstdlib>
#include <thread>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/util.hpp>
#include <util/platform.h>

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

	if (obs_frontend_replay_buffer_active()) {
		if (try_stop) {
			blog(LOG_INFO, "stopping replay-buffer...");
			obs_frontend_replay_buffer_stop();
		}
		ret = false;
	}

	if (obs_frontend_virtualcam_active()) {
		if (try_stop) {
			blog(LOG_INFO, "stopping virtual-camera...");
			obs_frontend_stop_virtualcam();
		}
		ret = false;
	}

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
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		blog(LOG_DEBUG, "received event %d", (int)event);
		if (can_shutdown(false))
			invoke_shutdown();
		return;
	case OBS_FRONTEND_EVENT_EXIT:
		obs_frontend_remove_event_callback(force_shutdown_cb, NULL);
		return;
	default:
		break;
	}
}

#if defined(__APPLE__) || defined(__linux__)
#define BASE_PATH ".."
#else
#define BASE_PATH "../.."
#endif
#define CONFIG_PATH BASE_PATH "/config"

static void invoke_exit(void *)
{
	blog(LOG_INFO, "force exit.");

	/* For portable mode */
	os_unlink(CONFIG_PATH "/obs-studio/safe_mode");

	/* For non-portable mode */
	BPtr sentinelPath = os_get_config_path_ptr("obs-studio/safe_mode");
	os_unlink(sentinelPath);

	exit(0);
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

	blog(LOG_INFO, "Shutting down obs-studio... Reason: %s", reason);
	if (support_url && *support_url) {
		blog(LOG_INFO, "If you need support, visit <%s>.", support_url);
	}

	int exit_timeout_ms = int(obs_data_get_double(request_data, "exit_timeout") * 1e3);
	if (exit_timeout_ms > 0) {
		blog(LOG_INFO, "will force exit in %d ms...", exit_timeout_ms);
		std::thread(
			[](int exit_timeout_ms) {
				os_sleep_ms(exit_timeout_ms);
				/* Calling `exit()` from non-UI thread causes crash with a message below.
				 *   QObject::killTimer: Timers cannot be stopped from another thread
				 *   QObject::~QObject: Timers cannot be stopped from another thread
				 * At first, try to invoke `exit()` from the UI thread.
				 * */
				obs_queue_task(OBS_TASK_UI, invoke_exit, NULL, false);

				/* If the UI thread is not responding, let's directly call `exit()` */
				os_sleep_ms(exit_timeout_ms);
				invoke_exit(NULL);
			},
			exit_timeout_ms)
			.detach();
	}

	if (obs_data_get_bool(request_data, "force")) {
		if (!can_shutdown(true)) {
			obs_frontend_add_event_callback(force_shutdown_cb, NULL);
			return;
		}
	}

	invoke_shutdown();
}

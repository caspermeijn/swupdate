/*
 * Author: Christian Storm
 * Copyright (C) 2016, Siemens AG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <network_ipc.h>
#include <swupdate_status.h>
#include "test.h"
#include "suricatta/suricatta.h"
#include "server_hawkbit.h"
#include "channel_hawkbit.h"
#include "suricatta/state.h"

#define JSON_OBJECT_FREED 1
#define JSONQUOTE(...) #__VA_ARGS__

extern json_object *json_get_key(json_object *json_root, const char *key);

extern int __real_ipc_wait_for_complete(getstatus callback);
int __wrap_ipc_wait_for_complete(getstatus callback);
int __wrap_ipc_wait_for_complete(getstatus callback)
{
	(void)callback;
	return mock_type(RECOVERY_STATUS);
}

extern channel_op_res_t __real_channel_put(void *data);
channel_op_res_t __wrap_channel_put(void *data);
channel_op_res_t __wrap_channel_put(void *data)
{
	(void)data;
	return mock_type(channel_op_res_t);
}

extern channel_op_res_t __real_channel_get_file(void *data);
channel_op_res_t __wrap_channel_get_file(void *data);
channel_op_res_t __wrap_channel_get_file(void *data)
{
	channel_data_t *channel_data = (channel_data_t *)data;
	strncpy(channel_data->sha1hash, mock_type(char *),
		SHA_DIGEST_LENGTH * 2 + 1);
	return mock_type(channel_op_res_t);
}

extern channel_op_res_t __real_channel_get(void *data);
channel_op_res_t __wrap_channel_get(void *data);
channel_op_res_t __wrap_channel_get(void *data)
{
	channel_data_t *channel_data = (channel_data_t *)data;
	channel_data->json_reply = mock_ptr_type(json_object *);
	return mock_type(channel_op_res_t);
}

extern server_op_res_t __real_save_state(char *key, update_state_t value);
server_op_res_t __wrap_save_state(void *data);
server_op_res_t __wrap_save_state(void *data)
{
	(void)data;
	return mock_type(server_op_res_t);
}

extern server_op_res_t server_has_pending_action(int *action_id);
static void test_server_has_pending_action(void **state)
{
	(void)state;

	/* clang-format off */
	static const char *json_reply_no_update = JSONQUOTE(
	{
		"config" : {
			"polling" : {
				"sleep" : "00:01:00"
			}
		}
	}
	);
	static const char *json_reply_update_available = JSONQUOTE(
	{
		"config" : {
			"polling" : {
				"sleep" : "00:01:00"
			}
		},
		"_links" : {
			"deploymentBase" : {
				"href" : "http://deploymentBase"
			}
		}
	}
	);
	static const char *json_reply_update_data = JSONQUOTE(
	{
		"id" : "12",
		"deployment" : {
			"download" : "forced",
			"update" : "forced",
			"chunks" : [
				{
					"part" : "part01",
					"version" : "v1.0.77",
					"name" : "oneapplication",
					"artifacts" : ["list of artifacts"]
				}
			]
		}
	}
	);
	static const char *json_reply_cancel_available = JSONQUOTE(
	{
		"config" : {
			"polling" : {
				"sleep" : "00:01:00"
			}
		},
		"_links" : {
			"cancelAction" : {
				"href" : "http://cancelAction"
			}
		}
	}
	);
	static const char *json_reply_cancel_data = JSONQUOTE(
	{
		"id" : "5",
		"cancelAction" : {
			"stopId" : "5"
		}
	}
	);
	/* clang-format on */

	/* Test Case: No Action available. */
	int action_id;
	will_return(__wrap_channel_get,
		    json_tokener_parse(json_reply_no_update));
	will_return(__wrap_channel_get, CHANNEL_OK);
	assert_int_equal(SERVER_NO_UPDATE_AVAILABLE,
			 server_has_pending_action(&action_id));

	/* Test Case: Update Action available. */
	will_return(__wrap_channel_get,
		    json_tokener_parse(json_reply_update_available));
	will_return(__wrap_channel_get, CHANNEL_OK);
	will_return(__wrap_channel_get,
		    json_tokener_parse(json_reply_update_data));
	will_return(__wrap_channel_get, CHANNEL_OK);
	assert_int_equal(SERVER_UPDATE_AVAILABLE,
			 server_has_pending_action(&action_id));

	/* Test Case: Cancel Action available. */
	will_return(__wrap_channel_get,
		    json_tokener_parse(json_reply_cancel_available));
	will_return(__wrap_channel_get, CHANNEL_OK);
	will_return(__wrap_channel_get,
		    json_tokener_parse(json_reply_cancel_data));
	will_return(__wrap_channel_get, CHANNEL_OK);
	will_return(__wrap_channel_put, CHANNEL_OK);
	assert_int_equal(SERVER_OK, server_has_pending_action(&action_id));
}

extern server_op_res_t server_set_polling_interval(json_object *json_root);
static void test_server_set_polling_interval(void **state)
{
	(void)state;

	/* clang-format off */
	static const char *json_string_valid = JSONQUOTE(
	{
		"config" : {
			"polling" : {
				"sleep" : "00:01:00"
			}
		}
	}
	);
	static const char *json_string_invalid_time = JSONQUOTE(
	{
		"config" : {
			"polling" : {
				"sleep" : "XX:00:00"
			}
		}
	}
	);
	/* clang-format on */

	assert_int_equal(SERVER_EBADMSG, server_set_polling_interval(NULL));

	json_object *json_data = NULL;
	assert_non_null((json_data = json_tokener_parse(json_string_valid)));
	assert_int_equal(SERVER_OK, server_set_polling_interval(json_data));
	assert_int_equal(server_hawkbit.polling_interval, 60);
	assert_int_equal(json_object_put(json_data), JSON_OBJECT_FREED);
	json_data = NULL;

	assert_non_null(
	    (json_data = json_tokener_parse(json_string_invalid_time)));
	assert_int_equal(SERVER_EBADMSG,
			 server_set_polling_interval(json_data));
	assert_int_equal(json_object_put(json_data), JSON_OBJECT_FREED);
}

extern server_op_res_t
server_send_deployment_reply(const int action_id, const int job_cnt_max,
			     const int job_cnt_cur, const char *finished,
			     const char *execution_status, const char *details);
static void test_server_send_deployment_reply(void **state)
{
	(void)state;
	int action_id = 23;

	/* Test Case: Channel sent reply. */
	will_return(__wrap_channel_put, CHANNEL_OK);
	assert_int_equal(SERVER_OK,
			 server_send_deployment_reply(
			     action_id, 5, 5,
			     reply_status_result_finished.success,
			     reply_status_execution.closed, "UNIT TEST"));

	/* Test Case: Channel didn't sent reply. */
	will_return(__wrap_channel_put, CHANNEL_EIO);
	assert_int_equal(SERVER_EERR,
			 server_send_deployment_reply(
			     action_id, 5, 5,
			     reply_status_result_finished.success,
			     reply_status_execution.closed, "UNIT TEST"));
}

extern server_op_res_t server_send_cancel_reply(const int action_id);
static void test_server_send_cancel_reply(void **state)
{
	(void)state;
	int action_id = 23;

	/* Test Case: Channel sent reply. */
	will_return(__wrap_channel_put, CHANNEL_OK);
	assert_int_equal(SERVER_OK, server_send_cancel_reply(action_id));

	/* Test Case: Channel didn't sent reply. */
	will_return(__wrap_channel_put, CHANNEL_EIO);
	assert_int_equal(SERVER_EERR, server_send_cancel_reply(action_id));
}

extern server_op_res_t
server_process_update_artifact(json_object *json_data_artifact,
			       const char *update_action, const char *part,
			       const char *version, const char *name);

static void test_server_process_update_artifact(void **state)
{
	(void)state;
	/* clang-format off */
	static const char *json_artifact = JSONQUOTE(
	{
		"artifacts": [
		{
			"filename" : "afile",
			"hashes" : {
				"sha1" : "CAFFEE",
				"md5" : "DEADBEEF",
			},
			"size" : 12,
			"_links" : {
				"download" : {
					"href" : "http://download"
				},
				"md5sum" : {
					"href" : "http://md5sum"
				}
			}
		}
		]
	}
	);
	/* clang-format on */

	/* Test Case: Artifact installed successfully. */
	json_object *json_data_artifact = json_tokener_parse(json_artifact);
	will_return(__wrap_channel_get_file, "CAFFEE");
	will_return(__wrap_channel_get_file, CHANNEL_OK);
	will_return(__wrap_ipc_wait_for_complete, SUCCESS);
	assert_int_equal(SERVER_OK,
			 server_process_update_artifact(
			     json_get_key(json_data_artifact, "artifacts"),
			     "update action", "part", "version", "name"));
	assert_int_equal(json_object_put(json_data_artifact),
			 JSON_OBJECT_FREED);
}

extern server_op_res_t server_install_update();
static void test_server_install_update(void **state)
{
	(void)state;

	/* clang-format off */
	static const char *json_reply_update_available = JSONQUOTE(
	{
		"config" : {
			"polling" : {
				"sleep" : "00:01:00"
			}
		},
		"_links" : {
			"deploymentBase" : {
				"href" : "http://deploymentBase"
			}
		}
	}
	);
	static const char *json_reply_update_invalid_data = JSONQUOTE(
	{
		"id" : "12",
		"deployment" : {
			"download" : "forced",
			"update" : "forced",
			"chunks" : [
				{
					"part" : "part01",
					"version" : "v1.0.77",
					"name" : "oneapplication",
					"artifacts" : ["no artifacts, failure"]
				}
			]
		}
	}
	);
	static const char *json_reply_update_valid_data = JSONQUOTE(
	{
		"id" : "12",
		"deployment" : {
		"download" : "forced",
		"update" : "forced",
		"chunks" : [
			{
			"part" : "part01",
			"version" : "v1.0.77",
			"name" : "oneapplication",
			"artifacts": [
				{
					"filename" : "afile",
					"hashes" : {
						"sha1" : "CAFFEE",
						"md5" : "DEADBEEF",
					},
					"size" : 12,
					"_links" : {
						"download" : {
							"href" : "http://download"
						},
						"md5sum" : {
							"href" : "http://md5sum"
						}
					}
				}
			]
			}
		]
		}
	}
	);
	/* clang-format on */

	json_object *json_data_update_details_valid = NULL;
	json_object *json_data_update_available = NULL;
	json_object *json_data_update_details_invalid = NULL;

	/* Test Case: Update details are malformed JSON. */
	json_data_update_available =
	    json_tokener_parse(json_reply_update_available);
	json_data_update_details_invalid =
	    json_tokener_parse(json_reply_update_invalid_data);
	will_return(__wrap_channel_get, json_data_update_available);
	will_return(__wrap_channel_get, CHANNEL_OK);
	will_return(__wrap_channel_get, json_data_update_details_invalid);
	will_return(__wrap_channel_get, CHANNEL_OK);
	will_return(__wrap_channel_put, CHANNEL_OK);
	will_return(__wrap_channel_put, CHANNEL_OK);
	server_install_update();

	/* Test Case: Update works. */
	json_data_update_available =
	    json_tokener_parse(json_reply_update_available);
	json_data_update_details_valid =
	    json_tokener_parse(json_reply_update_valid_data);
	will_return(__wrap_channel_get, json_data_update_available);
	will_return(__wrap_channel_get, CHANNEL_OK);
	will_return(__wrap_channel_get, json_data_update_details_valid);
	will_return(__wrap_channel_get, CHANNEL_OK);
	will_return(__wrap_channel_put, CHANNEL_OK);
	will_return(__wrap_channel_get_file, "CAFFEE");
	will_return(__wrap_channel_get_file, CHANNEL_OK);
	will_return(__wrap_ipc_wait_for_complete, SUCCESS);
	will_return(__wrap_channel_put, CHANNEL_OK);
	will_return(__wrap_save_state, SERVER_OK);
	will_return(__wrap_channel_put, CHANNEL_OK);
	server_install_update();
}

static int server_hawkbit_setup(void **state)
{
	(void)state;
	server_hawkbit.url = "http://void.me";
	server_hawkbit.tenant = "tenant";
	server_hawkbit.device_id = "deviceID";
	return 0;
}

static int server_hawkbit_teardown(void **state)
{
	(void)state;
	return 0;
}

int main(void)
{
	int error_count = 0;
	const struct CMUnitTest hawkbit_server_tests[] = {
	    cmocka_unit_test(test_server_install_update),
	    cmocka_unit_test(test_server_send_deployment_reply),
	    cmocka_unit_test(test_server_send_cancel_reply),
	    cmocka_unit_test(test_server_process_update_artifact),
	    cmocka_unit_test(test_server_set_polling_interval),
	    cmocka_unit_test(test_server_has_pending_action)};
	error_count += cmocka_run_group_tests_name(
	    "server_hawkbit", hawkbit_server_tests, server_hawkbit_setup,
	    server_hawkbit_teardown);
	return error_count;
}

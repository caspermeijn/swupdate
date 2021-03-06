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

#include <stdio.h>
#include <errno.h>
#include <util.h>
#include <fw_env.h>
#include "suricatta/state.h"

/*
 * This check is to avoid to corrupt the environment
 * An empty key is accepted, but U-Boot reports a corrupted
 * environment/
 */
#define CHECK_STATE_VAR(v) do { \
	if (strnlen(v, UBOOT_VAR_LENGTH) == 0) { \
		WARN("Update Status Storage Key (CONFIG_SURICATTA_STATE_UBOOT) " \
			"is empty, setting it to 'ustate'\n"); \
		v = (char *)"ustate"; \
	} \
} while(0)

#ifndef CONFIG_SURICATTA_STATE_CHOICE_UBOOT
server_op_res_t save_state(char *key, update_state_t value)
{
	(void)key;
	(void)value;
	return SERVER_OK;
}

server_op_res_t read_state(char *key, update_state_t *value)
{
	(void)key;
	(void)value;
	*value = STATE_NOT_AVAILABLE;
	return SERVER_OK;
}

server_op_res_t reset_state(char *key)
{
	(void)key;
	return SERVER_OK;
}
#else

server_op_res_t save_state(char *key, update_state_t value)
{
	CHECK_STATE_VAR(key);
	if (fw_env_open(fw_env_opts) != 0) {
		ERROR("Error: Cannot initialize U-Boot environment.\n");
		return SERVER_EERR;
	}
	if (fw_env_write(key, (char *)&value) != 0) {
		ERROR("Error: Cannot write to U-Boot's environment.\n");
		return SERVER_EERR;
	}
	return fw_env_close(fw_env_opts) == 0 ? SERVER_OK : SERVER_EERR;
}

server_op_res_t read_state(char *key, update_state_t *value)
{
	CHECK_STATE_VAR(key);
	if (fw_env_open(fw_env_opts) != 0) {
		ERROR("Error: Cannot initialize U-Boot environment.\n");
		return SERVER_EERR;
	}
	char *envval;
	if ((envval = fw_getenv(key)) == NULL) {
		INFO("Key '%s' not found in U-Boot environment.\n", key);
		*value = STATE_NOT_AVAILABLE;
		return SERVER_OK;
	}
	/* TODO It's a bit whacky just to cast this but as we're the only */
	/*      ones touching the variable, it's maybe OK for a PoC now. */
	*value = (update_state_t)*envval;
	return SERVER_OK;
}
server_op_res_t reset_state(char *key)
{
	CHECK_STATE_VAR(key);
	if (fw_env_open(fw_env_opts) != 0) {
		ERROR("Error: Cannot initialize U-Boot environment.\n");
		return SERVER_EERR;
	}
	if (fw_env_write(key, NULL) != 0) {
		ERROR("Error: Cannot write to U-Boot's environment.\n");
		return SERVER_EERR;
	}
	return fw_env_close(fw_env_opts) == 0 ? SERVER_OK : SERVER_EERR;
}
#endif /* CONFIG_SURICATTA_STATE_CHOICE_UBOOT */

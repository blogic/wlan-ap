/*
Copyright (c) 2019, Plume Design Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of the Plume Design Inc. nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL Plume Design Inc. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdbool.h>

#include <uci.h>
#include <uci_blob.h>

#include <target.h>

#include "ovsdb.h"
#include "ovsdb_update.h"
#include "ovsdb_sync.h"
#include "ovsdb_table.h"
#include "ovsdb_cache.h"

#include "nl80211.h"
#include "radio.h"
#include "vif.h"
#include "phy.h"
#include "log.h"
#include "evsched.h"
#include "uci.h"
#include "utils.h"

static struct uci_package *wireless;
struct uci_context *uci;
struct blob_buf b = { };
int reload_config = 0;

enum {
	WDEV_ATTR_PATH,
	WDEV_ATTR_DISABLED,
	WDEV_ATTR_CHANNEL,
	WDEV_ATTR_TXPOWER,
	WDEV_ATTR_BEACON_INT,
	WDEV_ATTR_HTMODE,
	WDEV_ATTR_HWMODE,
	WDEV_ATTR_COUNTRY,
	WDEV_ATTR_CHANBW,
	WDEV_ATTR_TX_ANTENNA,
	WDEV_ATTR_FREQ_BAND,
	__WDEV_ATTR_MAX,
};


static const struct blobmsg_policy wifi_device_policy[__WDEV_ATTR_MAX] = {
	[WDEV_ATTR_PATH] = { .name = "path", .type = BLOBMSG_TYPE_STRING },
	[WDEV_ATTR_DISABLED] = { .name = "disabled", .type = BLOBMSG_TYPE_BOOL },
	[WDEV_ATTR_CHANNEL] = { .name = "channel", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_TXPOWER] = { .name = "txpower", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_BEACON_INT] = { .name = "beacon_int", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_HTMODE] = { .name = "htmode", .type = BLOBMSG_TYPE_STRING },
	[WDEV_ATTR_HWMODE] = { .name = "hwmode", .type = BLOBMSG_TYPE_STRING },
	[WDEV_ATTR_COUNTRY] = { .name = "country", .type = BLOBMSG_TYPE_STRING },
	[WDEV_ATTR_CHANBW] = { .name = "chanbw", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_TX_ANTENNA] = { .name = "tx_antenna", .type = BLOBMSG_TYPE_INT32 },
	[WDEV_ATTR_FREQ_BAND] = { .name = "freq_band", .type = BLOBMSG_TYPE_STRING },
};

const struct uci_blob_param_list wifi_device_param = {
	.n_params = __WDEV_ATTR_MAX,
	.params = wifi_device_policy,
};

static bool radio_state_update(struct uci_section *s, struct schema_Wifi_Radio_Config *rconf)
{
	struct blob_attr *tb[__WDEV_ATTR_MAX] = { };
	struct schema_Wifi_Radio_State  rstate;
	char phy[6];

	LOGN("%s: get state", rstate.if_name);

	memset(&rstate, 0, sizeof(rstate));
	schema_Wifi_Radio_State_mark_all_present(&rstate);
	rstate._partial_update = true;
	rstate.channel_sync_present = false;
	rstate.channel_mode_present = false;
	rstate.radio_config_present = false;
	rstate.vif_states_present = false;

	blob_buf_init(&b, 0);
	uci_to_blob(&b, s, &wifi_device_param);
	blobmsg_parse(wifi_device_policy, __WDEV_ATTR_MAX, tb, blob_data(b.head), blob_len(b.head));

	SCHEMA_SET_STR(rstate.if_name, s->e.name);

	if (strcmp(s->e.name, "wifi0")) {
		STRSCPY(rstate.hw_config_keys[0], "dfs_enable");
		snprintf(rstate.hw_config[0], sizeof(rstate.hw_config[0]), "1");
		STRSCPY(rstate.hw_config_keys[1], "dfs_ignorecac");
		snprintf(rstate.hw_config[1], sizeof(rstate.hw_config[0]), "0");
		STRSCPY(rstate.hw_config_keys[2], "dfs_usenol");
		snprintf(rstate.hw_config[2], sizeof(rstate.hw_config[0]), "1");
		rstate.hw_config_len = 3;
	}

	if (!tb[WDEV_ATTR_PATH] ||
	    phy_from_path(blobmsg_get_string(tb[WDEV_ATTR_PATH]), phy)) {
		LOGE("%s has no phy", rstate.if_name);
		return false;
	}

	if (tb[WDEV_ATTR_CHANNEL])
		SCHEMA_SET_INT(rstate.channel, blobmsg_get_u32(tb[WDEV_ATTR_CHANNEL]));

	SCHEMA_SET_INT(rstate.enabled, 1);
	if (tb[WDEV_ATTR_DISABLED] && blobmsg_get_bool(tb[WDEV_ATTR_DISABLED]))
		SCHEMA_SET_INT(rstate.enabled, 0);
	else
		SCHEMA_SET_INT(rstate.enabled, 1);

	if (tb[WDEV_ATTR_TXPOWER]) {
		SCHEMA_SET_INT(rstate.tx_power, blobmsg_get_u32(tb[WDEV_ATTR_TXPOWER]));
		/* 0 means max in UCI, 32 is max in OVSDB */
		if (rstate.tx_power == 0)
			rstate.tx_power = 32;
	} else
		SCHEMA_SET_INT(rstate.tx_power, 32);

	if (tb[WDEV_ATTR_BEACON_INT])
		SCHEMA_SET_INT(rstate.bcn_int, blobmsg_get_u32(tb[WDEV_ATTR_BEACON_INT]));
	else
		SCHEMA_SET_INT(rstate.bcn_int, 100);

	if (tb[WDEV_ATTR_HTMODE])
		SCHEMA_SET_STR(rstate.ht_mode, blobmsg_get_string(tb[WDEV_ATTR_HTMODE]));

	if (tb[WDEV_ATTR_HWMODE])
		SCHEMA_SET_STR(rstate.hw_mode, blobmsg_get_string(tb[WDEV_ATTR_HWMODE]));

	if (tb[WDEV_ATTR_TX_ANTENNA])
		SCHEMA_SET_INT(rstate.tx_chainmask, blobmsg_get_u32(tb[WDEV_ATTR_TX_ANTENNA]));
	else
		SCHEMA_SET_INT(rstate.tx_chainmask, phy_get_tx_chainmask(phy));

	if (rstate.hw_mode_exists && rstate.ht_mode_exists) {
		struct mode_map *m = mode_map_get_cloud(rstate.ht_mode, rstate.hw_mode);

		if (m) {
			SCHEMA_SET_STR(rstate.hw_mode, m->hwmode);
			if (m->htmode)
				SCHEMA_SET_STR(rstate.ht_mode, m->htmode);
			else
				rstate.ht_mode_exists = false;
		} else {
			LOGE("%s: failed to decode ht/hwmode", rstate.if_name);
			rstate.hw_mode_exists = false;
			rstate.ht_mode_exists = false;
		}
	}

	if (tb[WDEV_ATTR_COUNTRY])
		SCHEMA_SET_STR(rstate.country, blobmsg_get_string(tb[WDEV_ATTR_COUNTRY]));
	else
		SCHEMA_SET_STR(rstate.country, "CA");

	rstate.allowed_channels_len = phy_get_channels(phy, rstate.allowed_channels);
	rstate.allowed_channels_present = true;

	if (tb[WDEV_ATTR_FREQ_BAND])
		SCHEMA_SET_STR(rstate.freq_band, blobmsg_get_string(tb[WDEV_ATTR_FREQ_BAND]));
	else if (!phy_get_band(phy, rstate.freq_band))
		rstate.freq_band_exists = true;

	if (!phy_get_mac(phy, rstate.mac))
		rstate.mac_exists = true;

	if (rconf) {
		LOGN("%s: updating radio config", rstate.if_name);
		radio_state_to_conf(&rstate, rconf);
		SCHEMA_SET_STR(rconf->hw_type, "ath10k");
		radio_ops->op_rconf(rconf);
	}
	LOGN("%s: updating radio state", rstate.if_name);
	radio_ops->op_rstate(&rstate);

	return true;
}

bool target_radio_config_set2(const struct schema_Wifi_Radio_Config *rconf,
			      const struct schema_Wifi_Radio_Config_flags *changed)
{
	struct uci_section *s;

	blob_buf_init(&b, 0);

	if (changed->channel && rconf->channel)
		blobmsg_add_u32(&b, "channel", rconf->channel);

	if (changed->enabled)
		blobmsg_add_u8(&b, "disabled", rconf->enabled ? 0 : 1);

	if (changed->tx_power)
		blobmsg_add_u32(&b, "txpower", rconf->tx_power);

	if (changed->tx_chainmask)
		blobmsg_add_u32(&b, "tx_antenna", rconf->tx_chainmask);

	if (changed->country)
		blobmsg_add_string(&b, "country", rconf->country);

	if (changed->bcn_int) {
		int beacon_int = rconf->bcn_int;

		if ((rconf->bcn_int < 50) || (rconf->bcn_int > 400))
			beacon_int = 100;
		blobmsg_add_u32(&b, "beacon_int", beacon_int);
	}

	if ((changed->ht_mode) || (changed->hw_mode) || (changed->freq_band)) {
		struct mode_map *m = mode_map_get_uci(rconf->freq_band, rconf->ht_mode,
						      rconf->hw_mode);
		if (m) {
			blobmsg_add_string(&b, "htmode", m->ucihtmode);
			blobmsg_add_string(&b, "hwmode", m->ucihwmode);
			blobmsg_add_u32(&b, "chanbw", 20);
		} else
			 LOGE("%s: failed to set ht/hwmode", rconf->if_name);
	}

	uci_load(uci, "wireless", &wireless);
	s = uci_lookup_section(uci, wireless, rconf->if_name);
	if (!s) {
		LOGE("%s: failed to lookup %s.%s", rconf->if_name,
		     "wireless", rconf->if_name);
		uci_unload(uci, wireless);
		return false;
	}

	blob_to_uci(b.head, &wifi_device_param, s);
	uci_commit(uci, &wireless, false);
	uci_unload(uci, wireless);

	reload_config = 1;

	return true;
}

static void periodic_task(void *arg)
{
	static int counter = 0;
	struct uci_element *e;

	if ((counter % 15) && !reload_config)
		goto done;

	if (reload_config) {
		LOGT("periodic: reload config");
		reload_config = 0;
		system("reload_config");
	}

	LOGT("periodic: start state update ");

	uci_load(uci, "wireless", &wireless);
	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-device"))
			radio_state_update(s, NULL);
	}

	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-iface"))
			vif_state_update(s, NULL);
	}
	uci_unload(uci, wireless);
	LOGT("periodic: stop state update ");

done:
	counter++;
	evsched_task_reschedule_ms(EVSCHED_SEC(1));
}

bool target_radio_config_init2(void)
{
	struct schema_Wifi_Radio_Config rconf;
	struct schema_Wifi_VIF_Config vconf;
	struct uci_element *e;

	uci_load(uci, "wireless", &wireless);
	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-device"))
			radio_state_update(s, &rconf);
	}

	uci_foreach_element(&wireless->sections, e) {
		struct uci_section *s = uci_to_section(e);

		if (!strcmp(s->type, "wifi-iface"))
			vif_state_update(s, &vconf);
	}
	uci_unload(uci, wireless);

	return true;
}

bool target_radio_init(const struct target_radio_ops *ops)
{
	uci = uci_alloc_context();

	target_map_init();
	target_map_insert("home-ap-24", "home_ap_24");
	target_map_insert("home-ap-50", "home_ap_50");
	target_map_insert("home-ap-l50", "home_ap_l50");
	target_map_insert("home-ap-u50", "home_ap_u50");

	target_map_insert("wifi0", "phy1");
	target_map_insert("wifi1", "phy2");
	target_map_insert("wifi2", "phy0");

	radio_ops = ops;

	evsched_task(&periodic_task, NULL, EVSCHED_SEC(5));
	nl80211_state_init();
	return true;
}

bool target_radio_config_need_reset(void)
{
	return true;
}

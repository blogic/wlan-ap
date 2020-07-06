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

#include "target.h"
#include <stdio.h>
#include <stdbool.h>
#include "iwinfo.h"

#include "nl80211.h"
#include "phy.h"
#include "utils.h"

#define NUM_MAX_CLIENTS 10

/*****************************************************************************
 *  INTERFACE definitions
 *****************************************************************************/

bool target_is_radio_interface_ready(char *phy_name)
{
	return true;
}

bool target_is_interface_ready(char *if_name)
{
	return true;
}

/******************************************************************************
 *  STATS definitions
 *****************************************************************************/

bool target_radio_tx_stats_enable(radio_entry_t *radio_cfg, bool enable)
{
	return true;
}

bool target_radio_fast_scan_enable(radio_entry_t *radio_cfg, ifname_t if_name)
{
	return true;
}


/******************************************************************************
 *  CLIENT definitions
 *****************************************************************************/

target_client_record_t* target_client_record_alloc()
{
	target_client_record_t *record = NULL;

	record = malloc(sizeof(target_client_record_t));
	if (record == NULL)
		return NULL;

	memset(record, 0, sizeof(target_client_record_t));

	return record;
}

void target_client_record_free(target_client_record_t *record)
{
	if (record != NULL)
		free(record);
}

bool target_stats_clients_get(radio_entry_t *radio_cfg, radio_essid_t *essid,
			      target_stats_clients_cb_t *client_cb,
			      ds_dlist_t *client_list, void *client_ctx)
{
	struct nl_call_param nl_call_param = {
		.ifname = target_map_ifname(radio_cfg->if_name),
		.type = radio_cfg->type,
		.list = client_list,
	};
	bool ret = true;

	if (nl80211_get_assoclist(&nl_call_param) < 0)
		ret = false;
	LOGN("%s: assoc returned %d, list %d", radio_cfg->if_name, ret, ds_dlist_is_empty(client_list));
	(*client_cb)(client_list, client_ctx, ret);

        return ret;
}

bool target_stats_clients_convert(radio_entry_t *radio_cfg, target_client_record_t *data_new,
				  target_client_record_t *data_old, dpp_client_record_t *client_record)
{
	memcpy(client_record->info.mac, data_new->info.mac, sizeof(data_new->info.mac));

	client_record->stats.bytes_tx   = data_new->stats.bytes_tx;
	client_record->stats.bytes_rx   = data_new->stats.bytes_rx;
	client_record->stats.rssi       = data_new->stats.rssi;
	client_record->stats.rate_tx    = data_new->stats.rate_tx;
	client_record->stats.rate_rx    = data_new->stats.rate_rx;

	return true;
}


/******************************************************************************
 *  SURVEY definitions
 *****************************************************************************/

target_survey_record_t* target_survey_record_alloc()
{
	target_survey_record_t *record = NULL;

	record = malloc(sizeof(target_survey_record_t));
	if (record == NULL)
		return NULL;

	memset(record, 0, sizeof(target_survey_record_t));

	return record;
}

void target_survey_record_free(target_survey_record_t *result)
{
	if (result != NULL)
		free(result);
}

bool target_stats_survey_get(radio_entry_t *radio_cfg, uint32_t *chan_list,
			     uint32_t chan_num, radio_scan_type_t scan_type,
			     target_stats_survey_cb_t *survey_cb,
			     ds_dlist_t *survey_list, void *survey_ctx)
{
	struct nl_call_param nl_call_param = {
		.ifname = target_map_ifname(radio_cfg->if_name),
		.type = radio_cfg->type,
		.list = survey_list,
	};
	bool ret = true;

	if (nl80211_get_survey(&nl_call_param) < 0)
		ret = false;
	LOGN("%s: survey returned %d, list %d", radio_cfg->if_name, ret, ds_dlist_is_empty(survey_list));
	(*survey_cb)(survey_list, survey_ctx, ret);

	return ret;
}

bool target_stats_survey_convert(radio_entry_t *radio_cfg, radio_scan_type_t scan_type,
				 target_survey_record_t *data_new, target_survey_record_t *data_old,
				 dpp_survey_record_t *survey_record)
{
	survey_record->info.chan     = data_new->info.chan;
	survey_record->chan_tx       = data_new->chan_tx;
	survey_record->chan_self     = data_new->chan_self;
	survey_record->chan_rx       = data_new->chan_rx;
	survey_record->chan_busy_ext = data_new->chan_busy_ext;
	survey_record->duration_ms   = data_new->duration_ms;
	survey_record->chan_busy     = data_new->chan_busy;

	return true;
}


/******************************************************************************
 *  NEIGHBORS definitions
 *****************************************************************************/

bool target_stats_scan_start(radio_entry_t *radio_cfg, uint32_t *chan_list, uint32_t chan_num,
			     radio_scan_type_t scan_type, int32_t dwell_time,
			     target_scan_cb_t *scan_cb, void *scan_ctx)
{
	struct nl_call_param nl_call_param = {
		.ifname = target_map_ifname(radio_cfg->if_name),
	};
	bool ret = true;

	if (nl80211_scan_trigger(&nl_call_param, chan_list, chan_num, dwell_time, scan_type, scan_cb, scan_ctx) < 0)
		ret = false;
	LOGN("%s: scan trigger returned %d", radio_cfg->if_name, ret);

	if (ret == false) {
		LOGN("%s: failed to trigger scan, aborting", radio_cfg->if_name);
		(*scan_cb)(scan_ctx, ret);
	}
	return ret;
}

bool target_stats_scan_stop(radio_entry_t *radio_cfg, radio_scan_type_t scan_type)
{
	struct nl_call_param nl_call_param = {
		.ifname = target_map_ifname(radio_cfg->if_name),
	};
	bool ret = true;

	if (nl80211_scan_abort(&nl_call_param) < 0)
		ret = false;
	LOGN("%s: scan abort returned %d", radio_cfg->if_name, ret);

	return true;
}

bool target_stats_scan_get(radio_entry_t *radio_cfg, uint32_t *chan_list, uint32_t chan_num,
			   radio_scan_type_t scan_type, dpp_neighbor_report_data_t *scan_results)
{
	struct nl_call_param nl_call_param = {
		.ifname = target_map_ifname(radio_cfg->if_name),
		.type = radio_cfg->type,
		.list = &scan_results->list,
	};
	bool ret = true;

	syslog(0, "blogic %s:%s[%d]\n", __FILE__, __func__, __LINE__);
	if (nl80211_scan_dump(&nl_call_param) < 0)
		ret = false;
	syslog(0, "blogic %s:%s[%d]\n", __FILE__, __func__, __LINE__);
	LOGN("%s: scan dump returned %d, list %d", radio_cfg->if_name, ret, ds_dlist_is_empty(&scan_results->list));

	return true;
}


/******************************************************************************
 *  DEVICE definitions
 *****************************************************************************/

bool target_stats_device_temp_get(radio_entry_t *radio_cfg, dpp_device_temp_t *temp_entry)
{
	char hwmon_path[PATH_MAX];
	int32_t temperature;
	FILE *fp = NULL;

	if (phy_find_hwmon(target_map_ifname(radio_cfg->phy_name), hwmon_path)) {
		LOG(ERR, "%s: hwmon is missing", radio_cfg->phy_name);
		return false;
	}

	fp = fopen(hwmon_path, "r");
	if (!fp) {
		LOG(ERR, "%s: Failed to open temp input files", radio_cfg->phy_name);
		return false;
	}

	if (fscanf(fp,"%d",&temperature) == EOF) {
		LOG(ERR, "%s:Temperature reading failed", radio_cfg->phy_name);
		fclose(fp);
		return false;
	}

	LOGN("%s: temperature : %d", radio_cfg->phy_name, temperature);

	fclose(fp);
	temp_entry->type  = radio_cfg->type;
	temp_entry->value = temperature / 1000;

	return true;
}

bool target_stats_device_txchainmask_get(radio_entry_t *radio_cfg, dpp_device_txchainmask_t *txchainmask_entry)
{
	bool ret = true;
	txchainmask_entry->type  = radio_cfg->type;
	if (nl80211_get_tx_chainmask(target_map_ifname(radio_cfg->phy_name), &txchainmask_entry->value) < 0)
		ret = false;
	LOGN("%s: tx_chainmask %d", radio_cfg->phy_name, txchainmask_entry->value);

	return ret;
}

bool target_stats_device_fanrpm_get(uint32_t *fan_rpm)
{
	*fan_rpm = 0;
	return true;
}

/******************************************************************************
 *  CAPACITY definitions
 *****************************************************************************/

bool target_stats_capacity_enable(radio_entry_t *radio_cfg, bool enabled)
{
	return true;
}

bool target_stats_capacity_get(radio_entry_t *radio_cfg,
			       target_capacity_data_t *capacity_new)
{
	return true;
}

bool target_stats_capacity_convert(target_capacity_data_t *capacity_new,
				   target_capacity_data_t *capacity_old,
				   dpp_capacity_record_t *capacity_entry)
{
	return true;
}
static __attribute__((constructor)) void sm_init(void)
{
	nl80211_call_init();
}

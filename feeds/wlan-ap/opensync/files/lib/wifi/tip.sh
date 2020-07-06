#!/bin/sh

append HOOKS "wlan_ap"

_wifi_rename() {
	local old=$1
	local new=$2
	local vif=$3
	local path
	local band=$5

	config_get path "${old}" path

	[ "${path}" == "$4" ] || return 0

	uci -q rename wireless.${old}=${new}
	uci -q set wireless.${new}.freq_band=${band}

	uci -q rename wireless.default_${old}=${vif}
	uci -q set wireless.${vif}.device=${new}
	uci -q set wireless.${vif}.ifname=${vif}
	uci -q set wireless.${vif}.index=0
}

wifi_rename() {
	local radio=$1
	local vif=$2
	local path=$3
	local band=$4

	[ -z  "$(uci -q get wireless.${vif}.device)" ] || return 0
	config_foreach _wifi_rename wifi-device "${radio}" "${vif}" "${path}" "${band}"
}

run_wlan_ap() {
	config_load wireless
	case "$(board_name)" in
	linksys,ea8300)
		wifi_rename wifi0 home_ap_24 'platform/soc/a000000.wifi' '2.4G'
		wifi_rename wifi1 home_ap_l50 'platform/soc/a800000.wifi' '5GL'
		wifi_rename wifi2 home_ap_u50 'soc/40000000.pci/pci0000:00/0000:00:00.0/0000:01:00.0' '5GU'
		;;
	*)
		;;
	esac
	uci commit wireless
}

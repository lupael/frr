/*
 * SHARP - vty code
 * Copyright (C) Cumulus Networks, Inc.
 *               Donald Sharp
 *
 * This file is part of FRR.
 *
 * FRR is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * FRR is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <zebra.h>

#include "vty.h"
#include "command.h"
#include "prefix.h"
#include "nexthop.h"
#include "log.h"
#include "vrf.h"
#include "zclient.h"
#include "nexthop_group.h"
#include "link_state.h"

#include "sharpd/sharp_globals.h"
#include "sharpd/sharp_zebra.h"
#include "sharpd/sharp_nht.h"
#include "sharpd/sharp_vty.h"
#ifndef VTYSH_EXTRACT_PL
#include "sharpd/sharp_vty_clippy.c"
#endif

DEFPY(watch_nexthop_v6, watch_nexthop_v6_cmd,
      "sharp watch [vrf NAME$vrf_name] <nexthop$n X:X::X:X$nhop|import$import X:X::X:X/M$inhop>  [connected$connected]",
      "Sharp routing Protocol\n"
      "Watch for changes\n"
      "The vrf we would like to watch if non-default\n"
      "The NAME of the vrf\n"
      "Watch for nexthop changes\n"
      "The v6 nexthop to signal for watching\n"
      "Watch for import check changes\n"
      "The v6 prefix to signal for watching\n"
      "Should the route be connected\n")
{
	struct vrf *vrf;
	struct prefix p;
	bool type_import;

	if (!vrf_name)
		vrf_name = VRF_DEFAULT_NAME;
	vrf = vrf_lookup_by_name(vrf_name);
	if (!vrf) {
		vty_out(vty, "The vrf NAME specified: %s does not exist\n",
			vrf_name);
		return CMD_WARNING;
	}

	memset(&p, 0, sizeof(p));

	if (n) {
		type_import = false;
		p.prefixlen = 128;
		memcpy(&p.u.prefix6, &nhop, 16);
		p.family = AF_INET6;
	} else {
		type_import = true;
		p = *(const struct prefix *)inhop;
	}

	sharp_nh_tracker_get(&p);
	sharp_zebra_nexthop_watch(&p, vrf->vrf_id, type_import,
				  true, !!connected);

	return CMD_SUCCESS;
}

DEFPY(watch_nexthop_v4, watch_nexthop_v4_cmd,
      "sharp watch [vrf NAME$vrf_name] <nexthop$n A.B.C.D$nhop|import$import A.B.C.D/M$inhop> [connected$connected]",
      "Sharp routing Protocol\n"
      "Watch for changes\n"
      "The vrf we would like to watch if non-default\n"
      "The NAME of the vrf\n"
      "Watch for nexthop changes\n"
      "The v4 address to signal for watching\n"
      "Watch for import check changes\n"
      "The v4 prefix for import check to watch\n"
      "Should the route be connected\n")
{
	struct vrf *vrf;
	struct prefix p;
	bool type_import;

	if (!vrf_name)
		vrf_name = VRF_DEFAULT_NAME;
	vrf = vrf_lookup_by_name(vrf_name);
	if (!vrf) {
		vty_out(vty, "The vrf NAME specified: %s does not exist\n",
			vrf_name);
		return CMD_WARNING;
	}

	memset(&p, 0, sizeof(p));

	if (n) {
		type_import = false;
		p.prefixlen = 32;
		p.u.prefix4 = nhop;
		p.family = AF_INET;
	}
	else {
		type_import = true;
		p = *(const struct prefix *)inhop;
	}

	sharp_nh_tracker_get(&p);
	sharp_zebra_nexthop_watch(&p, vrf->vrf_id, type_import,
				  true, !!connected);

	return CMD_SUCCESS;
}

DEFPY(sharp_nht_data_dump,
      sharp_nht_data_dump_cmd,
      "sharp data nexthop",
      "Sharp routing Protocol\n"
      "Data about what is going on\n"
      "Nexthop information\n")
{
	sharp_nh_tracker_dump(vty);

	return CMD_SUCCESS;
}

DEFPY (install_routes_data_dump,
       install_routes_data_dump_cmd,
       "sharp data route",
       "Sharp routing Protocol\n"
       "Data about what is going on\n"
       "Route Install/Removal Information\n")
{
	struct timeval r;

	timersub(&sg.r.t_end, &sg.r.t_start, &r);
	vty_out(vty, "Prefix: %pFX Total: %u %u %u Time: %jd.%ld\n",
		&sg.r.orig_prefix, sg.r.total_routes, sg.r.installed_routes,
		sg.r.removed_routes, (intmax_t)r.tv_sec, (long)r.tv_usec);

	return CMD_SUCCESS;
}

DEFPY (install_routes,
       install_routes_cmd,
       "sharp install routes [vrf NAME$vrf_name]\
	  <A.B.C.D$start4|X:X::X:X$start6>\
	  <nexthop <A.B.C.D$nexthop4|X:X::X:X$nexthop6>|\
	   nexthop-group NHGNAME$nexthop_group>\
	  [backup$backup <A.B.C.D$backup_nexthop4|X:X::X:X$backup_nexthop6>] \
	  (1-1000000)$routes [instance (0-255)$instance] [repeat (2-1000)$rpt] [opaque WORD]",
       "Sharp routing Protocol\n"
       "install some routes\n"
       "Routes to install\n"
       "The vrf we would like to install into if non-default\n"
       "The NAME of the vrf\n"
       "v4 Address to start /32 generation at\n"
       "v6 Address to start /32 generation at\n"
       "Nexthop to use(Can be an IPv4 or IPv6 address)\n"
       "V4 Nexthop address to use\n"
       "V6 Nexthop address to use\n"
       "Nexthop-Group to use\n"
       "The Name of the nexthop-group\n"
       "Backup nexthop to use(Can be an IPv4 or IPv6 address)\n"
       "Backup V4 Nexthop address to use\n"
       "Backup V6 Nexthop address to use\n"
       "How many to create\n"
       "Instance to use\n"
       "Instance\n"
       "Should we repeat this command\n"
       "How many times to repeat this command\n"
       "What opaque data to send down\n"
       "The opaque data\n")
{
	struct vrf *vrf;
	struct prefix prefix;
	uint32_t rts;
	uint32_t nhgid = 0;

	sg.r.total_routes = routes;
	sg.r.installed_routes = 0;

	if (rpt >= 2)
		sg.r.repeat = rpt * 2;
	else
		sg.r.repeat = 0;

	memset(&prefix, 0, sizeof(prefix));
	memset(&sg.r.orig_prefix, 0, sizeof(sg.r.orig_prefix));
	memset(&sg.r.nhop, 0, sizeof(sg.r.nhop));
	memset(&sg.r.nhop_group, 0, sizeof(sg.r.nhop_group));
	memset(&sg.r.backup_nhop, 0, sizeof(sg.r.nhop));
	memset(&sg.r.backup_nhop_group, 0, sizeof(sg.r.nhop_group));

	if (start4.s_addr != INADDR_ANY) {
		prefix.family = AF_INET;
		prefix.prefixlen = 32;
		prefix.u.prefix4 = start4;
	} else {
		prefix.family = AF_INET6;
		prefix.prefixlen = 128;
		prefix.u.prefix6 = start6;
	}
	sg.r.orig_prefix = prefix;

	if (!vrf_name)
		vrf_name = VRF_DEFAULT_NAME;

	vrf = vrf_lookup_by_name(vrf_name);
	if (!vrf) {
		vty_out(vty, "The vrf NAME specified: %s does not exist\n",
			vrf_name);
		return CMD_WARNING;
	}

	/* Explicit backup not available with named nexthop-group */
	if (backup && nexthop_group) {
		vty_out(vty, "%% Invalid: cannot specify both nexthop-group and backup\n");
		return CMD_WARNING;
	}

	if (nexthop_group) {
		struct nexthop_group_cmd *nhgc = nhgc_find(nexthop_group);
		if (!nhgc) {
			vty_out(vty,
				"Specified Nexthop Group: %s does not exist\n",
				nexthop_group);
			return CMD_WARNING;
		}

		nhgid = sharp_nhgroup_get_id(nexthop_group);
		sg.r.nhgid = nhgid;
		sg.r.nhop_group.nexthop = nhgc->nhg.nexthop;

		/* Use group's backup nexthop info if present */
		if (nhgc->backup_list_name[0]) {
			struct nexthop_group_cmd *bnhgc =
				nhgc_find(nhgc->backup_list_name);

			if (!bnhgc) {
				vty_out(vty, "%% Backup group %s not found for group %s\n",
					nhgc->backup_list_name,
					nhgc->name);
				return CMD_WARNING;
			}

			sg.r.backup_nhop.vrf_id = vrf->vrf_id;
			sg.r.backup_nhop_group.nexthop = bnhgc->nhg.nexthop;
		}
	} else {
		if (nexthop4.s_addr != INADDR_ANY) {
			sg.r.nhop.gate.ipv4 = nexthop4;
			sg.r.nhop.type = NEXTHOP_TYPE_IPV4;
		} else {
			sg.r.nhop.gate.ipv6 = nexthop6;
			sg.r.nhop.type = NEXTHOP_TYPE_IPV6;
		}

		sg.r.nhop.vrf_id = vrf->vrf_id;
		sg.r.nhop_group.nexthop = &sg.r.nhop;
	}

	/* Use single backup nexthop if specified */
	if (backup) {
		/* Set flag and index in primary nexthop */
		SET_FLAG(sg.r.nhop.flags, NEXTHOP_FLAG_HAS_BACKUP);
		sg.r.nhop.backup_num = 1;
		sg.r.nhop.backup_idx[0] = 0;

		if (backup_nexthop4.s_addr != INADDR_ANY) {
			sg.r.backup_nhop.gate.ipv4 = backup_nexthop4;
			sg.r.backup_nhop.type = NEXTHOP_TYPE_IPV4;
		} else {
			sg.r.backup_nhop.gate.ipv6 = backup_nexthop6;
			sg.r.backup_nhop.type = NEXTHOP_TYPE_IPV6;
		}

		sg.r.backup_nhop.vrf_id = vrf->vrf_id;
		sg.r.backup_nhop_group.nexthop = &sg.r.backup_nhop;
	}

	if (opaque)
		strlcpy(sg.r.opaque, opaque, ZAPI_MESSAGE_OPAQUE_LENGTH);
	else
		sg.r.opaque[0] = '\0';

	sg.r.inst = instance;
	sg.r.vrf_id = vrf->vrf_id;
	rts = routes;
	sharp_install_routes_helper(&prefix, sg.r.vrf_id, sg.r.inst, nhgid,
				    &sg.r.nhop_group, &sg.r.backup_nhop_group,
				    rts, sg.r.opaque);

	return CMD_SUCCESS;
}

DEFPY(vrf_label, vrf_label_cmd,
      "sharp label <ip$ipv4|ipv6$ipv6> vrf NAME$vrf_name label (0-100000)$label",
      "Sharp Routing Protocol\n"
      "Give a vrf a label\n"
      "Pop and forward for IPv4\n"
      "Pop and forward for IPv6\n"
      VRF_CMD_HELP_STR
      "The label to use, 0 specifies remove the label installed from previous\n"
      "Specified range to use\n")
{
	struct vrf *vrf;
	afi_t afi = (ipv4) ? AFI_IP : AFI_IP6;

	if (strcmp(vrf_name, "default") == 0)
		vrf = vrf_lookup_by_id(VRF_DEFAULT);
	else
		vrf = vrf_lookup_by_name(vrf_name);

	if (!vrf) {
		vty_out(vty, "Unable to find vrf you silly head");
		return CMD_WARNING_CONFIG_FAILED;
	}

	if (label == 0)
		label = MPLS_LABEL_NONE;

	vrf_label_add(vrf->vrf_id, afi, label);
	return CMD_SUCCESS;
}

DEFPY (remove_routes,
       remove_routes_cmd,
       "sharp remove routes [vrf NAME$vrf_name] <A.B.C.D$start4|X:X::X:X$start6> (1-1000000)$routes [instance (0-255)$instance]",
       "Sharp Routing Protocol\n"
       "Remove some routes\n"
       "Routes to remove\n"
       "The vrf we would like to remove from if non-default\n"
       "The NAME of the vrf\n"
       "v4 Starting spot\n"
       "v6 Starting spot\n"
       "Routes to uninstall\n"
       "instance to use\n"
       "Value of instance\n")
{
	struct vrf *vrf;
	struct prefix prefix;

	sg.r.total_routes = routes;
	sg.r.removed_routes = 0;
	uint32_t rts;

	memset(&prefix, 0, sizeof(prefix));

	if (start4.s_addr != INADDR_ANY) {
		prefix.family = AF_INET;
		prefix.prefixlen = 32;
		prefix.u.prefix4 = start4;
	} else {
		prefix.family = AF_INET6;
		prefix.prefixlen = 128;
		prefix.u.prefix6 = start6;
	}

	vrf = vrf_lookup_by_name(vrf_name ? vrf_name : VRF_DEFAULT_NAME);
	if (!vrf) {
		vty_out(vty, "The vrf NAME specified: %s does not exist\n",
			vrf_name ? vrf_name : VRF_DEFAULT_NAME);
		return CMD_WARNING;
	}

	sg.r.inst = instance;
	sg.r.vrf_id = vrf->vrf_id;
	rts = routes;
	sharp_remove_routes_helper(&prefix, sg.r.vrf_id,
				   sg.r.inst, rts);

	return CMD_SUCCESS;
}

DEFUN_NOSH (show_debugging_sharpd,
	    show_debugging_sharpd_cmd,
	    "show debugging [sharp]",
	    SHOW_STR
	    DEBUG_STR
	    "Sharp Information\n")
{
	vty_out(vty, "Sharp debugging status:\n");

	return CMD_SUCCESS;
}

DEFPY (sharp_lsp_prefix_v4, sharp_lsp_prefix_v4_cmd,
       "sharp lsp [update]$update (0-100000)$inlabel\
        nexthop-group NHGNAME$nhgname\
        [prefix A.B.C.D/M$pfx\
       " FRR_IP_REDIST_STR_ZEBRA "$type_str [instance (0-255)$instance]]",
       "Sharp Routing Protocol\n"
       "Add an LSP\n"
       "Update an LSP\n"
       "The ingress label to use\n"
       "Use nexthops from a nexthop-group\n"
       "The nexthop-group name\n"
       "Label a prefix\n"
       "The v4 prefix to label\n"
       FRR_IP_REDIST_HELP_STR_ZEBRA
       "Instance to use\n"
       "Instance\n")
{
	struct nexthop_group_cmd *nhgc = NULL;
	struct nexthop_group_cmd *backup_nhgc = NULL;
	struct nexthop_group *backup_nhg = NULL;
	struct prefix p = {};
	int type = 0;
	bool update_p;

	update_p = (update != NULL);

	/* We're offered a v4 prefix */
	if (pfx->family > 0 && type_str) {
		p.family = pfx->family;
		p.prefixlen = pfx->prefixlen;
		p.u.prefix4 = pfx->prefix;

		type = proto_redistnum(AFI_IP, type_str);
		if (type < 0) {
			vty_out(vty, "%%  Unknown route type '%s'\n", type_str);
			return CMD_WARNING;
		}
	} else if (pfx->family > 0 || type_str) {
		vty_out(vty, "%%  Must supply both prefix and type\n");
		return CMD_WARNING;
	}

	nhgc = nhgc_find(nhgname);
	if (!nhgc) {
		vty_out(vty, "%%  Nexthop-group '%s' does not exist\n",
			nhgname);
		return CMD_WARNING;
	}

	if (nhgc->nhg.nexthop == NULL) {
		vty_out(vty, "%%  Nexthop-group '%s' is empty\n", nhgname);
		return CMD_WARNING;
	}

	/* Use group's backup nexthop info if present */
	if (nhgc->backup_list_name[0]) {
		backup_nhgc = nhgc_find(nhgc->backup_list_name);

		if (!backup_nhgc) {
			vty_out(vty,
				"%% Backup group %s not found for group %s\n",
				nhgc->backup_list_name,
				nhgname);
			return CMD_WARNING;
		}
		backup_nhg = &(backup_nhgc->nhg);
	}

	if (sharp_install_lsps_helper(true /*install*/, update_p,
				      pfx->family > 0 ? &p : NULL,
				      type, instance, inlabel,
				      &(nhgc->nhg), backup_nhg) == 0)
		return CMD_SUCCESS;
	else {
		vty_out(vty, "%% LSP install failed!\n");
		return CMD_WARNING;
	}
}

DEFPY(sharp_remove_lsp_prefix_v4, sharp_remove_lsp_prefix_v4_cmd,
      "sharp remove lsp \
        (0-100000)$inlabel\
        [nexthop-group NHGNAME$nhgname] \
        [prefix A.B.C.D/M$pfx\
       " FRR_IP_REDIST_STR_ZEBRA "$type_str [instance (0-255)$instance]]",
      "Sharp Routing Protocol\n"
      "Remove data\n"
      "Remove an LSP\n"
      "The ingress label\n"
      "Use nexthops from a nexthop-group\n"
      "The nexthop-group name\n"
      "Specify a v4 prefix\n"
      "The v4 prefix to label\n"
      FRR_IP_REDIST_HELP_STR_ZEBRA
      "Routing instance\n"
      "Instance to use\n")
{
	struct nexthop_group_cmd *nhgc = NULL;
	struct prefix p = {};
	int type = 0;
	struct nexthop_group *nhg = NULL;

	/* We're offered a v4 prefix */
	if (pfx->family > 0 && type_str) {
		p.family = pfx->family;
		p.prefixlen = pfx->prefixlen;
		p.u.prefix4 = pfx->prefix;

		type = proto_redistnum(AFI_IP, type_str);
		if (type < 0) {
			vty_out(vty, "%%  Unknown route type '%s'\n", type_str);
			return CMD_WARNING;
		}
	} else if (pfx->family > 0 || type_str) {
		vty_out(vty, "%%  Must supply both prefix and type\n");
		return CMD_WARNING;
	}

	if (nhgname) {
		nhgc = nhgc_find(nhgname);
		if (!nhgc) {
			vty_out(vty, "%%  Nexthop-group '%s' does not exist\n",
				nhgname);
			return CMD_WARNING;
		}

		if (nhgc->nhg.nexthop == NULL) {
			vty_out(vty, "%%  Nexthop-group '%s' is empty\n",
				nhgname);
			return CMD_WARNING;
		}
		nhg = &(nhgc->nhg);
	}

	if (sharp_install_lsps_helper(false /*!install*/, false,
				      pfx->family > 0 ? &p : NULL,
				      type, instance, inlabel, nhg, NULL) == 0)
		return CMD_SUCCESS;
	else {
		vty_out(vty, "%% LSP remove failed!\n");
		return CMD_WARNING;
	}
}

DEFPY (logpump,
       logpump_cmd,
       "sharp logpump duration (1-60) frequency (1-1000000) burst (1-1000)",
       "Sharp Routing Protocol\n"
       "Generate bulk log messages for testing\n"
       "Duration of run (s)\n"
       "Duration of run (s)\n"
       "Frequency of bursts (s^-1)\n"
       "Frequency of bursts (s^-1)\n"
       "Number of log messages per each burst\n"
       "Number of log messages per each burst\n")
{
	sharp_logpump_run(vty, duration, frequency, burst);
	return CMD_SUCCESS;
}

DEFPY (create_session,
       create_session_cmd,
       "sharp create session (1-1024)",
       "Sharp Routing Protocol\n"
       "Create data\n"
       "Create a test session\n"
       "Session ID\n")
{
	if (sharp_zclient_create(session) != 0) {
		vty_out(vty, "%% Client session error\n");
		return CMD_WARNING;
	}

	return CMD_SUCCESS;
}

DEFPY (remove_session,
       remove_session_cmd,
       "sharp remove session (1-1024)",
       "Sharp Routing Protocol\n"
       "Remove data\n"
       "Remove a test session\n"
       "Session ID\n")
{
	sharp_zclient_delete(session);
	return CMD_SUCCESS;
}

DEFPY (send_opaque,
       send_opaque_cmd,
       "sharp send opaque type (1-255) (1-1000)$count",
       SHARP_STR
       "Send messages for testing\n"
       "Send opaque messages\n"
       "Type code to send\n"
       "Type code to send\n"
       "Number of messages to send\n")
{
	sharp_opaque_send(type, 0, 0, 0, count);
	return CMD_SUCCESS;
}

DEFPY (send_opaque_unicast,
       send_opaque_unicast_cmd,
       "sharp send opaque unicast type (1-255) \
       " FRR_IP_REDIST_STR_ZEBRA "$proto_str \
        [{instance (0-1000) | session (1-1000)}] (1-1000)$count",
       SHARP_STR
       "Send messages for testing\n"
       "Send opaque messages\n"
       "Send unicast messages\n"
       "Type code to send\n"
       "Type code to send\n"
       FRR_IP_REDIST_HELP_STR_ZEBRA
       "Daemon instance\n"
       "Daemon instance\n"
       "Session ID\n"
       "Session ID\n"
       "Number of messages to send\n")
{
	uint32_t proto;

	proto = proto_redistnum(AFI_IP, proto_str);

	sharp_opaque_send(type, proto, instance, session, count);

	return CMD_SUCCESS;
}

DEFPY (send_opaque_reg,
       send_opaque_reg_cmd,
       "sharp send opaque <reg$reg | unreg> \
       " FRR_IP_REDIST_STR_ZEBRA "$proto_str \
        [{instance (0-1000) | session (1-1000)}] type (1-1000)",
       SHARP_STR
       "Send messages for testing\n"
       "Send opaque messages\n"
       "Send opaque registration\n"
       "Send opaque unregistration\n"
       FRR_IP_REDIST_HELP_STR_ZEBRA
       "Daemon instance\n"
       "Daemon instance\n"
       "Session ID\n"
       "Session ID\n"
       "Opaque sub-type code\n"
       "Opaque sub-type code\n")
{
	int proto;

	proto = proto_redistnum(AFI_IP, proto_str);

	sharp_opaque_reg_send((reg != NULL), proto, instance, session, type);
	return CMD_SUCCESS;
}

DEFPY (neigh_discover,
       neigh_discover_cmd,
       "sharp neigh discover [vrf NAME$vrf_name] <A.B.C.D$dst4|X:X::X:X$dst6> IFNAME$ifname",
       SHARP_STR
       "Discover neighbours\n"
       "Send an ARP/NDP request\n"
       VRF_CMD_HELP_STR
       "v4 Destination address\n"
       "v6 Destination address\n"
       "Interface name\n")
{
	struct vrf *vrf;
	struct interface *ifp;
	struct prefix prefix;

	memset(&prefix, 0, sizeof(prefix));

	if (dst4.s_addr != INADDR_ANY) {
		prefix.family = AF_INET;
		prefix.prefixlen = 32;
		prefix.u.prefix4 = dst4;
	} else {
		prefix.family = AF_INET6;
		prefix.prefixlen = 128;
		prefix.u.prefix6 = dst6;
	}

	vrf = vrf_lookup_by_name(vrf_name ? vrf_name : VRF_DEFAULT_NAME);
	if (!vrf) {
		vty_out(vty, "The vrf NAME specified: %s does not exist\n",
			vrf_name ? vrf_name : VRF_DEFAULT_NAME);
		return CMD_WARNING;
	}

	ifp = if_lookup_by_name_vrf(ifname, vrf);
	if (ifp == NULL) {
		vty_out(vty, "%% Can't find interface %s\n", ifname);
		return CMD_WARNING;
	}

	sharp_zebra_send_arp(ifp, &prefix);

	return CMD_SUCCESS;
}

DEFPY (import_te,
       import_te_cmd,
       "sharp import-te",
       SHARP_STR
       "Import Traffic Engineering\n")
{
	sg.ted = ls_ted_new(1, "Sharp", 0);
	sharp_zebra_register_te();

	return CMD_SUCCESS;
}

DEFUN (show_sharp_ted,
       show_sharp_ted_cmd,
       "show sharp ted [<vertex [A.B.C.D]|edge [A.B.C.D]|subnet [A.B.C.D/M]>] [verbose|json]",
       SHOW_STR
       SHARP_STR
       "Traffic Engineering Database\n"
       "MPLS-TE Vertex\n"
       "MPLS-TE router ID (as an IP address)\n"
       "MPLS-TE Edge\n"
       "MPLS-TE Edge ID (as an IP address)\n"
       "MPLS-TE Subnet\n"
       "MPLS-TE Subnet ID (as an IP prefix)\n"
       "Verbose output\n"
       JSON_STR)
{
	int idx = 0;
	struct in_addr ip_addr;
	struct prefix pref;
	struct ls_vertex *vertex;
	struct ls_edge *edge;
	struct ls_subnet *subnet;
	uint64_t key;
	bool verbose = false;
	bool uj = use_json(argc, argv);
	json_object *json = NULL;

	if (sg.ted == NULL) {
		vty_out(vty, "MPLS-TE import is not enabled\n");
		return CMD_WARNING;
	}

	if (uj)
		json = json_object_new_object();

	if (argv[argc - 1]->arg && strmatch(argv[argc - 1]->text, "verbose"))
		verbose = true;

	if (argv_find(argv, argc, "vertex", &idx)) {
		/* Show Vertex */
		if (argv_find(argv, argc, "A.B.C.D", &idx)) {
			if (!inet_aton(argv[idx + 1]->arg, &ip_addr)) {
				vty_out(vty,
					"Specified Router ID %s is invalid\n",
					argv[idx + 1]->arg);
				return CMD_WARNING_CONFIG_FAILED;
			}
			/* Get the Vertex from the Link State Database */
			key = ((uint64_t)ip_addr.s_addr) & 0xffffffff;
			vertex = ls_find_vertex_by_key(sg.ted, key);
			if (!vertex) {
				vty_out(vty, "No vertex found for ID %pI4\n",
					&ip_addr);
				return CMD_WARNING;
			}
		} else
			vertex = NULL;

		if (vertex)
			ls_show_vertex(vertex, vty, json, verbose);
		else
			ls_show_vertices(sg.ted, vty, json, verbose);

	} else if (argv_find(argv, argc, "edge", &idx)) {
		/* Show Edge */
		if (argv_find(argv, argc, "A.B.C.D", &idx)) {
			if (!inet_aton(argv[idx]->arg, &ip_addr)) {
				vty_out(vty,
					"Specified Edge ID %s is invalid\n",
					argv[idx]->arg);
				return CMD_WARNING_CONFIG_FAILED;
			}
			/* Get the Edge from the Link State Database */
			key = ((uint64_t)ip_addr.s_addr) & 0xffffffff;
			edge = ls_find_edge_by_key(sg.ted, key);
			if (!edge) {
				vty_out(vty, "No edge found for ID %pI4\n",
					&ip_addr);
				return CMD_WARNING;
			}
		} else
			edge = NULL;

		if (edge)
			ls_show_edge(edge, vty, json, verbose);
		else
			ls_show_edges(sg.ted, vty, json, verbose);

	} else if (argv_find(argv, argc, "subnet", &idx)) {
		/* Show Subnet */
		if (argv_find(argv, argc, "A.B.C.D/M", &idx)) {
			if (!str2prefix(argv[idx]->arg, &pref)) {
				vty_out(vty, "Invalid prefix format %s\n",
					argv[idx]->arg);
				return CMD_WARNING_CONFIG_FAILED;
			}
			/* Get the Subnet from the Link State Database */
			subnet = ls_find_subnet(sg.ted, pref);
			if (!subnet) {
				vty_out(vty, "No subnet found for ID %pFX\n",
					&pref);
				return CMD_WARNING;
			}
		} else
			subnet = NULL;

		if (subnet)
			ls_show_subnet(subnet, vty, json, verbose);
		else
			ls_show_subnets(sg.ted, vty, json, verbose);

	} else {
		/* Show the complete TED */
		ls_show_ted(sg.ted, vty, json, verbose);
	}

	if (uj) {
		vty_out(vty, "%s\n",
			json_object_to_json_string_ext(
				json, JSON_C_TO_STRING_PRETTY));
		json_object_free(json);
	}
	return CMD_SUCCESS;
}

void sharp_vty_init(void)
{
	install_element(ENABLE_NODE, &install_routes_data_dump_cmd);
	install_element(ENABLE_NODE, &install_routes_cmd);
	install_element(ENABLE_NODE, &remove_routes_cmd);
	install_element(ENABLE_NODE, &vrf_label_cmd);
	install_element(ENABLE_NODE, &sharp_nht_data_dump_cmd);
	install_element(ENABLE_NODE, &watch_nexthop_v6_cmd);
	install_element(ENABLE_NODE, &watch_nexthop_v4_cmd);
	install_element(ENABLE_NODE, &sharp_lsp_prefix_v4_cmd);
	install_element(ENABLE_NODE, &sharp_remove_lsp_prefix_v4_cmd);
	install_element(ENABLE_NODE, &logpump_cmd);
	install_element(ENABLE_NODE, &create_session_cmd);
	install_element(ENABLE_NODE, &remove_session_cmd);
	install_element(ENABLE_NODE, &send_opaque_cmd);
	install_element(ENABLE_NODE, &send_opaque_unicast_cmd);
	install_element(ENABLE_NODE, &send_opaque_reg_cmd);
	install_element(ENABLE_NODE, &neigh_discover_cmd);
	install_element(ENABLE_NODE, &import_te_cmd);

	install_element(ENABLE_NODE, &show_debugging_sharpd_cmd);
	install_element(ENABLE_NODE, &show_sharp_ted_cmd);

	return;
}

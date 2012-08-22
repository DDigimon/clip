/**
 * @file   system_info.c
 * @brief  system_info probe
 * @author "Tomas Heinrich" <theinric@redhat.com>
 */

/*
 * Copyright 2009 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      "Tomas Heinrich" <theinric@redhat.com>
 */

/*
 * system_info probe:
 *
 *  (for internal use)
 *
 *  system_info_object
 *
 *  system_info_item
 *    attrs
 *      id
 *      status_enum status
 *    os_name
 *    os_version
 *    architecture
 *    primary_host_name
 *    //interfaces
 *    interface [0..*]
 *      attrs
 *        interface_name
 *        ip_address
 *        mac_address
 *    any [0..*]
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <seap.h>
#include <probe-api.h>

#include <sys/utsname.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(__linux__)
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <string.h>
#include <net/if.h>
#include <arpa/inet.h>

static int fd=-1;

static char *get_mac(const struct ifaddrs *ifa)
{
       struct ifreq ifr;
       unsigned char mac[6];
       static char mac_buf[20];

       memset(&ifr, 0, sizeof(struct ifreq));
       strcpy(ifr.ifr_name, ifa->ifa_name);
       if (ioctl(fd, SIOCGIFHWADDR, &ifr) >= 0) {
               memcpy(mac, ifr.ifr_hwaddr.sa_data, sizeof(mac));
               snprintf(mac_buf, sizeof(mac_buf),
                       "%02X:%02X:%02X:%02X:%02X:%02X",
                       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
       } else
               mac_buf[0] = 0;

       return mac_buf;
}

static int get_ifs(SEXP_t *item)
{
       struct ifaddrs *ifaddr, *ifa;
       int family, rc=1;
       char host[NI_MAXHOST], *mac;
       SEXP_t *attrs;
       SEXP_t *r0, *r1, *r2;

       if (getifaddrs(&ifaddr) == -1)
               return rc;

       fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
       if (fd < 0)
               goto leave1;

        /* Walk through linked list, maintaining head pointer so we
          can free list later */
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr) {
                        family = ifa->ifa_addr->sa_family;
                        if (family != AF_INET && family != AF_INET6)
                                continue;
                } else
                        continue;

                mac = get_mac(ifa);
		if (family == AF_INET) {
			rc = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
				host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			if (rc) {
				rc = 1;
				goto leave2;
			}
		}
		else {
			struct sockaddr_in6 *sin6p;
			sin6p = (struct sockaddr_in6 *) ifa->ifa_addr;
			if (! inet_ntop(family, (const void *)&sin6p->sin6_addr, host, NI_MAXHOST)) {
				rc = 1;
				goto leave2;
			}
		}


	        attrs = probe_attr_creat("name",
                                 r0 = SEXP_string_newf("%s", ifa->ifa_name),
                                 "ip_address",
                                 r1 = SEXP_string_newf("%s", host),
                                 "mac_address",
                                 r2 = SEXP_string_newf("%s", mac),
                                 NULL);
	        probe_item_ent_add(item, "interface", attrs, NULL);
        	SEXP_vfree(attrs, r0, r1, r2, NULL);
	}
leave2:
        close(fd);
leave1:
        freeifaddrs(ifaddr);
        return rc;
}

#else

static int get_ifs(SEXP_t *item)
{
        /* TODO */

        SEXP_t *attrs, *r0, *r1, *r2;

        attrs = probe_attr_creat("name",
                                 r0 = SEXP_string_newf("dummy0"),
                                 "ip_address",
                                 r1 = SEXP_string_newf("127.0.0.1"),
                                 "mac_address",
                                 r2 = SEXP_string_newf("aa:bb:cc:dd:ee:ff"),
                                 NULL);

        probe_item_ent_add(item, "interface", attrs, NULL);
        SEXP_vfree (attrs, r0, r1, r2, NULL);

        return 0;
}
#endif

int probe_main(probe_ctx *ctx, void *arg)
{
	SEXP_t *item;
        char *os_name, *os_version, *architecture, *hname;
        struct utsname sname;

        (void)arg;

        if (uname(&sname) == -1) {
		return PROBE_EUNKNOWN;
        }

        os_name = sname.sysname;
        os_version = sname.version;
        architecture = sname.machine;
	hname = sname.nodename;

        item = probe_item_create(OVAL_SUBTYPE_SYSINFO, NULL,
                                 "os_name",           OVAL_DATATYPE_STRING, os_name,
                                 "os_version",        OVAL_DATATYPE_STRING, os_version,
                                 "os_architecture",   OVAL_DATATYPE_STRING, architecture,
                                 "primary_host_name", OVAL_DATATYPE_STRING, hname,
                                 NULL);

        if (get_ifs(item)) {
		SEXP_free(item);
		return PROBE_EUNKNOWN;
	}

        probe_item_collect(ctx, item);

	return (0);
}

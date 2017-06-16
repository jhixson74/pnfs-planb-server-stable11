/*-
 * Copyright (c) 2017 Rick Macklem
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <getopt.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/extattr.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fs/nfs/nfsrvstate.h>

static void usage(void);

static struct option longopts[] = {
	{ "quiet",	no_argument,		NULL,	'q'	},
	{ "ds",		required_argument,	NULL,	's'	},
	{ "zerofh",	no_argument,		NULL,	'z'	},
	{ NULL,		0,			NULL,	0	}
};

/*
 * This program displays the location information of a data storage file
 * for a given file on a MetaData Server (MDS) in a pNFS service.  This program
 * must be run on the MDS and the file argument must be a file in a local
 * file system that has been exported for the pNFS service.
 */
int
main(int argc, char *argv[])
{
	struct addrinfo *res, *ad;
	struct sockaddr_in *sin, *adsin;
	struct sockaddr_in6 *sin6, *adsin6;
	char hostn[NI_MAXHOST + 1];
	struct pnfsdsfile dsfile;
	int ch, quiet, zerofh;

	zerofh = 0;
	quiet = 0;
	res = NULL;
	while ((ch = getopt_long(argc, argv, "qs:z", longopts, NULL)) != -1) {
		switch (ch) {
		case 'q':
			quiet = 1;
			break;
		case 's':
			/* Translate the server name to an IP address. */
			if (getaddrinfo(optarg, NULL, NULL, &res) != 0)
				errx(1, "Can't get IP# for %s\n", optarg);
			break;
		case 'z':
			zerofh = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	if (argc != 1)
		usage();
	argv += optind;

	/*
	 * The host address and directory where the data storage file is
	 * located is in the extended attribute "pnfsd.dsfile".
	 */
	if (extattr_get_file(*argv, EXTATTR_NAMESPACE_SYSTEM, "pnfsd.dsfile",
	    &dsfile, sizeof(dsfile)) != sizeof(dsfile))
		err(1, "Can't get extattr pnfsd.dsfile\n");

	/* Do the zerofh option.  You must be root to use this option. */
	if (zerofh != 0) {
		if (geteuid() != 0)
			errx(1, "Must be root/su to zerofh\n");

		/*
		 * Do it for the server specified by -s/--ds or all servers,
		 * if -s/--ds was not sepcified.
		 */
		sin = &dsfile.dsf_sin;
		sin6 = &dsfile.dsf_sin6;
		ad = res;
		while (ad != NULL) {
			adsin = (struct sockaddr_in *)ad->ai_addr;
			adsin6 = (struct sockaddr_in6 *)ad->ai_addr;
			if (adsin->sin_family == sin->sin_family) {
				if (sin->sin_family == AF_INET &&
				    sin->sin_addr.s_addr ==
				    adsin->sin_addr.s_addr)
					break;
				else if (sin->sin_family == AF_INET6 &&
				    IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
				    &adsin6->sin6_addr))
					break;
			}
			ad = ad->ai_next;
		}
		if (res == NULL || ad != NULL) {
			memset(&dsfile.dsf_fh, 0, sizeof(dsfile.dsf_fh));
			if (extattr_set_file(*argv, EXTATTR_NAMESPACE_SYSTEM,
			    "pnfsd.dsfile", &dsfile, sizeof(dsfile)) !=
			    sizeof(dsfile))
				err(1, "Can't set pnfsd.dsfile\n");
		}
	}

	if (quiet != 0)
		exit(0);

	/* Translate the IP address to a hostname. */
	if (getnameinfo((struct sockaddr *)&dsfile.dsf_sin,
	    dsfile.dsf_sin.sin_len, hostn, sizeof(hostn), NULL, 0, 0) < 0)
		err(1, "Can't get hostname\n");

	printf("%s\tds%d/%s\n", hostn, dsfile.dsf_dir, dsfile.dsf_filename);
}

static void
usage(void)
{

	fprintf(stderr, "pnfsdsfile [-q/--quiet] [-z/--zerofh] "
	    "[-s/--ds <dshostname>] <filename>\n");
	exit(1);
}


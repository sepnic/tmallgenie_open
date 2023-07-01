/*
 *  LibNoPoll: A websocket library
 *  Copyright (C) 2020 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is LGPL software: you are welcome to develop
 *  proprietary applications using this library without any royalty or
 *  fee but returning back any change, improvement or addition in the
 *  form of source code, project image, documentation patches, etc.
 *
 *  For commercial support on build Websocket enabled solutions
 *  contact us:
 *
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         Av. Juan Carlos I, Nº13, 2ºC
 *         Alcalá de Henares 28806 Madrid
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/nopoll
 */
// Copyright (c) 2021-2022 Qinglong<sysu.zqlong@gmail.com>
// History:
//  1. Add mbedtls support, you should define 'NOPOLL_HAVE_MBEDTLS_ENABLED'
//     if using mbedtls instead of openssl
//  2. Add macro 'NOPOLL_HAVE_IPV6_ENABLED', define it if ipv6 supported,
//     otherwise remove it
//  3. Add sysutils support, because sysutils has osal layer, we don't need
//     to care about platform dependent
//  4. Add lwip support
#ifndef __NOPOLL_CONN_OPTS_H__
#define __NOPOLL_CONN_OPTS_H__

#include "nopoll_namespace.h"
#include "nopoll.h"

BEGIN_C_DECLS

noPollConnOpts * nopoll_conn_opts_new (void);

#if defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
nopoll_bool  nopoll_conn_opts_set_ssl_certs    (noPollConnOpts * opts,
						const char     * certificate,
						int              certificate_size,
						const char     * private_key,
						int              private_key_size,
						const char     * chain_certificate,
						int              chain_certificate_size,
						const char     * ca_certificate,
						int              ca_certificate_size);
#else
void nopoll_conn_opts_set_ssl_protocol (noPollConnOpts * opts, noPollSslProtocol ssl_protocol);

nopoll_bool  nopoll_conn_opts_set_ssl_certs    (noPollConnOpts * opts,
						const char     * client_certificate,
						const char     * private_key,
						const char     * chain_certificate,
						const char     * ca_certificate);
#endif

void        nopoll_conn_opts_ssl_peer_verify (noPollConnOpts * opts, nopoll_bool verify);

void        nopoll_conn_opts_set_cookie (noPollConnOpts * opts, const char * cookie_content);

void        nopoll_conn_opts_skip_origin_check (noPollConnOpts * opts, nopoll_bool skip_check);

void        nopoll_conn_opts_add_origin_header (noPollConnOpts * opts, nopoll_bool add);

nopoll_bool nopoll_conn_opts_ref (noPollConnOpts * opts);

void        nopoll_conn_opts_unref (noPollConnOpts * opts);

void nopoll_conn_opts_set_reuse        (noPollConnOpts * opts, nopoll_bool reuse);

void nopoll_conn_opts_set_interface    (noPollConnOpts * opts, const char * _interface);

void nopoll_conn_opts_set_extra_headers (noPollConnOpts * opts, const char * extra_headers);

void nopoll_conn_opts_free (noPollConnOpts * opts);

/** internal API **/
void __nopoll_conn_opts_release_if_needed (noPollConnOpts * options);

END_C_DECLS

#endif

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
#ifndef __NOPOLL_CTX_H__
#define __NOPOLL_CTX_H__

#include "nopoll_namespace.h"
#include "nopoll.h"

BEGIN_C_DECLS

noPollCtx    * nopoll_ctx_new (void);

nopoll_bool    nopoll_ctx_ref (noPollCtx * ctx);

void           nopoll_ctx_unref (noPollCtx * ctx);

int            nopoll_ctx_ref_count (noPollCtx * ctx);

nopoll_bool    nopoll_ctx_register_conn (noPollCtx  * ctx,
					 noPollConn * conn);

void           nopoll_ctx_unregister_conn (noPollCtx  * ctx,
					   noPollConn * conn);

int            nopoll_ctx_conns (noPollCtx * ctx);

#if defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
nopoll_bool    nopoll_ctx_set_certificate (noPollCtx  * ctx,
					    const char * serverName,
					    const char * certificateFile,
					    int          certificateFile_size,
					    const char * privateKey,
					    int          privateKey_size,
					    const char * optionalChainFile,
					    int          optionalChainFile_size);

nopoll_bool    nopoll_ctx_find_certificate (noPollCtx   * ctx,
					    const char  * serverName,
					    const char ** certificateFile,
					    int         * certificateFile_size,
					    const char ** privateKey,
					    int         * privateKey_size,
					    const char ** optionalChainFile,
					    int         * optionalChainFile_size);
#else
nopoll_bool    nopoll_ctx_set_certificate (noPollCtx  * ctx,
					   const char * serverName,
					   const char * certificateFile,
					   const char * privateKey,
					   const char * optionalChainFile);

nopoll_bool    nopoll_ctx_find_certificate (noPollCtx   * ctx,
					    const char  * serverName,
					    const char ** certificateFile,
					    const char ** privateKey,
					    const char ** optionalChainFile);
#endif

void           nopoll_ctx_set_on_accept (noPollCtx           * ctx,
					 noPollActionHandler   on_accept,
					 noPollPtr             user_data);

void           nopoll_ctx_set_on_open (noPollCtx            * ctx,
				       noPollActionHandler    on_open,
				       noPollPtr              user_data);

void           nopoll_ctx_set_on_ready (noPollCtx          * ctx,
					noPollActionHandler  on_ready,
					noPollPtr            user_data);

void           nopoll_ctx_set_on_msg    (noPollCtx              * ctx,
					 noPollOnMessageHandler   on_msg,
					 noPollPtr                user_data);

#if !defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
void           nopoll_ctx_set_ssl_context_creator (noPollCtx                * ctx,
					      noPollSslContextCreator    context_creator,
					      noPollPtr                  user_data);

void           nopoll_ctx_set_post_ssl_check (noPollCtx          * ctx,
					      noPollSslPostCheck   post_ssl_check,
					      noPollPtr            user_data);
#endif

noPollConn   * nopoll_ctx_foreach_conn (noPollCtx * ctx, noPollForeachConn foreach, noPollPtr user_data);

void           nopoll_ctx_set_protocol_version (noPollCtx * ctx, int version);

void           nopoll_ctx_free (noPollCtx * ctx);

END_C_DECLS

#endif

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
#ifndef __NOPOLL_PRIVATE_H__
#define __NOPOLL_PRIVATE_H__

#include "nopoll_namespace.h"

#if defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
#include <mbedtls/debug.h>
#include <mbedtls/net.h>
#include <mbedtls/ssl.h>
#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>

#ifndef EVP_MAX_MD_SIZE
#define EVP_MAX_MD_SIZE	20
#endif

#else
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/x509v3.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>
#endif // defined(NOPOLL_HAVE_MBEDTLS_ENABLED)

#include "nopoll_handlers.h"

typedef struct _noPollCertificate {

	char * serverName;
	char * certificateFile;
	char * privateKey;
	char * optionalChainFile;

#if defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
	int    certificateFile_size;
	int    privateKey_size;
	int    optionalChainFile_size;
#endif
} noPollCertificate;

struct _noPollCtx {
	/**
	 * @internal Controls logs output..
	 */
	/* context reference counting */
	int             refs;

	/* console log */
	nopoll_bool     not_executed;
	nopoll_bool     debug_enabled;

	/* colored log */
	nopoll_bool     not_executed_color;
	nopoll_bool     debug_color_enabled;

	nopoll_bool     keep_looping;

	/**
	 * @internal noPollConn connection timeout.
	 */
	long        conn_connect_std_timeout;

	/**
	 * @internal Default listener connection backlog
	 */
	int         backlog;

	/**
	 * @internal Currently selected io engine on this context.
	 */
	noPollIoEngine * io_engine;

	/**
	 * @internal Connection array list and its length.
	 */
        int               conn_id;
	noPollConn     ** conn_list;
	int               conn_length;
	/**
	 * @internal Number of connections registered on this context.
	 */
	int               conn_num;

	/**
	 * @internal Reference to defined on accept handling.
	 */
	noPollActionHandler on_accept;
	noPollPtr           on_accept_data;

	/**
	 * @internal Reference to defined on ready handling.
	 */
	noPollActionHandler on_ready;
	noPollPtr           on_ready_data;

	/**
	 * @internal Reference to defined on open handling.
	 */
	noPollActionHandler on_open;
	noPollPtr           on_open_data;

	/**
	 * @internal Reference to the defined on message handling.
	 */
	noPollOnMessageHandler on_msg;
	noPollPtr              on_msg_data;

	/**
	 * @internal Basic fake support for protocol version, by
	 * default: 13, due to RFC6455 standard
	 */
	int protocol_version;

	/**
	 * @internal Certificates added..
	 */
	noPollCertificate *  certificates;
	int                  certificates_length;

	/* mutex */
	noPollPtr            ref_mutex;

	/* log handling */
	noPollLogHandler     log_handler;
	noPollPtr            log_user_data;

#if !defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
	/* context creator */
	noPollSslContextCreator context_creator;
	noPollPtr               context_creator_data;

	/* SSL postcheck */
	noPollSslPostCheck      post_ssl_check;
	noPollPtr               post_ssl_check_data;
#endif
};

struct _noPollConn {
	/**
	 * @internal Connection id.
	 */
	int              id;

	/**
	 * @internal The context associated to this connection.
	 */
	noPollCtx      * ctx;

	/**
	 * @internal This is the actual socket handler associated to
	 * the noPollConn object.
	 */
	NOPOLL_SOCKET    session;
	/**
	 * @internal Flag to signal this connection has finished its
	 * handshake.
	 */
	nopoll_bool      handshake_ok;

	char             handshake_buffer[NOPOLL_HANDSHAKE_BUFFER_SIZE];

	/**
	 * @internal Current connection receive function.
	 */
	noPollRead       receive;

	/**
	 * @internal Current connection receive function.
	 */
	noPollRead       send;

	/**
	 * @internal The connection role.
	 */
	noPollRole       role;

	/**
	 * @internal Conection host ip location (connecting or listening).
	 */
	char           * host;

	/**
	 * @internal Connection port location (connecting or
	 * listening).
	 */
	char           * port;

	/**
	 * @internal Host name requested on the connection.
	 */
	char           * host_name;

	/**
	 * @internal Origin requested on the connection.
	 */
	char           * origin;

	/**
	 * @internal reference to the get url.
	 */
	char           * get_url;

	/**
	 * @internal Reference to protocols requested to be opened on
	 * this connection.
	 */
	char           * protocols;
	/* @internal reference to the protocol that was replied by the server */
	char           * accepted_protocol;

	/* close status and reason */
	int              peer_close_status;
	char           * peer_close_reason;

	/**
	 * @internal Reference to the defined on message handling.
	 */
	noPollOnMessageHandler on_msg;
	noPollPtr              on_msg_data;

	/**
	 * @internal Reference to defined on ready handling.
	 */
	noPollActionHandler on_ready;
	noPollPtr           on_ready_data;

	/**
	 * @internal Reference to the defined on close handling.
	 */
	noPollOnCloseHandler   on_close;
	noPollPtr              on_close_data;

	/* reference to the handshake */
	noPollHandShake  * handshake;

	/* reference to a buffer with pending content */
	char * pending_line;

	/**
	 * @internal connection reference counting.
	 */
	int    refs;

	/**
	 * @internal References to pending content to be read
	 */
	noPollMsg   * pending_msg;
	long int      pending_diff;
	long int      pending_desp;

	/**
	 * @internal Flag to handle TLS support upon connection
	 * reception.
	 */
	nopoll_bool   tls_on;
	/**
	 * @internal Flag that indicates that the provided session
	 * must call to accept the TLS session before proceeding.
	 */
	nopoll_bool   pending_ssl_accept;

	/* SSL support */
#if defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
	mbedtls_net_context fd_ctx;
	mbedtls_ssl_context ssl;
	mbedtls_ssl_config ssl_conf;
	mbedtls_x509_crt ssl_cert; //The own certificate chain
	mbedtls_x509_crt ssl_ca_cert; //The ca certificate
	mbedtls_pk_context ssl_pkey;
	mbedtls_entropy_context ssl_entropy;
	mbedtls_ctr_drbg_context ssl_ctr_drbg;
#else
	SSL_CTX        * ssl_ctx;
	SSL            * ssl;
#endif

	/* certificates */
	char           * certificate;
	char           * private_key;
	char           * chain_certificate;
#if defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
	int              certificate_size;
	int              private_key_size;
	int              chain_certificate_size;
#endif

	/* pending buffer */
	char             pending_buf[100];
	int              pending_buf_bytes;

	/**
	 * @internal Support for an user defined pointer.
	 */
	noPollPtr             hook;

	/**
	 * @internal Mutexes
	 */
	noPollPtr             ref_mutex;
	noPollPtr             handshake_mutex;

	/**
	 * @internal Variable to track pending bytes from previous
	 * read that must be completed.
	 */
	noPollMsg           * previous_msg;
	/* allows to track if previous message was a fragment to flag
	 * next message, even having FIN enabled as a fragment. */
	nopoll_bool           previous_was_fragment;

	char                * pending_write;
	int                   pending_write_bytes;
	int                   pending_write_desp;
        int                   pending_write_added_header;

	/**
	 * @internal Internal reference to the connection options.
	 */
	noPollConnOpts      * opts;

	/**
	 * @internal Reference to the listener in the case this is a
	 * connection that was created due to a listener running.
	 */
	noPollConn          * listener;

	/**
	 * @internal Flag to track internal header pending
	 */
	nopoll_bool           read_pending_header;


	/**** debug values ****/
	/* force stop after header: do not use this, it is just for
	   testing purposes */
	nopoll_bool  __force_stop_after_header;
};

struct _noPollIoEngine {
	noPollPtr              io_object;
	noPollCtx            * ctx;
	noPollIoMechCreate     create;
	noPollIoMechDestroy    destroy;
	noPollIoMechClear      clear;
	noPollIoMechWait       wait;
	noPollIoMechAddTo      add_to;
	noPollIoMechIsSet      is_set;
};

struct _noPollMsg {
	nopoll_bool    has_fin;
	short          op_code;
	nopoll_bool    is_masked;

	noPollPtr      payload;
	long int       payload_size;

	int            refs;
	noPollPtr      ref_mutex;

	char           mask[4];
	int            remain_bytes;

	nopoll_bool    is_fragment;
	int            unmask_desp;
};

struct _noPollHandshake {
	/**
	 * @internal Reference to the to the GET url HTTP/1.1 header
	 * part.
	 */
	nopoll_bool     upgrade_websocket;
	nopoll_bool     connection_upgrade;
	nopoll_bool     received_101;
	char          * websocket_key;
	char          * websocket_version;
	char          * websocket_accept;
	char          * expected_accept;

	/* reference to cookie header */
	char          * cookie;
};

struct _noPollConnOpts {
	/* If the connection options object should be reused across calls */
	nopoll_bool          reuse;

	/* mutex */
	noPollPtr            mutex;
	int                  refs;

#if !defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
	/* What ssl protocol should be used */
	noPollSslProtocol    ssl_protocol;
#endif

	/* SSL options */
	char * certificate;
	char * private_key;
	char * chain_certificate;
	char * ca_certificate;
#if defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
	int    certificate_size;
	int    private_key_size;
	int    chain_certificate_size;
	int    ca_certificate_size;
#endif

	nopoll_bool  disable_ssl_verify;

	/* cookie support */
	char * cookie;

	/* skip origin check flag */
	nopoll_bool skip_origin_header_check;

	/* network interface to bind to */
	char * _interface;

	/* extra HTTP headers to send during the connection */
	char * extra_headers;

	/* control whether origin header is added or not (see
	 * nopoll_conn_opts_add_origin_header) */
	nopoll_bool add_origin_header;
};

#endif

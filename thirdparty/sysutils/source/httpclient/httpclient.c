/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/** Copyright (C) 2018-2022 Qinglong <sysu.zqlong@gmail.com> */

#include <stdint.h>
#include <string.h>
#include "osal/os_thread.h"
#include "cutils/memory_helper.h"
#include "cutils/log_helper.h"
#include "httpclient/httpclient.h"

#if defined(OS_RTOS)
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/tcp.h"
#include "lwip/err.h"
#else
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#define TAG "httpclient"

//#define SYSUTILS_HAVE_MBEDTLS_ENABLED

//#define ENABLE_HTTPCLIENT_DEBUG

#ifdef ENABLE_HTTPCLIENT_DEBUG
#define VERBOSE(fmt, arg...) OS_LOGV(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#define DBG(fmt, arg...)     OS_LOGD(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#define INFO(fmt, arg...)    OS_LOGI(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#define WARN(fmt, arg...)    OS_LOGW(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#define ERR(fmt, arg...)     OS_LOGE(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#else
#define VERBOSE(fmt, arg...) //OS_LOGV(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#define DBG(fmt, arg...)     //OS_LOGD(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#define INFO(fmt, arg...)    OS_LOGI(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#define WARN(fmt, arg...)    OS_LOGW(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#define ERR(fmt, arg...)     OS_LOGE(TAG, "%s:%d: " fmt, __func__, __LINE__, ##arg)
#endif

#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

#define HTTP_PORT                  80
#define HTTPS_PORT                 443
#define HTTPCLIENT_AUTHB_SIZE      128
#define HTTPCLIENT_CHUNK_SIZE_STR_LEN 6 //1234\r\n means chunk size is 0x1234
#define HTTPCLIENT_HEADER_BUF_SIZE 1024
#define HTTPCLIENT_SEND_BUF_SIZE   1024
#define HTTPCLIENT_MAX_HOST_LEN    64
#define HTTPCLIENT_MAX_URL_LEN     512
#define HTTPCLIENT_REDIRECT_MAX    5
#define HTTPCLIENT_TIMEOUT_SEC     3

#ifdef SYSUTILS_HAVE_MBEDTLS_ENABLED
#include "mbedtls/debug.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/certs.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

typedef struct {
    mbedtls_ssl_context ssl_ctx;        /* mbedtls ssl context */
    mbedtls_net_context net_ctx;        /* Fill in socket id */
    mbedtls_ssl_config ssl_conf;        /* SSL configuration */
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_x509_crt_profile profile;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;
} httpclient_ssl_t;
#endif

#if defined(MBEDTLS_DEBUG_C)
/* Debug levels
 *  - 0 No debug
 *  - 1 Error
 *  - 2 State change
 *  - 3 Informational
 *  - 4 Verbose
*/
#define MBEDTLS_DEBUG_LEVEL        1
#endif

static void httpclient_base64enc(char *out, const char *in)
{
    const char code[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=" ;
    int i = 0, x = 0, l = 0;

    for (; *in; in++) {
        x = x << 8 | *in;
        for (l += 8; l >= 6; l -= 6) {
            out[i++] = code[(x >> (l - 6)) & 0x3f];
        }
    }
    if (l > 0) {
        x <<= 6 - l;
        out[i++] = code[x & 0x3f];
    }
    for (; i % 4;) {
        out[i++] = '=';
    }
    out[i] = '\0' ;
}

static int httpclient_conn(httpclient_t *client, char *host)
{
    struct addrinfo hints, *addr_list, *cur;
    int ret = 0;
    char port[10] = {0};
    struct timeval timeout;
    timeout.tv_sec = HTTPCLIENT_TIMEOUT_SEC;
    timeout.tv_usec = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    snprintf(port, sizeof(port), "%d", client->remote_port) ;
    if (getaddrinfo(host, port , &hints, &addr_list) != 0) {
        ERR("getaddrinfo failed");
        return HTTPCLIENT_UNRESOLVED_DNS;
    }

    /* Try the sockaddrs until a connection succeeds */
    ret = HTTPCLIENT_UNRESOLVED_DNS;
    for (cur = addr_list; cur != NULL; cur = cur->ai_next) {
        client->socket = socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (client->socket < 0) {
            ret = HTTPCLIENT_ERROR_CONN;
            continue;
        }

        // set receive timeout
        if (setsockopt(client->socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
            WARN("setsockopt failed, cancel receive timeout");
        }

        ret = connect(client->socket, cur->ai_addr, cur->ai_addrlen);
        if (ret == 0) {
            break;
        }

        close(client->socket);
        ret = HTTPCLIENT_ERROR_CONN;
    }

    freeaddrinfo(addr_list);
    return ret;
}

static int httpclient_parse_url(const char *url, char *scheme, size_t max_scheme_len,
                                char *host, size_t maxhost_len, int *port,
                                char *path, size_t max_path_len)
{
    char *scheme_ptr = (char *)url;
    char *host_ptr = (char *)strstr(url, "://");
    size_t host_len = 0;
    size_t path_len = 0;

    if (host_ptr == NULL) {
        host_ptr = (char *)url;
        memcpy(scheme, "http", 5);
    } else {
        if (max_scheme_len < host_ptr - scheme_ptr + 1) { /* including NULL-terminating char */
            ERR("scheme str is too long (%d < %d)", (int)max_scheme_len, (int)(host_ptr - scheme_ptr + 1));
            return HTTPCLIENT_ERROR_PARSE;
        }
        memcpy(scheme, scheme_ptr, host_ptr - scheme_ptr);
        scheme[host_ptr - scheme_ptr] = '\0';
        host_ptr += 3;
    }

    char *port_ptr = strchr(host_ptr, ':');
    if (port_ptr != NULL) {
        unsigned short tport;
        host_len = port_ptr - host_ptr;
        port_ptr++;
        if (sscanf(port_ptr, "%hu", &tport) != 1) {
            ERR("can not find port");
            return HTTPCLIENT_ERROR_PARSE;
        }
        *port = (int)tport;
    } else {
        *port = 0;
    }

    char *path_ptr = strchr(host_ptr, '/');
    if (path_ptr == NULL) {
        path_ptr = host_ptr + strlen(host_ptr);
    }

    if (host_len == 0) {
        host_len = path_ptr - host_ptr;
    }

    if (maxhost_len < host_len + 1) { /* including NULL-terminating char */
        ERR("host str is too long (%d < %d)", (int)maxhost_len, (int)(host_len + 1));
        return HTTPCLIENT_ERROR_PARSE;
    }
    memcpy(host, host_ptr, host_len);
    host[host_len] = '\0';

    if (*path_ptr == '\0') {
        path_len = 1;
        memcpy(path, "/", path_len + 1);
        return HTTPCLIENT_OK;
    }

    char *fragment_ptr = strchr(host_ptr, '#');
    if (fragment_ptr != NULL) {
        path_len = fragment_ptr - path_ptr;
    } else {
        path_len = strlen(path_ptr);
    }

    if (max_path_len < path_len + 1) { /* including NULL-terminating char */
        ERR("path str is too long (%d < %d)", (int)max_path_len, (int)(path_len + 1));
        return HTTPCLIENT_ERROR_PARSE;
    }
    memcpy(path, path_ptr, path_len);
    path[path_len] = '\0';

    return HTTPCLIENT_OK;
}

static int httpclient_tcp_send_all(int sock_fd, char *data, int length)
{
    int written_len = 0;
    while (written_len < length) {
        int ret = send(sock_fd, data + written_len, length - written_len, 0);
        if (ret > 0) {
            written_len += ret;
            continue;
        } else if (ret == 0) {
            return written_len;
        } else {
            return -1; /* Connnection error */
        }
    }
    return written_len;
}

#ifdef SYSUTILS_HAVE_MBEDTLS_ENABLED
#if 0
static int httpclient_ssl_nonblock_recv( void *ctx, unsigned char *buf, size_t len )
{
    int ret;
    int fd = ((mbedtls_net_context *) ctx)->fd;
    if ( fd < 0 ) {
        return ( MBEDTLS_ERR_NET_INVALID_CONTEXT );
    }
    ret = (int) recv( fd, buf, len, MSG_DONTWAIT );
    if ( ret < 0 ) {
        if ( errno == EPIPE || errno == ECONNRESET ) {
            return ( MBEDTLS_ERR_NET_CONN_RESET );
        }
        if ( errno == EINTR ) {
            return ( MBEDTLS_ERR_SSL_WANT_READ );
        }
        if (ret == -1 && errno == EWOULDBLOCK) {
            return ret;
        }
        return ( MBEDTLS_ERR_NET_RECV_FAILED );
    }
    return ( ret );
}
#endif

static void httpclient_ssl_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    DBG("%s", str);
}

static int httpclient_ssl_send_all(mbedtls_ssl_context *ssl, const char *data, size_t length)
{
    size_t written_len = 0;
    while (written_len < length) {
        int ret = mbedtls_ssl_write(ssl, (unsigned char *)(data + written_len), (length - written_len));
        if (ret > 0) {
            written_len += ret;
            continue;
        } else if (ret == 0) {
            return written_len;
        } else {
            return -1; /* Connnection error */
        }
    }
    return written_len;
}

static int httpclient_ssl_conn(httpclient_t *client, char *host)
{
    int authmode = MBEDTLS_SSL_VERIFY_NONE;
    const char *pers = "https";
    int value, ret = -1;
    uint32_t flags;
    char port[10] = {0};
    httpclient_ssl_t *ssl;

    client->ssl = OS_MALLOC(sizeof(httpclient_ssl_t));
    if (!client->ssl) {
        ERR("ssl context malloc failed");
        goto ssl_conn_exit;
    }
    ssl = (httpclient_ssl_t *)client->ssl;

    if (client->server_cert)
        authmode = MBEDTLS_SSL_VERIFY_REQUIRED;

    /* Initialize the RNG and the session data */
#if defined(MBEDTLS_DEBUG_C)
    mbedtls_debug_set_threshold(MBEDTLS_DEBUG_LEVEL);
#endif
    mbedtls_net_init(&ssl->net_ctx);
    mbedtls_ssl_init(&ssl->ssl_ctx);
    mbedtls_ssl_config_init(&ssl->ssl_conf);
    mbedtls_x509_crt_init(&ssl->cacert);
    mbedtls_x509_crt_init(&ssl->clicert);
    mbedtls_pk_init(&ssl->pkey);
    mbedtls_ctr_drbg_init(&ssl->ctr_drbg);
    mbedtls_entropy_init(&ssl->entropy);
    if ((value = mbedtls_ctr_drbg_seed(&ssl->ctr_drbg,
                               mbedtls_entropy_func,
                               &ssl->entropy,
                               (const unsigned char *)pers,
                               strlen(pers))) != 0) {
        ERR("mbedtls_ctr_drbg_seed failed: %d", value);
        goto ssl_conn_exit;
    }

    /* Load the Client certificate */
    if (client->client_cert && client->client_pk) {
        value = mbedtls_x509_crt_parse(&ssl->clicert, (const unsigned char *)client->client_cert, client->client_cert_len);
        if (value < 0) {
            ERR("mbedtls_x509_crt_parse failed: %d", value);
            goto ssl_conn_exit;
        }

        value = mbedtls_pk_parse_key(&ssl->pkey, (const unsigned char *)client->client_pk, client->client_pk_len, NULL, 0);
        if (value != 0) {
            ERR("mbedtls_pk_parse_key failed: %d", value);
            goto ssl_conn_exit;
        }
    }

    /* Load the trusted CA, cert_len passed in is gotten from sizeof not strlen */
    if (client->server_cert && ((value = mbedtls_x509_crt_parse(&ssl->cacert,
                                        (const unsigned char *)client->server_cert,
                                        client->server_cert_len)) < 0)) {
        ERR("mbedtls_x509_crt_parse failed: %d", value);
        goto ssl_conn_exit;
    }

    /* Start the connection */
    snprintf(port, sizeof(port), "%d", client->remote_port) ;
    if ((value = mbedtls_net_connect(&ssl->net_ctx, host, port, MBEDTLS_NET_PROTO_TCP)) != 0) {
        ERR("mbedtls_net_connect failed: %d", value);
        goto ssl_conn_exit;
    }

    /* Setup stuff */
    if ((value = mbedtls_ssl_config_defaults(&ssl->ssl_conf,
                                           MBEDTLS_SSL_IS_CLIENT,
                                           MBEDTLS_SSL_TRANSPORT_STREAM,
                                           MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ERR("mbedtls_ssl_config_defaults failed: %d", value);
        goto ssl_conn_exit;
    }

    // TODO: add customerization encryption algorithm
    memcpy(&ssl->profile, ssl->ssl_conf.cert_profile, sizeof(mbedtls_x509_crt_profile));
    ssl->profile.allowed_mds = ssl->profile.allowed_mds | MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_MD5);
    mbedtls_ssl_conf_cert_profile(&ssl->ssl_conf, &ssl->profile);

    mbedtls_ssl_conf_authmode(&ssl->ssl_conf, authmode);
    mbedtls_ssl_conf_ca_chain(&ssl->ssl_conf, &ssl->cacert, NULL);

    if (client->client_cert &&
        (value = mbedtls_ssl_conf_own_cert(&ssl->ssl_conf, &ssl->clicert, &ssl->pkey)) != 0) {
        ERR("mbedtls_ssl_conf_own_cert failed: %d", value);
        goto ssl_conn_exit;
    }

    mbedtls_ssl_conf_rng(&ssl->ssl_conf, mbedtls_ctr_drbg_random, &ssl->ctr_drbg);
    mbedtls_ssl_conf_dbg(&ssl->ssl_conf, httpclient_ssl_debug, NULL);

    if ((value = mbedtls_ssl_setup(&ssl->ssl_ctx, &ssl->ssl_conf)) != 0) {
        ERR("mbedtls_ssl_setup failed: %d", value);
        goto ssl_conn_exit;
    }

    mbedtls_ssl_set_bio(&ssl->ssl_ctx, &ssl->net_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);

    /* Handshake */
    while ((value = mbedtls_ssl_handshake(&ssl->ssl_ctx)) != 0) {
        if (value != MBEDTLS_ERR_SSL_WANT_READ && value != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ERR("mbedtls_ssl_handshake failed: %d", value);
            goto ssl_conn_exit;
        }
    }

    /* Verify the server certificate
     *  In real life, we would have used MBEDTLS_SSL_VERIFY_REQUIRED so that the
     *  handshake would not succeed if the peer's cert is bad.  Even if we used
     *  MBEDTLS_SSL_VERIFY_OPTIONAL, we would bail out here if ret != 0
     */
    if ((flags = mbedtls_ssl_get_verify_result(&ssl->ssl_ctx)) != 0) {
        char vrfy_buf[512];
        ERR("mbedtls_ssl_get_verify_result failed");
        mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", flags);
        ERR("mbedtls_x509_crt_verify_info: %s", vrfy_buf);
        //goto ssl_conn_exit;
    }

    ret = 0;

ssl_conn_exit:
    INFO("httpclient_ssl_conn: %s", ret == 0 ? "succeed" : "failed");
    return ret;
}

static void httpclient_ssl_close(httpclient_t *client)
{
    httpclient_ssl_t *ssl = (httpclient_ssl_t *)client->ssl;
    client->client_cert = NULL;
    client->server_cert = NULL;
    client->client_pk = NULL;

    if (ssl == NULL)
        return;
    mbedtls_ssl_close_notify(&ssl->ssl_ctx);
    mbedtls_net_free(&ssl->net_ctx);
    mbedtls_x509_crt_free(&ssl->cacert);
    mbedtls_x509_crt_free(&ssl->clicert);
    mbedtls_pk_free(&ssl->pkey);
    mbedtls_ssl_free(&ssl->ssl_ctx);
    mbedtls_ssl_config_free(&ssl->ssl_conf);
    mbedtls_ctr_drbg_free(&ssl->ctr_drbg);
    mbedtls_entropy_free(&ssl->entropy);
    OS_FREE(ssl);
}
#endif

static int httpclient_get_info(httpclient_t *client, char *send_buf, int *send_idx, char *buf, size_t len)   /* 0 on success, err code on failure */
{
    int ret = 0;
    int cp_len = 0;
    int idx = *send_idx;

    if (len == 0) {
        len = strlen(buf);
    }

    do {
        if ((HTTPCLIENT_SEND_BUF_SIZE - idx) >= len) {
            cp_len = len;
        } else {
            cp_len = HTTPCLIENT_SEND_BUF_SIZE - idx;
        }

        memcpy(send_buf + idx, buf, cp_len);
        idx += cp_len;
        len -= cp_len;

        if (idx == HTTPCLIENT_SEND_BUF_SIZE) {
            if (client->is_https) {
                ERR("send_buf overflow");
                return HTTPCLIENT_ERROR;
            }
            ret = httpclient_tcp_send_all(client->socket, send_buf, HTTPCLIENT_SEND_BUF_SIZE) ;
            if (ret != 0) {
                return ret;
            }
        }
    } while (len);

    *send_idx = idx;
    return HTTPCLIENT_OK;
}

static int httpclient_send_auth(httpclient_t *client, char *send_buf, int *send_idx)
{
    char b_auth[(int)((HTTPCLIENT_AUTHB_SIZE + 3) * 4 / 3 + 3)];
    char base64buff[HTTPCLIENT_AUTHB_SIZE + 3];

    httpclient_get_info(client, send_buf, send_idx, "Authorization: Basic ", 0);
    sprintf(base64buff, "%s:%s", client->auth_user, client->auth_password);
    VERBOSE("base64buff: %s", base64buff);
    httpclient_base64enc(b_auth, base64buff);
    b_auth[strlen(b_auth) + 2] = '\0';
    b_auth[strlen(b_auth) + 1] = '\n';
    b_auth[strlen(b_auth)] = '\r';
    VERBOSE("b_auth: %s", b_auth);
    httpclient_get_info(client, send_buf, send_idx, b_auth, 0);
    return HTTPCLIENT_OK;
}

static int httpclient_send_header(httpclient_t *client, char *url, int method, httpclient_data_t *client_data)
{
    char scheme[8] = {0};
    char host[HTTPCLIENT_MAX_HOST_LEN] = {0};
    char path[HTTPCLIENT_MAX_URL_LEN] = {0};
    char send_buf[HTTPCLIENT_SEND_BUF_SIZE] = {0};
    char temp_buf[HTTPCLIENT_SEND_BUF_SIZE] = {0};
    char *meth = "";
    int len = 0;
    int ret = 0, port = HTTP_PORT;

    switch (method) {
    case HTTPCLIENT_GET:
        meth = "GET";
        break;
    case HTTPCLIENT_POST:
        meth = "POST";
        break;
    case HTTPCLIENT_PUT:
        meth = "PUT";
        break;
    case HTTPCLIENT_DELETE:
        meth = "DELETE";
        break;
    case HTTPCLIENT_HEAD:
        meth = "HEAD";
        break;
    default:
        ERR("Invalid method: %d", method);
        return HTTPCLIENT_ERROR;
    }

    /* First we need to parse the url (http[s]://host[:port][/[path]]) */
    ret = httpclient_parse_url(url, scheme, sizeof(scheme), host, sizeof(host), &(port), path, sizeof(path));
    if (ret != HTTPCLIENT_OK) {
        ERR("httpclient_parse_url failed: %d", ret);
        return ret;
    }

    /* Send request */
    memset(send_buf, 0, HTTPCLIENT_SEND_BUF_SIZE);
    snprintf(temp_buf, sizeof(temp_buf), "%s %s HTTP/1.1\r\nHost: %s\r\n", meth, path, host); /* Write request */
    ret = httpclient_get_info(client, send_buf, &len, temp_buf, strlen(temp_buf));
    if (ret != HTTPCLIENT_OK) {
        ERR("httpclient_get_info failed: %d", ret);
        return HTTPCLIENT_ERROR_CONN;
    }

    /* Send all headers */
    if (client->auth_user != NULL) {
        httpclient_send_auth(client, send_buf, &len) ; /* send out Basic Auth header */
    }

    /* Add user header information */
    if (client->header != NULL) {
        httpclient_get_info(client, send_buf, &len, (char *)client->header, strlen(client->header));
    }

    if (client_data->post_buf != NULL) {
        snprintf(temp_buf, sizeof(temp_buf), "Content-Length: %d\r\n", client_data->post_buf_len);
        httpclient_get_info(client, send_buf, &len, temp_buf, strlen(temp_buf));
        if (client_data->post_content_type != NULL)  {
            snprintf(temp_buf, sizeof(temp_buf), "Content-Type: %s\r\n", client_data->post_content_type);
            httpclient_get_info(client, send_buf, &len, temp_buf, strlen(temp_buf));
        }
    }

    /* Close headers */
    httpclient_get_info(client, send_buf, &len, "\r\n", 0);

    DBG("sending header: \n%s", send_buf);

#ifdef SYSUTILS_HAVE_MBEDTLS_ENABLED
    if (client->is_https) {
        httpclient_ssl_t *ssl = (httpclient_ssl_t *)client->ssl;
        if (httpclient_ssl_send_all(&ssl->ssl_ctx, send_buf, len) != len) {
            ERR("httpclient_ssl_send_all failed");
            return HTTPCLIENT_ERROR;
        }
        return HTTPCLIENT_OK;
    } else
#endif
    {
        ret = httpclient_tcp_send_all(client->socket, send_buf, len);
        if (ret > 0) {
            VERBOSE("written %d bytes", ret);
        } else if (ret == 0) {
            WARN("connection was closed by server");
            return HTTPCLIENT_CLOSED; /* Connection was closed by server */
        } else {
            ERR("connection error: %d", ret);
            return HTTPCLIENT_ERROR_CONN;
        }
    }
    return HTTPCLIENT_OK;
}

static int httpclient_send_userdata(httpclient_t *client, httpclient_data_t *client_data)
{
    int ret = 0;

    if (client_data->post_buf && client_data->post_buf_len) {
        VERBOSE("post_buf: %s", client_data->post_buf);
#ifdef SYSUTILS_HAVE_MBEDTLS_ENABLED
        if (client->is_https) {
            httpclient_ssl_t *ssl = (httpclient_ssl_t *)client->ssl;
            ret = httpclient_ssl_send_all(&ssl->ssl_ctx, client_data->post_buf, client_data->post_buf_len);
            if (ret != client_data->post_buf_len) {
                ERR("httpclient_ssl_send_all failed");
                return HTTPCLIENT_ERROR;
            }
        }
        else
#endif
        {
            ret = httpclient_tcp_send_all(client->socket, client_data->post_buf, client_data->post_buf_len);
            if (ret > 0) {
                VERBOSE("written %d bytes", ret);
            } else if (ret == 0) {
                WARN("connection was closed by server");
                return HTTPCLIENT_CLOSED; /* Connection was closed by server */
            } else {
                ERR("connection error: %d", ret);
                return HTTPCLIENT_ERROR_CONN;
            }
        }
    }
    return HTTPCLIENT_OK;
}

static int httpclient_recv(httpclient_t *client, char *buf, int min_len, int max_len, int *p_read_len)
{
    int ret = 0;
    size_t readLen = 0;

    while (readLen < max_len && readLen < min_len) {
        buf[readLen] = '\0';
#ifdef SYSUTILS_HAVE_MBEDTLS_ENABLED
        if (client->is_https) {
            httpclient_ssl_t *ssl = (httpclient_ssl_t *)client->ssl;
        #if 0
            if (readLen < min_len) {                
                mbedtls_ssl_set_bio(&ssl->ssl_ctx, &ssl->net_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);
                ret = mbedtls_ssl_read(&ssl->ssl_ctx, (unsigned char *)buf + readLen, min_len - readLen);
                VERBOSE("mbedtls_ssl_read [blocking] return: %d", ret);
            } else {
                mbedtls_ssl_set_bio(&ssl->ssl_ctx, &ssl->net_ctx, mbedtls_net_send, httpclient_ssl_nonblock_recv, NULL);
                ret = mbedtls_ssl_read(&ssl->ssl_ctx, (unsigned char *)buf + readLen, max_len - readLen);
                VERBOSE("mbedtls_ssl_read [not blocking] return: %d", ret);
                if (ret == -1 && errno == EWOULDBLOCK) {
                    DBG("mbedtls_ssl_read [not blocking] EWOULDBLOCK");
                    break;
                }
            }
        #else
            mbedtls_ssl_set_bio(&ssl->ssl_ctx, &ssl->net_ctx, mbedtls_net_send, mbedtls_net_recv, NULL);
            ret = mbedtls_ssl_read(&ssl->ssl_ctx, (unsigned char *)buf + readLen, max_len - readLen);
        #endif
            /* read already complete(if call mbedtls_ssl_read again, it will return 0(eof)) */
            if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                ret = HTTPCLIENT_CLOSED;
            }
        } else
#endif
        {
        #if 0
            if (readLen < min_len) {
                ret = recv(client->socket, buf + readLen, min_len - readLen, 0);
                VERBOSE("recv [blocking] return: %d", ret);
                if (ret == 0) {
                    DBG("recv [blocking] return 0 may disconnected");
                    ret = HTTPCLIENT_CLOSED;
                }
            } else {
                ret = recv(client->socket, buf + readLen, max_len - readLen, MSG_DONTWAIT);
                VERBOSE("recv [not blocking] return: %d", ret);
                if (ret == -1 && errno == EWOULDBLOCK) {
                    DBG("recv [not blocking] EWOULDBLOCK");
                    break;
                }
            }
        #else
            ret = recv(client->socket, buf + readLen, max_len - readLen, 0);
            if (ret == 0) {
                DBG("recv [blocking] return 0 may disconnected");
                ret = HTTPCLIENT_CLOSED;
            }
        #endif
        }

        if (ret > 0) {
            readLen += ret;
        } else if (ret == 0) {
            ret = HTTPCLIENT_CLOSED;
            break;
        } else if (ret == HTTPCLIENT_CLOSED) {
            break;
        } else {
            ERR("connection error (recv: %d)", ret);
            ret = HTTPCLIENT_ERROR_CONN;
            break;
        }
    }

    VERBOSE("read %d bytes", (int)readLen);
    *p_read_len = readLen;
    buf[readLen] = '\0';

    if (ret < 0) {
        return ret;
    } else {
        return HTTPCLIENT_OK;
    }
}

static int httpclient_retrieve_content(httpclient_t *client, int len, httpclient_data_t *client_data)
{
    int count = 0;
    int templen = 0;
    int crlf_pos;
    char *data = client_data->response_buf;
    /* Receive data */
    //VERBOSE("response_buf_len: %d", client_data->response_buf_len);
    client_data->is_more = true;
    client_data->content_block_len = 0;

    if (client_data->response_content_len == -1 && client_data->is_chunked == false) {
        while (true) {
            if (count + len < client_data->response_buf_len - 1) {
                data += len;
                count += len;
                client_data->response_buf[count] = '\0';
                client_data->content_block_len += len;
            } else {
                client_data->response_buf[client_data->response_buf_len - 1] = '\0';
                client_data->content_block_len = client_data->response_buf_len - 1;
                return HTTPCLIENT_RETRIEVE_MORE_DATA;
            }

            int ret = httpclient_recv(client, data, 1, client_data->response_buf_len - 1 - count, &len);
            /* Receive data */
            VERBOSE("httpclient_recv: ret=%d, len=%d, count=%d", ret, len, count);
            if (ret == HTTPCLIENT_ERROR_CONN) {
                ERR("httpclient_recv failed: %d", ret);
                return ret;
            }

            if (len == 0) {/* read no more data */
                DBG("no more data, recv 0 bytes");
                client_data->is_more = false;
                return HTTPCLIENT_OK;
            }
        }
    }

    while (true) {
        size_t readLen = 0;

        if (client_data->is_chunked && client_data->retrieve_len <= 0) {
            /* Read chunk header */
            bool foundCrlf;
            do {
                VERBOSE("len=%d, count=%d, content_block_len=%d, response_buf_len=%d",
                    len, count, client_data->content_block_len, client_data->response_buf_len);
                foundCrlf = false;
                crlf_pos = 0;
                data[len] = 0;
                if (len >= 2) {
                    for (; crlf_pos < len - 2; crlf_pos++) {
                        if (data[crlf_pos] == '\r' && data[crlf_pos + 1] == '\n') {
                            foundCrlf = true;
                            break;
                        }
                    }
                }
                /* Only 2 status will in this case: 1. The first time calling the function after header read.
                 *                                  2. One chunk read finished and len == 0
                 * */
                if (!foundCrlf) { /* Try to read more */
                        int new_trf_len, ret;
                        /* Sometimes the chunk size is x\r\n, sometimes is xxx\r\n or xxxx\r\n. The xxxx is hex value of chunk size.
                         * If only 2 bytes reserved in client_data->response_buf, when read 6 bytes for case xxxx\r\n, but
                         * current chunk size is x\r\n, the left 3 bytes must be stored in client_data->response_buf.
                         */
                        if (client_data->response_buf_len-client_data->content_block_len-1+3 < HTTPCLIENT_CHUNK_SIZE_STR_LEN) {
                            if (len == 0) { // when client_data->response_buf is full
                                return HTTPCLIENT_RETRIEVE_MORE_DATA;
                            } else {
                                return HTTPCLIENT_ERROR;
                            }
                        }
                        ret = httpclient_recv(client, data+len, 1,
                            client_data->response_buf_len-client_data->content_block_len-1+3-len, &new_trf_len);
                        len += new_trf_len;
                        if (ret == HTTPCLIENT_ERROR_CONN || (ret == HTTPCLIENT_CLOSED && new_trf_len == 0)) {
                            return ret;
                        } else {
                            continue;
                        }
                }
            } while (!foundCrlf);
            data[crlf_pos] = '\0';
            int n = sscanf(data, "%x", (unsigned int *)&readLen);/* chunk length */
            client_data->retrieve_len = readLen;
            client_data->response_content_len += client_data->retrieve_len;
            if (n != 1) {
                ERR("can not read chunk length");
                return HTTPCLIENT_ERROR_PRTCL;
            }

            memmove(data, &data[crlf_pos + 2], len - (crlf_pos + 2)); /* Not need to move NULL-terminating char any more */
            len -= (crlf_pos + 2);

            if (readLen == 0) {
                /* Last chunk */
                client_data->is_more = false;
                DBG("no more data, last chunk");
                break;
            }
        } else {
            readLen = client_data->retrieve_len;
        }

        VERBOSE("retrieving %d bytes", (int)readLen);

        do {
            VERBOSE("readLen=%d, len=%d", (int)readLen, len);
            templen = MIN(len, readLen);
            if (count + templen < client_data->response_buf_len - 1) {
                count += templen;
                client_data->response_buf[count] = '\0';
                client_data->retrieve_len -= templen;
                client_data->content_block_len += templen;
            } else {
                client_data->response_buf[client_data->response_buf_len - 1] = '\0';
                client_data->retrieve_len -= (client_data->response_buf_len - 1 - count);
                client_data->content_block_len = client_data->response_buf_len - 1;
                return HTTPCLIENT_RETRIEVE_MORE_DATA;
            }

            if (len >= readLen) {
                VERBOSE("readLen=%d, len=%d, retrieve_len=%d", (int)readLen, len, client_data->retrieve_len);
                data += readLen;
                len -= readLen;
                readLen = 0;
                client_data->retrieve_len = 0;
            } else {
                data += len;
                readLen -= len;
            }

            if (readLen) {
                int ret;
                int max_len = MIN(client_data->response_buf_len - 1 - count, readLen);
                ret = httpclient_recv(client, data, 1, max_len, &len);
                if (ret == HTTPCLIENT_ERROR_CONN || (ret == HTTPCLIENT_CLOSED && len == 0)) {
                    return ret;
                }
            }
        } while (readLen);

        if (client_data->is_chunked) {
            if (len < 2) {
                int new_trf_len = 0, ret;
                int max_recv = client_data->response_buf_len - 1 - count - len + 2;
                if (max_recv + len < 2 + HTTPCLIENT_CHUNK_SIZE_STR_LEN) { // To avoid read too much data
                    max_recv = 2 - len;
                }
                /* Read missing chars to find end of chunk */
                ret = httpclient_recv(client, data + len, 2 - len, max_recv, &new_trf_len);
                if (ret == HTTPCLIENT_ERROR_CONN || (ret == HTTPCLIENT_CLOSED && new_trf_len == 0)) {
                    return ret;
                }
                len += new_trf_len;
            }
            if (data[0] != '\r' || data[1] != '\n') {
                ERR("format error"); /* after memmove, the beginning of next chunk */
                return HTTPCLIENT_ERROR_PRTCL;
            }
            memmove(data, &data[2], len - 2); /* remove the \r\n */
            len -= 2;
        } else {
            DBG("no more data, reach content-length");
            client_data->is_more = false;
            break;
        }
    }
    client_data->content_block_len = count;
    return HTTPCLIENT_OK;
}

static int httpclient_redirect(char *url, httpclient_t *client, httpclient_data_t *client_data)
{
    if (client->response_code != 301 && client->response_code != 302) {
        ERR("response code: %d, will not redirect", client->response_code);
        return HTTPCLIENT_ERROR;
    }
    if (client->redirect_times >= HTTPCLIENT_REDIRECT_MAX) {
        ERR("redirect_times(%d) >= REDIRECT_MAX(%d), abort", client->redirect_times, HTTPCLIENT_REDIRECT_MAX);
        return HTTPCLIENT_ERROR;
    }

    INFO("redirecting...");
    client->redirect_times++;

    httpclient_close(client);
    client_data->is_more = false;
    client_data->retrieve_len = 0;
    int ret = httpclient_connect(client, url);
    if (ret != 0) {
        return ret;
    }
    // Send request to server
    ret = httpclient_send_request(client, url, HTTPCLIENT_GET, client_data);
    if (ret < 0) {
        return ret;
    }
    //Receive response from server
    ret = httpclient_recv_response(client, client_data);
    return ret;
}

static int httpclient_response_parse(httpclient_t *client, int len, httpclient_data_t *client_data)
{
    int crlf_pos;
    int header_buf_len = client_data->header_buf_len;
    int read_result;
    char *data = client_data->header_buf;

    client_data->response_content_len = -1;
    char *crlf_ptr = strstr(data, "\r\n");
    if (crlf_ptr == NULL) {
        ERR("crlf not found");
        return HTTPCLIENT_ERROR_PRTCL;
    }

    crlf_pos = crlf_ptr - data;
    data[crlf_pos] = '\0';

    /* Parse HTTP response */
    if (sscanf(data, "HTTP/%*d.%*d %d %*[^\r\n]", &(client->response_code)) != 1) {
        /* Cannot match string, error */
        ERR("not a correct HTTP answer : %s", data);
        return HTTPCLIENT_ERROR_PRTCL;
    }

    if (client->response_code < 200 || client->response_code >= 400) {
        /* Did not return a 2xx code; TODO fetch headers/(&data?) anyway and implement a mean of writing/reading headers */
        WARN("response code: %d", client->response_code);
    }

    VERBOSE("reading headers: %s", data);

    memmove(data, &data[crlf_pos + 2], len - (crlf_pos + 2) + 1); /* Be sure to move NULL-terminating char as well */
    len -= (crlf_pos + 2);
    client_data->is_chunked = false;

    /* Now get headers */
    while (true) {
        char *colon_ptr, *key_ptr, *value_ptr;
        int key_len, value_len;

        crlf_ptr = strstr(data, "\r\n");
        if (crlf_ptr == NULL) {
            if (len < header_buf_len - 1) {
                VERBOSE("header_buff_len > header_len, remove part to recv more");
                int new_trf_len = 0;
                memmove(client_data->header_buf, data, len);
                data = client_data->header_buf;
                read_result = httpclient_recv(client, data + len, 1, header_buf_len - len - 1, &new_trf_len);
                len += new_trf_len;
                data[len] = '\0';
                VERBOSE("read %d chars, data: [%s]", new_trf_len, data);
                if (read_result == HTTPCLIENT_ERROR_CONN || (read_result == HTTPCLIENT_CLOSED && new_trf_len == 0)) {
                    return read_result;
                } else {
                    continue;
                }
            } else {
                ERR("header is too long: %d >= %d", len, header_buf_len - 1);
                return HTTPCLIENT_ERROR;
            }
        }

        crlf_pos = crlf_ptr - data;
        if (crlf_pos == 0) { /* End of headers */
            if (client_data->response_buf_len > len - 2 + 1) {
                memcpy(client_data->response_buf, &data[2], len - 2 + 1); /* Be sure to move NULL-terminating char as well */
                memset(data, 0, len + 1);
                len -= 2;
            } else {
                ERR("response data is too long: %d > %d", len-1, client_data->response_buf_len);
                return HTTPCLIENT_ERROR;
            }
            break;
        }

        colon_ptr = strstr(data, ": ");
        if (colon_ptr) {
            key_len = colon_ptr - data;
            value_len = crlf_ptr - colon_ptr - strlen(": ");
            key_ptr = data;
            value_ptr = colon_ptr + strlen(": ");

            VERBOSE("reading headers: %.*s: %.*s", key_len, key_ptr, value_len, value_ptr);
            if (0 == strncasecmp(key_ptr, "Content-Length", key_len)) {
                sscanf(value_ptr, "%d[^\r]", &(client_data->response_content_len));
                client_data->retrieve_len = client_data->response_content_len;
            } else if (0 == strncasecmp(key_ptr, "Transfer-Encoding", key_len)) {
                if (0 == strncasecmp(value_ptr, "Chunked", value_len)) {
                    client_data->is_chunked = true;
                    client_data->response_content_len = 0;
                    client_data->retrieve_len = 0;
                }
            } else if (0 == strncasecmp(key_ptr, "Location", key_len)) {
                char location[HTTPCLIENT_MAX_HOST_LEN + HTTPCLIENT_MAX_URL_LEN];
                memset(location, 0x0, sizeof(location));
                sscanf(value_ptr, "%s[^\r]", location);
                INFO("redirect url: %s", location);
                int ret = httpclient_redirect(location, client, client_data);
                return ret;
            }
            data += crlf_pos + 2;
            len -= (crlf_pos + 2);
        } else {
            ERR("Can not parse header");
            return HTTPCLIENT_ERROR;
        }
    }

    return httpclient_retrieve_content(client, len, client_data);
}


HTTPCLIENT_RESULT httpclient_connect(httpclient_t *client, char *url)
{
    char host[HTTPCLIENT_MAX_HOST_LEN] = {0};
    char path[HTTPCLIENT_MAX_URL_LEN] = {0};
    char scheme[8] = {0};
    int ret = HTTPCLIENT_ERROR_CONN;

    /* First we need to parse the url (http[s]://host[:port][/[path]]) */
    ret = httpclient_parse_url(url, scheme, sizeof(scheme), host, sizeof(host), &(client->remote_port), path, sizeof(path));
    if (ret != HTTPCLIENT_OK) {
        ERR("httpclient_parse_url failed: %d", ret);
        return (HTTPCLIENT_RESULT)ret;
    }

    // http or https
    if (strcmp(scheme, "https") == 0) {
        client->is_https = true;
    } else if (strcmp(scheme, "http") == 0) {
        client->is_https = false;
    }

    // default http 80 port, https 443 port
    if (client->remote_port == 0) {
        if (client->is_https) {
            client->remote_port = HTTPS_PORT;
        } else {
            client->remote_port = HTTP_PORT;
        }
    }

    VERBOSE("is_https?: %d, port: %d, host: %s", client->is_https, client->remote_port, host);

    ret = HTTPCLIENT_ERROR_CONN;
    client->socket = -1;
    if (client->is_https) {
#ifdef SYSUTILS_HAVE_MBEDTLS_ENABLED
        ret = httpclient_ssl_conn(client, host);
        if (ret == 0) {
            httpclient_ssl_t *ssl = (httpclient_ssl_t *)client->ssl;
            client->socket = ssl->net_ctx.fd;
        }
#else
        ERR("https not supported, please set C_FLAGS with SYSUTILS_HAVE_MBEDTLS_ENABLED");
#endif
    } else {
        ret = httpclient_conn(client, host);
    }

    INFO("httpclient_connect() result: %d, client: %p", ret, client);
    return (HTTPCLIENT_RESULT)ret;
}

HTTPCLIENT_RESULT httpclient_send_request(httpclient_t *client, char *url, int method, httpclient_data_t *client_data)
{
    int ret = HTTPCLIENT_ERROR_CONN;
    if (client->socket < 0) {
        return (HTTPCLIENT_RESULT)ret;
    }
    ret = httpclient_send_header(client, url, method, client_data);
    if (ret != 0) {
        return (HTTPCLIENT_RESULT)ret;
    }
    if (method == HTTPCLIENT_POST || method == HTTPCLIENT_PUT) {
        ret = httpclient_send_userdata(client, client_data);
    }
    DBG("httpclient_send_request() result: %d, client: %p", ret, client);
    return (HTTPCLIENT_RESULT)ret;
}

HTTPCLIENT_RESULT httpclient_recv_response(httpclient_t *client, httpclient_data_t *client_data)
{
    int reclen = 0;
    int ret = HTTPCLIENT_ERROR_CONN;
    bool header_malloc = false;
    // TODO: header format:  name + value must not bigger than HTTPCLIENT_CHUNK_SIZE.

    if (client->socket < 0) {
        return (HTTPCLIENT_RESULT)ret;
    }

    if (client_data->is_more) {
        client_data->response_buf[0] = '\0';
        ret = httpclient_retrieve_content(client, reclen, client_data);
    } else {
        if (client_data->header_buf == NULL) {
            client_data->header_buf = (char *)OS_MALLOC(HTTPCLIENT_HEADER_BUF_SIZE);
            client_data->header_buf_len = HTTPCLIENT_HEADER_BUF_SIZE;
            header_malloc = true;
        }
        ret = httpclient_recv(client, client_data->header_buf, 1, client_data->header_buf_len - 1, &reclen);
        if (ret != HTTPCLIENT_OK && ret != HTTPCLIENT_CLOSED) {
            if (header_malloc) {
                OS_FREE(client_data->header_buf);
                client_data->header_buf = NULL;
                client_data->header_buf_len = 0;
            }
            return (HTTPCLIENT_RESULT)ret;
        }

        client_data->header_buf[reclen] = '\0';

        if (reclen) {
            DBG("response header: \n%s", client_data->header_buf);
            ret = httpclient_response_parse(client, reclen, client_data);
        }
    }

    if (header_malloc) {
        OS_FREE(client_data->header_buf);
        client_data->header_buf = NULL;
        client_data->header_buf_len = 0;
    }

    VERBOSE("httpclient_recv_response() result: %d, client: %p", ret, client);
    return (HTTPCLIENT_RESULT)ret;
}

void httpclient_close(httpclient_t *client)
{
#ifdef SYSUTILS_HAVE_MBEDTLS_ENABLED
    if (client->is_https) {
        httpclient_ssl_close(client);
    } else
#endif
    {
        if (client->socket >= 0)
            close(client->socket);
    }
    client->socket = -1;
    INFO("httpclient_close() client: %p", client);
}

int httpclient_get_response_code(httpclient_t *client)
{
    return client->response_code;
}

static HTTPCLIENT_RESULT httpclient_common(httpclient_t *client, char *url, int method, httpclient_data_t *client_data)
{
    HTTPCLIENT_RESULT ret = httpclient_connect(client, url);
    if (!ret) {
        ret = httpclient_send_request(client, url, method, client_data);
        if (!ret) {
            ret = httpclient_recv_response(client, client_data);
        }
    }
    httpclient_close(client);
    return ret;
}

HTTPCLIENT_RESULT httpclient_get(httpclient_t *client, char *url, httpclient_data_t *client_data)
{
    return httpclient_common(client, url, HTTPCLIENT_GET, client_data);
}

HTTPCLIENT_RESULT httpclient_post(httpclient_t *client, char *url, httpclient_data_t *client_data)
{
    return httpclient_common(client, url, HTTPCLIENT_POST, client_data);
}

HTTPCLIENT_RESULT httpclient_put(httpclient_t *client, char *url, httpclient_data_t *client_data)
{
    return httpclient_common(client, url, HTTPCLIENT_PUT, client_data);
}

HTTPCLIENT_RESULT httpclient_delete(httpclient_t *client, char *url, httpclient_data_t *client_data)
{
    return httpclient_common(client, url, HTTPCLIENT_DELETE, client_data);
}

int httpclient_get_response_header_value(char *header_buf, char *name, int *val_pos, int *val_len)
{
    char *data = header_buf;
    char *crlf_ptr, *colon_ptr, *key_ptr, *value_ptr;
    int key_len, value_len;

    if (header_buf == NULL || name == NULL || val_pos == NULL  || val_len == NULL )
        return -1;

    while (true) {
        crlf_ptr = strstr(data, "\r\n");
        colon_ptr = strstr(data, ": ");
        if (colon_ptr) {
            key_len = colon_ptr - data;
            value_len = crlf_ptr - colon_ptr - strlen(": ");
            key_ptr = data;
            value_ptr = colon_ptr + strlen(": ");

            VERBOSE("key:value: %.*s:%.*s", key_len, key_ptr, value_len, value_ptr);
            if (0 == strncasecmp(key_ptr, name, key_len)) {
                *val_pos = value_ptr - header_buf;
                *val_len = value_len;
                return 0;
            } else {
                data = crlf_ptr + 2;
                continue;
            }
        } else {
            return -1;
        }
    }
}

void httpclient_set_custom_header(httpclient_t *client, char *header)
{
    client->header = header ;
}

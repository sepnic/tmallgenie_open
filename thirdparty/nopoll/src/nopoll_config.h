/*
 * Nopoll Library nopoll_config.h
 * Platform dependant definitions.
 *
 * This file is maintained manually for those people that do not
 * compile nopoll using autoconf. It should look really similar to
 * nopoll_config.h file created on a i386 linux platform but changing
 * NOPOLL_OS_UNIX to NOPOLL_OS_WIN32 (at least for now).
 *
 *  For commercial support on build WebSocket enabled solutions contact us:
 *
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         Av. Juan Carlos I, Nº13, 2ºC
 *         Alcalá de Henares 28806 Madrid
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/nopoll
 *
 */
// Copyright (c) 2021 Qinglong<sysu.zqlong@gmail.com>
// History:
//  1. Add mbedtls support, you should define 'NOPOLL_HAVE_MBEDTLS_ENABLED'
//     if using mbedtls instead of openssl
//  2. Add macro 'NOPOLL_HAVE_IPV6_ENABLED', define it if ipv6 supported,
//     otherwise remove it
//  3. Add sysutils support, because sysutils has osal layer, we don't need
//     to care about platform dependent
//  4. Add lwip support

#ifndef __NOPOLL_CONFIG_H__
#define __NOPOLL_CONFIG_H__

/**
 * \addtogroup nopoll_decl_module
 * @{
 */

/**
 * @brief Allows to convert integer value (including constant values)
 * into a pointer representation.
 *
 * Use the oposite function to restore the value from a pointer to a
 * integer: \ref PTR_TO_INT.
 *
 * @param integer The integer value to cast to pointer.
 *
 * @return A \ref noPollPtr reference.
 */
#ifndef INT_TO_PTR
#define INT_TO_PTR(integer)   ((noPollPtr) (long) ((int)integer))
#endif

/**
 * @brief Allows to convert a pointer reference (\ref noPollPtr),
 * which stores an integer that was stored using \ref INT_TO_PTR.
 *
 * Use the oposite function to restore the pointer value stored in the
 * integer value.
 *
 * @param ptr The pointer to cast to a integer value.
 *
 * @return A int value.
 */
#ifndef PTR_TO_INT
#define PTR_TO_INT(ptr) ((int) (long) (ptr))
#endif

/**
 * @brief where we have support for sysutils support.
 * sysutils has osal layer that means we don't need to care about
 * platform dependent
 * See https://github.com/sepnic/sysutils
 */
#if defined(NOPOLL_HAVE_SYSUTILS_ENABLED)
#define NOPOLL_OS_UNIX (1)
#endif

#if defined(NOPOLL_OS_UNIX)
#
#elif defined(NOPOLL_OS_WIN32)
#
#else
#error No platform defined
#endif

/**
 * @brief Indicates where we have support for LwIP.
 */
//#define NOPOLL_HAVE_LWIP_ENABLED (1)


#if !defined(NOPOLL_HAVE_MBEDTLS_ENABLED)
    /**
     * @brief Indicates where we have support for TLSv1.0 support.
     */
    #define NOPOLL_HAVE_TLSv10_ENABLED (1)
    /**
     * @brief Indicates where we have support for TLSv1.1 support.
     */
    #define NOPOLL_HAVE_TLSv11_ENABLED (1)
    /**
     * @brief Indicates where we have support for TLSv1.2 support.
     */
    #define NOPOLL_HAVE_TLSv12_ENABLED (1)
    /**
     * @brief Indicates where we have support for TLS flexible method where the highest TLS version will be negotiated.
     */
    #define NOPOLL_HAVE_TLS_FLEXIBLE_ENABLED (1)
#endif // !defined(NOPOLL_HAVE_MBEDTLS_ENABLED)

/* @} */

#endif

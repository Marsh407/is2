//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _SCHEME_H
#define _SCHEME_H
namespace ns_is2 {
//! ----------------------------------------------------------------------------
//! Enums
//! ----------------------------------------------------------------------------
// Schemes
typedef enum scheme_enum {
        SCHEME_TCP = 0,
#ifdef BUILD_TLS_WITH_OPENSSL
        SCHEME_TLS,
#endif
        SCHEME_NONE
} scheme_t;
}
#endif

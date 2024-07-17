//! ----------------------------------------------------------------------------
//! Copyright Edgio Inc.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
#ifndef _T_SRVR_H
#define _T_SRVR_H
//! ----------------------------------------------------------------------------
//! includes
//! ----------------------------------------------------------------------------
#include <stdint.h>
#include <queue>
#include <signal.h>
#include <string>
#include <list>
#include "is2/evr/evr.h"
#include "is2/nconn/scheme.h"
#include "is2/srvr/stat.h"
#include "dns/nresolver.h"
#include "srvr/nconn_pool.h"
//! ----------------------------------------------------------------------------
//! external fwd decl's
//! ----------------------------------------------------------------------------
typedef struct ssl_ctx_st SSL_CTX;
//! ----------------------------------------------------------------------------
//! fwd decl's
//! ----------------------------------------------------------------------------
namespace ns_is2 {
class url_router;
struct host_info;
class nbq;
class nconn;
}
namespace ns_is2 {
class session;
class lsnr;
class subr;
typedef std::list <subr *> subr_list_t;
#ifndef resp_done_cb_t
// TODO move to handler specific resp cb...
typedef int32_t (*resp_done_cb_t)(session &);
#endif
//! ----------------------------------------------------------------------------
//! fwd decl's
//! ----------------------------------------------------------------------------
class srvr;
//! ----------------------------------------------------------------------------
//! Virtual Server conf
//! TODO Allow many t_srvr conf's
//! one per listener
//! ----------------------------------------------------------------------------
typedef struct t_conf
{
        evr_loop_type_t m_evr_loop_type;
        int32_t m_num_parallel;
        uint32_t m_timeout_ms;
        int32_t m_num_reqs_per_conn;
        resp_done_cb_t m_resp_done_cb;
        nresolver *m_nresolver;
        std::string m_server_name;
        bool m_dns_use_sync;
        uint32_t m_stat_update_ms;
#ifdef BUILD_TLS_WITH_OPENSSL
        // -------------------------------------------------
        // tls server config
        // -------------------------------------------------
        SSL_CTX* m_tls_server_ctx;
        std::string m_tls_server_ctx_key;
        std::string m_tls_server_ctx_crt;
        std::string m_tls_server_ctx_cipher_list;
        std::string m_tls_server_ctx_options_str;
        long m_tls_server_ctx_options;
        // -------------------------------------------------
        // tls client config
        // -------------------------------------------------
        SSL_CTX *m_tls_client_ctx;
        std::string m_tls_client_ctx_cipher_list;
        std::string m_tls_client_ctx_options_str;
        long m_tls_client_ctx_options;
        std::string m_tls_client_ctx_ca_file;
        std::string m_tls_client_ctx_ca_path;
#endif
        // -------------------------------------------------
        // Defaults...
        // -------------------------------------------------
        t_conf():
#if defined(__linux__)
                m_evr_loop_type(EVR_LOOP_EPOLL),
#elif defined(__FreeBSD__) || defined(__APPLE__)
                m_evr_loop_type(EVR_LOOP_SELECT),
#else
                m_evr_loop_type(EVR_LOOP_SELECT),
#endif
                m_num_parallel(1024),
                m_timeout_ms(10000),
                m_num_reqs_per_conn(-1),
                m_resp_done_cb(NULL),
                m_nresolver(NULL),
                m_server_name("srvr"),
                m_dns_use_sync(false),
                m_stat_update_ms(0)
#ifdef BUILD_TLS_WITH_OPENSSL
                ,
                m_tls_server_ctx(NULL),
                m_tls_server_ctx_key(),
                m_tls_server_ctx_crt(),
                m_tls_server_ctx_cipher_list(),
                m_tls_server_ctx_options_str(),
                m_tls_server_ctx_options(0),
                m_tls_client_ctx(NULL),
                m_tls_client_ctx_cipher_list(),
                m_tls_client_ctx_options_str(),
                m_tls_client_ctx_options(0),
                m_tls_client_ctx_ca_file(),
                m_tls_client_ctx_ca_path()
#endif
        {}
private:
        // Disallow copy/assign
        t_conf& operator=(const t_conf &);
        t_conf(const t_conf &);
} conf_t;
//! ----------------------------------------------------------------------------
//! t_srvr
//! ----------------------------------------------------------------------------
class t_srvr
{
public:
        // -------------------------------------------------
        // Types
        // -------------------------------------------------
        typedef std::list <lsnr *> lsnr_list_t;
        typedef std::list <nconn *> listening_nconn_list_t;
        // -------------------------------------------------
        // Public methods
        // -------------------------------------------------
        t_srvr(const t_conf *a_t_conf);
        ~t_srvr();
        int32_t init(void);
        int run(void);
        void *t_run(void *a_nothing);
        void stop(void);
        bool is_running(void) { return !m_stopped; }
        uint32_t get_timeout_ms(void) { return m_t_conf->m_timeout_ms;};
        nconn *get_new_client_conn(scheme_t a_scheme, lsnr *a_lsnr);
        int32_t add_lsnr(lsnr &a_lsnr);
        int32_t queue_event(evr_event **ao_event,
                            evr_event_cb_t a_cb,
                            void *a_data);
        int32_t add_timer(uint32_t a_time_ms, evr_event_cb_t a_cb, void *a_data, void **ao_event);
        int32_t cancel_event(void *a_event);
        void signal(void);
        nbq *get_nbq(nbq *a_nbq);
        bool get_stopped(void) { return (bool)m_stopped; }
        const std::string &get_server_name(void) const { return m_t_conf->m_server_name;}
        evr_loop *get_evr_loop(void) {return m_evr_loop;}
        srvr *get_srvr_instance(void) { return m_srvr; }
        void set_srvr_instance(srvr *a_srvr) { m_srvr = a_srvr; }
        int32_t run_loop(void);
        int32_t get_stat(t_stat_cntr_t &ao_stat);
        static int32_t s_stat_update(void *a_data);
        void stat_update(void);
        // -------------------------------------------------
        // Public members
        // -------------------------------------------------
        // Needs to be public for now -to join externally
        pthread_t m_t_run_thread;
        // TODO hide -or prefer getters
        // Orphan q's
        nbq *m_orphan_in_q;
        nbq *m_orphan_out_q;
        t_stat_cntr_t m_stat;
        t_stat_cntr_t m_stat_cache;
        pthread_mutex_t m_stat_cache_mutex;
        const t_conf *m_t_conf;
        // -------------------------------------------------
        // *************************************************
        // proxy support
        // -------------------------------------------------
        // *************************************************
        subr_list_t m_subr_list;
        uint64_t m_subr_list_size;
        nconn_pool m_nconn_proxy_pool;
#ifdef ASYNC_DNS_SUPPORT
        nresolver::adns_ctx *m_adns_ctx;
#endif
private:
        // -------------------------------------------------
        // Private methods
        // -------------------------------------------------
        // Disallow copy/assign
        t_srvr& operator=(const t_srvr &);
        t_srvr(const t_srvr &);
        //Helper for pthreads
        static void *t_run_static(void *a_context)
        {
                return reinterpret_cast<t_srvr *>(a_context)->t_run(NULL);
        }
        // -------------------------------------------------
        // Private members
        // -------------------------------------------------
        sig_atomic_t m_stopped;
        int32_t m_start_time_s;
        evr_loop *m_evr_loop;
        lsnr_list_t m_lsnr_list;
        listening_nconn_list_t m_listening_nconn_list;
        bool m_is_initd;
        srvr *m_srvr;
};
} //namespace ns_is2 {
#endif // #ifndef _T_SRVR_HTTP

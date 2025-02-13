//! ----------------------------------------------------------------------------
//! Copyright Verizon.
//!
//! \file:    TODO
//! \details: TODO
//!
//! Licensed under the terms of the Apache 2.0 open source license.
//! Please refer to the LICENSE file in the project root for the terms.
//! ----------------------------------------------------------------------------
//! ----------------------------------------------------------------------------
//! Includes
//! ----------------------------------------------------------------------------
#include "is2/status.h"
#include "is2/support/trace.h"
#include "is2/support/time_util.h"
#include "is2/evr/evr.h"
#include "is2/support/ndebug.h"
#include "evr/evr_select.h"
#include "evr/evr_epoll.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>
namespace ns_is2 {
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
evr_loop::evr_loop(evr_loop_type_t a_type,
                   uint32_t a_max_events):
        m_event_pq(),
        m_max_events(a_max_events),
        m_loop_type(a_type),
        m_events(NULL),
        m_stopped(false),
        m_evr(NULL)
{
        // -------------------------------------------
        // TODO:
        // EPOLL specific for now
        // -------------------------------------------
        m_events = (evr_events_t *)malloc(sizeof(evr_events_t)*m_max_events);
        //std::vector<struct epoll_event> l_epoll_event_vector(m_max_parallel_connections);
        // -------------------------------------------
        // Get the event handler...
        // -------------------------------------------
#if defined(__linux__)
        if (m_loop_type == EVR_LOOP_EPOLL)
        {
                //NDBG_PRINT("Using evr_select\n");
                m_evr = new evr_epoll();
        }
#endif
        if (!m_evr)
        {
                //NDBG_PRINT("Using evr_select\n");
                m_evr = new evr_select();
        }
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
evr_loop::~evr_loop(void)
{
        // Clean out timer q
        while(!m_event_pq.empty())
        {
                evr_event_t *l_timer = m_event_pq.top();
                if (l_timer)
                {
                        delete l_timer;
                        l_timer = NULL;
                }
                m_event_pq.pop();
        }
        if (m_events)
        {
                free(m_events);
                m_events = NULL;
        }
        if (m_evr)
        {
                delete m_evr;
                m_evr = NULL;
        }
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
uint32_t evr_loop::dequeue_events(void)
{
        // -------------------------------------------------
        // field timers/timeouts
        // -------------------------------------------------
        evr_event_t *l_event = NULL;
        uint32_t l_time_diff_ms = EVR_DEFAULT_TIME_WAIT_MS;
        // Pop events off pq until time > now
        while(!m_event_pq.empty())
        {
                uint64_t l_now_ms = get_time_ms();
                l_event = m_event_pq.top();
                if (!l_event ||
                   (l_event->m_magic != EVR_EVENT_MAGIC))
                {
                        TRC_ERROR("bad event -ignoring.\n");
                        m_event_pq.pop();
                        continue;
                }
                if (l_event->m_state == EVR_EVENT_CANCELLED)
                {
                        //NDBG_PRINT("%sDELETING%s TIMER: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_timer);
                        m_event_pq.pop();
                        delete l_event;
                        l_event = NULL;
                        continue;
                }
                uint64_t l_ev_time_ms = l_event->m_time_ms;
                if (l_now_ms < l_ev_time_ms)
                {
                        l_time_diff_ms = l_ev_time_ms - l_now_ms;
                        break;
                }
                // remove -service event
                m_event_pq.pop();
                if (l_event->m_cb)
                {
                        //NDBG_PRINT("%sRUNNING_%s TIMER: %p at %lu ms\n",ANSI_COLOR_FG_YELLOW, ANSI_COLOR_OFF,l_event,l_now_ms);
                        int32_t l_s;
                        l_s = l_event->m_cb(l_event->m_data);
                        (void)l_s;
                }
                //NDBG_PRINT("%sDELETING%s TIMER: %p\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_timer);
                delete l_event;
                l_event = NULL;
        }
        return l_time_diff_ms;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t evr_loop::run(void)
{
        // -------------------------------------------------
        // field timers/timeouts
        // -------------------------------------------------
        uint32_t l_time_diff_ms;
        l_time_diff_ms = dequeue_events();
        // -------------------------------------------------
        // Wait for events
        // -------------------------------------------------
        int l_num_events = 0;
        //NDBG_PRINT("%sWAIT4_CONNECTIONS%s: l_time_diff_ms = %d\n", ANSI_COLOR_FG_RED, ANSI_COLOR_OFF, l_time_diff_ms);
        l_num_events = m_evr->wait(m_events, m_max_events, l_time_diff_ms);
        //NDBG_PRINT("%sSTART_CONNECTIONS%s: l_num_events = %d\n", ANSI_COLOR_FG_MAGENTA, ANSI_COLOR_OFF, l_num_events);
        if (l_num_events < 0)
        {
                TRC_ERROR("performing wait.\n");
                return STATUS_ERROR;
        }
        else if (l_num_events == 0)
        {
                // dequeue any pending timeouts
                l_time_diff_ms = dequeue_events();
                UNUSED(l_time_diff_ms);
                return STATUS_OK;
        }
        // -------------------------------------------------
        // service events
        // -------------------------------------------------
        for(int i_event = 0; (i_event < l_num_events) && (!m_stopped); ++i_event)
        {
                evr_fd_t* l_evr_fd = static_cast<evr_fd_t*>(m_events[i_event].data.ptr);
                uint32_t l_events = m_events[i_event].events;
                //NDBG_PRINT("%sEVENTS%s: l_events %d/%d = 0x%08X\n",
                //           ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF,
                //           i_event, l_num_events, l_events);
                // Service callbacks per type
                // -----------------------------------------
                // Validity checks
                // -----------------------------------------
                if (!l_evr_fd ||
                   (l_evr_fd->m_magic != EVR_EVENT_FD_MAGIC))
                {
                        TRC_ERROR("bad event -ignoring.\n");
                        continue;
                }
                // -----------------------------------------
                // in
                // -----------------------------------------
                if (l_events & EVR_EV_VAL_READABLE)
                {
                        if (l_evr_fd->m_read_cb &&
                          (l_evr_fd->m_attr_mask & EVR_FILE_ATTR_VAL_READABLE))
                        {
                                int32_t l_status;
                                //NDBG_PRINT("%sEVENTS%s: %s%sREAD%s\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, ANSI_COLOR_FG_RED, ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF);
                                l_status = l_evr_fd->m_read_cb(l_evr_fd->m_data);
                                if (l_status == STATUS_DONE)
                                {
                                        // Skip handling more events for this fd
                                        continue;
                                }
                                if (l_status != STATUS_OK)
                                {
                                        TRC_ERROR("performing read_cb\n");
                                        // Skip handling more events for this fd
                                        continue;
                                }
                        }
                        if (l_events & EVR_EV_HUP)
                        {
                                // Skip handling more events for this fd
                                //TRC_ERROR("EVR_EV_HUP\n");
                                //continue;
                        }
                        if (l_events & EVR_EV_ERR)
                        {
                                // Skip handling more events for this fd
                                TRC_ERROR("EVR_EV_ERR\n");
                                continue;
                        }
                }
                // -----------------------------------------
                // out
                // -----------------------------------------
                if (l_events & EVR_EV_VAL_WRITEABLE)
                {
                        if (l_evr_fd->m_write_cb &&
                          (l_evr_fd->m_attr_mask & EVR_FILE_ATTR_VAL_WRITEABLE))
                        {
                                int32_t l_status;
                                //NDBG_PRINT("%sEVENTS%s: %s%sWRITE%s\n", ANSI_COLOR_FG_CYAN, ANSI_COLOR_OFF, ANSI_COLOR_FG_BLUE, ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF);
                                l_status = l_evr_fd->m_write_cb(l_evr_fd->m_data);
                                if (l_status == STATUS_DONE)
                                {
                                        // Skip handling more events for this fd
                                        continue;
                                }
                                if (l_status != STATUS_OK)
                                {
                                        TRC_ERROR("performing write_cb\n");
                                        // Skip handling more events for this fd
                                        continue;
                                }
                        }
                }
                // -----------------------------------------
                // errors
                // -----------------------------------------
                // TODO other errors???
                // Currently "most" errors handled
                // by read callbacks
                // -----------------------------------------
                //uint32_t l_other_events = l_events & (~(EPOLLIN | EPOLLOUT));
                //if (l_events & EPOLLRDHUP)
                //if (l_events & EPOLLERR)
                //if (0)
                //{
                //        if (l_evr_event->m_error_cb)
                //        {
                //                int32_t l_status = STATUS_OK;
                //                l_status = l_evr_event->m_error_cb(l_evr_event->m_data);
                //                if (l_status == STATUS_DONE)
                //                {
                //                        // Skip handling more events for this fd
                //                        continue;
                //                }
                //                if (l_status != STATUS_OK)
                //                {
                //                        //NDBG_PRINT("Error: l_status: %d\n", l_status);
                //                        // Skip handling more events for this fd
                //                        continue;
                //                }
                //        }
                //}
        }
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t evr_loop::add_fd(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        if (!a_evr_fd_event)
        {
                return STATUS_ERROR;
        }
        int l_status;
        //NDBG_PRINT("%sADD_FD%s: fd[%d], mask: 0x%08X\n", ANSI_COLOR_BG_WHITE, ANSI_COLOR_OFF, a_fd, a_attr_mask);
        l_status = m_evr->add(a_fd, a_attr_mask, a_evr_fd_event);
        a_evr_fd_event->m_attr_mask = a_attr_mask;
        return l_status;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t evr_loop::mod_fd(int a_fd, uint32_t a_attr_mask, evr_fd_t *a_evr_fd_event)
{
        if (!a_evr_fd_event)
        {
                return STATUS_ERROR;
        }
        int l_status;
        //NDBG_PRINT("%sMOD_FD%s: fd[%d], mask: 0x%08X\n", ANSI_COLOR_FG_WHITE, ANSI_COLOR_OFF, a_fd, a_attr_mask);
        l_status = m_evr->mod(a_fd, a_attr_mask, a_evr_fd_event);
        a_evr_fd_event->m_attr_mask = a_attr_mask;
        return l_status;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t evr_loop::del_fd(int a_fd)
{
        if (!m_evr)
        {
                return STATUS_OK;
        }
        int l_status;
        l_status = m_evr->del(a_fd);
        return l_status;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t evr_loop::add_event(uint32_t a_time_ms,
                            evr_event_cb_t a_cb,
                            void *a_data,
                            evr_event_t **ao_event)
{
        evr_event_t *l_event = new evr_event_t();
        l_event->m_magic = EVR_EVENT_MAGIC;
        l_event->m_data = a_data;
        l_event->m_state = EVR_EVENT_ACTIVE;
        l_event->m_time_ms = get_time_ms() + a_time_ms;
        l_event->m_cb = a_cb;
        //NDBG_PRINT("%sADDING__%s[%p] TIMER: %p at %u ms --> %lu\n",
        //        ANSI_COLOR_FG_GREEN, ANSI_COLOR_OFF,
        //        a_data, l_timer,a_time_ms, l_timer->m_time_ms);
        //NDBG_PRINT_BT();
        m_event_pq.push(l_event);
        *ao_event = l_event;
        return STATUS_OK;
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t evr_loop::cancel_event(evr_event_t *a_event)
{
        //NDBG_PRINT("%sCANCEL__%s TIMER: %p\n",ANSI_COLOR_BG_RED, ANSI_COLOR_OFF,a_timer);
        //NDBG_PRINT_BT();
        // TODO synchronization???
        if (a_event)
        {
                //printf("%sXXX%s: %p TIMER AT %24lu ms --> %24lu\n",ANSI_COLOR_FG_RED, ANSI_COLOR_OFF,a_timer,0,l_timer_event->m_time_ms);
                a_event->m_cb = NULL;
                a_event->m_state = EVR_EVENT_CANCELLED;
                a_event->m_data = NULL;
                a_event->m_cb = NULL;
                return STATUS_OK;
        }
        else
        {
                return STATUS_ERROR;
        }
}
//! ----------------------------------------------------------------------------
//! \details: TODO
//! \return:  TODO
//! \param:   TODO
//! ----------------------------------------------------------------------------
int32_t evr_loop::signal(void)
{
        //NDBG_PRINT("%sSIGNAL%s\n", ANSI_COLOR_BG_RED, ANSI_COLOR_OFF);
        if (!m_evr)
        {
                return STATUS_ERROR;
        }
        return m_evr->signal();
}
} //namespace ns_is2 {

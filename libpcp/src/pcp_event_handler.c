/*
 * Copyright (c) 2013 by Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#ifdef WIN32
#include "pcp_win_defines.h"
#include "pcp_gettimeofday.h"
#else
//#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#endif

#include "pcp.h"
#include "pcp_utils.h"
#include "pcp_msg.h"
#include "pcp_msg_structs.h"
#include "pcp_logger.h"
#include "pcp_event_handler.h"
#include "pcp_server_discovery.h"

//   IRT:   Initial retransmission time, SHOULD be 3 seconds
#define PCP_RETX_IRT 3
//   MRC:   Maximum retransmission count, SHOULD be 0 (0 indicates no maximum)
#define PCP_RETX_MRC 3
//   MRT:   Maximum retransmission time, SHOULD be 1024 seconds
#define PCP_RETX_MRT 1024
//   MRD:   Maximum retransmission duration, SHOULD be 0(0 indicates no maximum)
#define PCP_RETX_MRD 0
#define MIN(a, b) (a<b?a:b)
#define MAX(a, b) (a>b?a:b)
#define PCP_RT(rtprev) ((rtprev=rtprev<<1),(((8192+(1024-(rand()&2047))) \
        * MIN (MAX(rtprev,PCP_RETX_IRT), PCP_RETX_MRT))>>13))


static pcp_flow_event_e
fhndl_send(pcp_flow_t f, pcp_recv_msg_t* msg);

static pcp_flow_event_e
fhndl_resend(pcp_flow_t f, pcp_recv_msg_t* msg);

static pcp_flow_event_e
fhndl_send_renew(pcp_flow_t f, pcp_recv_msg_t* msg);

static pcp_flow_event_e
fhndl_shortlifeerror(pcp_flow_t f, pcp_recv_msg_t* msg);

static pcp_flow_event_e
fhndl_received_success(pcp_flow_t f, pcp_recv_msg_t* msg);

static pcp_flow_event_e
fhndl_clear_timeouts(pcp_flow_t f, pcp_recv_msg_t* msg);

static pcp_flow_event_e
fhndl_waitresp(pcp_flow_t f, pcp_recv_msg_t* msg);

static pcp_server_state_e handle_wait_io_receive_msg(pcp_server_t* s);
static pcp_server_state_e handle_server_ping(pcp_server_t *s);
static pcp_server_state_e handle_wait_ping_resp_timeout(pcp_server_t *s);
static pcp_server_state_e handle_wait_ping_resp_recv(pcp_server_t* s);
static pcp_server_state_e handle_version_negotiation(pcp_server_t* s);
static pcp_server_state_e handle_send_all_msgs(pcp_server_t* s);
static pcp_server_state_e handle_server_restart(pcp_server_t* s);
static pcp_server_state_e handle_wait_io_timeout(pcp_server_t *s);
static pcp_server_state_e handle_server_set_not_working(pcp_server_t *s);
static pcp_server_state_e handle_server_not_working(pcp_server_t *s);
static pcp_server_state_e handle_server_reping(pcp_server_t *s);
static pcp_server_state_e pcp_terminate_server(pcp_server_t* s);
static pcp_server_state_e log_unexepected_state_event(pcp_server_t* s);

#if PCP_MAX_LOG_LEVEL>=PCP_DEBUG_DEBUG


//LCOV_EXCL_START
static const char* dbg_get_func_name(void* f)
{
    if (f==fhndl_send) {
        return "fhndl_send";
    } else if (f==fhndl_send_renew) {
        return "fhndl_send_renew";
    } else if (f==fhndl_resend) {
        return "fhndl_resend";
    } else if (f==fhndl_shortlifeerror) {
        return "fhndl_shortlifeerror";
    } else if (f==fhndl_received_success) {
        return "fhndl_received_success";
    } else if (f==fhndl_clear_timeouts) {
        return "fhndl_clear_timeouts";
    } else if (f==fhndl_waitresp) {
        return "fhndl_waitresp";
    } else if (f==handle_wait_io_receive_msg) {
        return "handle_wait_io_receive_msg";
    } else if (f==handle_server_ping) {
        return "handle_server_ping";
    } else if (f==handle_wait_ping_resp_timeout) {
        return "handle_wait_ping_resp_timeout";
    } else if (f==handle_wait_ping_resp_recv) {
        return "handle_wait_ping_resp_recv";
    } else if (f==handle_version_negotiation) {
        return "handle_version_negotiation";
    } else if (f==handle_send_all_msgs) {
        return "handle_send_all_msgs";
    } else if (f==handle_server_restart) {
        return "handle_server_restart";
    } else if (f==handle_wait_io_timeout) {
        return "handle_wait_io_timeout";
    } else if (f==handle_server_set_not_working) {
        return "handle_server_set_not_working";
    } else if (f==handle_server_not_working) {
        return "handle_server_not_working";
    } else if (f==handle_server_reping) {
        return "handle_server_reping";
    } else if (f==pcp_terminate_server) {
        return "pcp_terminate_server";
    } else if (f==log_unexepected_state_event) {
        return "log_unexepected_state_event";
    } else {
        return "unknown";
    }
}

static const char *dbg_get_event_name(pcp_flow_event_e ev)
{
    static const char*event_names[]={
            "fev_flow_timedout",
            "fev_server_initialized",
            "fev_send",
            "fev_msg_sent",
            "fev_failed",
            "fev_none",
            "fev_server_restarted",
            "fev_ignored",
            "fev_res_success",
            "fev_res_unsupp_version",
            "fev_res_not_authorized",
            "fev_res_malformed_request",
            "fev_res_unsupp_opcode",
            "fev_res_unsupp_option",
            "fev_res_malformed_option",
            "fev_res_network_failure",
            "fev_res_no_resources",
            "fev_res_unsupp_protocol",
            "fev_res_user_ex_quota",
            "fev_res_cant_provide_ext",
            "fev_res_address_mismatch",
            "fev_res_exc_remote_peers",
    };

    if (((int)ev>=0)&&(ev<sizeof(event_names)/sizeof(event_names[0]))) {
        return event_names[ev];
    } else {
        return "";
    }
}

static const char *dbg_get_state_name(pcp_flow_state_e s)
{
    static const char*state_names[]={
            "pfs_idle",
            "pfs_wait_for_server_init",
            "pfs_send",
            "pfs_wait_resp",
            "pfs_wait_after_short_life_error",
            "pfs_wait_for_lifetime_renew",
            "pfs_send_renew",
            "pfs_failed"
    };

    if ((s>=0)&&(s<(int)(sizeof(state_names)/sizeof(state_names[0])))) {
        return state_names[s];
    } else {
        return "";
    }
}

static const char *dbg_get_sevent_name(pcp_event_e ev)
{
    static const char*sevent_names[]={
        "pcpe_any",
        "pcpe_timeout",
        "pcpe_io_event",
        "pcpe_terminate"
    };

    if (((int)ev>=0)&&(ev<sizeof(sevent_names)/sizeof(sevent_names[0]))) {
        return sevent_names[ev];
    } else {
        return "";
    }
}

static const char *dbg_get_sstate_name(pcp_server_state_e s)
{
    static const char*server_state_names[]={
        "pss_unitialized",
        "pss_allocated",
        "pss_ping",
        "pss_wait_ping_resp",
        "pss_version_negotiation",
        "pss_send_all_msgs",
        "pss_wait_io",
        "pss_wait_io_calc_nearest_timeout",
        "pss_server_restart",
        "pss_server_reping",
        "pss_set_not_working",
        "pss_not_working"
    };

    if ((s>=0)&&
        (s<(int)(sizeof(server_state_names)/sizeof(server_state_names[0])))) {
        return server_state_names[s];
    } else {
        return "";
    }
}

static const char *dbg_get_fstate_name(pcp_fstate_e s)
{
    static const char* flow_state_names[]={
        "pcp_state_processing",
        "pcp_state_succeeded",
        "pcp_state_partial_result",
        "pcp_state_short_lifetime_error",
        "pcp_state_failed"
    };

    if ((s>=0)&&(s<(int)sizeof(flow_state_names)/sizeof(flow_state_names[0]))) {
        return flow_state_names[s];
    } else {
        return "";
    }
}
//LCOV_EXCL_STOP
#endif

////////////////////////////////////////////////////////////////////////////////
//                  Flow State Machine definition

typedef pcp_flow_event_e
        (*handle_flow_state_event)(pcp_flow_t f, pcp_recv_msg_t* msg);

typedef struct pcp_flow_state_trans {
    pcp_flow_state_e state_from;
    pcp_flow_state_e state_to;
    handle_flow_state_event handler;
} pcp_flow_state_trans_t;

pcp_flow_state_trans_t flow_transitions[] = {
    { pfs_any,  pfs_wait_resp, fhndl_waitresp },
    { pfs_wait_resp, pfs_send, fhndl_resend },
    { pfs_any, pfs_send, fhndl_send },
    { pfs_any, pfs_wait_after_short_life_error, fhndl_shortlifeerror},
    { pfs_wait_resp, pfs_wait_for_lifetime_renew, fhndl_received_success},
    { pfs_any, pfs_send_renew, fhndl_send_renew},
    { pfs_wait_for_lifetime_renew, pfs_wait_for_lifetime_renew, fhndl_received_success},
    { pfs_any, pfs_wait_for_server_init, fhndl_clear_timeouts},
    { pfs_any, pfs_failed, fhndl_clear_timeouts },
};

#define FLOW_TRANS_COUNT (sizeof(flow_transitions)/sizeof(*flow_transitions))

typedef struct pcp_flow_state_event {
    pcp_flow_state_e state;
    pcp_flow_event_e event;
    pcp_flow_state_e new_state;
} pcp_flow_state_events_t;

pcp_flow_state_events_t flow_events_sm[] = {
    { pfs_any, fev_send, pfs_send},
    { pfs_wait_for_server_init, fev_server_initialized, pfs_send},
    { pfs_wait_resp, fev_res_success, pfs_wait_for_lifetime_renew},
    { pfs_wait_resp, fev_res_unsupp_version, pfs_wait_for_server_init},
    { pfs_wait_resp, fev_res_network_failure, pfs_wait_after_short_life_error},
    { pfs_wait_resp, fev_res_no_resources, pfs_wait_after_short_life_error},
    { pfs_wait_resp, fev_res_exc_remote_peers, pfs_wait_after_short_life_error},
    { pfs_wait_resp, fev_res_user_ex_quota, pfs_wait_after_short_life_error},
    { pfs_wait_resp, fev_flow_timedout, pfs_send},
    { pfs_wait_resp, fev_server_initialized, pfs_send},
    { pfs_send, fev_server_initialized, pfs_send},
    { pfs_send, fev_msg_sent, pfs_wait_resp},
    { pfs_send, fev_flow_timedout, pfs_send},
    { pfs_wait_after_short_life_error, fev_flow_timedout, pfs_send},
    { pfs_wait_for_lifetime_renew, fev_flow_timedout, pfs_send_renew},
    { pfs_wait_for_lifetime_renew, fev_res_success, pfs_wait_for_lifetime_renew},
    { pfs_wait_for_lifetime_renew, fev_res_unsupp_version, pfs_wait_for_server_init},
    { pfs_wait_for_lifetime_renew, fev_res_network_failure, pfs_send_renew},
    { pfs_wait_for_lifetime_renew, fev_res_no_resources, pfs_send_renew},
    { pfs_wait_for_lifetime_renew, fev_res_exc_remote_peers, pfs_send_renew},
    { pfs_wait_for_lifetime_renew, fev_failed, pfs_send},
    { pfs_wait_for_lifetime_renew, fev_res_user_ex_quota, pfs_send_renew},
    { pfs_send_renew, fev_msg_sent, pfs_wait_for_lifetime_renew},
    { pfs_send_renew, fev_flow_timedout, pfs_send_renew},
    { pfs_send_renew, fev_failed, pfs_send},
    { pfs_send, fev_ignored, pfs_wait_for_lifetime_renew},
//    { pfs_failed, fev_server_restarted, pfs_send},
    { pfs_any, fev_server_restarted, pfs_send},
    { pfs_any, fev_failed, pfs_failed},
///////////////////////////////////////////////////////////////////////////////
//                  Long lifetime Error Responses from PCP server
    { pfs_wait_resp, fev_res_not_authorized, pfs_failed},
    { pfs_wait_resp, fev_res_malformed_request, pfs_failed},
    { pfs_wait_resp, fev_res_unsupp_opcode, pfs_failed},
    { pfs_wait_resp, fev_res_unsupp_option, pfs_failed},
    { pfs_wait_resp, fev_res_unsupp_protocol, pfs_failed},
    { pfs_wait_resp, fev_res_cant_provide_ext, pfs_failed},
    { pfs_wait_resp, fev_res_address_mismatch, pfs_failed},
    { pfs_wait_for_lifetime_renew, fev_res_not_authorized, pfs_failed},
    { pfs_wait_for_lifetime_renew, fev_res_malformed_request, pfs_failed},
    { pfs_wait_for_lifetime_renew, fev_res_unsupp_opcode, pfs_failed},
    { pfs_wait_for_lifetime_renew, fev_res_unsupp_option, pfs_failed},
    { pfs_wait_for_lifetime_renew, fev_res_unsupp_protocol, pfs_failed},
    { pfs_wait_for_lifetime_renew, fev_res_cant_provide_ext, pfs_failed},
    { pfs_wait_for_lifetime_renew, fev_res_address_mismatch, pfs_failed},
};

#define FLOW_EVENTS_SM_COUNT (sizeof(flow_events_sm)/sizeof(*flow_events_sm))

static pcp_errno pcp_flow_send_msg(pcp_flow_t flow, pcp_server_t *s)
{
    ssize_t ret;
    size_t to_send_count;
    unsigned long iMode = 1;

    OSDEP(iMode);

    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);

    if ((!flow->pcp_msg_buffer) || (flow->pcp_msg_len == 0)) {
        build_pcp_msg(flow);
        if (flow->pcp_msg_buffer == NULL) {
            PCP_LOGGER(PCP_DEBUG_DEBUG, "Cannot build PCP MSG (flow bucket:%d)",
                    flow->key_bucket);
            PCP_LOGGER_END(PCP_DEBUG_DEBUG);
            return PCP_ERR_SEND_FAILED;
        }
    }

    to_send_count = flow->pcp_msg_len;

    while (to_send_count != 0) {
        ret = flow->pcp_msg_len - to_send_count;
#ifdef WIN32
        ioctlsocket(s->pcp_server_socket, FIONBIO, &iMode);
        ret = send(s->pcp_server_socket, flow->pcp_msg_buffer + ret,
            flow->pcp_msg_len - ret, 0);
#else
        ret = send(s->pcp_server_socket, flow->pcp_msg_buffer + ret,
                flow->pcp_msg_len - ret, MSG_DONTWAIT);
#endif //WIN32
        if (ret == PCP_SOCKET_ERROR) { //LCOV_EXCL_START
#ifdef WIN32
            int wsa_error = WSAGetLastError();
            if ( (wsa_error == EAGAIN) || (wsa_error == WSAEWOULDBLOCK) ) {
#else
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
#endif //WIN32
                break;
            } else {
                char buff[128];
                pcp_strerror(errno, buff, sizeof(buff));
                PCP_LOGGER(PCP_DEBUG_WARN, "Error (%s) occurred while sending "
                        "PCP packet to server %s", buff, s->pcp_server_paddr);
                PCP_LOGGER_END(PCP_DEBUG_DEBUG);
                return PCP_ERR_SEND_FAILED;
            }
        }                               //LCOV_EXCL_STOP
        to_send_count -= ret;
    }

    PCP_LOGGER(PCP_DEBUG_INFO, "Sent PCP MSG (flow bucket:%d)",
            flow->key_bucket);

    pcp_flow_clear_msg_buf(flow);

    if (to_send_count == 0) {
        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return PCP_ERR_SUCCESS;
    } else {                            //LCOV_EXCL_START
        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return PCP_ERR_SEND_FAILED;
    }                                   //LCOV_EXCL_STOP

    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return PCP_ERR_SUCCESS;
}

static pcp_errno read_msg_from_socket(PCP_SOCKET socket, pcp_recv_msg_t *msg)
{
    ssize_t ret;
    socklen_t src_len = sizeof(msg->rcvd_from_addr);
    unsigned long iMode;

    OSDEP(iMode);
    memset(msg, 0, sizeof(*msg));

#ifdef WIN32
    ioctlsocket(socket, FIONBIO, &iMode);
    if ((ret = recvfrom(socket, msg->pcp_msg_buffer,
            sizeof(msg->pcp_msg_buffer), 0,
            (struct sockaddr*) &msg->rcvd_from_addr, &src_len)) == PCP_SOCKET_ERROR) {
#else
    if ((ret = recvfrom(socket, msg->pcp_msg_buffer,
            sizeof(msg->pcp_msg_buffer), MSG_DONTWAIT,
            (struct sockaddr*) &msg->rcvd_from_addr, &src_len)) == PCP_SOCKET_ERROR) {
#endif //WIN32
        char err_buf[ERR_BUF_LEN];

        pcp_strerror(errno, err_buf, sizeof(err_buf));

        PCP_LOGGER(PCP_DEBUG_PERR, "FD: %d, recv: %s", socket, err_buf);

        return PCP_ERR_RECV_FAILED;
    }

    msg->pcp_msg_len = ret;

    return PCP_ERR_SUCCESS;
}


///////////////////////////////////////////////////////////////////////////////
//              Flow State Transitions Handlers

static pcp_flow_event_e fhndl_send(pcp_flow_t f, UNUSED pcp_recv_msg_t* msg)
{
    pcp_server_t*s = get_pcp_server(f->pcp_server_indx);
    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);

    if (!s) {                       //LCOV_EXCL_START
        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return fev_failed;
    }                               //LCOV_EXCL_STOP

    if (s->restart_flow_msg == f) {
        return fev_ignored;
    }

    if (pcp_flow_send_msg(f, s)!=PCP_ERR_SUCCESS) {
        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return fev_failed;
    }

    f->resend_timeout = PCP_RETX_IRT;
    //set timeout field
    gettimeofday(&f->timeout, NULL);
    f->timeout.tv_sec += f->resend_timeout;

    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return fev_msg_sent;
}

static pcp_flow_event_e fhndl_resend(pcp_flow_t f, UNUSED pcp_recv_msg_t* msg)
{
    pcp_server_t* s = get_pcp_server(f->pcp_server_indx);
    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);

    if (!s) {                       //LCOV_EXCL_START
        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return fev_failed;
    }                               //LCOV_EXCL_STOP

#if PCP_RETX_MRC>0
    if (++f->retry_count > PCP_RETX_MRC) {
        return fev_failed;
    }
#endif

    if (pcp_flow_send_msg(f, s)!=PCP_ERR_SUCCESS) {
        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return fev_failed;
    }

    f->resend_timeout = PCP_RT(f->resend_timeout);

#if (PCP_RETX_MRD>0)
    {
        int tdiff = curtime - f->created_time;
        if (tdiff > PCP_RETX_MRD) {
            return fev_failed;
        }
        if (tdiff > f->resend_timeout) {
            f->resend_timeout = tdiff;
        }
    }
#endif

    //set timeout field
    gettimeofday(&f->timeout, NULL);
    f->timeout.tv_sec += f->resend_timeout;

    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return fev_msg_sent;
}

static pcp_flow_event_e
fhndl_shortlifeerror(pcp_flow_t f, pcp_recv_msg_t* msg)
{
    PCP_LOGGER(PCP_DEBUG_DEBUG,
            "f->pcp_server_index=%d, f->state = %d, f->key_bucket=%d",
            f->pcp_server_indx, f->state, f->key_bucket);

    f->recv_result = msg->recv_result;

    gettimeofday(&f->timeout, NULL);
    f->timeout.tv_sec += msg->recv_lifetime;

    return fev_none;
}

static pcp_flow_event_e
fhndl_received_success(pcp_flow_t f, pcp_recv_msg_t* msg)
{
    struct timeval ctv;

    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);
    f->recv_lifetime = msg->received_time + msg->recv_lifetime;
    if ((f->kd.operation == PCP_OPCODE_MAP) ||
            (f->kd.operation == PCP_OPCODE_PEER)) {
        f->map_peer.ext_ip = msg->assigned_ext_ip;
        f->map_peer.ext_port = msg->assigned_ext_port;
    } else if (f->kd.operation == PCP_OPCODE_SADSCP) {
        f->sadscp.learned_dscp = msg->recv_dscp;
    }
    f->recv_result = msg->recv_result;

    gettimeofday(&ctv, NULL);

    if (msg->recv_lifetime == 0) {
        f->timeout.tv_sec = 0;
        f->timeout.tv_usec = 0;
    } else {
        f->timeout = ctv;
        f->timeout.tv_sec += (long int) ((f->recv_lifetime - ctv.tv_sec)>>1);
    }

    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return fev_none;
}

static pcp_flow_event_e
fhndl_send_renew(pcp_flow_t f, UNUSED pcp_recv_msg_t* msg)
{
    pcp_server_t* s = get_pcp_server(f->pcp_server_indx);
    long timeout_add;

    if (!s) {                       //LCOV_EXCL_START
        return fev_failed;
    }                               //LCOV_EXCL_STOP

    if (pcp_flow_send_msg(f, s)!=PCP_ERR_SUCCESS) {
        return fev_failed;
    }

    gettimeofday(&f->timeout, NULL);
    timeout_add = (long) ((f->recv_lifetime - f->timeout.tv_sec)>>1);

    if (timeout_add == 0) {
        return fev_failed;
    } else {
        f->timeout.tv_sec += timeout_add;
    }

    return fev_msg_sent;
}


static pcp_flow_event_e
fhndl_clear_timeouts(pcp_flow_t f, pcp_recv_msg_t* msg)
{
    if (msg) {
        f->recv_result = msg->recv_result;
    }
    pcp_flow_clear_msg_buf(f);
    f->timeout.tv_sec=0;
    f->timeout.tv_usec=0;

    return fev_none;
}

static pcp_flow_event_e fhndl_waitresp(pcp_flow_t f, UNUSED pcp_recv_msg_t* msg)
{
    struct timeval ctv;
    gettimeofday(&ctv, NULL);
    if (timeval_comp(&f->timeout, &ctv) < 0) {
        return fev_failed;
    }

    return fev_none;
}

static void flow_change_notify(pcp_flow_t flow, pcp_fstate_e state);

static pcp_flow_state_e
handle_flow_event(pcp_flow_t f, pcp_flow_event_e ev, pcp_recv_msg_t *r)
{
    pcp_flow_state_e cur_state = f->state, next_state;
    pcp_flow_state_events_t *esm;
    pcp_flow_state_events_t *esm_end = flow_events_sm + FLOW_EVENTS_SM_COUNT;
    pcp_flow_state_trans_t *trans;
    pcp_flow_state_trans_t *trans_end = flow_transitions + FLOW_TRANS_COUNT;
    pcp_fstate_e before, after;
    struct in6_addr prev_ext_addr = f->map_peer.ext_ip;
    in_port_t prev_ext_port = f->map_peer.ext_port;

    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);
    pcp_eval_flow_state(f, &before);
    for(;;) {
        for (esm = flow_events_sm; esm<esm_end; ++esm) {
            if (((esm->state == cur_state) || (esm->state == pfs_any)) &&
                 (esm->event == ev)) {
                break;
            }
        }

        if (esm==esm_end) {
            //TODO:log
            goto end;
        }

        next_state = esm->new_state;

        for (trans = flow_transitions; trans<trans_end; ++trans) {
            if (((trans->state_from == cur_state)
                    || (trans->state_from == pfs_any))
                && (trans->state_to == next_state)) {

#if PCP_MAX_LOG_LEVEL>=PCP_DEBUG_DEBUG
                pcp_flow_event_e prev_ev = ev;
#endif
                f->state=next_state;

                PCP_LOGGER_DEBUG(
                        "Executing event handler %s\n    flow \t: %d (server %d)\n"
                        "    states\t: %s => %s\n    event\t: %s",
                        dbg_get_func_name(trans->handler),
                        f->key_bucket, f->pcp_server_indx,
                        dbg_get_state_name(cur_state),
                        dbg_get_state_name(next_state),
                        dbg_get_event_name(prev_ev));

                ev = trans->handler(f, r);

                PCP_LOGGER_DEBUG(
                        "Return from event handler's %s \n    result event: %s",
                        dbg_get_func_name(trans->handler),
                        dbg_get_event_name(ev));

                cur_state = next_state;

                if (ev == fev_none) {
                    goto end;
                }
                break;
            }
        }

        //no transition handler
        if (trans==trans_end) {
            f->state=next_state;
            goto end;
        }
    }
end:
    pcp_eval_flow_state(f, &after);
    if ((before!=after) ||
            (!IN6_ARE_ADDR_EQUAL(&prev_ext_addr, &f->map_peer.ext_ip)) ||
            (prev_ext_port!=f->map_peer.ext_port)) {
        flow_change_notify(f, after);
    }
    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return f->state;
}

///////////////////////////////////////////////////////////////////////////////
//            Helper functions for server state handlers

static pcp_flow_t server_process_rcvd_pcp_msg(pcp_server_t *s, pcp_recv_msg_t* msg)
{

    if (msg->recv_version == 0) {  //LCOV_EXCL_START
        //TODO: extend version support to NAT-PMP
        if (msg->kd.operation == NATPMP_OPCODE_ANNOUNCE) {

        }
        return NULL;
    } else { //LCOV_EXCL_STOP
        pcp_flow_t f = pcp_get_flow(&msg->kd, s->index);
        if (!f) {
            char in6[INET6_ADDRSTRLEN];
            PCP_LOGGER(PCP_DEBUG_INFO,"%s",
                    "Couldn't find matching flow to received PCP message.");
            PCP_LOGGER(PCP_DEBUG_PERR, "  Operation   : %u", msg->kd.operation);
            if ((msg->kd.operation == PCP_OPCODE_MAP) ||
                (msg->kd.operation == PCP_OPCODE_PEER)) {
                PCP_LOGGER(PCP_DEBUG_PERR, "  Protocol    : %u",
                        msg->kd.map_peer.protocol);
                PCP_LOGGER(PCP_DEBUG_PERR, "  Source      : %s:%hu",
                        inet_ntop(s->af, &msg->kd.src_ip, in6,
                            sizeof(in6)),
                        ntohs(msg->kd.map_peer.src_port));
                PCP_LOGGER(PCP_DEBUG_PERR, "  Destination : %s:%hu",
                        inet_ntop(s->af, &msg->kd.map_peer.dst_ip, in6,
                            sizeof(in6)),
                        ntohs(msg->kd.map_peer.dst_port));
            } else {
                //TODO: add print of SADSCP params
            }
            return NULL;
        }

        PCP_LOGGER(PCP_DEBUG_INFO,
                "Found matching flow %d to received PCP message.",
                f->key_bucket);

        handle_flow_event(f, FEV_RES_BEGIN + msg->recv_result, msg);
        return f;
    }
}

static int check_flow_timeout(pcp_flow_t f, void * timeout)
{
    struct timeval * tout = timeout;
    struct timeval ctv;

    if ((f->timeout.tv_sec==0) && (f->timeout.tv_usec==0)) {
        return 0;
    }

    gettimeofday(&ctv, NULL);
    if (timeval_comp(&f->timeout, &ctv)<=0) {
        // timed out
        if (f->state == pfs_wait_resp) {
            PCP_LOGGER(PCP_DEBUG_WARN,
                "Recv of PCP response for flow %d timed out.", f->key_bucket);
        }
        handle_flow_event(f, fev_flow_timedout, NULL);
    }

    if ((f->timeout.tv_sec==0) && (f->timeout.tv_usec==0)) {
        return 0;
    }

    timeval_subtract(&ctv, &f->timeout, &ctv);

    if ((tout->tv_sec == 0) && (tout->tv_usec == 0)) {
        *tout = ctv;
        return 0;
    }

    if (timeval_comp(&ctv, tout) < 0) {
        *tout = ctv;
    }
    return 0;
}

struct get_first_flow_iter_data {
    pcp_server_t * s;
    pcp_flow_t   msg;
};

static int get_first_flow_iter(pcp_flow_t f, void* data)
{
    struct get_first_flow_iter_data  *d =
            (struct get_first_flow_iter_data  *) data;

    if (f->pcp_server_indx == d->s->index) {
        d->msg = f;
        return 1;
    } else {
        return 0;
    }
}

static inline pcp_flow_t get_ping_msg(pcp_server_t *s)
{
    struct get_first_flow_iter_data find_data;

    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);
    if (!s)
        return NULL;

    find_data.s = s;
    find_data.msg = NULL;

    pcp_db_foreach_flow(get_first_flow_iter, &find_data);

    s->ping_flow_msg = find_data.msg;

    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return find_data.msg;
}

struct flow_iterator_data {
    pcp_server_t * s;
    pcp_event_e   event;
};

static int flow_send_event_iter(pcp_flow_t f, void* data)
{
    struct flow_iterator_data * d = (struct flow_iterator_data *) data;

    if (f->pcp_server_indx == d->s->index) {
        handle_flow_event(f, d->event, NULL);
        check_flow_timeout(f, &d->s->next_timeout);
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//                 Server state machine event handlers

static pcp_server_state_e handle_server_ping(pcp_server_t *s)
{
    pcp_flow_t msg;
    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);
    s->ping_count = 0;
    if (s->pcp_server_socket == PCP_INVALID_SOCKET) {
        if (psd_create_pcp_server_socket(s->index) != PCP_ERR_SUCCESS) {

            gettimeofday(&s->next_timeout, NULL);

            return pss_set_not_working;
        }
    }

    msg = get_ping_msg(s);

    if (!msg) {
        s->next_timeout.tv_sec = 0;
        s->next_timeout.tv_usec = 0;
        return pss_ping;
    }

    msg->retry_count = 0;

    PCP_LOGGER(PCP_DEBUG_INFO, "Pinging PCP server at address %s",
            s->pcp_server_paddr);

    if (handle_flow_event(msg, fev_send, NULL) != pfs_failed) {
        s->next_timeout = msg->timeout;

        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return pss_wait_ping_resp;
    }

    gettimeofday(&s->next_timeout, NULL);
    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return pss_set_not_working;
}

static pcp_server_state_e handle_wait_ping_resp_timeout(pcp_server_t *s)
{
    if (++s->ping_count >= PCP_MAX_PING_COUNT) {
        gettimeofday(&s->next_timeout, NULL);
        return pss_set_not_working;
    }

    if (!s->ping_flow_msg) {
        gettimeofday(&s->next_timeout, NULL);
        return pss_ping;
    }

    if (handle_flow_event(s->ping_flow_msg, fev_flow_timedout, NULL)
            == pfs_failed) {
        gettimeofday(&s->next_timeout, NULL);
        return pss_set_not_working;
    }

    s->next_timeout = s->ping_flow_msg->timeout;
    return pss_wait_ping_resp;
}

static pcp_server_state_e handle_wait_ping_resp_recv(pcp_server_t* s)
{
    pcp_server_state_e res = handle_wait_io_receive_msg(s);
    switch (res) {
    case pss_wait_io_calc_nearest_timeout : res = pss_send_all_msgs; break;
    case pss_wait_io                      : res = pss_wait_ping_resp; break;
    default                               : break;
    }
    return res;
}

static pcp_server_state_e handle_version_negotiation(pcp_server_t* s)
{
    pcp_flow_t ping_msg;
    if (s->pcp_version == 0) {
        PCP_LOGGER(PCP_DEBUG_WARN,
                "Version negotiation failed for PCP server %s. "
                        "Disabling sending PCP messages to this server.",
                s->pcp_server_paddr);

        return pss_set_not_working;
    }

    s->pcp_version--;
    PCP_LOGGER(PCP_DEBUG_INFO,
            "Version %d not supported by server %s. Trying version %d.",
            s->pcp_version + 1, s->pcp_server_paddr, s->pcp_version);

    ping_msg = s->ping_flow_msg;
    if (!ping_msg) {
        ping_msg = get_ping_msg(s);
        if (!ping_msg) {
            s->next_timeout.tv_sec = 0;
            s->next_timeout.tv_usec = 0;
            return pss_ping;
        }
    }

    ping_msg->retry_count = 0;
    ping_msg->resend_timeout = 0;

    handle_flow_event(ping_msg, fev_send, NULL);
    if (ping_msg->state == pfs_failed) {
        return pss_set_not_working;
    }

    s->next_timeout = ping_msg->timeout;

    return pss_wait_ping_resp;
}

static pcp_server_state_e handle_send_all_msgs(pcp_server_t* s)
{
    struct flow_iterator_data d = {s, fev_server_initialized};
    pcp_db_foreach_flow(flow_send_event_iter, &d);

    gettimeofday(&s->next_timeout, NULL);
    return pss_wait_io_calc_nearest_timeout;
}

static pcp_server_state_e handle_server_restart(pcp_server_t* s)
{
    struct flow_iterator_data d = {s, fev_server_restarted};
    pcp_db_foreach_flow(flow_send_event_iter, &d);
    s->restart_flow_msg = NULL;

    gettimeofday(&s->next_timeout, NULL);
    return pss_wait_io_calc_nearest_timeout;
}



static pcp_server_state_e handle_wait_io_receive_msg(pcp_server_t* s)
{
    pcp_recv_msg_t msg;
    pcp_flow_t f;

    if (read_msg_from_socket(s->pcp_server_socket, &msg) != PCP_ERR_SUCCESS) {
        gettimeofday(&s->next_timeout, NULL);
        return pss_set_not_working;
    }

    if (!validate_pcp_msg(&msg)) {
        PCP_LOGGER(PCP_DEBUG_PERR, "%s","Invalid PCP msg");
        return pss_wait_io;
    }

    if ((parse_response(&msg)) != PCP_ERR_SUCCESS) {
        PCP_LOGGER(PCP_DEBUG_PERR, "%s", "Cannot parse PCP msg");
        return pss_wait_io;
    }

    memcpy(&msg.kd.src_ip, s->src_ip, sizeof(struct in6_addr));
    memcpy(&msg.kd.pcp_server_ip, s->pcp_ip, sizeof(struct in6_addr));
    if (msg.recv_version<2) {
        memcpy(&msg.kd.nonce, &s->nonce, sizeof(struct pcp_nonce));
    }

    msg.received_time = time(NULL);
    msg.pcp_server_indx = s->index;

    PCP_LOGGER(PCP_DEBUG_INFO,
        "Received PCP packet from server at %s, size %d, result_code %d, epoch %d",
        s->pcp_server_paddr, msg.pcp_msg_len, msg.recv_result, msg.recv_epoch);

    switch (msg.recv_result) {
    case PCP_RES_UNSUPP_VERSION:
        PCP_LOGGER(PCP_DEBUG_DEBUG, "PCP server %s returned "
                "result_code=Unsupported version", s->pcp_server_paddr);
        gettimeofday(&s->next_timeout, NULL);
        return pss_version_negotiation;
    case PCP_RES_ADDRESS_MISMATCH:
        PCP_LOGGER(PCP_DEBUG_WARN, "There is PCP-unaware NAT present "
                "between client and PCP server %s. "
                "Sending of PCP messages was disabled.", s->pcp_server_paddr);
        gettimeofday(&s->next_timeout, NULL);
        return pss_set_not_working;
    }

    f = server_process_rcvd_pcp_msg(s, &msg);

    if (compare_epochs(&msg, s)) {
        s->epoch = msg.recv_epoch;
        s->cepoch = msg.received_time;
        gettimeofday(&s->next_timeout, NULL);
        s->restart_flow_msg = f;

        return pss_server_restart;
    }

    gettimeofday(&s->next_timeout, NULL);
    return pss_wait_io_calc_nearest_timeout;
}

static pcp_server_state_e handle_wait_io_timeout(pcp_server_t *s)
{
    struct timeval ctv;
    s->next_timeout.tv_sec = 0;
    s->next_timeout.tv_usec = 0;

    pcp_db_foreach_flow(check_flow_timeout, &s->next_timeout);

    if ((s->next_timeout.tv_sec != 0) || (s->next_timeout.tv_usec != 0)) {
        gettimeofday(&ctv, NULL);
        s->next_timeout.tv_sec += ctv.tv_sec;
        s->next_timeout.tv_usec += ctv.tv_usec;
        timeval_align(&s->next_timeout);
    }

    return pss_wait_io;
}

static pcp_server_state_e handle_server_set_not_working(pcp_server_t *s)
{
    struct flow_iterator_data d = {s, fev_failed};
    PCP_LOGGER(PCP_DEBUG_DEBUG, "Entered function %s", __FUNCTION__);
    PCP_LOGGER(PCP_DEBUG_WARN, "PCP server %s failed to respond. "
            "Disabling sending of PCP messages to this server for %d minutes.",
            s->pcp_server_paddr, PCP_SERVER_DISCOVERY_RETRY_DELAY / 60);

    pcp_db_foreach_flow(flow_send_event_iter, &d);

    gettimeofday(&s->next_timeout, NULL);
    s->next_timeout.tv_sec += PCP_SERVER_DISCOVERY_RETRY_DELAY;

    return pss_not_working;
}

static pcp_server_state_e handle_server_not_working(pcp_server_t *s)
{
    struct timeval ctv;
    gettimeofday(&ctv, NULL);
    if (timeval_comp(&ctv,&s->next_timeout)<0) {
        pcp_recv_msg_t msg;
        pcp_flow_t f;
        if (read_msg_from_socket(s->pcp_server_socket, &msg) !=
            PCP_ERR_SUCCESS) {
            gettimeofday(&s->next_timeout, NULL);
            return pss_set_not_working;
        }

        if (!validate_pcp_msg(&msg)) {
            PCP_LOGGER(PCP_DEBUG_PERR, "%s", "Invalid PCP msg");
            return pss_not_working;
        }

        if ((parse_response(&msg)) != PCP_ERR_SUCCESS) {
            PCP_LOGGER(PCP_DEBUG_PERR, "%s", "Cannot parse PCP msg");
            return pss_not_working;
        }

        memcpy(&msg.kd.src_ip, s->src_ip, sizeof(struct in6_addr));
        memcpy(&msg.kd.pcp_server_ip, s->pcp_ip,
                sizeof(struct in6_addr));
        if (msg.recv_version<2) {
            memcpy(&msg.kd.nonce, &s->nonce, sizeof(struct pcp_nonce));
        }

        msg.received_time = time(NULL);
        msg.pcp_server_indx = s->index;

        PCP_LOGGER(PCP_DEBUG_INFO,
            "Received PCP packet from server at %s, size %d, result_code %d, epoch %d",
            s->pcp_server_paddr, msg.pcp_msg_len, msg.recv_result, msg.recv_epoch);

        switch (msg.recv_result) {
        case PCP_RES_UNSUPP_VERSION:
            return pss_not_working;
        case PCP_RES_ADDRESS_MISMATCH:
            return pss_not_working;
        }

        f = server_process_rcvd_pcp_msg(s, &msg);

        s->epoch = msg.recv_epoch;
        s->cepoch = msg.received_time;
        gettimeofday(&s->next_timeout, NULL);
        s->restart_flow_msg = f;

        return pss_server_restart;
    } else {
        s->next_timeout = ctv;
        return pss_server_reping;
    }
}

static pcp_server_state_e handle_server_reping(pcp_server_t *s)
{
    PCP_LOGGER(PCP_DEBUG_INFO, "Trying to ping PCP server %s again. ",
            s->pcp_server_paddr);
    CLOSE(s->pcp_server_socket);
    s->pcp_version = PCP_MAX_SUPPORTED_VERSION;
    psd_create_pcp_server_socket(s->index);

    gettimeofday(&s->next_timeout, NULL);
    return pss_ping;
}

static pcp_server_state_e pcp_terminate_server(pcp_server_t* s)
{
    if (s->pcp_server_socket != PCP_INVALID_SOCKET ) {
        CLOSE(s->pcp_server_socket);
        pcp_fd_change_notify(s, 0);
    }
    s->pcp_server_socket = PCP_INVALID_SOCKET;
    s->next_timeout.tv_sec = 0;
    s->next_timeout.tv_usec = 0;

    PCP_LOGGER(PCP_DEBUG_INFO, "PCP server %s terminated. ",
            s->pcp_server_paddr);

    return pss_allocated;
}

//LCOV_EXCL_START
static pcp_server_state_e log_unexepected_state_event(pcp_server_t* s)
{
    PCP_LOGGER(PCP_DEBUG_PERR,
            "Event happened in the state %d on PCP server %s"
            " and there is no event handler defined.", s->server_state,
            s->pcp_server_paddr);

    return s->server_state;
}
//LCOV_EXCL_STOP
////////////////////////////////////////////////////////////////////////////////
//                  Server State Machine definition

typedef pcp_server_state_e (*handle_server_state_event)(pcp_server_t *s);

typedef struct pcp_server_state_machine {
    pcp_server_state_e state;
    pcp_event_e event;
    handle_server_state_event handler;
} pcp_server_state_machine_t;

pcp_server_state_machine_t server_sm[] = {
        { pss_any, pcpe_terminate, pcp_terminate_server },
        // -> allocated
        { pss_ping, pcpe_any, handle_server_ping },
        // -> wait_ping_resp | set_not_working
        { pss_wait_ping_resp, pcpe_timeout, handle_wait_ping_resp_timeout },
        // -> wait_ping_resp | set_not_working
        { pss_wait_ping_resp, pcpe_io_event, handle_wait_ping_resp_recv },
        // -> wait ping_resp | pss_send_waiting_msgs | set_not_working | version_neg
        { pss_version_negotiation, pcpe_any, handle_version_negotiation },
        // -> wait ping_resp | set_not_working
        { pss_send_all_msgs, pcpe_any, handle_send_all_msgs },
        // -> wait_io
        { pss_wait_io, pcpe_io_event, handle_wait_io_receive_msg },
        // -> wait_io_calc_nearest_timeout | server_restart |version_negotiation | set_not_working
        { pss_wait_io, pcpe_timeout, handle_wait_io_timeout },
        // -> wait_io | server_restart
        { pss_wait_io_calc_nearest_timeout, pcpe_any, handle_wait_io_timeout },
        // -> wait_io
        { pss_server_restart, pcpe_any, handle_server_restart },
        // -> wait_io
        { pss_server_reping, pcpe_any, handle_server_reping },
        // -> ping
        { pss_set_not_working, pcpe_any, handle_server_set_not_working },
        // -> not_working
        { pss_not_working, pcpe_any, handle_server_not_working },
        // -> reping
        { pss_any, pcpe_any, log_unexepected_state_event }
// -> last_state
        };

#define SERVER_STATE_MACHINE_COUNT (sizeof(server_sm)/sizeof(*server_sm))

pcp_errno run_server_state_machine(pcp_server_t * s, pcp_event_e event)
{
    unsigned i;

    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);
    if (!s) {
        return PCP_ERR_BAD_ARGS;
    }

    for (i = 0; i < SERVER_STATE_MACHINE_COUNT; ++i) {
        pcp_server_state_machine_t *state_def = server_sm + i;
        if ((state_def->state == s->server_state)
                || (state_def->state == pss_any)) {
            if ((state_def->event == pcpe_any) || (state_def->event == event)) {
                PCP_LOGGER_DEBUG(
                        "Executing server state handler %s\n    server \t: %s (index %d)\n"
                        "    state\t: %s\n"
                        "    event\t: %s",
                        dbg_get_func_name(state_def->handler),
                        s->pcp_server_paddr, s->index,
                        dbg_get_sstate_name(s->server_state),
                        dbg_get_sevent_name(event));

                s->server_state = state_def->handler(s);

                PCP_LOGGER_DEBUG(
                        "Return from server state handler's %s \n    result state: %s",
                        dbg_get_func_name(state_def->handler),
                        dbg_get_sstate_name(s->server_state));

                break;
            }
        }
    }
    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return PCP_ERR_SUCCESS;
}

struct pcp_hevent_data {
    fd_set *read_fd_set;
    int fd_max;
    struct timeval *select_timeout;
};

static int handle_event_server(pcp_server_t* s, void* data)
{
    struct pcp_hevent_data *params = (struct pcp_hevent_data*) data;
    struct timeval ctv;

    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);
    if ((s == NULL) || (s->server_state == pss_unitialized) || (data == NULL)) {
        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return 0;
    }

    if ((s->pcp_server_socket!=PCP_INVALID_SOCKET) &&
        (s->pcp_server_socket < (PCP_SOCKET)params->fd_max)
            && (FD_ISSET(s->pcp_server_socket, params->read_fd_set))) {
        FD_CLR(s->pcp_server_socket, params->read_fd_set);
        run_server_state_machine(s, pcpe_io_event);
    }

    while(1) {
        gettimeofday(&ctv, NULL);
        if (((s->next_timeout.tv_sec == 0) && (s->next_timeout.tv_usec == 0))
            || (!timeval_subtract(&ctv, &s->next_timeout, &ctv))) {
            break;
        }
        run_server_state_machine(s, pcpe_timeout);
    }

    if ((!params->select_timeout)
            || ((s->next_timeout.tv_sec == 0)
                    && (s->next_timeout.tv_usec == 0))) {
        PCP_LOGGER_END(PCP_DEBUG_DEBUG);
        return 0;
    }

    if ((params->select_timeout->tv_sec == 0)
            && (params->select_timeout->tv_usec == 0)) {

        *params->select_timeout = ctv;

    } else if (timeval_comp(&ctv, params->select_timeout) < 0) {

        *params->select_timeout = ctv;
    }

    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
//                       Exported functions


void pcp_handle_select(int fd_max, fd_set *read_fd_set,
        struct timeval *select_timeout)
{
    struct timeval t_help;
    fd_set fd_help;
    struct pcp_hevent_data params =
            { read_fd_set, fd_max, select_timeout };
    PCP_LOGGER_BEGIN(PCP_DEBUG_DEBUG);
    if (!params.select_timeout) {
        params.select_timeout = &t_help;
    }
    if (!read_fd_set) {
        struct timeval tv = {0, 0};
        FD_ZERO(&fd_help);
        params.read_fd_set = &fd_help;
        pcp_set_read_fdset(&params.fd_max, params.read_fd_set);
        select(params.fd_max, params.read_fd_set, NULL, NULL, &tv);
    }
    pcp_db_foreach_server(handle_event_server, &params);
    PCP_LOGGER_END(PCP_DEBUG_DEBUG);
}

int
pcp_handle_fd_event(PCP_SOCKET fd, int timed_out, struct timeval *next_timeout)
{
    pcp_server_t* s = get_pcp_server_by_fd(fd);

    if (!s) {
        return PCP_ERR_BAD_ARGS;
    } else {
        fd_set fds_t;
        struct timeval t_help;
        struct pcp_hevent_data data = { &fds_t, fd+1, next_timeout };

        if (!data.select_timeout) {
            data.select_timeout = &t_help;
            t_help.tv_sec = 0;
            t_help.tv_usec = 0;
        }

        FD_ZERO(&fds_t);
        if (!timed_out) {
            FD_SET(fd, &fds_t);
        }

        handle_event_server(s, &data);
        return (data.select_timeout->tv_sec*1000)+(data.select_timeout->tv_usec/1000);
    }
}

void pcp_flow_updated(pcp_flow_t f)
{
    struct timeval curtime;
    pcp_server_t*s;

    gettimeofday(&curtime, NULL);
    s=get_pcp_server(f->pcp_server_indx);
    if (s) {
        s->next_timeout = curtime;
    }
    pcp_flow_clear_msg_buf(f);
    f->timeout = curtime;
    if ((f->state!= pfs_wait_for_server_init) &&
            (f->state!= pfs_idle) &&
            (f->state!= pfs_failed)) {
        f->state = pfs_send;
    }
}

pcp_flow_change_notify pcp_flow_change_cb_fun=NULL;

void pcp_set_flow_change_cb(pcp_flow_change_notify cb_fun)
{
    pcp_flow_change_cb_fun = cb_fun;
}

static void
fill_sockaddr(struct sockaddr* dst, struct in6_addr* sip, in_port_t sport)
{
    if (IN6_IS_ADDR_V4MAPPED(sip)) {
        struct sockaddr_in *s = (struct sockaddr_in *)dst;
        s->sin_family = AF_INET;
        s->sin_addr.s_addr = S6_ADDR32(sip)[3];
        s->sin_port = sport;
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)dst;
        s->sin6_family = AF_INET6;
        s->sin6_addr = *sip;
        s->sin6_port = sport;
    }
}

static void flow_change_notify(pcp_flow_t flow, pcp_fstate_e state)
{
    struct sockaddr_storage src_addr, ext_addr;
    PCP_LOGGER_DEBUG(
            "Flow's %d state changed to: %s",
            flow->key_bucket,
            dbg_get_fstate_name(state));
    if (pcp_flow_change_cb_fun)
    {
        fill_sockaddr((struct sockaddr*)&src_addr, &flow->kd.src_ip,
                flow->kd.map_peer.src_port);
        if (state == pcp_state_succeeded) {
            fill_sockaddr((struct sockaddr*)&ext_addr, &flow->map_peer.ext_ip,
                    flow->map_peer.ext_port);
        } else {
            pcp_server_t *s = get_pcp_server(flow->pcp_server_indx);
            memset(&ext_addr,0,sizeof(ext_addr));
            ext_addr.ss_family = s?s->af:AF_INET;
        }
        pcp_flow_change_cb_fun(flow, (struct sockaddr*)&src_addr,
                (struct sockaddr*)&ext_addr, state);
    }
}

static pcp_fd_change_cb_t pcp_fd_change_cb = NULL;
static void *pcp_fd_change_cb_arg = NULL;

int pcp_set_fd_change_cb(pcp_fd_change_cb_t cb, void* cb_arg)
{
    pcp_fd_change_cb = cb;
    pcp_fd_change_cb_arg = cb_arg;

    return 0;
}

void pcp_fd_change_notify(pcp_server_t *s, int added)
{
    if ((pcp_fd_change_cb)&&(s)) {
        pcp_fd_change_cb(s->pcp_server_socket, added, pcp_fd_change_cb_arg,
                &s->app_data);
    }
}
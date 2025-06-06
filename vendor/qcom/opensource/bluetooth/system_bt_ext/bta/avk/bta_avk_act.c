/******************************************************************************
 *
 *  Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *  Not a Contribution
 *  Copyright (C) 2004-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  This file contains action functions for advanced audio/video main state
 *  machine.
 *
 ******************************************************************************/

#include "bt_target.h"
#if defined(BTA_AV_INCLUDED) && (BTA_AV_INCLUDED == TRUE)


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bta_avk_api.h"
#include "bta_avk_int.h"
#include "avdt_api.h"
#include "utl.h"
#include "l2c_api.h"
#include "osi/include/list.h"
#if( defined BTA_AR_INCLUDED ) && (BTA_AR_INCLUDED == TRUE)
#include "bta_ar_api.h"
#endif
#include "bt_utils.h"
#include <errno.h>

#define LOG_TAG "bt_bta_av"
#include "osi/include/log.h"
#include "osi/include/osi.h"

#if (AVRC_CTLR_INCLUDED == TRUE)
#include <cutils/properties.h>
#endif
#include "bta_ar_int_ext.h"

static const UINT8 browsing_sniff_black_list_prefix[][3] = {{0x34, 0xc0, 0x59}};

/*****************************************************************************
**  Constants
*****************************************************************************/
/* the timer in milliseconds to wait for open req after setconfig for incoming connections */
#ifndef BTA_AVK_SIG_TIME_VAL
#define BTA_AVK_SIG_TIME_VAL 8000
#endif

/* In millisec to wait for signalling from SNK when it is initiated from SNK.   */
/* If not, we will start signalling from SRC.                                   */
#ifndef BTA_AVK_ACP_SIG_TIME_VAL
#define BTA_AVK_ACP_SIG_TIME_VAL 2000
#endif

#ifndef BTA_AVK_RC_BR_TIME_VAL
#define BTA_AVK_RC_BR_TIME_VAL 5000
#endif

#ifndef AVRC_MIN_META_CMD_LEN
#define AVRC_MIN_META_CMD_LEN 20
#endif

extern fixed_queue_t *btu_bta_alarm_queue;

/* state machine states */
enum
{
    BTA_AVK_INIT_SST,
    BTA_AVK_INCOMING_SST,
    BTA_AVK_OPENING_SST,
    BTA_AVK_OPEN_SST,
    BTA_AVK_RCFG_SST,
    BTA_AVK_CLOSING_SST
};

struct avk_blacklist_entry
{
    int ver;
    char addr[3];
    BOOLEAN is_src;
};

static void bta_avk_acp_sig_timer_cback (void *data);
static void bta_avk_rc_br_timer_cback (void *data);


/*******************************************************************************
**
** Function         bta_avk_get_rcb_by_shdl
**
** Description      find the RCB associated with the given SCB handle.
**
** Returns          tBTA_AVK_RCB
**
*******************************************************************************/
tBTA_AVK_RCB * bta_avk_get_rcb_by_shdl(UINT8 shdl)
{
    tBTA_AVK_RCB *p_rcb = NULL;
    int         i;

    for (i=0; i<BTA_AVK_NUM_RCB; i++)
    {
        if (bta_avk_cb.rcb[i].shdl == shdl && bta_avk_cb.rcb[i].handle != BTA_AVK_RC_HANDLE_NONE)
        {
            p_rcb = &bta_avk_cb.rcb[i];
            break;
        }
    }
    return p_rcb;
}
#define BTA_AVK_STS_NO_RSP       0xFF    /* a number not used by tAVRC_STS */

/*******************************************************************************
**
** Function         bta_avk_del_rc
**
** Description      delete the given AVRC handle.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_del_rc(tBTA_AVK_RCB *p_rcb)
{
    tBTA_AVK_SCB  *p_scb;
    UINT8        rc_handle;      /* connected AVRCP handle */
    UINT16       status;

    p_scb = NULL;
    if(p_rcb->handle != BTA_AVK_RC_HANDLE_NONE)
    {
        if(p_rcb->shdl)
        {
            /* Validate array index*/
            if ((p_rcb->shdl - 1) < BTA_AVK_NUM_STRS)
            {
                p_scb = bta_avk_cb.p_scb[p_rcb->shdl - 1];
            }
            if(p_scb)
            {
                APPL_TRACE_DEBUG("bta_avk_del_rc shdl:%d, srch:%d rc_handle:%d", p_rcb->shdl,
                                  p_scb->rc_handle, p_rcb->handle);
                if(p_scb->rc_handle == p_rcb->handle)
                    p_scb->rc_handle = BTA_AVK_RC_HANDLE_NONE;
                /* just in case the RC timer is active
                if(bta_avk_cb.features & BTA_AVK_FEAT_RCCT && p_scb->chnl == BTA_AVK_CHNL_AUDIO) */
                    alarm_cancel(p_scb->avrc_ct_timer);
            }
        }

        APPL_TRACE_EVENT("bta_avk_del_rc  handle: %d status=0x%x, rc_acp_handle:%d, idx:%d",
            p_rcb->handle, p_rcb->status, bta_avk_cb.rc_acp_handle, bta_avk_cb.rc_acp_idx);
        rc_handle = p_rcb->handle;
        if(!(p_rcb->status & BTA_AVK_RC_CONN_MASK) ||
            ((p_rcb->status & BTA_AVK_RC_ROLE_MASK) == BTA_AVK_RC_ROLE_INT) )
        {
            p_rcb->status = 0;
            p_rcb->handle = BTA_AVK_RC_HANDLE_NONE;
            p_rcb->shdl = 0;
            p_rcb->lidx = 0;
        }
        /* else ACP && connected. do not clear the handle yet */
        status = AVRC_Close(rc_handle);
        if(status != AVRC_SUCCESS)
        {
            APPL_TRACE_ERROR("bta_avk_del_rc: Error in AVRC_Close %d", status);
        }

        if (rc_handle == bta_avk_cb.rc_acp_handle)
            bta_avk_cb.rc_acp_handle = BTA_AVK_RC_HANDLE_NONE;
        APPL_TRACE_EVENT("end del_rc handle: %d status=0x%x, rc_acp_handle:%d, lidx:%d",
            p_rcb->handle, p_rcb->status, bta_avk_cb.rc_acp_handle, p_rcb->lidx);
    }
}


/*******************************************************************************
**
** Function         bta_avk_close_all_rc
**
** Description      close the all AVRC handle.
**
** Returns          void
**
*******************************************************************************/
static void bta_avk_close_all_rc(tBTA_AVK_CB *p_cb)
{
    int i;

    for(i=0; i<BTA_AVK_NUM_RCB; i++)
    {
        if ((p_cb->disabling == TRUE) || (bta_avk_cb.rcb[i].shdl != 0))
            bta_avk_del_rc(&bta_avk_cb.rcb[i]);
    }
}

/*******************************************************************************
**
** Function         bta_avk_del_sdp_rec
**
** Description      delete the given SDP record handle.
**
** Returns          void
**
*******************************************************************************/
static void bta_avk_del_sdp_rec(UINT32 *p_sdp_handle)
{
    if(*p_sdp_handle != 0)
    {
        SDP_DeleteRecord(*p_sdp_handle);
        *p_sdp_handle = 0;
    }
}

/*******************************************************************************
**
** Function         bta_avk_avrc_sdp_cback
**
** Description      AVRCP service discovery callback.
**
** Returns          void
**
*******************************************************************************/
static void bta_avk_avrc_sdp_cback(UINT16 status)
{
    BT_HDR *p_msg;
    UNUSED(status);

    if ((p_msg = (BT_HDR *) osi_malloc(sizeof(BT_HDR))) != NULL)
    {
        p_msg->event = BTA_AVK_SDP_AVRC_DISC_EVT;
        bta_sys_sendmsg(p_msg);
    }
}

/*******************************************************************************
**
** Function         bta_avk_rc_ctrl_cback
**
** Description      AVRCP control callback.
**
** Returns          void
**
*******************************************************************************/
static void bta_avk_rc_ctrl_cback(UINT8 handle, UINT8 event, UINT16 result, BD_ADDR peer_addr)
{
    tBTA_AVK_RC_CONN_CHG *p_msg;
    UINT16 msg_event = 0;
    UNUSED(result);

#if (defined(BTA_AVK_MIN_DEBUG_TRACES) && BTA_AVK_MIN_DEBUG_TRACES == TRUE)
    APPL_TRACE_EVENT("rc_ctrl handle: %d event=0x%x", handle, event);
#else
    BTIF_TRACE_IMP("bta_avk_rc_ctrl_cback handle: %d event=0x%x", handle, event);
#endif
    if (event == AVRC_OPEN_IND_EVT)
    {
        /* save handle of opened connection
        bta_avk_cb.rc_handle = handle;*/

        msg_event = BTA_AVK_AVRC_OPEN_EVT;

        APPL_TRACE_EVENT("AVRC_OPEN_IND_EVT peer_features: %d avrc_ct_cat=%d !~", bta_avk_cb.rcb[handle].peer_features, p_bta_avk_cfg->avrc_ct_cat);

      if (BTA_AvkIsBrowsingSupported () && bta_avk_cb.rcb[handle].peer_features & BTA_AVK_FEAT_BROWSE)
        {
            BTIF_TRACE_IMP("bta_avk_rc_ctrl_cback AVRC_OpenBrowseChannel handle: %d !~", handle);
            AVRC_OpenBrowseChannel (handle);
        }
    }
    else if (event == AVRC_CLOSE_IND_EVT)
    {
        msg_event = BTA_AVK_AVRC_CLOSE_EVT;
    }
    else if (event == AVRC_BROWSE_OPEN_IND_EVT)
    {
        BTIF_TRACE_IMP("bta_avk_rc_ctrl_cback :AVRC_BROWSE_OPEN_IND_EVT !~");
        msg_event = BTA_AVK_AVRC_BROWSE_OPEN_EVT;
    }
    else if (event == AVRC_BROWSE_CLOSE_IND_EVT)
    {
        BTIF_TRACE_IMP("bta_avk_rc_ctrl_cback :AVRC_BROWSE_CLOSE_IND_EVT !~");
        msg_event = BTA_AVK_AVRC_BROWSE_CLOSE_EVT;
    }

    if (msg_event)
    {
        if ((p_msg = (tBTA_AVK_RC_CONN_CHG *) osi_malloc(sizeof(tBTA_AVK_RC_CONN_CHG))) != NULL)
        {
            p_msg->hdr.event = msg_event;
            p_msg->handle    = handle;
            if(peer_addr)
                bdcpy(p_msg->peer_addr, peer_addr);
            bta_sys_sendmsg(p_msg);
        }
    }
}

/*******************************************************************************
**
** Function         bta_avk_rc_msg_cback
**
** Description      AVRCP message callback.
**
** Returns          void
**
*******************************************************************************/
static void bta_avk_rc_msg_cback(UINT8 handle, UINT8 label, UINT8 opcode, tAVRC_MSG *p_msg)
{
    UINT8           *p_data_src = NULL;
    UINT16          data_len = 0;

    BTIF_TRACE_IMP("%s handle: %u opcode=0x%x", __func__, handle, opcode);

    /* Determine the size of the buffer we need */
    if (opcode == AVRC_OP_VENDOR && p_msg->vendor.p_vendor_data != NULL) {
        p_data_src = p_msg->vendor.p_vendor_data;
        data_len = (UINT16) p_msg->vendor.vendor_len;
    } else if (opcode == AVRC_OP_PASS_THRU && p_msg->pass.p_pass_data != NULL) {
        p_data_src = p_msg->pass.p_pass_data;
        data_len = (UINT16) p_msg->pass.pass_len;
    } /*else if (opcode == AVRC_OP_BROWSE && p_msg->browse.p_browse_data != NULL) {
        APPL_TRACE_EVENT("bta_avk_rc_msg_cback Browse Data");
        p_data_src  = p_msg->browse.p_browse_data;
        data_len = (UINT16) p_msg->browse.browse_len;
    }*/


    /* Create a copy of the message */
    tBTA_AVK_RC_MSG *p_buf =
        (tBTA_AVK_RC_MSG *)osi_malloc((UINT16)(sizeof(tBTA_AVK_RC_MSG) + data_len));
    if (p_buf != NULL) {
        p_buf->hdr.event = BTA_AVK_AVRC_MSG_EVT;
        p_buf->handle = handle;
        p_buf->label = label;
        p_buf->opcode = opcode;
        memcpy(&p_buf->msg, p_msg, sizeof(tAVRC_MSG));
        /* Copy the data payload, and set the pointer to it */
        if (p_data_src != NULL) {
            UINT8 *p_data_dst = (UINT8 *)(p_buf + 1);
            memcpy(p_data_dst, p_data_src, data_len);
            if (opcode == AVRC_OP_VENDOR)
                p_buf->msg.vendor.p_vendor_data = p_data_dst;
            else if (opcode == AVRC_OP_PASS_THRU)
                p_buf->msg.pass.p_pass_data = p_data_dst;
            /*else if (opcode == AVRC_OP_BROWSE)
                p_buf->msg.browse.p_browse_data = p_data_dst;*/
        }
        if (opcode == AVRC_OP_BROWSE) {
          /* set p_pkt to NULL, so avrc would not free the buffer */
          p_msg->browse.p_browse_pkt = NULL;
        }
        bta_sys_sendmsg(p_buf);
    }
}

/*******************************************************************************
**
** Function         bta_avk_rc_create
**
** Description      alloc RCB and call AVRC_Open
**
** Returns          the created rc handle
**
*******************************************************************************/
UINT8 bta_avk_rc_create(tBTA_AVK_CB *p_cb, UINT8 role, UINT8 shdl, UINT8 lidx)
{
    tAVRC_CONN_CB ccb;
    BD_ADDR_PTR   bda = (BD_ADDR_PTR)bd_addr_any;
    UINT8         status = BTA_AVK_RC_ROLE_ACP;
    tBTA_AVK_SCB  *p_scb;
    int i;
    UINT8   rc_handle;
    tBTA_AVK_RCB *p_rcb;

    if(role == AVCT_INT)
    {
        p_scb = p_cb->p_scb[shdl - 1];
        bda = p_scb->peer_addr;
        status = BTA_AVK_RC_ROLE_INT;
    }
    else
    {
        if ((p_rcb = bta_avk_get_rcb_by_shdl(shdl)) != NULL )
        {
            APPL_TRACE_ERROR("bta_avk_rc_create ACP handle exist for shdl:%d", shdl);
            return p_rcb->handle;
        }
    }

    ccb.p_ctrl_cback = bta_avk_rc_ctrl_cback;
    ccb.p_msg_cback = bta_avk_rc_msg_cback;
    ccb.company_id = p_bta_avk_cfg->company_id;
    ccb.conn = role;
    ccb.av_sep_type = BTA_AV_RC_PROFILE_SINK;
    /* note: BTA_AVK_FEAT_RCTG = AVRC_CT_TARGET, BTA_AVK_FEAT_RCCT = AVRC_CT_CONTROL */
    ccb.control = p_cb->features & (BTA_AVK_FEAT_RCTG | BTA_AVK_FEAT_RCCT | AVRC_CT_PASSIVE);

    BTIF_TRACE_IMP("bta_avk_rc_create - AVRC_Open !!c~~");
    if (AVRC_Open(&rc_handle, &ccb, bda) != AVRC_SUCCESS)
        return BTA_AVK_RC_HANDLE_NONE;

    i = rc_handle;
    p_rcb = &p_cb->rcb[i];

    if (p_rcb->handle != BTA_AVK_RC_HANDLE_NONE)
    {
        APPL_TRACE_ERROR("bta_avk_rc_create found duplicated handle:%d", rc_handle);
    }

    p_rcb->handle = rc_handle;
    p_rcb->status = status;
    p_rcb->shdl = shdl;
    p_rcb->lidx = lidx;
    p_rcb->peer_features = 0;
//  p_rcb->cover_art_psm = 0;
    p_rcb->br_conn_timer = alarm_new("bta_avk.br_conn_timer");
    if(lidx == (BTA_AVK_NUM_LINKS + 1))
    {
        /* this LIDX is reserved for the AVRCP ACP connection */
        p_cb->rc_acp_handle = p_rcb->handle;
        p_cb->rc_acp_idx = (i + 1);
        APPL_TRACE_DEBUG("rc_acp_handle:%d idx:%d", p_cb->rc_acp_handle, p_cb->rc_acp_idx);
    }
    APPL_TRACE_DEBUG("create %d, role: %d, shdl:%d, rc_handle:%d, lidx:%d, status:0x%x",
        i, role, shdl, p_rcb->handle, lidx, p_rcb->status);

    return rc_handle;
}

/*******************************************************************************
**
** Function         bta_avk_valid_group_navi_msg
**
** Description      Check if it is Group Navigation Msg for Metadata
**
** Returns          BTA_AVK_RSP_ACCEPT or BTA_AVK_RSP_NOT_IMPL.
**
*******************************************************************************/
static tBTA_AVK_CODE bta_avk_group_navi_supported(UINT8 len, UINT8 *p_data, BOOLEAN is_inquiry)
{
    tBTA_AVK_CODE ret=BTA_AVK_RSP_NOT_IMPL;
    UINT8 *p_ptr = p_data;
    UINT16 u16;
    UINT32 u32;

    if (p_bta_avk_cfg->avrc_group && len == BTA_GROUP_NAVI_MSG_OP_DATA_LEN)
    {
        BTA_AVK_BE_STREAM_TO_CO_ID(u32, p_ptr);
        BE_STREAM_TO_UINT16(u16, p_ptr);

        if (u32 == AVRC_CO_METADATA)
        {
            if (is_inquiry)
            {
                if (u16 <= AVRC_PDU_PREV_GROUP)
                    ret = BTA_AVK_RSP_IMPL_STBL;
            }
            else
            {
                if (u16 <= AVRC_PDU_PREV_GROUP)
                    ret = BTA_AVK_RSP_ACCEPT;
                else
                    ret = BTA_AVK_RSP_REJ;
            }
        }
    }

    return ret;
}

/*******************************************************************************
**
** Function         bta_avk_op_supported
**
** Description      Check if remote control operation is supported.
**
** Returns          BTA_AVK_RSP_ACCEPT of supported, BTA_AVK_RSP_NOT_IMPL if not.
**
*******************************************************************************/
static tBTA_AVK_CODE bta_avk_op_supported(tBTA_AVK_RC rc_id, BOOLEAN is_inquiry)
{
    tBTA_AVK_CODE ret_code = BTA_AVK_RSP_NOT_IMPL;

    if (p_bta_avk_rc_id)
    {
        if (is_inquiry)
        {
            if (p_bta_avk_rc_id[rc_id >> 4] & (1 << (rc_id & 0x0F)))
            {
                ret_code = BTA_AVK_RSP_IMPL_STBL;
            }
        }
        else
        {
            if (p_bta_avk_rc_id[rc_id >> 4] & (1 << (rc_id & 0x0F)))
            {
                ret_code = BTA_AVK_RSP_ACCEPT;
            }
            else if ((p_bta_avk_cfg->rc_pass_rsp == BTA_AVK_RSP_INTERIM) && p_bta_avk_rc_id_ac)
            {
                if (p_bta_avk_rc_id_ac[rc_id >> 4] & (1 << (rc_id & 0x0F)))
                {
                    ret_code = BTA_AVK_RSP_INTERIM;
                }
            }
        }

    }
    return ret_code;
}

/*******************************************************************************
**
** Function         bta_avk_find_lcb
**
** Description      Given BD_addr, find the associated LCB.
**
** Returns          NULL, if not found.
**
*******************************************************************************/
tBTA_AVK_LCB * bta_avk_find_lcb(BD_ADDR addr, UINT8 op)
{
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    int     xx;
    UINT8   mask;
    tBTA_AVK_LCB *p_lcb = NULL;

    for(xx=0; xx<BTA_AVK_NUM_LINKS; xx++)
    {
        mask = 1 << xx; /* the used mask for this lcb */
        if((mask & p_cb->conn_lcb) && 0 ==( bdcmp(p_cb->lcb[xx].addr, addr)))
        {
            p_lcb = &p_cb->lcb[xx];
            if(op == BTA_AVK_LCB_FREE)
            {
                p_cb->conn_lcb &= ~mask; /* clear the connect mask */
                APPL_TRACE_DEBUG("conn_lcb: 0x%x", p_cb->conn_lcb);
            }
            break;
        }
    }
    return p_lcb;
}

/*******************************************************************************
**
** Function         bta_avk_rc_opened
**
** Description      Set AVRCP state to opened.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_opened(tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_RC_OPEN rc_open;
    tBTA_AVK_SCB     *p_scb;
    int         i;
    UINT8       shdl = 0;
    tBTA_AVK_LCB *p_lcb;
    tBTA_AVK_RCB *p_rcb;
    UINT8       tmp;
    UINT8       disc = 0;

	APPL_TRACE_DEBUG("%s ========================!~", __func__);

    /* find the SCB & stop the timer */
    for(i=0; i<BTA_AVK_NUM_STRS; i++)
    {
        p_scb = p_cb->p_scb[i];
        if(p_scb && bdcmp(p_scb->peer_addr, p_data->rc_conn_chg.peer_addr) == 0)
        {
            p_scb->rc_handle = p_data->rc_conn_chg.handle;
            APPL_TRACE_DEBUG("bta_avk_rc_opened shdl:%d, srch %d", i + 1, p_scb->rc_handle);
            shdl = i+1;
            LOG_INFO("%s allow incoming AVRCP connections:%d", __func__, p_scb->use_rc);
            alarm_cancel(p_scb->avrc_ct_timer);
            disc = p_scb->hndl;
            break;
        }
    }

    i = p_data->rc_conn_chg.handle;
    if (p_cb->rcb[i].handle == BTA_AVK_RC_HANDLE_NONE)
    {
        APPL_TRACE_ERROR("not a valid handle:%d any more", i);
        return;
    }


    if (p_cb->rcb[i].lidx == (BTA_AVK_NUM_LINKS + 1) && shdl != 0)
    {
        /* rc is opened on the RC only ACP channel, but is for a specific
         * SCB -> need to switch RCBs */
        p_rcb = bta_avk_get_rcb_by_shdl(shdl);
        if (p_rcb)
        {
            p_rcb->shdl = p_cb->rcb[i].shdl;
            tmp         = p_rcb->lidx;
            p_rcb->lidx = p_cb->rcb[i].lidx;
            p_cb->rcb[i].lidx = tmp;
            p_cb->rc_acp_handle = p_rcb->handle;
            p_cb->rc_acp_idx = (p_rcb - p_cb->rcb) + 1;
            APPL_TRACE_DEBUG("switching RCB rc_acp_handle:%d idx:%d",
                               p_cb->rc_acp_handle, p_cb->rc_acp_idx);
        }
    }

    p_cb->rcb[i].shdl = shdl;
    rc_open.rc_handle = i;
    APPL_TRACE_ERROR("bta_avk_rc_opened rcb[%d] shdl:%d lidx:%d/%d",
            i, shdl, p_cb->rcb[i].lidx, p_cb->lcb[BTA_AVK_NUM_LINKS].lidx);
    p_cb->rcb[i].status |= BTA_AVK_RC_CONN_MASK;
    APPL_TRACE_DEBUG(" RC role ACP = %d",
                                p_cb->rcb[i].status & BTA_AVK_RC_ROLE_MASK);
 /*   if((p_cb->rcb[i].status & BTA_AVK_RC_ROLE_MASK) != 0)
    {
        APPL_TRACE_DEBUG("bta_avk_rc_opened alarm_set_on_queue i: %d !~", i);
        alarm_set_on_queue(p_cb->rcb[i].br_conn_timer,
                           (period_ms_t)BTA_AVK_RC_BR_TIME_VAL,
                           bta_avk_rc_br_timer_cback,
                           INT_TO_PTR(i),
                           btu_bta_alarm_queue);
    }*/

    if(!shdl && 0 == p_cb->lcb[BTA_AVK_NUM_LINKS].lidx)
    {
        /* no associated SCB -> connected to an RC only device
         * update the index to the extra LCB */
        p_lcb = &p_cb->lcb[BTA_AVK_NUM_LINKS];
        bdcpy(p_lcb->addr, p_data->rc_conn_chg.peer_addr);
        APPL_TRACE_DEBUG("rc_only bd_addr:%02x-%02x-%02x-%02x-%02x-%02x",
                      p_lcb->addr[0], p_lcb->addr[1],
                      p_lcb->addr[2], p_lcb->addr[3],
                      p_lcb->addr[4], p_lcb->addr[5]);
        p_lcb->lidx = BTA_AVK_NUM_LINKS + 1;
            p_cb->rcb[i].lidx = p_lcb->lidx;
        p_lcb->conn_msk = 1;
        APPL_TRACE_ERROR("rcb[%d].lidx=%d, lcb.conn_msk=x%x",
            i, p_cb->rcb[i].lidx, p_lcb->conn_msk);
        disc = p_data->rc_conn_chg.handle|BTA_AVK_CHNL_MSK;
    }

    bdcpy(rc_open.peer_addr, p_data->rc_conn_chg.peer_addr);
    rc_open.peer_features = p_cb->rcb[i].peer_features;
    rc_open.status = BTA_AVK_SUCCESS;
    APPL_TRACE_DEBUG("local features:x%x peer_features:x%x", p_cb->features,
                      rc_open.peer_features);
    if(rc_open.peer_features == 0)
    {
        /* we have not done SDP on peer RC capabilities.
         * peer must have initiated the RC connection
         * We Don't have SDP records of Peer, so we by
         * default will take values depending upon registered
         * features */
        if (p_cb->features & BTA_AVK_FEAT_RCTG)
            rc_open.peer_features |= BTA_AVK_FEAT_RCCT;
        bta_avk_rc_disc(disc);
    }
    (*p_cb->p_cback)(BTA_AVK_RC_OPEN_EVT, (tBTA_AVK *) &rc_open);

/* if local initiated AVRCP connection and both peer and locals device support
 * browsing channel, open the browsing channel now
 * TODO (sanketa): Some TG would not broadcast browse feature hence check
 * inter-op. */

/*    if ((p_cb->features & BTA_AVK_FEAT_BROWSE) &&
        (rc_open.peer_features & BTA_AVK_FEAT_BROWSE) &&
        ((p_cb->rcb[i].status & BTA_AVK_RC_ROLE_MASK) == BTA_AVK_RC_ROLE_INT)) {
    APPL_TRACE_DEBUG("%s opening AVRC Browse channel!~", __func__);
    AVRC_OpenBrowseChannel(p_data->rc_conn_chg.handle);
         }

  APPL_TRACE_DEBUG("bta_avk_rc_opened alarm_set_on_queue i: %d !~", i);
    alarm_set_on_queue(p_cb->rcb[i].br_conn_timer,
                       (period_ms_t)BTA_AVK_RC_BR_TIME_VAL,
                       bta_avk_rc_br_timer_cback,
                       INT_TO_PTR(i),
                       btu_bta_alarm_queue);*/
}

void bta_avk_rc_br_opened (tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_RC_BROWSE_OPEN br_open;

    br_open.rc_handle = p_data->rc_conn_chg.handle;
    bta_avk_cb.rcb[br_open.rc_handle].status |= BTA_AVK_RC_CONN_BR_MASK;
    APPL_TRACE_DEBUG("bta_avk_rc_br_opened ");
    if(bta_avk_cb.rcb[br_open.rc_handle].br_conn_timer != NULL)
        alarm_cancel(bta_avk_cb.rcb[br_open.rc_handle].br_conn_timer);

    br_open.status = BTA_AVK_SUCCESS;
    bdcpy(br_open.peer_addr, p_data->rc_conn_chg.peer_addr);
    (*p_cb->p_cback)(BTA_AVK_RC_BROWSE_OPEN_EVT, (tBTA_AVK *)&br_open);
}

BOOLEAN browsing_dev_blacklisted_for_sniff (BD_ADDR addr)
{
    int blacklistsize = 0;
    int i =0;

    blacklistsize = sizeof(browsing_sniff_black_list_prefix)/
                    sizeof(browsing_sniff_black_list_prefix[0]);
    for (i = 0; i < blacklistsize; i++)
    {
        if (0 == memcmp(browsing_sniff_black_list_prefix[i], addr, 3))
        {
            APPL_TRACE_DEBUG(" Device Blacklisted for Sniff ");
            return true;
        }
    }
    return false;
}

/*******************************************************************************
**
** Function         bta_avk_rc_remote_cmd
**
** Description      Send an AVRCP remote control command.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_remote_cmd(tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_RCB    *p_rcb;
    if (p_cb->features & BTA_AVK_FEAT_RCCT)
    {
        if(p_data->hdr.layer_specific < BTA_AVK_NUM_RCB)
        {
            p_rcb = &p_cb->rcb[p_data->hdr.layer_specific];
            if(p_rcb->status & BTA_AVK_RC_CONN_MASK)
            {
                AVRC_PassCmd(p_rcb->handle, p_data->api_remote_cmd.label,
                     &p_data->api_remote_cmd.msg);
            }
        }
    }
}

/*******************************************************************************
**
** Function         bta_avk_rc_vendor_cmd
**
** Description      Send an AVRCP vendor specific command.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_vendor_cmd(tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_RCB    *p_rcb;
    if ( (p_cb->features & (BTA_AVK_FEAT_RCCT | BTA_AVK_FEAT_VENDOR)) ==
         (BTA_AVK_FEAT_RCCT | BTA_AVK_FEAT_VENDOR))
    {
        if(p_data->hdr.layer_specific < BTA_AVK_NUM_RCB)
        {
            p_rcb = &p_cb->rcb[p_data->hdr.layer_specific];
            AVRC_VendorCmd(p_rcb->handle, p_data->api_vendor.label, &p_data->api_vendor.msg);
        }
    }
}

/*******************************************************************************
**
** Function         bta_avk_rc_vendor_rsp
**
** Description      Send an AVRCP vendor specific response.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_vendor_rsp(tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_RCB    *p_rcb;
    if ( (p_cb->features & (BTA_AVK_FEAT_RCTG | BTA_AVK_FEAT_VENDOR)) ==
         (BTA_AVK_FEAT_RCTG | BTA_AVK_FEAT_VENDOR))
    {
        if(p_data->hdr.layer_specific < BTA_AVK_NUM_RCB)
        {
            p_rcb = &p_cb->rcb[p_data->hdr.layer_specific];
            AVRC_VendorRsp(p_rcb->handle, p_data->api_vendor.label, &p_data->api_vendor.msg);
        }
    }
}

/*******************************************************************************
**
** Function         bta_avk_rc_meta_rsp
**
** Description      Send an AVRCP metadata/advanced control command/response.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_meta_rsp(tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_RCB *p_rcb;
    BOOLEAN         do_free = TRUE;

    if ((p_cb->features & BTA_AVK_FEAT_METADATA) && (p_data->hdr.layer_specific < BTA_AVK_NUM_RCB))
    {
        if ((p_data->api_meta_rsp.is_rsp && (p_cb->features & BTA_AVK_FEAT_RCTG)) ||
            (!p_data->api_meta_rsp.is_rsp && (p_cb->features & BTA_AVK_FEAT_RCCT)) )
        {
            p_rcb = &p_cb->rcb[p_data->hdr.layer_specific];
            if ((p_rcb->handle != BTA_AVK_RC_HANDLE_NONE) && (p_rcb->handle < AVCT_NUM_CONN))  {
                AVRC_MsgReq(p_rcb->handle, p_data->api_meta_rsp.label,
                            p_data->api_meta_rsp.rsp_code,
                            p_data->api_meta_rsp.p_pkt);
                do_free = FALSE;
            }
        }
    }

    if (do_free)
        osi_free (p_data->api_meta_rsp.p_pkt);
}

/*******************************************************************************
**
** Function         bta_avk_rc_free_rsp
**
** Description      free an AVRCP metadata command buffer.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_free_rsp (tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    UNUSED(p_cb);

    osi_free (p_data->api_meta_rsp.p_pkt);
}

/*******************************************************************************
**
** Function         bta_avk_rc_meta_req
**
** Description      Send an AVRCP metadata command.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_free_msg (tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    UNUSED(p_cb);
    UNUSED(p_data);
}

/*******************************************************************************
 *
 * Function         bta_av_rc_free_browse_msg
 *
 * Description      free an AVRCP browse message buffer.
 *
 * Returns          void
 *
 ******************************************************************************/
void bta_avk_rc_free_browse_msg(UNUSED_ATTR tBTA_AVK_CB* p_cb,
                               tBTA_AVK_DATA* p_data) {
  if (p_data->rc_msg.opcode == AVRC_OP_BROWSE) {
    osi_free_and_reset((void**)&p_data->rc_msg.msg.browse.p_browse_pkt);
  }
}


/*******************************************************************************
**
** Function         bta_avk_chk_notif_evt_id
**
** Description      make sure the requested player id is valid.
**
** Returns          BTA_AVK_STS_NO_RSP, if no error
**
*******************************************************************************/
static tAVRC_STS bta_avk_chk_notif_evt_id(tAVRC_MSG_VENDOR *p_vendor)
{
    tAVRC_STS   status = BTA_AVK_STS_NO_RSP;
    UINT8       xx;
    UINT16      u16;
    UINT8       *p = p_vendor->p_vendor_data + 2;

    BE_STREAM_TO_UINT16 (u16, p);
    /* double check the fixed length */
    if ((u16 != 5) || (p_vendor->vendor_len != 9))
    {
        status = AVRC_STS_INTERNAL_ERR;
    }
    else
    {
        /* make sure the player_id is valid */
        for (xx=0; xx<p_bta_avk_cfg->num_evt_ids; xx++)
        {
            if (*p == p_bta_avk_cfg->p_meta_evt_ids[xx])
            {
                break;
            }
        }
        if (xx == p_bta_avk_cfg->num_evt_ids)
        {
            status = AVRC_STS_BAD_PARAM;
        }
    }

    return status;
}

/*******************************************************************************
**
** Function         bta_avk_proc_meta_cmd
**
** Description      Process an AVRCP metadata command from the peer.
**
** Returns          TRUE to respond immediately
**
*******************************************************************************/
tBTA_AVK_EVT bta_avk_proc_meta_cmd(tAVRC_RESPONSE  *p_rc_rsp, tBTA_AVK_RC_MSG *p_msg, UINT8 *p_ctype)
{
    tBTA_AVK_EVT evt = BTA_AVK_META_MSG_EVT;
    UINT8       u8, pdu, *p, i;
    UINT16      u16;
    tAVRC_MSG_VENDOR    *p_vendor = &p_msg->msg.vendor;
    BD_ADDR     addr;
    BOOLEAN is_dev_avrcpv_blacklisted = FALSE;

#if (AVRC_METADATA_INCLUDED == TRUE)

    pdu = *(p_vendor->p_vendor_data);
    p_rc_rsp->pdu = pdu;
    *p_ctype = AVRC_RSP_REJ;
    /* Check for valid Meta Length, AVRCP minimum Meta command length 20 */
    if ((AVRC_MIN_META_CMD_LEN + p_vendor->vendor_len) > AVRC_META_CMD_BUF_SIZE)
    {
        /* reject it */
        evt = 0;
        p_rc_rsp->rsp.status = AVRC_STS_BAD_PARAM;
        APPL_TRACE_ERROR("Bailing out: Invalid meta-command length: %d", p_vendor->vendor_len);
        return evt;
    }
    /* Metadata messages only use PANEL sub-unit type */
    if (p_vendor->hdr.subunit_type != AVRC_SUB_PANEL)
    {
        APPL_TRACE_DEBUG("SUBUNIT must be PANEL");
        /* reject it */
        evt=0;
        p_vendor->hdr.ctype = BTA_AVK_RSP_NOT_IMPL;
        AVRC_VendorRsp(p_msg->handle, p_msg->label, &p_msg->msg.vendor);
    }
    else if (!AVRC_IsValidAvcType(pdu, p_vendor->hdr.ctype) )
    {
        APPL_TRACE_DEBUG("Invalid pdu/ctype: 0x%x, %d", pdu, p_vendor->hdr.ctype);
        /* reject invalid message without reporting to app */
        evt = 0;
        p_rc_rsp->rsp.status = AVRC_STS_BAD_CMD;
    }
    else
    {
        switch (pdu)
        {
        case AVRC_PDU_GET_CAPABILITIES:
            /* process GetCapabilities command without reporting the event to app */
            evt = 0;
            u8 = *(p_vendor->p_vendor_data + 4);
            p = p_vendor->p_vendor_data + 2;
            p_rc_rsp->get_caps.capability_id = u8;
            BE_STREAM_TO_UINT16 (u16, p);
            if ((u16 != 1) || (p_vendor->vendor_len != 5))
            {
                p_rc_rsp->get_caps.status = AVRC_STS_INTERNAL_ERR;
            }
            else
            {
                p_rc_rsp->get_caps.status = AVRC_STS_NO_ERROR;
                if (u8 == AVRC_CAP_COMPANY_ID)
                {
                    *p_ctype = AVRC_RSP_IMPL_STBL;
                    p_rc_rsp->get_caps.count = p_bta_avk_cfg->num_co_ids;
                    memcpy(p_rc_rsp->get_caps.param.company_id, p_bta_avk_cfg->p_meta_co_ids,
                           (p_bta_avk_cfg->num_co_ids << 2));
                }
                else if (u8 == AVRC_CAP_EVENTS_SUPPORTED)
                {
                    *p_ctype = AVRC_RSP_IMPL_STBL;
                    p_rc_rsp->get_caps.count = p_bta_avk_cfg->num_evt_ids;
                    /* DUT has blacklisted few remote dev for Avrcp Version hence
                     * respose for event supported should not have AVRCP 1.5/1.4
                     * version events
                     */
                    if (avct_get_peer_addr_by_ccb(p_msg->handle, addr) == TRUE)
                    {
                        is_dev_avrcpv_blacklisted = SDP_Dev_Blacklisted_For_Avrcp15(addr);
                        BTIF_TRACE_ERROR("Blacklist for AVRCP1.5 = %d", is_dev_avrcpv_blacklisted);
                    }
                    BTIF_TRACE_DEBUG("Blacklist for AVRCP1.5 = %d", is_dev_avrcpv_blacklisted);
                    if (is_dev_avrcpv_blacklisted == TRUE)
                    {
                        for (i = 0; i <= p_bta_avk_cfg->num_evt_ids; ++i)
                        {
                           if (p_bta_avk_cfg->p_meta_evt_ids[i] == AVRC_EVT_AVAL_PLAYERS_CHANGE)
                              break;
                        }
                        p_rc_rsp->get_caps.count = i;
                        memcpy(p_rc_rsp->get_caps.param.event_id, p_bta_avk_cfg->p_meta_evt_ids, i);
                    }
                    else
                    {
                        memcpy(p_rc_rsp->get_caps.param.event_id, p_bta_avk_cfg->p_meta_evt_ids,
                               p_bta_avk_cfg->num_evt_ids);
                    }
                }
                else
                {
                    APPL_TRACE_DEBUG("Invalid capability ID: 0x%x", u8);
                    /* reject - unknown capability ID */
                    p_rc_rsp->get_caps.status = AVRC_STS_BAD_PARAM;
                }
            }
            break;


        case AVRC_PDU_REGISTER_NOTIFICATION:
            /* make sure the event_id is implemented */
            p_rc_rsp->rsp.status = bta_avk_chk_notif_evt_id (p_vendor);
            if (p_rc_rsp->rsp.status != BTA_AVK_STS_NO_RSP)
                evt = 0;
            break;

        }
    }
#else
    BTIF_TRACE_IMP("AVRCP 1.3 Metadata not supporteed. Reject command.");
    /* reject invalid message without reporting to app */
    evt = 0;
    p_rc_rsp->rsp.status = AVRC_STS_BAD_CMD;
#endif

    return evt;
}

/*****************************************************************************
**
** Function        bta_avk_proc_browse_cmd
**
** Description     Process and AVRCP browse command from the peer
**
** Returns         status
**
****************************************************************************/
tBTA_AVK_EVT bta_avk_proc_browse_cmd(tAVRC_RESPONSE  *p_rc_rsp, tBTA_AVK_RC_MSG *p_msg)
{
    tBTA_AVK_EVT evt = BTA_AVK_BROWSE_MSG_EVT;
    UINT8       u8, pdu, *p;
    UINT16      u16;
    tAVRC_MSG_BROWSE *p_browse = &p_msg->msg.browse;

    pdu = p_browse->p_browse_data[0];
    APPL_TRACE_DEBUG("bta_avk_proc_browse_cmd browse cmd: %x", pdu);
    switch (pdu)
    {
        case AVRC_PDU_GET_FOLDER_ITEMS:
        case AVRC_PDU_SET_BROWSED_PLAYER:
        case AVRC_PDU_CHANGE_PATH:
        case AVRC_PDU_GET_ITEM_ATTRIBUTES:
        case AVRC_PDU_GET_TOTAL_NUMBER_OF_ITEMS:
            break;
        default:
            evt = 0;
            p_rc_rsp->rsp.status = AVRC_STS_BAD_CMD;
            APPL_TRACE_ERROR("### Not supported cmd: %x", pdu);
            break;
    }
    return evt;
}

/*******************************************************************************
**
** Function         bta_avk_rc_msg
**
** Description      Process an AVRCP message from the peer.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_msg(tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_EVT evt = 0;
    tBTA_AVK     av;
    BT_HDR      *p_pkt = NULL;
    tAVRC_MSG_VENDOR    *p_vendor = &p_data->rc_msg.msg.vendor;
    BOOLEAN is_inquiry = ((p_data->rc_msg.msg.hdr.ctype == AVRC_CMD_SPEC_INQ) || p_data->rc_msg.msg.hdr.ctype == AVRC_CMD_GEN_INQ);
#if (AVRC_METADATA_INCLUDED == TRUE)
    UINT8       ctype = 0;
    tAVRC_RESPONSE  rc_rsp;
    char value[PROPERTY_VALUE_MAX];

    rc_rsp.rsp.status = BTA_AVK_STS_NO_RSP;
#endif
    APPL_TRACE_DEBUG("bta_avk_rc_msg opcode: %x",p_data->rc_msg.opcode);

    if (p_data->rc_msg.opcode == AVRC_OP_PASS_THRU)
    {
    /* if this is a pass thru command */
        if ((p_data->rc_msg.msg.hdr.ctype == AVRC_CMD_CTRL) ||
            (p_data->rc_msg.msg.hdr.ctype == AVRC_CMD_SPEC_INQ) ||
            (p_data->rc_msg.msg.hdr.ctype == AVRC_CMD_GEN_INQ)
            )
        {
        /* check if operation is supported */
            if (p_data->rc_msg.msg.pass.op_id == AVRC_ID_VENDOR)
            {
                p_data->rc_msg.msg.hdr.ctype = BTA_AVK_RSP_NOT_IMPL;
#if (AVRC_METADATA_INCLUDED == TRUE)
                if (p_cb->features & BTA_AVK_FEAT_METADATA)
                    p_data->rc_msg.msg.hdr.ctype =
                        bta_avk_group_navi_supported(p_data->rc_msg.msg.pass.pass_len,
                        p_data->rc_msg.msg.pass.p_pass_data, is_inquiry);
#endif
            }
#if (AVRC_CTLR_INCLUDED == TRUE)
            else if (((p_data->rc_msg.msg.pass.op_id == AVRC_ID_VOL_UP)||
                      (p_data->rc_msg.msg.pass.op_id == AVRC_ID_VOL_DOWN))&&
                     ((property_get_bt("bluetooth.pts.avrcp_ct.support", value, "false"))&&
                      (!strcmp(value, "true"))))
            {
                p_data->rc_msg.msg.hdr.ctype = BTA_AVK_RSP_ACCEPT;
            }
#endif
            else
            {
                p_data->rc_msg.msg.hdr.ctype = bta_avk_op_supported(p_data->rc_msg.msg.pass.op_id, is_inquiry);
            }

            APPL_TRACE_DEBUG("ctype %d",p_data->rc_msg.msg.hdr.ctype)

            /* send response */
            if (p_data->rc_msg.msg.hdr.ctype != BTA_AVK_RSP_INTERIM)
                AVRC_PassRsp(p_data->rc_msg.handle, p_data->rc_msg.label, &p_data->rc_msg.msg.pass);

            /* set up for callback if supported */
            if (p_data->rc_msg.msg.hdr.ctype == BTA_AVK_RSP_ACCEPT || p_data->rc_msg.msg.hdr.ctype == BTA_AVK_RSP_INTERIM)
            {
                evt = BTA_AVK_REMOTE_CMD_EVT;
                av.remote_cmd.rc_id = p_data->rc_msg.msg.pass.op_id;
                av.remote_cmd.key_state = p_data->rc_msg.msg.pass.state;
                av.remote_cmd.p_data = p_data->rc_msg.msg.pass.p_pass_data;
                av.remote_cmd.len = p_data->rc_msg.msg.pass.pass_len;
                memcpy(&av.remote_cmd.hdr, &p_data->rc_msg.msg.hdr, sizeof (tAVRC_HDR));
                av.remote_cmd.label = p_data->rc_msg.label;
            }
        }
        /* else if this is a pass thru response */
        else if (p_data->rc_msg.msg.hdr.ctype >= AVRC_RSP_NOT_IMPL)
        {
            /* set up for callback */
            evt = BTA_AVK_REMOTE_RSP_EVT;
            av.remote_rsp.rc_id = p_data->rc_msg.msg.pass.op_id;
            av.remote_rsp.key_state = p_data->rc_msg.msg.pass.state;
            av.remote_rsp.rsp_code = p_data->rc_msg.msg.hdr.ctype;
            av.remote_rsp.label = p_data->rc_msg.label;
        }
        /* must be a bad ctype -> reject*/
        else
        {
            p_data->rc_msg.msg.hdr.ctype = BTA_AVK_RSP_REJ;
            AVRC_PassRsp(p_data->rc_msg.handle, p_data->rc_msg.label, &p_data->rc_msg.msg.pass);
        }
    }
    /* else if this is a vendor specific command or response */
    else if (p_data->rc_msg.opcode == AVRC_OP_VENDOR)
    {
        /* set up for callback */
        av.vendor_cmd.code = p_data->rc_msg.msg.hdr.ctype;
        av.vendor_cmd.company_id = p_vendor->company_id;
        av.vendor_cmd.label = p_data->rc_msg.label;
        av.vendor_cmd.p_data = p_vendor->p_vendor_data;
        av.vendor_cmd.len = p_vendor->vendor_len;

        /* if configured to support vendor specific and it's a command */
        if ((p_cb->features & BTA_AVK_FEAT_VENDOR)  &&
            p_data->rc_msg.msg.hdr.ctype <= AVRC_CMD_GEN_INQ)
        {
#if (AVRC_METADATA_INCLUDED == TRUE)
            if ((p_cb->features & BTA_AVK_FEAT_METADATA) &&
               (p_vendor->company_id == AVRC_CO_METADATA))
            {
                av.meta_msg.p_msg = &p_data->rc_msg.msg;
                evt = bta_avk_proc_meta_cmd (&rc_rsp, &p_data->rc_msg, &ctype);
            }
            else
#endif
                evt = BTA_AVK_VENDOR_CMD_EVT;
        }
        /* else if configured to support vendor specific and it's a response */
        else if ((p_cb->features & BTA_AVK_FEAT_VENDOR) &&
                 p_data->rc_msg.msg.hdr.ctype >= AVRC_RSP_NOT_IMPL)
        {
#if (AVRC_METADATA_INCLUDED == TRUE)
            if ((p_cb->features & BTA_AVK_FEAT_METADATA) &&
               (p_vendor->company_id == AVRC_CO_METADATA))
            {
                av.meta_msg.p_msg = &p_data->rc_msg.msg;
                evt = BTA_AVK_META_MSG_EVT;
            }
            else
#endif
                evt = BTA_AVK_VENDOR_RSP_EVT;

        }
        /* else if not configured to support vendor specific and it's a command */
        else if (!(p_cb->features & BTA_AVK_FEAT_VENDOR)  &&
            p_data->rc_msg.msg.hdr.ctype <= AVRC_CMD_GEN_INQ)
        {
           if(p_data->rc_msg.msg.vendor.p_vendor_data[0] == AVRC_PDU_INVALID)
           {
           /* reject it */
              p_data->rc_msg.msg.hdr.ctype = BTA_AVK_RSP_REJ;
              p_data->rc_msg.msg.vendor.p_vendor_data[4] = AVRC_STS_BAD_CMD;
           }
           else
              p_data->rc_msg.msg.hdr.ctype = BTA_AVK_RSP_NOT_IMPL;
           AVRC_VendorRsp(p_data->rc_msg.handle, p_data->rc_msg.label, &p_data->rc_msg.msg.vendor);
        }
    }
#if (AVCT_BROWSE_INCLUDED == TRUE)
    else if (p_data->rc_msg.opcode == AVRC_OP_BROWSE )
    {
       /* set up for callback */
       av.meta_msg.rc_handle = p_data->rc_msg.handle;
       av.meta_msg.company_id = p_vendor->company_id;
       av.meta_msg.code = p_data->rc_msg.msg.hdr.ctype;
       av.meta_msg.label = p_data->rc_msg.label;
       av.meta_msg.p_msg = &p_data->rc_msg.msg;
       av.meta_msg.p_data = p_data->rc_msg.msg.browse.p_browse_data;
       av.meta_msg.len = p_data->rc_msg.msg.browse.browse_len;
       evt = BTA_AV_META_MSG_EVT;
     }
#endif
#if (AVRC_METADATA_INCLUDED == TRUE)
    if (evt == 0 && rc_rsp.rsp.status != BTA_AVK_STS_NO_RSP)
    {
        if (!p_pkt)
        {
            rc_rsp.rsp.opcode = p_data->rc_msg.opcode;
            AVRC_BldResponse (0, &rc_rsp, &p_pkt);
        }
        if (p_pkt)
            AVRC_MsgReq (p_data->rc_msg.handle, p_data->rc_msg.label, ctype, p_pkt);
    }
#endif

    /* call callback */
    if (evt != 0)
    {
        av.remote_cmd.rc_handle = p_data->rc_msg.handle;
        (*p_cb->p_cback)(evt, &av);
        /* If browsing message, then free the browse message buffer */
        bta_avk_rc_free_browse_msg(p_cb, p_data);

    }
}

/*******************************************************************************
**
** Function         bta_avk_rc_close
**
** Description      close the specified AVRC handle.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_close (tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    UINT16 handle = p_data->hdr.layer_specific;
    tBTA_AVK_SCB  *p_scb;
    tBTA_AVK_RCB *p_rcb;

    if(handle < BTA_AVK_NUM_RCB)
    {
        p_rcb = &p_cb->rcb[handle];

        APPL_TRACE_DEBUG("bta_avk_rc_close handle: %d, status=0x%x", p_rcb->handle, p_rcb->status);
        if(p_rcb->handle != BTA_AVK_RC_HANDLE_NONE)
        {
            if(p_rcb->shdl)
            {
                p_scb = bta_avk_cb.p_scb[p_rcb->shdl - 1];
                if(p_scb)
                {
                    /* just in case the RC timer is active
                    if(bta_avk_cb.features & BTA_AVK_FEAT_RCCT &&
                       p_scb->chnl == BTA_AVK_CHNL_AUDIO) */
                        alarm_cancel(p_scb->avrc_ct_timer);
                }
            }

            AVRC_Close(p_rcb->handle);
        }
    }
}

void bta_avk_rc_br_close(tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_RC_BROWSE_CLOSE br_close;

    br_close.rc_handle = p_data->rc_conn_chg.handle;
    bta_avk_cb.rcb[br_close.rc_handle].status &= ~BTA_AVK_RC_CONN_BR_MASK;
    APPL_TRACE_DEBUG("bta_avk_rc_br_close ");

    bdcpy(br_close.peer_addr, p_data->rc_conn_chg.peer_addr);
    (*p_cb->p_cback)(BTA_AVK_RC_BROWSE_CLOSE_EVT, (tBTA_AVK *)&br_close);

}


/*******************************************************************************
**
** Function         bta_avk_get_shdl
**
** Returns          The index to p_scb[]
**
*******************************************************************************/
static UINT8 bta_avk_get_shdl(tBTA_AVK_SCB *p_scb)
{
    int     i;
    UINT8   shdl = 0;
    /* find the SCB & stop the timer */
    for(i=0; i<BTA_AVK_NUM_STRS; i++)
    {
        if(p_scb == bta_avk_cb.p_scb[i])
        {
            shdl = i+1;
            break;
        }
    }
    return shdl;
}

/*******************************************************************************
**
** Function         bta_avk_stream_chg
**
** Description      audio streaming status changed.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_stream_chg(tBTA_AVK_SCB *p_scb, BOOLEAN started)
{
    UINT8   started_msk;
    int     i;
    UINT8   *p_streams;
    BOOLEAN no_streams = FALSE;
    tBTA_AVK_SCB *p_scbi;

    started_msk = BTA_AVK_HNDL_TO_MSK(p_scb->hdi);
    APPL_TRACE_DEBUG ("bta_avk_stream_chg started:%d started_msk:x%x chnl:x%x", started,
                                                  started_msk, p_scb->chnl);
    if (BTA_AVK_CHNL_AUDIO == p_scb->chnl)
        p_streams = &bta_avk_cb.audio_streams;
    else
        p_streams = &bta_avk_cb.video_streams;

    if (started)
    {
        /* Let L2CAP know this channel is processed with high priority */
        L2CA_SetAclPriority(p_scb->peer_addr, L2CAP_PRIORITY_HIGH);
        (*p_streams) |= started_msk;
    }
    else
    {
        (*p_streams) &= ~started_msk;
    }

    if (!started)
    {
        i=0;
        if (BTA_AVK_CHNL_AUDIO == p_scb->chnl)
        {
            if (bta_avk_cb.video_streams == 0)
                no_streams = TRUE;
        }
        else
        {
            no_streams = TRUE;
            if ( bta_avk_cb.audio_streams )
            {
                for (; i<BTA_AVK_NUM_STRS; i++)
                {
                    p_scbi = bta_avk_cb.p_scb[i];
                    /* scb is used and started */
                    if ( p_scbi && (bta_avk_cb.audio_streams & BTA_AVK_HNDL_TO_MSK(i))
                        && bdcmp(p_scbi->peer_addr, p_scb->peer_addr) == 0)
                    {
                        no_streams = FALSE;
                        break;
                    }
                }

            }
        }

        APPL_TRACE_DEBUG ("no_streams:%d i:%d, audio_streams:x%x, video_streams:x%x", no_streams, i,
                           bta_avk_cb.audio_streams, bta_avk_cb.video_streams);
        if (no_streams)
        {
            /* Let L2CAP know this channel is processed with low priority */
            L2CA_SetAclPriority(p_scb->peer_addr, L2CAP_PRIORITY_NORMAL);
        }
    }
}


/*******************************************************************************
**
** Function         bta_avk_conn_chg
**
** Description      connetion status changed.
**                  Open an AVRCP acceptor channel, if new conn.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_conn_chg(tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    tBTA_AVK_SCB     *p_scb = NULL;
    tBTA_AVK_SCB     *p_scbi;
    UINT8   mask;
    UINT8   conn_msk;
    UINT8   old_msk;
    int i;
    int index = (p_data->hdr.layer_specific & BTA_AVK_HNDL_MSK) - 1;
    tBTA_AVK_LCB *p_lcb;
    tBTA_AVK_LCB *p_lcb_rc;
    tBTA_AVK_RCB *p_rcb, *p_rcb2;
    BOOLEAN     chk_restore = FALSE;

    /* Validate array index*/
    /* Fix for below klockwork issue
     * Array 'p_scb' size is 2.
     * Possible attempt to access element -1 of array 'p_scb' */
    if (index >= 0 && index < BTA_AVK_NUM_STRS)
    {
        p_scb = p_cb->p_scb[index];
    }
    mask = BTA_AVK_HNDL_TO_MSK(index);
    p_lcb = bta_avk_find_lcb(p_data->conn_chg.peer_addr, BTA_AVK_LCB_FIND);
    conn_msk = 1 << (index + 1);
    if(p_data->conn_chg.is_up)
    {
        /* set the conned mask for this channel */
        if(p_scb)
        {
            if(p_lcb)
            {
                p_lcb->conn_msk |= conn_msk;
                for (i=0; i<BTA_AVK_NUM_RCB; i++)
                {
                    if (bta_avk_cb.rcb[i].lidx == p_lcb->lidx)
                    {
                        bta_avk_cb.rcb[i].shdl = index + 1;
                        APPL_TRACE_DEBUG("conn_chg up[%d]: %d, status=0x%x, shdl:%d, lidx:%d", i,
                                          bta_avk_cb.rcb[i].handle, bta_avk_cb.rcb[i].status,
                                          bta_avk_cb.rcb[i].shdl, bta_avk_cb.rcb[i].lidx);
                        break;
                    }
                }
            }
            if (p_scb->chnl == BTA_AVK_CHNL_AUDIO)
            {
                old_msk = p_cb->conn_audio;
                p_cb->conn_audio |= mask;
            }
            else
            {
                old_msk = p_cb->conn_video;
                p_cb->conn_video |= mask;
            }

            if ((old_msk & mask) == 0)
            {
                /* increase the audio open count, if not set yet */
                bta_avk_cb.audio_open_cnt++;
            }


            APPL_TRACE_DEBUG("rc_acp_handle:%d rc_acp_idx:%d", p_cb->rc_acp_handle, p_cb->rc_acp_idx);
            /* check if the AVRCP ACP channel is already connected */
            if(p_lcb && p_cb->rc_acp_handle != BTA_AVK_RC_HANDLE_NONE && p_cb->rc_acp_idx)
            {
                p_lcb_rc = &p_cb->lcb[BTA_AVK_NUM_LINKS];
                APPL_TRACE_DEBUG("rc_acp is connected && conn_chg on same addr p_lcb_rc->conn_msk:x%x",
                                  p_lcb_rc->conn_msk);
                /* check if the RC is connected to the scb addr */
                APPL_TRACE_DEBUG ("p_lcb_rc->addr: %02x:%02x:%02x:%02x:%02x:%02x",
                       p_lcb_rc->addr[0], p_lcb_rc->addr[1], p_lcb_rc->addr[2], p_lcb_rc->addr[3],
                       p_lcb_rc->addr[4], p_lcb_rc->addr[5]);
                APPL_TRACE_DEBUG ("conn_chg.peer_addr: %02x:%02x:%02x:%02x:%02x:%02x",
                       p_data->conn_chg.peer_addr[0], p_data->conn_chg.peer_addr[1],
                       p_data->conn_chg.peer_addr[2],
                       p_data->conn_chg.peer_addr[3], p_data->conn_chg.peer_addr[4],
                       p_data->conn_chg.peer_addr[5]);
                if (p_lcb_rc->conn_msk && bdcmp(p_lcb_rc->addr, p_data->conn_chg.peer_addr) == 0)
                {
                    /* AVRCP is already connected.
                     * need to update the association betwen SCB and RCB */
                    p_lcb_rc->conn_msk = 0; /* indicate RC ONLY is not connected */
                    p_lcb_rc->lidx = 0;
                    p_scb->rc_handle = p_cb->rc_acp_handle;
                    p_rcb = &p_cb->rcb[p_cb->rc_acp_idx - 1];
                    p_rcb->shdl = bta_avk_get_shdl(p_scb);
                    APPL_TRACE_DEBUG("update rc_acp shdl:%d/%d srch:%d", index + 1, p_rcb->shdl,
                                      p_scb->rc_handle );

                    p_rcb2 = bta_avk_get_rcb_by_shdl(p_rcb->shdl);
                    if (p_rcb2)
                    {
                        /* found the RCB that was created to associated with this SCB */
                        p_cb->rc_acp_handle = p_rcb2->handle;
                        p_cb->rc_acp_idx = (p_rcb2 - p_cb->rcb) + 1;
                        APPL_TRACE_DEBUG("new rc_acp_handle:%d, idx:%d", p_cb->rc_acp_handle,
                                           p_cb->rc_acp_idx);
                        p_rcb2->lidx = (BTA_AVK_NUM_LINKS + 1);
                        APPL_TRACE_DEBUG("rc2 handle:%d lidx:%d/%d",p_rcb2->handle, p_rcb2->lidx,
                                          p_cb->lcb[p_rcb2->lidx-1].lidx);
                    }
                    p_rcb->lidx = p_lcb->lidx;
                    APPL_TRACE_DEBUG("rc handle:%d lidx:%d/%d",p_rcb->handle, p_rcb->lidx,
                                      p_cb->lcb[p_rcb->lidx-1].lidx);
                }
            }
        }
    }
    else
    {
        if ((p_cb->conn_audio & mask) && bta_avk_cb.audio_open_cnt)
        {
            /* this channel is still marked as open. decrease the count */
            bta_avk_cb.audio_open_cnt--;
        }

        /* clear the conned mask for this channel */
        p_cb->conn_audio &= ~mask;
        p_cb->conn_video &= ~mask;
        if(p_scb)
        {
            /* the stream is closed.
             * clear the peer address, so it would not mess up the AVRCP for the next round of operation */
            bdcpy(p_scb->peer_addr, bd_addr_null);
            if(p_scb->chnl == BTA_AVK_CHNL_AUDIO)
            {
                if(p_lcb)
                {
                    p_lcb->conn_msk &= ~conn_msk;
                }
                /* audio channel is down. make sure the INT channel is down */
                /* just in case the RC timer is active
                if(p_cb->features & BTA_AVK_FEAT_RCCT) */
                {
                    alarm_cancel(p_scb->avrc_ct_timer);
                }
                /* one audio channel goes down. check if we need to restore high priority */
                chk_restore = TRUE;
            }
        }

        APPL_TRACE_DEBUG("bta_avk_conn_chg shdl:%d", index + 1);
        for (i=0; i<BTA_AVK_NUM_RCB; i++)
        {
            APPL_TRACE_DEBUG("conn_chg dn[%d]: %d, status=0x%x, shdl:%d, lidx:%d", i,
                              bta_avk_cb.rcb[i].handle, bta_avk_cb.rcb[i].status,
                              bta_avk_cb.rcb[i].shdl, bta_avk_cb.rcb[i].lidx);
            if(bta_avk_cb.rcb[i].shdl == index + 1)
            {
                bta_avk_del_rc(&bta_avk_cb.rcb[i]);
                break;
            }
        }

        if(p_cb->conn_audio == 0 && p_cb->conn_video == 0)
        {
            /* if both channels are not connected,
             * close all RC channels */
            bta_avk_close_all_rc(p_cb);
        }

        /* if the AVRCP is no longer listening, create the listening channel */
        if (bta_avk_cb.rc_acp_handle == BTA_AVK_RC_HANDLE_NONE && bta_avk_cb.features & BTA_AVK_FEAT_RCTG)
            bta_avk_rc_create(&bta_avk_cb, AVCT_ACP, 0, BTA_AVK_NUM_LINKS + 1);
    }

    APPL_TRACE_DEBUG("bta_avk_conn_chg audio:%x video:%x up:%d conn_msk:0x%x chk_restore:%d audio_open_cnt:%d",
        p_cb->conn_audio, p_cb->conn_video, p_data->conn_chg.is_up, conn_msk, chk_restore, p_cb->audio_open_cnt);

    if (chk_restore)
    {
        if (p_cb->audio_open_cnt == 1)
        {
            /* one audio channel goes down and there's one audio channel remains open.
             * restore the switch role in default link policy */
            bta_sys_set_default_policy(BTA_ID_AVK, HCI_ENABLE_MASTER_SLAVE_SWITCH);
            /* allow role switch, if this is the last connection */
            bta_avk_restore_switch();
        }
        if (p_cb->audio_open_cnt)
        {
            /* adjust flush timeout settings to longer period */
            for (i=0; i<BTA_AVK_NUM_STRS; i++)
            {
                p_scbi = bta_avk_cb.p_scb[i];
                if (p_scbi && p_scbi->chnl == BTA_AVK_CHNL_AUDIO && p_scbi->co_started)
                {
                    /* may need to update the flush timeout of this already started stream */
                    if (p_scbi->co_started != bta_avk_cb.audio_open_cnt)
                    {
                        p_scbi->co_started = bta_avk_cb.audio_open_cnt;
                        L2CA_SetFlushTimeout(p_scbi->peer_addr, p_bta_avk_cfg->p_audio_flush_to[p_scbi->co_started - 1] );
                    }
                }
            }
        }
    }
}

/*******************************************************************************
**
** Function         bta_avk_disable
**
** Description      disable AV.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_disable(tBTA_AVK_CB *p_cb, tBTA_AVK_DATA *p_data)
{
    BT_HDR  hdr;
    UINT16  xx;
    UNUSED(p_data);

    p_cb->disabling = TRUE;
    bta_avk_close_all_rc(p_cb);

    /*Cancel SDP if it had been started. */
    if(p_cb->p_disc_db) {
        (void)SDP_CancelServiceSearch (p_cb->p_disc_db);
         osi_free_and_reset((void **) &p_cb->p_disc_db);
    }

    /* disable audio/video - de-register all channels,
     * expect BTA_AVK_DEREG_COMP_EVT when deregister is complete */
    for(xx=0; xx<BTA_AVK_NUM_STRS; xx++)
    {
        hdr.layer_specific = xx + 1;
        bta_avk_api_deregister((tBTA_AVK_DATA *)&hdr);
    }
    for(xx=0; xx<BTA_AVK_NUM_RCB; xx++)
    {
        tBTA_AVK_RCB    *p_rcb = &p_cb->rcb[xx];
        if(p_rcb->br_conn_timer != NULL)
        {
            alarm_free(p_rcb->br_conn_timer);
            p_rcb->br_conn_timer = NULL;
        }
    }
    alarm_free(p_cb->link_signalling_timer);
    p_cb->link_signalling_timer = NULL;
    alarm_free(p_cb->accept_signalling_timer);
    p_cb->accept_signalling_timer = NULL;
}

/*******************************************************************************
**
** Function         bta_avk_api_disconnect
**
** Description      .
**
** Returns          void
**
*******************************************************************************/
void bta_avk_api_disconnect(tBTA_AVK_DATA *p_data)
{
    AVDT_DisconnectReq(p_data->api_discnt.bd_addr, bta_avk_conn_cback);
    alarm_cancel(bta_avk_cb.link_signalling_timer);
}

/*******************************************************************************
**
** Function         bta_avk_sig_chg
**
** Description      process AVDT signal channel up/down.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_sig_chg(tBTA_AVK_DATA *p_data)
{
    UINT16 event = p_data->str_msg.hdr.layer_specific;
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    int     xx;
    UINT8   mask;
    UINT8   sep_type;
    tBTA_AVK_LCB *p_lcb = NULL;

    BTIF_TRACE_IMP("%s bta_avk_sig_chg event: %d",
            __FUNCTION__, event);
    if(event == AVDT_CONNECT_IND_EVT)
    {
        p_lcb = bta_avk_find_lcb(p_data->str_msg.bd_addr, BTA_AVK_LCB_FIND);
        if(!p_lcb)
        {
            if (p_cb->conn_lcb > 0)
            {
                APPL_TRACE_DEBUG("Already connected to LCBs: 0x%x", p_cb->conn_lcb);
            }
            /* Check if busy processing a connection, if yes, Reject the
             * new incoming connection.
             * This is very rare case to happen as the timeout to start
             * signalling procedure is just 2 sec.
             * Also sink initiators will have retry machanism.
             * Even though busy flag is set during outgoing connection to
             * reject incoming connection at L2CAP connect request, there
             * is a chance to get here if the incoming connection has passed
             * the L2CAP connection stage.
             */
            if((p_data->hdr.offset == AVDT_ACP) && (AVDT_GetServiceBusyState() == TRUE))
            {
                APPL_TRACE_ERROR("%s Incoming conn while processing another.. Reject",
                    __FUNCTION__);
                AVDT_DisconnectReq (p_data->str_msg.bd_addr, NULL);
                return;
            }

            /* if the address does not have an LCB yet, alloc one */
            for(xx=0; xx<BTA_AVK_NUM_LINKS; xx++)
            {
                mask = 1 << xx;
                APPL_TRACE_DEBUG("The current conn_lcb: 0x%x", p_cb->conn_lcb);

                /* look for a p_lcb with its p_scb registered */
                if((!(mask & p_cb->conn_lcb)) && (p_cb->p_scb[xx] != NULL))
                {
                    /* Check if the SCB is Free before using for
                     * ACP connection
                     */
                    if ((p_data->hdr.offset == AVDT_ACP) &&
                        (p_cb->p_scb[xx]->state != BTA_AVK_INIT_SST))
                    {
                        APPL_TRACE_DEBUG("SCB in use %d", xx);
                        continue;
                    }

                    APPL_TRACE_DEBUG("Found a free p_lcb : 0x%x", xx);
                    p_lcb = &p_cb->lcb[xx];
                    p_lcb->lidx = xx + 1;
                    bdcpy(p_lcb->addr, p_data->str_msg.bd_addr);
                    p_lcb->conn_msk = 0; /* clear the connect mask */
                    /* start listening when the signal channel is open */
                    if (p_cb->features & BTA_AVK_FEAT_RCTG)
                    {
                        bta_avk_rc_create(p_cb, AVCT_ACP, 0, p_lcb->lidx);
                    }
                    /* this entry is not used yet. */
                    p_cb->conn_lcb |= mask;     /* mark it as used */
                    APPL_TRACE_DEBUG("start sig timer %d", p_data->hdr.offset);
                    if (p_data->hdr.offset == AVDT_ACP)
                    {
                        APPL_TRACE_DEBUG("Incoming L2CAP acquired, set state as incoming", NULL);
                        bdcpy(p_cb->p_scb[xx]->peer_addr, p_data->str_msg.bd_addr);
                        p_cb->p_scb[xx]->use_rc = TRUE;     /* allowing RC for incoming connection */
                        APPL_TRACE_DEBUG("Event sent =0x%x, Base Event = 0x%x diff = %d", BTA_AVK_ACP_CONNECT_EVT, BTA_AVK_FIRST_SSM_EVT, BTA_AVK_ACP_CONNECT_EVT-BTA_AVK_FIRST_SSM_EVT);
                        bta_avk_ssm_execute(p_cb->p_scb[xx], BTA_AVK_ACP_CONNECT_EVT, p_data);
                        APPL_TRACE_DEBUG("Will start Avk sig timer");

                        /* The Pending Event should be sent as soon as the L2CAP signalling channel
                         * is set up, which is NOW. Earlier this was done only after
                         * BTA_AVK_SIG_TIME_VAL milliseconds.
                         * The following function shall send the event and start the recurring timer
                         */
                        bta_avk_sig_timer(NULL);
                        APPL_TRACE_DEBUG("Re-start timer for AVDTP service");
                        bta_sys_conn_open(BTA_ID_AVK, p_cb->p_scb[xx]->app_id,
                                p_cb->p_scb[xx]->peer_addr);
                        /* Possible collision : need to avoid outgoing processing while the timer is running */
                        p_cb->p_scb[xx]->coll_mask = BTA_AVK_COLL_INC_TMR;

                        alarm_set_on_queue(p_cb->accept_signalling_timer,
                                           BTA_AVK_ACP_SIG_TIME_VAL,
                                           bta_avk_acp_sig_timer_cback,
                                           UINT_TO_PTR(xx),
                                           btu_bta_alarm_queue);
                    }
                    break;
                }
            }

            /* check if we found something */
            if (xx == BTA_AVK_NUM_LINKS)
            {
                /* We do not have scb for this avdt connection.     */
                /* Silently close the connection.                   */
                APPL_TRACE_ERROR("avk scb not available for avdt connection");
                AVDT_DisconnectReq (p_data->str_msg.bd_addr, NULL);
                return;
            }
        }
    }
#if( defined BTA_AR_INCLUDED ) && (BTA_AR_INCLUDED == TRUE)
    else if (event == BTA_AR_AVDT_CONN_EVT)
    {
        alarm_cancel(bta_avk_cb.link_signalling_timer);
    }
#endif
    else
    {
        /* disconnected. */
        int is_lcb_used = bta_avk_cb.conn_lcb;
        APPL_TRACE_DEBUG(" is_lcb_used is %d",is_lcb_used);
        sep_type = get_remote_sep_type(p_data->str_msg.bd_addr);
        if(!(sep_type & BTA_AR_EXT_AV_MASK))
            dealloc_ar_device_info(p_data->str_msg.bd_addr);
        p_lcb = bta_avk_find_lcb(p_data->str_msg.bd_addr, BTA_AVK_LCB_FREE);
        if (p_lcb && (p_lcb->conn_msk || is_lcb_used))
        {
            APPL_TRACE_DEBUG("conn_msk: 0x%x", p_lcb->conn_msk);
            /* clean up ssm  */
            for(xx=0; xx < BTA_AVK_NUM_STRS; xx++)
            {

                if ((p_cb->p_scb[xx]) &&
                        (bdcmp(p_cb->p_scb[xx]->peer_addr, p_data->str_msg.bd_addr) == 0) )
                {
                    APPL_TRACE_DEBUG("Closing timer for AVDTP service");
                    bta_sys_conn_close(BTA_ID_AVK, p_cb->p_scb[xx]->app_id,p_cb->p_scb[xx]->peer_addr);
                }
                mask = 1 << (xx + 1);
                if (((mask & p_lcb->conn_msk) || is_lcb_used)
                     && (p_cb->p_scb[xx]) &&
                    (bdcmp(p_cb->p_scb[xx]->peer_addr, p_data->str_msg.bd_addr) == 0))
                {
                    APPL_TRACE_DEBUG("Sending AVDT_DISCONNECT_EVT");
                    bta_avk_ssm_execute(p_cb->p_scb[xx], BTA_AVK_AVDT_DISCONNECT_EVT, NULL);
                }
            }
        }
    }
    APPL_TRACE_DEBUG("sig_chg conn_lcb: 0x%x", p_cb->conn_lcb);
}

/*******************************************************************************
**
** Function         bta_avk_sig_timer
**
** Description      process the signal channel timer. This timer is started
**                  when the AVDTP signal channel is connected. If no profile
**                  is connected, the timer goes off every BTA_AVK_SIG_TIME_VAL
**
** Returns          void
**
*******************************************************************************/
void bta_avk_sig_timer(tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    int     xx;
    UINT8   mask;
    tBTA_AVK_LCB *p_lcb = NULL;
    tBTA_AVK_PEND pend;
    UNUSED(p_data);

    APPL_TRACE_DEBUG("bta_avk_sig_timer");
    for(xx=0; xx<BTA_AVK_NUM_LINKS; xx++)
    {
        mask = 1 << xx;
        if(mask & p_cb->conn_lcb)
        {
            /* this entry is used. check if it is connected */
            p_lcb = &p_cb->lcb[xx];
            if(!p_lcb->conn_msk)
            {
                bta_sys_start_timer(p_cb->link_signalling_timer, BTA_AVK_SIG_TIME_VAL,
                                    BTA_AVK_SIG_TIMER_EVT, 0);
                bdcpy(pend.bd_addr, p_lcb->addr);
                APPL_TRACE_DEBUG("bta_avk_sig_timer on IDX = %d",xx);
                //Copy the handle of SCB
                pend.hndl = p_cb->p_scb[xx]->hndl;
                (*p_cb->p_cback)(BTA_AVK_PENDING_EVT, (tBTA_AVK *) &pend);
            }
        }
    }
}

static void bta_avk_rc_br_timer_cback (void* data)
{
 /*   UINT8 handle = (UINT8)data;
    APPL_TRACE_DEBUG(" bta_avk_rc_br_timer_cback handle %d ", handle);
    if(!(bta_avk_cb.rcb[handle].status & BTA_AVK_RC_CONN_MASK))
    {
        APPL_TRACE_DEBUG(" Control channel disconnected, returning");
        return;
    }
    if (BTA_AvkIsBrowsingSupported () && bta_avk_cb.rcb[handle].peer_features & BTA_AVK_FEAT_BROWSE)
    {
        BTIF_TRACE_IMP("bta_avk_rc_br_timer_cback AVRC_OpenBrowseChannel handle: %d !~", handle);
        AVRC_OpenBrowseChannel (handle);
    }*/
}


/*******************************************************************************
**
** Function         bta_avk_acp_sig_timer_cback
**
** Description      Process the timeout when SRC is accepting connection
**                  and SNK did not start signalling.
**
** Returns          void
**
*******************************************************************************/
static void bta_avk_acp_sig_timer_cback (void *data)
{
    UINT8   inx = PTR_TO_UINT(data);
    tBTA_AVK_CB  *p_cb = &bta_avk_cb;
    tBTA_AVK_SCB *p_scb = NULL;

    if (inx < BTA_AVK_NUM_STRS)
    {
        p_scb = p_cb->p_scb[inx];
    }
    if (p_scb)
    {
        APPL_TRACE_DEBUG("bta_avk_acp_sig_timer_cback, coll_mask = 0x%02X", p_scb->coll_mask);

        if (p_scb->coll_mask & BTA_AVK_COLL_INC_TMR)
        {
            p_scb->coll_mask &= ~BTA_AVK_COLL_INC_TMR;

            if (bta_avk_is_scb_opening(p_scb))
            {
                if (p_scb->sdp_discovery_started)
                {
                    /* We are still doing SDP. Run the timer again. */
                    p_scb->coll_mask |= BTA_AVK_COLL_INC_TMR;

                    alarm_set_on_queue(p_cb->accept_signalling_timer,
                                       BTA_AVK_ACP_SIG_TIME_VAL,
                                       bta_avk_acp_sig_timer_cback,
                                       UINT_TO_PTR(inx),
                                       btu_bta_alarm_queue);
                }
                else
                {
                    /* SNK did not start signalling, resume signalling process. */
                    bta_avk_discover_req (p_scb, NULL);
                }
            }
            else if (bta_avk_is_scb_incoming(p_scb))
            {
                /* Stay in incoming state if SNK does not start signalling */

                /* API open was called right after SNK opened L2C connection. */
                if (p_scb->coll_mask & BTA_AVK_COLL_API_CALLED)
                {
                    p_scb->coll_mask &= ~BTA_AVK_COLL_API_CALLED;

                    /* BTA_AVK_API_OPEN_EVT */
                     tBTA_AVK_API_OPEN  *p_buf =
                        (tBTA_AVK_API_OPEN *)osi_malloc(sizeof(tBTA_AVK_API_OPEN));
                    if (p_buf != NULL)
                    {
                        memcpy(p_buf, &(p_scb->open_api), sizeof(tBTA_AVK_API_OPEN));
                        bta_sys_sendmsg(p_buf);
                    }
                }
            }
        }
    }
}

BOOLEAN bta_avk_check_store_avrc_tg_version(BD_ADDR addr, UINT16 ver)
{
    BOOLEAN is_present = FALSE;
    struct avk_blacklist_entry data;
    FILE *fp;
    BOOLEAN is_file_updated = FALSE;

    APPL_TRACE_DEBUG("%s target BD Addr: %x:%x:%x", __func__,\
                        addr[0], addr[1], addr[2]);
    fp = fopen(AVRC_PEER_VERSION_CONF_FILE, "rb");
    if (!fp)
    {
      APPL_TRACE_ERROR("%s unable to open AVRC Conf file for read: error: (%s)",\
                                                        __func__, strerror(errno));
    }
    else
    {
        while (fread(&data, sizeof(data), 1, fp) != 0)
        {
            APPL_TRACE_DEBUG("Entry: addr = %x:%x:%x, ver = 0x%x",\
                    data.addr[0], data.addr[1], data.addr[2], data.ver);
            if(!memcmp(addr, data.addr, 3))
            {
                is_present = TRUE;
                APPL_TRACE_DEBUG("Entry alreday present, bailing out");
                break;
            }
        }
        fclose(fp);
    }

    if (is_present == FALSE)
    {
        fp = fopen(AVRC_PEER_VERSION_CONF_FILE, "ab");
        if (!fp)
        {
            APPL_TRACE_ERROR("%s unable to open AVRC Conf file for write: error: (%s)",\
                                                              __func__, strerror(errno));
        }
        else
        {
            data.ver = ver;
            memcpy(data.addr, (const char *)addr, 3);
            data.is_src = true;
            APPL_TRACE_DEBUG("Avrcp version to store = 0x%x", ver);
            fwrite(&data, sizeof(data), 1, fp);
            fclose(fp);
            is_file_updated = TRUE;
        }
    }
    return is_file_updated;
}

/*******************************************************************************
**
** Function         bta_avk_src_check_peer_features
**
** Description      check supported features on the peer device from the SDP record
**                  and return the feature mask
**
** Returns          tBTA_AVK_FEAT peer device feature mask
**
*******************************************************************************/
tBTA_AVK_FEAT bta_avk_src_check_peer_features (UINT16 service_uuid)
{
    tBTA_AVK_FEAT peer_features = 0;
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    tSDP_DISC_REC       *p_rec = NULL;
    tSDP_DISC_ATTR      *p_attr;
    UINT16              peer_rc_version=0; /*Assuming Default peer version as 1.3*/
    UINT16              categories = 0;
    char a2dp_role[PROPERTY_VALUE_MAX] = "false";

    APPL_TRACE_DEBUG("bta_avk_src_check_peer_features service_uuid:x%x", service_uuid);
    /* loop through all records we found */
    while (TRUE)
    {
        /* get next record; if none found, we're done */
        if ((p_rec = SDP_FindServiceInDb(p_cb->p_disc_db, service_uuid, p_rec)) == NULL)
        {
            break;
        }

        if (( SDP_FindAttributeInRec(p_rec, ATTR_ID_SERVICE_CLASS_ID_LIST)) != NULL)
        {
            /* find peer features */
            if (SDP_FindServiceInDb(p_cb->p_disc_db, UUID_SERVCLASS_AV_REMOTE_CONTROL, NULL))
            {
                peer_features |= BTA_AVK_FEAT_RCCT;
            }
            if (SDP_FindServiceInDb(p_cb->p_disc_db, UUID_SERVCLASS_AV_REM_CTRL_TARGET, NULL))
            {
                peer_features |= BTA_AVK_FEAT_RCTG;
            }
        }

        if (( SDP_FindAttributeInRec(p_rec, ATTR_ID_BT_PROFILE_DESC_LIST)) != NULL)
        {
            /* get profile version (if failure, version parameter is not updated) */
            SDP_FindProfileVersionInRec(p_rec, UUID_SERVCLASS_AV_REMOTE_CONTROL,
                                                                &peer_rc_version);
            APPL_TRACE_DEBUG("peer_rc_version 0x%x", peer_rc_version);

            if (peer_rc_version >= AVRC_REV_1_3)
                peer_features |= (BTA_AVK_FEAT_VENDOR | BTA_AVK_FEAT_METADATA);

            if (peer_rc_version >= AVRC_REV_1_4)
            {
                peer_features |= (BTA_AVK_FEAT_ADV_CTRL);
                /* get supported categories */
                if ((p_attr = SDP_FindAttributeInRec(p_rec,
                                ATTR_ID_SUPPORTED_FEATURES)) != NULL)
                {
                    categories = p_attr->attr_value.v.u16;
                    if (categories & AVRC_SUPF_CT_BROWSE)
                    {
                        peer_features |= (BTA_AVK_FEAT_BROWSE);
                        APPL_TRACE_DEBUG("peer supports browsing");
                    }
                    if (categories & AVRC_SUPF_CT_COVER_ART_GET_IMAGE & AVRC_SUPF_CT_COVER_ART_GET_THUMBNAIL)
                    {
                        peer_features |=  BTA_AVK_FEAT_CA;
                        APPL_TRACE_DEBUG("peer supports cover art");
                    }
                }
            }
            APPL_TRACE_DEBUG("peer version to update: 0x%x", peer_rc_version);
            bta_avk_check_store_avrc_tg_version(p_rec->remote_bd_addr, peer_rc_version);
        }
    }
    APPL_TRACE_DEBUG("peer_features:x%x", peer_features);
    return peer_features;
}

/*******************************************************************************
**
** Function         bta_avk_sink_check_peer_features
**
** Description      check supported features on the peer device from the SDP record
**                  and return the feature mask
**
** Returns          tBTA_AVK_FEAT peer device feature mask
**
*******************************************************************************/
tBTA_AVK_FEAT bta_avk_sink_check_peer_features (UINT16 service_uuid)
{
    tBTA_AVK_FEAT peer_features = 0;
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    tSDP_DISC_REC       *p_rec = NULL;
    tSDP_DISC_ATTR      *p_attr;
    UINT16              peer_rc_version=0;
    UINT16              categories = 0;
    BOOLEAN             val;
    char dy_version[PROPERTY_VALUE_MAX] = "false";

    APPL_TRACE_DEBUG("bta_avk_sink_check_peer_features service_uuid:x%x", service_uuid);
    /* loop through all records we found */
    while (TRUE)
    {
        /* get next record; if none found, we're done */
        if ((p_rec = SDP_FindServiceInDb(p_cb->p_disc_db, service_uuid, p_rec)) == NULL)
        {
            break;
        }
        APPL_TRACE_DEBUG(" found Service record for x%x", service_uuid);

        if (( SDP_FindAttributeInRec(p_rec, ATTR_ID_SERVICE_CLASS_ID_LIST)) != NULL)
        {
            /* find peer features */
            if (SDP_FindServiceInDb(p_cb->p_disc_db, UUID_SERVCLASS_AV_REMOTE_CONTROL, NULL))
            {
                peer_features |= BTA_AVK_FEAT_RCCT;
            }
            if (SDP_FindServiceInDb(p_cb->p_disc_db, UUID_SERVCLASS_AV_REM_CTRL_TARGET, NULL))
            {
                peer_features |= BTA_AVK_FEAT_RCTG;
            }
        }

        if (( SDP_FindAttributeInRec(p_rec, ATTR_ID_BT_PROFILE_DESC_LIST)) != NULL)
        {
            /* get profile version (if failure, version parameter is not updated) */
            val = SDP_FindProfileVersionInRec(p_rec, UUID_SERVCLASS_AV_REMOTE_CONTROL, &peer_rc_version);
            APPL_TRACE_DEBUG("peer_rc_version for TG 0x%x, profile_found %d", peer_rc_version, val);

            property_get("persist.avrcp.enable.dy_version", dy_version, "false");
            if (!strncmp("true", dy_version, 4))
                bta_avk_check_store_avrc_tg_version(p_rec->remote_bd_addr, peer_rc_version);
            if (peer_rc_version >= AVRC_REV_1_3)
                peer_features |= (BTA_AVK_FEAT_VENDOR | BTA_AVK_FEAT_METADATA);

            /*
             * Though Absolute Volume came after in 1.4 and above, but there are few devices
             * in market which supports absolute Volume and they are still 1.3
             * TO avoid IOT issuses with those devices, we check for 1.3 as minimum version
             */
            if (peer_rc_version >= AVRC_REV_1_3)
            {
                APPL_TRACE_DEBUG("peer_rc_version >= AVRC_REV_1_3 !~");
                /* get supported categories */
                if ((p_attr = SDP_FindAttributeInRec(p_rec,
                                ATTR_ID_SUPPORTED_FEATURES)) != NULL)
                {
                    categories = p_attr->attr_value.v.u16;
                    if (categories & AVRC_SUPF_CT_CAT2)
                        peer_features |= (BTA_AVK_FEAT_ADV_CTRL);
                    if(peer_rc_version >= AVRC_REV_1_4)
                    {
                        if (categories & AVRC_SUPF_CT_BROWSE)
                        {
                            peer_features |= (BTA_AVK_FEAT_BROWSE);
                            APPL_TRACE_DEBUG("peer supports browsing !~");
                        }
                    }
                }
            }
        }
    }
    APPL_TRACE_DEBUG("peer_features:x%x", peer_features);
    return peer_features;
}

/*******************************************************************************
**
** Function         bta_avk_rc_disc_done
**
** Description      Handle AVRCP service discovery results.  If matching
**                  service found, open AVRCP connection.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_disc_done(tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    tBTA_AVK_SCB  *p_scb = NULL;
    tBTA_AVK_LCB  *p_lcb;
    tBTA_AVK_RC_OPEN rc_open;
    tBTA_AVK_RC_FEAT rc_feat;
    UINT8               rc_handle;
    tBTA_AVK_FEAT        peer_features = 0;  /* peer features mask */
    UNUSED(p_data);

    APPL_TRACE_DEBUG("bta_avk_rc_disc_done disc:x%x", p_cb->disc);
    if (!p_cb->disc)
    {
        return;
    }

    if ((p_cb->disc & BTA_AVK_CHNL_MSK) == BTA_AVK_CHNL_MSK)
    {
        /* this is the rc handle/index to tBTA_AVK_RCB */
        rc_handle = p_cb->disc & (~BTA_AVK_CHNL_MSK);
    }
    else
    {
        /* Validate array index*/
        /* Fix for below klockwork issue
         * Array 'p_scb' size is 2
         * Possible attempt to access element -1 of array 'p_scb' */
        if ((((p_cb->disc & BTA_AVK_HNDL_MSK) - 1) < BTA_AVK_NUM_STRS) && (((p_cb->disc & BTA_AVK_HNDL_MSK) - 1) >= 0))
        {
            p_scb = p_cb->p_scb[(p_cb->disc & BTA_AVK_HNDL_MSK) - 1];
        }
        if (p_scb)
        {
            rc_handle = p_scb->rc_handle;
        }
        else
        {
            p_cb->disc = 0;
            return;
        }
    }

    APPL_TRACE_DEBUG("rc_handle %d", rc_handle);
    if (p_cb->sdp_a2d_snk_handle)
    {
        /* This is Sink + CT + TG(Abs Vol) */
        peer_features = bta_avk_sink_check_peer_features(UUID_SERVCLASS_AV_REM_CTRL_TARGET);
        if (BTA_AVK_FEAT_ADV_CTRL & bta_avk_sink_check_peer_features(UUID_SERVCLASS_AV_REMOTE_CONTROL))
            peer_features |= (BTA_AVK_FEAT_ADV_CTRL|BTA_AVK_FEAT_RCCT);
    }
    else if(p_cb->sdp_a2d_handle)
    {
        /* check peer version and whether support CT and TG role */
        peer_features = bta_avk_src_check_peer_features (UUID_SERVCLASS_AV_REMOTE_CONTROL);
        if ((p_cb->features & BTA_AVK_FEAT_ADV_CTRL) && ((peer_features&BTA_AVK_FEAT_ADV_CTRL) == 0))
        {
            /* if we support advance control and peer does not, check their support on TG role
             * some implementation uses 1.3 on CT ans 1.4 on TG */
            peer_features |= bta_avk_src_check_peer_features (UUID_SERVCLASS_AV_REM_CTRL_TARGET);
        }
    }

    p_cb->disc = 0;
    osi_free_and_reset((void **) &p_cb->p_disc_db);

    APPL_TRACE_DEBUG("peer_features 0x%x, features 0x%x", peer_features, p_cb->features);

    /* if we have no rc connection */
    if (rc_handle == BTA_AVK_RC_HANDLE_NONE)
    {
        if (p_scb)
        {
            /* if peer remote control service matches ours and USE_RC is TRUE */
            if ((((p_cb->features & BTA_AVK_FEAT_RCCT) && (peer_features & BTA_AVK_FEAT_RCTG)) ||
                 ((p_cb->features & BTA_AVK_FEAT_RCTG) && (peer_features & BTA_AVK_FEAT_RCCT))) )
            {
                p_lcb = bta_avk_find_lcb(p_scb->peer_addr, BTA_AVK_LCB_FIND);
                if(p_lcb)
                {
                    APPL_TRACE_DEBUG("bta_avk_rc_disc_done - bta_avk_rc_create !~");
                    rc_handle = bta_avk_rc_create(p_cb, AVCT_INT, (UINT8)(p_scb->hdi + 1), p_lcb->lidx);
                    if((rc_handle != BTA_AVK_RC_HANDLE_NONE) && (rc_handle < BTA_AVK_NUM_RCB))
                    {
                        p_cb->rcb[rc_handle].peer_features = peer_features;
                    }
                    else
                    {
                        /* cannot create valid rc_handle for current device */
                        APPL_TRACE_ERROR(" No link resources available");
                        p_scb->use_rc = FALSE;
                        bdcpy(rc_open.peer_addr, p_scb->peer_addr);
                        rc_open.peer_features = 0;
                        rc_open.status = BTA_AVK_FAIL_RESOURCES;
                        (*p_cb->p_cback)(BTA_AVK_RC_CLOSE_EVT, (tBTA_AVK *) &rc_open);
                    }
                }
#if (BT_USE_TRACES == TRUE || BT_TRACE_APPL == TRUE)
                else
                {
                    APPL_TRACE_ERROR("can not find LCB!!");
                }
#endif
            }
            else if(p_scb->use_rc)
            {
                /* can not find AVRC on peer device. report failure */
                p_scb->use_rc = FALSE;
                bdcpy(rc_open.peer_addr, p_scb->peer_addr);
                rc_open.peer_features = 0;
                rc_open.status = BTA_AVK_FAIL_SDP;
                (*p_cb->p_cback)(BTA_AVK_RC_OPEN_EVT, (tBTA_AVK *) &rc_open);
            }
        }
    }
    else if (rc_handle < BTA_AVK_NUM_RCB)
    {
        p_cb->rcb[rc_handle].peer_features = peer_features;
        rc_feat.rc_handle =  rc_handle;
        rc_feat.peer_features = peer_features;
        if (p_scb == NULL)
        {
            /*
             * In case scb is not created by the time we are done with SDP
             * we still need to send RC feature event. So we need to get BD
             * from Message
             */
            bdcpy(rc_feat.peer_addr, p_cb->lcb[p_cb->rcb[rc_handle].lidx].addr);
        }
        else
            bdcpy(rc_feat.peer_addr, p_scb->peer_addr);
        (*p_cb->p_cback)(BTA_AVK_RC_FEAT_EVT, (tBTA_AVK *) &rc_feat);
    }
}

/*******************************************************************************
**
** Function         bta_avk_rc_closed
**
** Description      Set AVRCP state to closed.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_closed(tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    tBTA_AVK_RC_CLOSE rc_close;
    tBTA_AVK_RC_CONN_CHG *p_msg = (tBTA_AVK_RC_CONN_CHG *)p_data;
    tBTA_AVK_RCB    *p_rcb;
    tBTA_AVK_SCB    *p_scb;
    int i;
    BOOLEAN conn = FALSE;
    tBTA_AVK_LCB *p_lcb;

    rc_close.rc_handle = BTA_AVK_RC_HANDLE_NONE;
    p_scb = NULL;
    APPL_TRACE_DEBUG("bta_avk_rc_closed rc_handle:%d", p_msg->handle);
    for(i=0; i<BTA_AVK_NUM_RCB; i++)
    {
        p_rcb = &p_cb->rcb[i];
        APPL_TRACE_DEBUG("bta_avk_rc_closed rcb[%d] rc_handle:%d, status=0x%x", i, p_rcb->handle, p_rcb->status);
        if(p_rcb->handle == p_msg->handle)
        {
            rc_close.rc_handle = i;
            p_rcb->status &= ~BTA_AVK_RC_CONN_MASK;

            if(p_rcb->br_conn_timer != NULL)
                alarm_cancel(p_rcb->br_conn_timer);


            p_rcb->peer_features = 0;
            APPL_TRACE_DEBUG("       shdl:%d, lidx:%d", p_rcb->shdl, p_rcb->lidx);
            if(p_rcb->shdl)
            {
                if ((p_rcb->shdl - 1) < BTA_AVK_NUM_STRS)
                {
                    p_scb = bta_avk_cb.p_scb[p_rcb->shdl - 1];
                }
                if(p_scb)
                {
                    bdcpy(rc_close.peer_addr, p_scb->peer_addr);
                    if(p_scb->rc_handle == p_rcb->handle)
                        p_scb->rc_handle = BTA_AVK_RC_HANDLE_NONE;
                    APPL_TRACE_DEBUG("shdl:%d, srch:%d", p_rcb->shdl, p_scb->rc_handle);
                }
                p_rcb->shdl = 0;
            }
            else if(p_rcb->lidx == (BTA_AVK_NUM_LINKS + 1) )
            {
                /* if the RCB uses the extra LCB, use the addr for event and clean it */
                p_lcb = &p_cb->lcb[BTA_AVK_NUM_LINKS];
                bdcpy(rc_close.peer_addr, p_msg->peer_addr);
                APPL_TRACE_DEBUG("rc_only closed bd_addr:%02x-%02x-%02x-%02x-%02x-%02x",
                              p_msg->peer_addr[0], p_msg->peer_addr[1],
                              p_msg->peer_addr[2], p_msg->peer_addr[3],
                              p_msg->peer_addr[4], p_msg->peer_addr[5]);
                p_lcb->conn_msk = 0;
                p_lcb->lidx = 0;
            }
            p_rcb->lidx = 0;

            if((p_rcb->status & BTA_AVK_RC_ROLE_MASK) == BTA_AVK_RC_ROLE_INT)
            {
                /* AVCT CCB is deallocated */
                p_rcb->handle = BTA_AVK_RC_HANDLE_NONE;
                p_rcb->status = 0;
            }
            else
            {
                /* AVCT CCB is still there. dealloc */
                bta_avk_del_rc(p_rcb);

                /* if the AVRCP is no longer listening, create the listening channel */
                if (bta_avk_cb.rc_acp_handle == BTA_AVK_RC_HANDLE_NONE && bta_avk_cb.features & BTA_AVK_FEAT_RCTG)
                    bta_avk_rc_create(&bta_avk_cb, AVCT_ACP, 0, BTA_AVK_NUM_LINKS + 1);
            }
        }
        else if((p_rcb->handle != BTA_AVK_RC_HANDLE_NONE) && (p_rcb->status & BTA_AVK_RC_CONN_MASK))
        {
            /* at least one channel is still connected */
            conn = TRUE;
        }
    }

    if(!conn)
    {
        /* no AVRC channels are connected, go back to INIT state */
        bta_avk_sm_execute(p_cb, BTA_AVK_AVRC_NONE_EVT, NULL);
    }

    if (rc_close.rc_handle == BTA_AVK_RC_HANDLE_NONE)
    {
        rc_close.rc_handle = p_msg->handle;
        bdcpy(rc_close.peer_addr, p_msg->peer_addr);
    }
    (*p_cb->p_cback)(BTA_AVK_RC_CLOSE_EVT, (tBTA_AVK *) &rc_close);
}

/*******************************************************************************
**
** Function         bta_avk_rc_disc
**
** Description      start AVRC SDP discovery.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_rc_disc(UINT8 disc)
{
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    tAVRC_SDP_DB_PARAMS db_params;
      UINT16              attr_list[] = {ATTR_ID_SERVICE_CLASS_ID_LIST,
                                       ATTR_ID_BT_PROFILE_DESC_LIST,
                                       ATTR_ID_SUPPORTED_FEATURES};
    UINT8       hdi;
    tBTA_AVK_SCB *p_scb;
    UINT8       *p_addr = NULL;
    UINT8       rc_handle;

    APPL_TRACE_DEBUG("bta_avk_rc_disc 0x%x, %d", disc, bta_avk_cb.disc);
    if ((bta_avk_cb.disc != 0) || (disc == 0))
        return;

    if ((disc & BTA_AVK_CHNL_MSK) == BTA_AVK_CHNL_MSK)
    {
        /* this is the rc handle/index to tBTA_AVK_RCB */
        rc_handle = disc & (~BTA_AVK_CHNL_MSK);
        if (p_cb->rcb[rc_handle].lidx)
        {
            p_addr = p_cb->lcb[p_cb->rcb[rc_handle].lidx-1].addr;
        }
    }
    else
    {
        hdi = (disc & BTA_AVK_HNDL_MSK) - 1;
        p_scb = p_cb->p_scb[hdi];

        if (p_scb)
        {
            APPL_TRACE_DEBUG("rc_handle %d", p_scb->rc_handle);
            p_addr = p_scb->peer_addr;
        }
    }

    if (p_addr)
    {
        /* allocate discovery database */
        if (p_cb->p_disc_db == NULL)
        {
            p_cb->p_disc_db = (tSDP_DISCOVERY_DB *) osi_malloc(BTA_AVK_DISC_BUF_SIZE);
        }

        if (p_cb->p_disc_db)
        {
            /* set up parameters */
            db_params.db_len = BTA_AVK_DISC_BUF_SIZE;
            db_params.num_attr = 3;
            db_params.p_db = p_cb->p_disc_db;
            db_params.p_attrs = attr_list;

            /* searching for UUID_SERVCLASS_AV_REMOTE_CONTROL gets both TG and CT */
            if (AVRC_FindService(UUID_SERVCLASS_AV_REMOTE_CONTROL, p_addr, &db_params,
                            bta_avk_avrc_sdp_cback) == AVRC_SUCCESS)
            {
                p_cb->disc = disc;
                APPL_TRACE_DEBUG("disc %d", p_cb->disc);
            }
        }
    }
}

/*******************************************************************************
**
** Function         bta_avk_dereg_comp
**
** Description      deregister complete. free the stream control block.
**
** Returns          void
**
*******************************************************************************/
void bta_avk_dereg_comp(tBTA_AVK_DATA *p_data)
{
    tBTA_AVK_CB   *p_cb = &bta_avk_cb;
    tBTA_AVK_SCB  *p_scb;
    tBTA_UTL_COD    cod;
    UINT8   mask;
    BT_HDR  *p_buf;

    /* find the stream control block */
    p_scb = bta_avk_hndl_to_scb(p_data->hdr.layer_specific);

    if(p_scb)
    {
        APPL_TRACE_DEBUG("deregistered %d(h%d)", p_scb->chnl, p_scb->hndl);
        mask = BTA_AVK_HNDL_TO_MSK(p_scb->hdi);
        if(p_scb->chnl == BTA_AVK_CHNL_AUDIO)
        {
            p_cb->reg_audio  &= ~mask;
            if ((p_cb->conn_audio & mask) && bta_avk_cb.audio_open_cnt)
            {
                /* this channel is still marked as open. decrease the count */
                bta_avk_cb.audio_open_cnt--;
            }
            p_cb->conn_audio &= ~mask;

            if (p_scb->q_tag == BTA_AVK_Q_TAG_STREAM && p_scb->a2d_list) {
                /* make sure no buffers are in a2d_list */
                while (!list_is_empty(p_scb->a2d_list)) {
                    p_buf = (BT_HDR*)list_front(p_scb->a2d_list);
                    list_remove(p_scb->a2d_list, p_buf);
                    osi_free(p_buf);
                }
            }

            /* remove the A2DP SDP record, if no more audio stream is left */
            if(!p_cb->reg_audio)
            {
#if( defined BTA_AR_INCLUDED ) && (BTA_AR_INCLUDED == TRUE)
                bta_ar_dereg_avrc (UUID_SERVCLASS_AV_REMOTE_CONTROL, BTA_ID_AVK);
#endif
                if (p_cb->sdp_a2d_handle)
                {
                    bta_avk_del_sdp_rec(&p_cb->sdp_a2d_handle);
                    p_cb->sdp_a2d_handle = 0;
                    bta_sys_remove_uuid(UUID_SERVCLASS_AUDIO_SOURCE);
                }

#if (BTA_AV_SINK_INCLUDED == TRUE)
                if (p_cb->sdp_a2d_snk_handle)
                {
                    bta_avk_del_sdp_rec(&p_cb->sdp_a2d_snk_handle);
                    p_cb->sdp_a2d_snk_handle = 0;
                    bta_sys_remove_uuid(UUID_SERVCLASS_AUDIO_SINK);
                }
#endif
            }
        }
        else
        {
            p_cb->reg_video  &= ~mask;
            /* make sure that this channel is not connected */
            p_cb->conn_video &= ~mask;
            /* remove the VDP SDP record, (only one video stream at most) */
            bta_avk_del_sdp_rec(&p_cb->sdp_vdp_handle);
            bta_sys_remove_uuid(UUID_SERVCLASS_VIDEO_SOURCE);
        }

        /* make sure that the timer is not active */
        alarm_cancel(p_scb->avrc_ct_timer);
        osi_free_and_reset((void **)&p_cb->p_scb[p_scb->hdi]);
    }

    APPL_TRACE_DEBUG("audio 0x%x, video: 0x%x, disable:%d",
        p_cb->reg_audio, p_cb->reg_video, p_cb->disabling);
    /* if no stream control block is active */
    if((p_cb->reg_audio + p_cb->reg_video) == 0)
    {
#if( defined BTA_AR_INCLUDED ) && (BTA_AR_INCLUDED == TRUE)
        /* deregister from AVDT */
        bta_ar_dereg_avdt(BTA_ID_AVK);

        /* deregister from AVCT */
        bta_ar_dereg_avrc (UUID_SERVCLASS_AV_REM_CTRL_TARGET, BTA_ID_AVK);
        bta_ar_dereg_avct(BTA_ID_AVK);
#endif

        if(p_cb->disabling)
        {
            p_cb->disabling     = FALSE;
            bta_avk_cb.features  = 0;
        }

        /* Clear the Capturing service class bit */
        cod.service = BTM_COD_SERVICE_CAPTURING;
        utl_set_device_class(&cod, BTA_UTL_CLR_COD_SERVICE_CLASS);
    }
}
#endif /* BTA_AV_INCLUDED */


#include "app_config.h"
#include "app_action.h"

#include "system/includes.h"
#include "spp_user.h"
#include "string.h"
#include "circular_buf.h"
#include "bt_common.h"
#include "online_db_deal.h"
#include "spp_online_db.h"

#if TCFG_ANC_TOOL_DEBUG_ONLINE
#include "app_anctool.h"
#endif


#if 1
extern void printf_buf(u8 *buf, u32 len);
#define log_info          printf
#define log_info_hexdump  printf_buf
#else
#define log_info(...)
#define log_info_hexdump(...)
#endif

#if APP_ONLINE_DEBUG

static struct db_online_api_t *db_api;
static struct spp_operation_t *spp_api = NULL;
static u8 spp_state;

int online_spp_send_data(u8 *data, u16 len)
{
    if (spp_api) {
        /* log_info("spp_api_tx(%d) \n", len); */
        /* log_info_hexdump(data, len); */
        return spp_api->send_data(NULL, data, len);
    }
    return SPP_USER_ERR_SEND_FAIL;
}

int online_spp_send_data_check(u16 len)
{
    if (spp_api) {
        if (spp_api->busy_state()) {
            return 0;
        }
    }
    return 1;
}

static void online_spp_state_cbk(u8 state)
{
    spp_state = state;
    switch (state) {
    case SPP_USER_ST_CONNECT:
        log_info("SPP_USER_ST_CONNECT ~~~\n");
        db_api->init(DB_COM_TYPE_SPP);
        db_api->register_send_data(online_spp_send_data);
#if TCFG_ANC_TOOL_DEBUG_ONLINE
        app_anctool_spp_connect();
#endif
        break;

    case SPP_USER_ST_DISCONN:
        log_info("SPP_USER_ST_DISCONN ~~~\n");
        db_api->exit();
#if TCFG_ANC_TOOL_DEBUG_ONLINE
        app_anctool_spp_disconnect();
#endif
        break;

    default:
        break;
    }

}

static void online_spp_send_wakeup(void)
{
    /* putchar('W'); */
    db_api->send_wake_up();
}

static u8 *send_version = "V001";
static u8 receive_data[] = {0xAB, 0x02, 0x03, 0xFF};
static u8 verify_num = 0;
//AB 02 03 FF
static void online_spp_recieve_cbk(void *priv, u8 *buf, u16 len)
{
    log_info("spp_api_rx(%d) \n", len);
    /* log_info_hexdump(buf, len); */
#if TCFG_ANC_TOOL_DEBUG_ONLINE
    if (app_anctool_spp_rx_data(buf, len)) {
        return;
    }
#endif
    db_api->packet_handle(buf, len);

    printf("***********send succeed\n***********");

    u8 data_len = sizeof(receive_data) / sizeof(receive_data[0]);
    if(data_len == len){
        for(int i = 0; i < len; i++)
        {
            if(*(send_version + i) == *(buf + i)){
                verify_num++;
            }
        }
        if(len == verify_num){
            online_spp_send_data(send_version,strlen(send_version));
            verify_num = 0;
        }
        else{
            verify_num = 0;
        }
    }
    //loop send data for test
    /* if (online_spp_send_data_check(len)) { */
    /* online_spp_send_data(buf, len); */
    /* } */
}

void online_spp_init(void)
{
    spp_state = 0;
    spp_get_operation_table(&spp_api);
    spp_api->regist_recieve_cbk(0, online_spp_recieve_cbk);
    spp_api->regist_state_cbk(0, online_spp_state_cbk);
    spp_api->regist_wakeup_send(NULL, online_spp_send_wakeup);
    db_api = app_online_get_api_table();
}

#endif


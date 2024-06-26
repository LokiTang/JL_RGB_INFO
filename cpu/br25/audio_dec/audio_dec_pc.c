/*****************************************************************
>file name : audio_dec_pc.c
>author : lichao
>create time : Wed 22 May 2019 10:36:22 AM CST
*****************************************************************/
#include "uac_stream.h"
#include "app_config.h"
#include "audio_decoder.h"
#include "media/includes.h"
#include "audio_config.h"
#include "system/includes.h"
#include "audio_enc.h"
#include "media/audio_eq_drc_apply.h"
#include "app_config.h"
#include "audio_config.h"
#include "audio_dec.h"
#include "app_main.h"
#include "clock_cfg.h"
#include "media/pcm_decoder.h"
#include "bt_tws.h"
#include "media/audio_stream.h"
#include "media/convert_data.h"
#include "audio_effect/audio_dynamic_eq_demo.h"
#include "media/effects_adj.h"
#include "audio_effect/audio_eff_default_parm.h"

#if TCFG_UI_ENABLE
#include "ui/ui_api.h"
#endif


#if TCFG_APP_PC_EN

//////////////////////////////////////////////////////////////////////////////
#define VOL_INDEPENDENT_EN       0  //左右声道的音量是否要单独设置,置1则单独设置

struct uac_dec_hdl {
    struct audio_stream *stream;	// 音频流
    struct pcm_decoder pcm_dec;		// pcm解码句柄
    struct audio_res_wait wait;		// 资源等待句柄
    struct audio_mixer_ch mix_ch;	// 叠加句柄

#if AUDIO_SURROUND_CONFIG
    surround_hdl *surround;         //环绕音效句柄
#endif

#if AUDIO_VBASS_CONFIG
    struct aud_gain_process *vbass_prev_gain;
    NOISEGATE_API_STRUCT *ns_gate;
    vbass_hdl *vbass;               //虚拟低音句柄
#endif

#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
    struct audio_eq  *high_bass;
    struct audio_drc *hb_drc;//高低音后的drc
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    struct convert_data *hb_convert;
#endif
#endif

    struct audio_eq *eq;    //eq drc句柄
    struct audio_drc *drc;    // drc句柄
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    struct convert_data *convert;
#endif
#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
    struct audio_eq *ext_eq;    //eq drc句柄 扩展eq
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
    struct audio_eq *eq2;    //eq drc句柄
    struct dynamic_eq_hdl *dy_eq;
    struct convert_data *convert2;
#endif
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    struct aud_gain_process *gain;
#endif

    u32 id;				// 唯一标识符，随机值
    u32 start : 1;		// 正在解码
    u32 source : 8;		// 音频源

    u32 cnt: 8;
    u32 state: 1;
    u32 in_info;
    int check_data_timer;
};

//////////////////////////////////////////////////////////////////////////////

static struct uac_dec_hdl *uac_dec = NULL;	// pc解码句柄

static u16 sys_event_id = 0;

//////////////////////////////////////////////////////////////////////////////

extern u16 uac_get_cur_vol(const u8 id, u16 *l_vol, u16 *r_vol);
extern u8 uac_get_mute(void);
extern int usb_audio_mic_open(void *_info);
extern int usb_audio_mic_close(void *arg);
extern void usb_audio_mic_set_gain(int gain);

/* void *pc_eq_drc_open(u16 sample_rate, u8 ch_num); */
/* void pc_eq_drc_close(struct audio_eq_drc *eq_drc); */

//////////////////////////////////////////////////////////////////////////////

/*----------------------------------------------------------------------------*/
/**@brief    pc音量值转换
   @param    vol: pc音量
   @return   转换后的系统音量
   @note
*/
/*----------------------------------------------------------------------------*/
int uac_vol_switch(int vol)
{
    u16 valsum = vol * (SYS_MAX_VOL + 1) / 100;

    if (valsum > SYS_MAX_VOL) {
        valsum = SYS_MAX_VOL;
    }
    return valsum;
}

/*----------------------------------------------------------------------------*/
/**@brief    uac解码释放
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void uac_dec_relaese()
{
    if (!uac_dec) {
        return;
    }
    audio_decoder_task_del_wait(&decode_task, &uac_dec->wait);

    clock_remove(DEC_PCM_CLK);

    local_irq_disable();
    free(uac_dec);
    uac_dec = NULL;
    local_irq_enable();
}


/*----------------------------------------------------------------------------*/
/**@brief    uac解码数据流激活
   @param    *p: 私有句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void uac_dec_out_stream_resume(void *p)
{
    struct uac_dec_hdl *dec = p;
    audio_decoder_resume(&dec->pcm_dec.decoder);
}

/*----------------------------------------------------------------------------*/
/**@brief    uac解码激活
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void uac_dec_resume(void)
{
    if (uac_dec) {
        audio_decoder_resume(&uac_dec->pcm_dec.decoder);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    uac获取buf总长度
   @param    *priv: 私有参数
   @return   总长度
   @note
*/
/*----------------------------------------------------------------------------*/
static int uac_dec_get_total(void *priv)
{
    return uac_speaker_stream_length();
}

/*----------------------------------------------------------------------------*/
/**@brief    uac获取数据长度
   @param    *priv: 私有参数
   @return   数据长度
   @note
*/
/*----------------------------------------------------------------------------*/
static int uac_dec_get_size(void *priv)
{
    return uac_speaker_stream_size();
}

/*----------------------------------------------------------------------------*/
/**@brief    pc解码关闭
   @param
   @return   0：成功
   @note
*/
/*----------------------------------------------------------------------------*/
static int uac_audio_close(void)
{
    if (!uac_dec || !uac_dec->start) {
        return 0;
    }

    uac_dec->start = 0;
    if (uac_dec->check_data_timer) {
        sys_hi_timer_del(uac_dec->check_data_timer);
    }
    pcm_decoder_close(&uac_dec->pcm_dec);

#if AUDIO_SURROUND_CONFIG
    surround_close_demo(uac_dec->surround);
#endif


#if AUDIO_VBASS_CONFIG
    audio_gain_close_demo(uac_dec->vbass_prev_gain);
    audio_noisegate_close_demo(uac_dec->ns_gate);
    audio_vbass_close_demo(uac_dec->vbass);
#endif

#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
    high_bass_eq_close(uac_dec->high_bass);
    high_bass_drc_close(uac_dec->hb_drc);
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(uac_dec->hb_convert);
#endif
#endif

#if TCFG_EQ_ENABLE && TCFG_PC_MODE_EQ_ENABLE
    music_eq_close(uac_dec->eq);
#if TCFG_DRC_ENABLE && TCFG_PC_MODE_DRC_ENABLE
    music_drc_close(uac_dec->drc);
#endif
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(uac_dec->convert);
#endif

#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
    music_ext_eq_close(uac_dec->ext_eq);
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
    music_eq2_close(uac_dec->eq2);
    audio_dynamic_eq_ctrl_close(uac_dec->dy_eq);
    convet_data_close(uac_dec->convert2);
#endif
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    audio_gain_close_demo(uac_dec->gain);
#endif
#endif

    audio_mixer_ch_close(&uac_dec->mix_ch);
    app_audio_state_exit(APP_AUDIO_STATE_MUSIC);

#if TCFG_PCM_ENC2TWS_ENABLE
    if (uac_dec->pcm_dec.dec_no_out_sound) {
        uac_dec->pcm_dec.dec_no_out_sound = 0;
        localtws_enc_api_close();
    }
#endif


#if SYS_DIGVOL_GROUP_EN
    sys_digvol_group_ch_close("music_pc");
#endif // SYS_DIGVOL_GROUP_EN


    // 先关闭各个节点，最后才close数据流
    if (uac_dec->stream) {
        audio_stream_close(uac_dec->stream);
        uac_dec->stream = NULL;
    }



    clock_set_cur();
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    uac解码事件处理
   @param    *decoder: 解码器句柄
   @param    argc: 参数个数
   @param    *argv: 参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void uac_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        puts("USB AUDIO_DEC_EVENT_END\n");
        if (!uac_dec) {
            log_i("uac_dec handle err ");
            break;
        }

        if (uac_dec->id != argv[1]) {
            log_w("uac_dec id err : 0x%x, 0x%x \n", uac_dec->id, argv[1]);
            break;
        }

        /*uac_audio_close();*/
        break;
    }
}
/*----------------------------------------------------------------------------*/
/**@brief    检测uac是否收到数据，没数据时暂停mix_ch
   @param    *priv: 私有参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void audio_pc_check_timer(void *priv)
{
#if 1
    u8 alive = uac_speaker_get_alive();
    if (alive) {
        uac_dec->cnt++;
        if (uac_dec->cnt > 5) {
            if (!uac_dec->state) {
#if TCFG_DEC2TWS_ENABLE
                if (uac_dec->pcm_dec.dec_no_out_sound) {
                    localtws_decoder_pause(1);
                }
#endif
                audio_mixer_ch_pause(&uac_dec->mix_ch, 1);
                audio_decoder_resume_all(&decode_task);
                uac_dec->state = 1;
            }
        }
    } else {
        if (uac_dec->cnt) {
            uac_dec->cnt--;
        }
        if (uac_dec->state) {
            uac_dec->state = 0;
            uac_dec->cnt = 0;
#if TCFG_DEC2TWS_ENABLE
            if (uac_dec->pcm_dec.dec_no_out_sound) {
                localtws_decoder_pause(0);
            }
#endif
            audio_mixer_ch_pause(&uac_dec->mix_ch, 0);
            audio_decoder_resume_all(&decode_task);
        }
    }
    uac_speaker_set_alive(1);
#else
    static u8 cnt = 0;
    if (uac_speaker_stream_size(NULL) == 0) {
        if (cnt < 6) {
            cnt++;
        }
        if (cnt == 5) {
#if TCFG_DEC2TWS_ENABLE
            if (uac_dec->pcm_dec.dec_no_out_sound) {
                localtws_decoder_pause(1);
            }
#endif
            audio_mixer_ch_pause(&uac_dec->mix_ch, 1);
            audio_decoder_resume_all(&decode_task);
        }
    } else {
        if (cnt > 4) {
#if TCFG_DEC2TWS_ENABLE
            if (uac_dec->pcm_dec.dec_no_out_sound) {
                localtws_decoder_pause(0);
            }
#endif
            audio_mixer_ch_pause(&uac_dec->mix_ch, 0);
            audio_decoder_resume_all(&decode_task);
        }
        cnt = 0;
    }
#endif
}

/*----------------------------------------------------------------------------*/
/**@brief    计算pc输入采样率
   @param
   @return   采样率
   @note
*/
/*----------------------------------------------------------------------------*/
static int audio_pc_input_sample_rate(void *priv)
{
    struct uac_dec_hdl *dec = (struct uac_dec_hdl *)priv;
    int sample_rate = uac_speaker_stream_sample_rate();
    int buf_size = uac_speaker_stream_size();
#if TCFG_PCM_ENC2TWS_ENABLE
    if (dec->pcm_dec.dec_no_out_sound) {
        /*TWS的上限在uac输入buffer，下限在tws push buffer*/
        if (buf_size >= uac_speaker_stream_length() / 2) {
            sample_rate += (sample_rate * 5 / 10000);
        } else if (tws_api_local_media_trans_check_ready_total() < 1024) {
            sample_rate -= (sample_rate * 5 / 10000);
        }
        return sample_rate;
    }
#endif
    if (buf_size >= (uac_speaker_stream_length() * 3 / 4)) {
        sample_rate += (sample_rate * 5 / 10000);
    }
    if (buf_size <= (uac_speaker_stream_length() / 4)) {
        sample_rate -= (sample_rate * 5 / 10000);
    }
    return sample_rate;
}

/*----------------------------------------------------------------------------*/
/**@brief    uac解码开始
   @param
   @return   0：成功
   @return   非0：失败
   @note
*/
/*----------------------------------------------------------------------------*/
static int uac_audio_start(void)
{
    int err;
    struct uac_dec_hdl *dec = uac_dec;
    struct audio_mixer *p_mixer = &mixer;

    if (!uac_dec) {
        return -EINVAL;
    }

    // 打开pcm解码器
    err = pcm_decoder_open(&dec->pcm_dec, &decode_task);
    if (err) {
        goto __err1;
    }

    pcm_decoder_set_event_handler(&dec->pcm_dec, uac_dec_event_handler, dec->id);
    pcm_decoder_set_read_data(&dec->pcm_dec, uac_speaker_read, NULL);


#if TCFG_PCM_ENC2TWS_ENABLE
    {
        // localtws使用sbc等编码转发
        struct audio_fmt enc_f;
        memcpy(&enc_f, &dec->pcm_dec.decoder.fmt, sizeof(struct audio_fmt));
        enc_f.coding_type = AUDIO_CODING_SBC;
        if (dec->pcm_dec.ch_num == 2) { // 如果是双声道数据，localtws在解码时才变成对应声道
            enc_f.channel = 2;
        }
        int ret = localtws_enc_api_open(&enc_f, LOCALTWS_ENC_FLAG_STREAM);
        if (ret == true) {
            dec->pcm_dec.dec_no_out_sound = 1;
            // 重定向mixer
            p_mixer = &g_localtws.mixer;
            // 关闭资源等待。最终会在localtws解码处等待
            audio_decoder_task_del_wait(&decode_task, &dec->wait);
            if (dec->pcm_dec.output_ch_num != enc_f.channel) {
                dec->pcm_dec.output_ch_num = dec->pcm_dec.decoder.fmt.channel = enc_f.channel;
                if (enc_f.channel == 2) {
                    dec->pcm_dec.output_ch_type = AUDIO_CH_LR;
                } else {
                    dec->pcm_dec.output_ch_type = AUDIO_CH_DIFF;
                }
            }
        }
    }
#endif

    if (!dec->pcm_dec.dec_no_out_sound) {
        audio_mode_main_dec_open(AUDIO_MODE_MAIN_STATE_DEC_PC);
    }

    // 设置叠加功能
    audio_mixer_ch_open_head(&dec->mix_ch, p_mixer);
    audio_mixer_ch_follow_resample_enable(&dec->mix_ch, dec, audio_pc_input_sample_rate);

#if AUDIO_SURROUND_CONFIG
    //环绕音效
    dec->surround = surround_open_demo(AEID_MUSIC_SURROUND, dec->pcm_dec.output_ch_type);
#endif


#if AUDIO_VBASS_CONFIG
    dec->vbass_prev_gain = audio_gain_open_demo(AEID_MUSIC_VBASS_PREV_GAIN, dec->pcm_dec.output_ch_num);
    dec->ns_gate = audio_noisegate_open_demo(AEID_MUSIC_NS_GATE, dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);
    //虚拟低音
    dec->vbass = audio_vbass_open_demo(AEID_MUSIC_VBASS, dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);
#endif

#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
    dec->high_bass = high_bass_eq_open(dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);
    dec->hb_drc = high_bass_drc_open(dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    if (dec->hb_drc && dec->hb_drc->run32bit) {
        dec->hb_convert = convet_data_open(0, 512);
    }
#endif
#endif

#if TCFG_EQ_ENABLE && TCFG_PC_MODE_EQ_ENABLE
    dec->eq = music_eq_open(dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);// eq
#if TCFG_DRC_ENABLE && TCFG_PC_MODE_DRC_ENABLE
    dec->drc = music_drc_open(dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);//drc
#endif/*TCFG_PC_MODE_DRC_ENABLE*/
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    if (dec->eq && dec->eq->out_32bit) {
        dec->convert = convet_data_open(0, 512);
    }
#endif
#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
    dec->ext_eq = music_ext_eq_open(dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
    dec->eq2 = music_eq2_open(dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);// eq
    dec->dy_eq = audio_dynamic_eq_ctrl_open(AEID_MUSIC_DYNAMIC_EQ, dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);//动态eq
    dec->convert2 = convet_data_open(0, 512);
#endif/*TCFG_DYNAMIC_EQ_ENABLE*/
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    dec->gain = audio_gain_open_demo(AEID_MUSIC_GAIN, dec->pcm_dec.output_ch_num);
#endif
#endif/*TCFG_PC_MODE_EQ_ENABLE*/

    struct audio_stream_entry *entries[16] = {NULL};
    u8 entry_cnt = 0;
    u8 rl_rr_entry_start = 0;
    entries[entry_cnt++] = &dec->pcm_dec.decoder.entry;

#if SYS_DIGVOL_GROUP_EN
    void *dvol_entry = sys_digvol_group_ch_open("music_pc", -1, NULL);
    entries[entry_cnt++] = dvol_entry;
#endif // SYS_DIGVOL_GROUP_EN




#if AUDIO_VBASS_CONFIG
    if (dec->vbass_prev_gain) {
        entries[entry_cnt++] = &dec->vbass_prev_gain->entry;
    }
    if (dec->ns_gate) {
        entries[entry_cnt++] = &dec->ns_gate->entry;
    }

    if (dec->vbass) {
        entries[entry_cnt++] = &dec->vbass->entry;
    }
#endif

#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
    if (dec->high_bass) { //高低音
        entries[entry_cnt++] = &dec->high_bass->entry;
    }
    if (dec->hb_drc) { //高低音后drc
        entries[entry_cnt++] = &dec->hb_drc->entry;
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
        if (dec->hb_convert) {
            entries[entry_cnt++] = &dec->hb_convert->entry;
        }
#endif
    }
#endif

    rl_rr_entry_start = entry_cnt - 1;//记录eq的上一个节点
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    if (dec->gain) {
        entries[entry_cnt++] = &dec->gain->entry;
    }
#endif
#if AUDIO_SURROUND_CONFIG
    if (dec->surround) {
        entries[entry_cnt++] = &dec->surround->entry;
    }
#endif
#if TCFG_EQ_ENABLE && TCFG_PC_MODE_EQ_ENABLE
    if (dec->eq) {
        entries[entry_cnt++] = &dec->eq->entry;
        if (dec->drc) {
            entries[entry_cnt++] = &dec->drc->entry;
        }
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
        if (dec->convert) {
            entries[entry_cnt++] = &dec->convert->entry;
        }
#endif
#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
        if (dec->ext_eq) {
            entries[entry_cnt++] = &dec->ext_eq->entry;
        }
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
        if (dec->eq2) {
            entries[entry_cnt++] = &dec->eq2->entry;
        }
        if (dec->dy_eq && dec->dy_eq->dy_eq) {
            entries[entry_cnt++] = &dec->dy_eq->dy_eq->entry;
        }
        if (dec->convert2) {
            entries[entry_cnt++] = &dec->convert2->entry;
        }
#endif


    }
#endif



    entries[entry_cnt++] = &dec->mix_ch.entry;
    // 创建数据流，把所有节点连接起来
    dec->stream = audio_stream_open(dec, uac_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);


    // 设置音量
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);
    u16 l_volume, r_volume;
    uac_get_cur_vol(0, &l_volume, &r_volume);
    u8 vol = uac_vol_switch(l_volume);
    u8 vol_r = uac_vol_switch(r_volume);

#if !VOL_INDEPENDENT_EN
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, vol, 0);
#else
    app_audio_set_volume_each_channel(APP_AUDIO_STATE_MUSIC, vol, vol_r, 1);
#endif

#if (TCFG_DEC2TWS_ENABLE)
    bt_tws_sync_volume();
#endif

    // 开始解码
    dec->start = 1;
    err = audio_decoder_start(&dec->pcm_dec.decoder);
    if (err) {
        goto __err3;
    }

    dec->state = 0;
    dec->cnt = 0;
    dec->check_data_timer = sys_hi_timer_add(NULL, audio_pc_check_timer, 10);
    clock_set_cur();
    return 0;

__err3:
    dec->start = 0;


#if AUDIO_SURROUND_CONFIG
    surround_close_demo(dec->surround);
#endif



#if AUDIO_VBASS_CONFIG
    audio_gain_close_demo(dec->vbass_prev_gain);
    audio_noisegate_close_demo(dec->ns_gate);
    audio_vbass_close_demo(dec->vbass);
#endif
#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
    high_bass_eq_close(dec->high_bass);
    high_bass_drc_close(dec->hb_drc);
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(dec->hb_convert);
#endif
#endif
#if TCFG_EQ_ENABLE && TCFG_PC_MODE_EQ_ENABLE
    music_eq_close(dec->eq);
#if TCFG_DRC_ENABLE && TCFG_PC_MODE_DRC_ENABLE
    music_drc_close(dec->drc);
#endif
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(dec->convert);
#endif
#endif

#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
    music_ext_eq_close(dec->ext_eq);
#endif
#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
    music_eq2_close(dec->eq2);
    audio_dynamic_eq_ctrl_close(dec->dy_eq);
    convet_data_close(dec->convert2);
#endif
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
    audio_gain_close_demo(dec->gain);
#endif

    audio_mixer_ch_close(&dec->mix_ch);
#if TCFG_PCM_ENC2TWS_ENABLE
    if (dec->pcm_dec.dec_no_out_sound) {
        dec->pcm_dec.dec_no_out_sound = 0;
        localtws_enc_api_close();
    }
#endif

#if SYS_DIGVOL_GROUP_EN
    sys_digvol_group_ch_close("music_pc");
#endif // SYS_DIGVOL_GROUP_EN


    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }



    pcm_decoder_close(&dec->pcm_dec);
__err1:
    uac_dec_relaese();
    return err;
}



/*----------------------------------------------------------------------------*/
/**@brief    uac解码资源等待
   @param    *wait: 句柄
   @param    event: 事件
   @return   0：成功
   @note     用于多解码打断处理
*/
/*----------------------------------------------------------------------------*/
static int uac_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;

    if (event == AUDIO_RES_GET) {
        // 启动解码
        err = uac_audio_start();
    } else if (event == AUDIO_RES_PUT) {
        // 被打断
        uac_audio_close();
    }

    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    uac收到数据处理
   @param    event: 事件
   @param    *data: 数据
   @param    len: 数据长度
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void uac_speaker_stream_rx_handler(int event, void *data, int len)
{
    /* putchar('A'); */
    uac_dec_resume();
}

/*----------------------------------------------------------------------------*/
/**@brief    打开pc解码
   @param    *_info: pc信息
   @return   0：成功
   @note
*/
/*----------------------------------------------------------------------------*/
static int usb_audio_play_open(void *_info)
{
    struct uac_dec_hdl *dec;

    if (uac_dec) {
        return 0;
    }

    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }

    uac_dec = dec;

#if TCFG_DEC2TWS_ENABLE
    // 设置localtws重播接口
    localtws_globle_set_dec_restart(uac_dec_push_restart);
#endif

    dec->in_info = (u32)_info;
    dec->pcm_dec.sample_rate = (u32)_info & 0xFFFFFF;
    dec->pcm_dec.ch_num = (u32)_info >> 24;
    dec->pcm_dec.output_ch_num = audio_output_channel_num();
    dec->pcm_dec.output_ch_type = audio_output_channel_type();
    printf("usb_audio_play_open sr:%d ch:%d\n", dec->pcm_dec.sample_rate, dec->pcm_dec.ch_num);
    set_uac_speaker_rx_handler(dec, uac_speaker_stream_rx_handler);
    dec->wait.priority = 2;
    dec->wait.preemption = 0;
    dec->wait.snatch_same_prio = 1;
    dec->wait.handler = uac_wait_res_handler;
    audio_decoder_task_add_wait(&decode_task, &dec->wait);

    clock_add(DEC_PCM_CLK);

    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    关闭pc解码
   @param    *arg: 参数
   @return   0：成功
   @note
*/
/*----------------------------------------------------------------------------*/
static int usb_audio_play_close(void *arg)
{
    int err = 0;

    if (!uac_dec) {
        return 0;
    }

    if (uac_dec->start) {
        uac_audio_close();
    }

    uac_dec_relaese();

    return 0;
}


/*----------------------------------------------------------------------------*/
/**@brief    pc解码重新开始
   @param    id: 文件解码id
   @return   0：成功
   @return   非0：失败
   @note
*/
/*----------------------------------------------------------------------------*/
int uac_dec_restart(int id)
{
    if ((!uac_dec) || (id != uac_dec->id)) {
        return -1;
    }
    int _info = uac_dec->in_info;//(uac_dec->pcm_dec.ch_num << 24) | uac_dec->pcm_dec.sample_rate;
    usb_audio_play_close(NULL);
    int err = usb_audio_play_open((void *)_info);
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    推送pc解码重新开始命令
   @param
   @return   true：成功
   @return   false：失败
   @note
*/
/*----------------------------------------------------------------------------*/
int uac_dec_push_restart(void)
{
    if (!uac_dec) {
        return false;
    }
    int argv[3];
    argv[0] = (int)uac_dec_restart;
    argv[1] = 1;
    argv[2] = (int)uac_dec->id;
    os_taskq_post_type(os_current_task(), Q_CALLBACK, ARRAY_SIZE(argv), argv);
    return true;
}


/*----------------------------------------------------------------------------*/
/**@brief    pc事件处理
   @param    event: 事件
   @param    value: 参数
   @return   0：成功
   @note
*/
/*----------------------------------------------------------------------------*/
static int usb_device_event_handler(u8 event, int value)
{
    switch (event) {
    case USB_AUDIO_PLAY_OPEN:
        /*tone_play_stop();*/
        usb_audio_play_open((void *)value);
        break;
    case USB_AUDIO_PLAY_CLOSE:
        usb_audio_play_close((void *)value);
        break;
    case USB_AUDIO_MIC_OPEN:
        usb_audio_mic_open((void *)value);
        break;
    case USB_AUDIO_MIC_CLOSE:
        usb_audio_mic_close((void *)value);
        break;
    case USB_AUDIO_SET_MIC_VOL:
        usb_audio_mic_set_gain(value);
        break;
    case USB_AUDIO_SET_PLAY_VOL:
#if !VOL_INDEPENDENT_EN
        value = uac_vol_switch(value & 0xffff);  //left ch volume
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, value, 1);
#else
        u32 vol_l, vol_r = 0;
        vol_l = uac_vol_switch(value & 0xffff);  //left ch volume
        vol_r = uac_vol_switch((value >> 16) & 0xffff);
        app_audio_set_volume_each_channel(APP_AUDIO_STATE_MUSIC, vol_l, vol_r, 1);
#endif
#if (TCFG_DEC2TWS_ENABLE)
        bt_tws_sync_volume();
#endif
#if TCFG_UI_ENABLE
        u8 vol = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
        UI_SHOW_MENU(MENU_MAIN_VOL, 1000, vol, NULL);
#endif //TCFG_UI_ENABLE
        break;
    default:
        break;
    }
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    pc系统事件处理
   @param    *event: 事件
   @param    *priv: 参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void usb_audio_event_handler(struct sys_event *event)
{
    log_d("usb device event : %d %x\n", event->u.dev.event, event->u.dev.value);
    usb_device_event_handler(event->u.dev.event, event->u.dev.value);
}

/*----------------------------------------------------------------------------*/
/**@brief    音频设备初始化
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
__attribute__((weak))int audio_dev_init()
{
    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    usb音频初始化
   @param
   @return   0：成功
   @note
*/
/*----------------------------------------------------------------------------*/
int usb_audio_demo_init(void)
{
    int err = 0;

    audio_dev_init();
    register_sys_event_handler(SYS_DEVICE_EVENT, DEVICE_EVENT_FROM_UAC, 2,
                               usb_audio_event_handler);

    return 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    usb音频退出
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void usb_audio_demo_exit(void)
{
    unregister_sys_event_handler(usb_audio_event_handler);
    usb_audio_play_close(NULL);
    usb_audio_mic_close(NULL);
}

#endif /* TCFG_APP_PC_EN */


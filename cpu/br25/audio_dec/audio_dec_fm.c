/*
 ****************************************************************
 *File : audio_dec_fm.c
 *Note :
 *
 ****************************************************************
 */

#include "asm/includes.h"
#include "media/includes.h"
#include "media/pcm_decoder.h"
#include "system/includes.h"
#include "effectrs_sync.h"
#include "media/audio_eq_drc_apply.h"
#include "app_config.h"
#include "audio_config.h"
#include "audio_dec.h"
#include "app_config.h"
#include "app_main.h"
#include "audio_enc.h"
#include "audio_dec.h"
#include "clock_cfg.h"
#include "dev_manager.h"
#include "media/convert_data.h"
#include "audio_effect/audio_dynamic_eq_demo.h"
#include "media/effects_adj.h"
#include "audio_effect/audio_eff_default_parm.h"

#if TCFG_PCM_ENC2TWS_ENABLE
#include "bt_tws.h"
#endif

#if TCFG_FM_ENABLE


//////////////////////////////////////////////////////////////////////////////

struct fm_dec_hdl {
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
    u32 source : 8;		// fm音频源
    void *fm;			// 底层驱动句柄
};

//////////////////////////////////////////////////////////////////////////////

struct fm_dec_hdl *fm_dec = NULL;	// fm解码句柄


//////////////////////////////////////////////////////////////////////////////
int linein_sample_size(void *hdl);
int linein_sample_total(void *hdl);

//////////////////////////////////////////////////////////////////////////////

/*----------------------------------------------------------------------------*/
/**@brief    fm数据填充
   @param    *data: 数据
   @param    len: 数据长度
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
AT(.fm_data_code)
void fm_sample_output_handler(s16 *data, int len)
{
    struct fm_dec_hdl *dec = fm_dec;
    if ((dec) && (dec->fm) && (dec->start)) {
        fm_inside_output_handler(dec->fm, data, len);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    fm解码释放
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void fm_dec_relaese(void)
{
    if (fm_dec) {
        audio_decoder_task_del_wait(&decode_task, &fm_dec->wait);
        clock_remove(DEC_FM_CLK);
        local_irq_disable();
        free(fm_dec);
        fm_dec = NULL;
        local_irq_enable();
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    fm解码事件处理
   @param    *decoder: 解码器句柄
   @param    argc: 参数个数
   @param    *argv: 参数
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void fm_dec_event_handler(struct audio_decoder *decoder, int argc, int *argv)
{
    switch (argv[0]) {
    case AUDIO_DEC_EVENT_END:
        if (!fm_dec) {
            log_i("fm_dec handle err ");
            break;
        }

        if (fm_dec->id != argv[1]) {
            log_w("fm_dec id err : 0x%x, 0x%x \n", fm_dec->id, argv[1]);
            break;
        }

        fm_dec_close();
        break;
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    fm解码数据输出
   @param    *entry: 音频流句柄
   @param    *in: 输入信息
   @param    *out: 输出信息
   @return   输出长度
   @note     *out未使用
*/
/*----------------------------------------------------------------------------*/
AT(.fm_data_code)
static int fm_dec_data_handler(struct audio_stream_entry *entry,
                               struct audio_data_frame *in,
                               struct audio_data_frame *out)
{
    struct audio_decoder *decoder = container_of(entry, struct audio_decoder, entry);
    struct pcm_decoder *pcm_dec = container_of(decoder, struct pcm_decoder, decoder);
    struct fm_dec_hdl *dec = container_of(pcm_dec, struct fm_dec_hdl, pcm_dec);
    if (!dec->start) {
        return 0;
    }
    audio_stream_run(&decoder->entry, in);
    return decoder->process_len;
}

/*----------------------------------------------------------------------------*/
/**@brief    fm解码数据流激活
   @param    *p: 私有句柄
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
AT(.fm_data_code)
static void fm_dec_out_stream_resume(void *p)
{
    struct fm_dec_hdl *dec = p;
    audio_decoder_resume(&dec->pcm_dec.decoder);
}

/*----------------------------------------------------------------------------*/
/**@brief    fm解码激活
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
AT(.fm_data_code)
static void fm_dec_resume(void)
{
    if (fm_dec) {
        audio_decoder_resume(&fm_dec->pcm_dec.decoder);
    }
}

/*----------------------------------------------------------------------------*/
/**@brief    计算fm输入采样率
   @param
   @return   采样率
   @note
*/
/*----------------------------------------------------------------------------*/
AT(.fm_data_code)
static int audio_fm_input_sample_rate(void *priv)
{
    struct fm_dec_hdl *dec = (struct fm_dec_hdl *)priv;
    int sample_rate = linein_stream_sample_rate(dec->fm);
    int buf_size = linein_sample_size(dec->fm);
#if TCFG_PCM_ENC2TWS_ENABLE
    if (dec->pcm_dec.dec_no_out_sound) {
        /*TWS的上限在fm输入buffer，下限在tws push buffer*/
        if (buf_size >= linein_sample_total(dec->fm) / 2) {
            sample_rate += (sample_rate * 1 / 10000);
        } else if (tws_api_local_media_trans_check_ready_total() < 1024) {
            sample_rate -= (sample_rate * 1 / 10000);
        }
    }
#endif
    if (buf_size >= (linein_sample_total(dec->fm) * 3 / 4)) {
        sample_rate += (sample_rate * 1 / 10000);
    }
    if (buf_size <= (linein_sample_total(dec->fm) * 1 / 4)) {
        sample_rate -= (sample_rate * 5 / 10000);
    }
    return sample_rate;
}

/*----------------------------------------------------------------------------*/
/**@brief    fm解码开始
   @param
   @return   0：成功
   @return   非0：失败
   @note
*/
/*----------------------------------------------------------------------------*/
int fm_dec_start(void)
{
    int err;
    struct fm_dec_hdl *dec = fm_dec;
    struct audio_mixer *p_mixer = &mixer;

    if (!fm_dec) {
        return -EINVAL;
    }

    err = pcm_decoder_open(&dec->pcm_dec, &decode_task);
    if (err) {
        goto __err1;
    }

    // 打开fm驱动
    dec->fm = fm_sample_open(dec->source, dec->pcm_dec.sample_rate);
    linein_sample_set_resume_handler(dec->fm, fm_dec_resume);

    pcm_decoder_set_event_handler(&dec->pcm_dec, fm_dec_event_handler, dec->id);
    pcm_decoder_set_read_data(&dec->pcm_dec, linein_sample_read, dec->fm);
    pcm_decoder_set_data_handler(&dec->pcm_dec, fm_dec_data_handler);

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
        audio_mode_main_dec_open(AUDIO_MODE_MAIN_STATE_DEC_FM);
    }

    // 设置叠加功能
    audio_mixer_ch_open_head(&dec->mix_ch, p_mixer);
    audio_mixer_ch_set_no_wait(&dec->mix_ch, 1, 10); // 超时自动丢数
    audio_mixer_ch_follow_resample_enable(&dec->mix_ch, dec, audio_fm_input_sample_rate);


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

#if TCFG_EQ_ENABLE && TCFG_FM_MODE_EQ_ENABLE
    dec->eq = music_eq_open(dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);// eq
#if TCFG_DRC_ENABLE && TCFG_FM_MODE_DRC_ENABLE
    dec->drc = music_drc_open(dec->pcm_dec.sample_rate, dec->pcm_dec.output_ch_num);//drc
#endif/*TCFG_FM_MODE_DRC_ENABLE*/
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
#endif/*TCFG_FM_MODE_EQ_ENABLE*/

    struct audio_stream_entry *entries[16] = {NULL};
    u8 entry_cnt = 0;
    u8 rl_rr_entry_start = 0;
    entries[entry_cnt++] = &dec->pcm_dec.decoder.entry;

#if SYS_DIGVOL_GROUP_EN
    void *dvol_entry = sys_digvol_group_ch_open("music_fm", -1, NULL);
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

#if TCFG_EQ_ENABLE && TCFG_FM_MODE_EQ_ENABLE
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
    dec->stream = audio_stream_open(dec, fm_dec_out_stream_resume);
    audio_stream_add_list(dec->stream, entries, entry_cnt);

    // 设置音频输出音量
    audio_output_set_start_volume(APP_AUDIO_STATE_MUSIC);

    // 开始解码
    dec->start = 1;
    err = audio_decoder_start(&dec->pcm_dec.decoder);
    if (err) {
        goto __err3;
    }
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

#if TCFG_EQ_ENABLE && TCFG_FM_MODE_EQ_ENABLE
    music_eq_close(dec->eq);
#if TCFG_DRC_ENABLE && TCFG_FM_MODE_DRC_ENABLE
    music_drc_close(dec->drc);
#endif
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
    convet_data_close(dec->convert);
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
#endif

    if (dec->fm) {
        local_irq_disable();
        fm_sample_close(dec->fm, dec->source);
        dec->fm = NULL;
        local_irq_enable();
    }
    audio_mixer_ch_close(&dec->mix_ch);
#if TCFG_PCM_ENC2TWS_ENABLE
    if (dec->pcm_dec.dec_no_out_sound) {
        dec->pcm_dec.dec_no_out_sound = 0;
        localtws_enc_api_close();
    }
#endif

#if SYS_DIGVOL_GROUP_EN
    sys_digvol_group_ch_close("music_fm");
#endif // SYS_DIGVOL_GROUP_EN


    if (dec->stream) {
        audio_stream_close(dec->stream);
        dec->stream = NULL;
    }


    pcm_decoder_close(&dec->pcm_dec);
__err1:
    fm_dec_relaese();
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    fm解码关闭
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
static void __fm_dec_close(void)
{
    if (fm_dec && fm_dec->start) {
        fm_dec->start = 0;

        pcm_decoder_close(&fm_dec->pcm_dec);

        local_irq_disable();
        fm_sample_close(fm_dec->fm, fm_dec->source);
        fm_dec->fm = NULL;
        local_irq_enable();
#if AUDIO_SURROUND_CONFIG
        surround_close_demo(fm_dec->surround);
#endif

#if AUDIO_VBASS_CONFIG
        audio_gain_close_demo(fm_dec->vbass_prev_gain);
        audio_noisegate_close_demo(fm_dec->ns_gate);
        audio_vbass_close_demo(fm_dec->vbass);
#endif

#if TCFG_EQ_ENABLE && TCFG_AUDIO_OUT_EQ_ENABLE
        high_bass_eq_close(fm_dec->high_bass);
        high_bass_drc_close(fm_dec->hb_drc);
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
        convet_data_close(fm_dec->hb_convert);
#endif
#endif

#if TCFG_EQ_ENABLE && TCFG_FM_MODE_EQ_ENABLE
        music_eq_close(fm_dec->eq);
#if TCFG_DRC_ENABLE && TCFG_FM_MODE_DRC_ENABLE
        music_drc_close(fm_dec->drc);
#endif
#if defined(TCFG_DRC_ENABLE) && TCFG_DRC_ENABLE
        convet_data_close(fm_dec->convert);
#endif
#if defined(MUSIC_EXT_EQ_AFTER_DRC) && MUSIC_EXT_EQ_AFTER_DRC
        music_ext_eq_close(fm_dec->ext_eq);
#endif

#if defined(TCFG_DYNAMIC_EQ_ENABLE) && TCFG_DYNAMIC_EQ_ENABLE
        music_eq2_close(fm_dec->eq2);
        audio_dynamic_eq_ctrl_close(fm_dec->dy_eq);
        convet_data_close(fm_dec->convert2);
#endif
#if defined(TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE) && TCFG_PHASER_GAIN_AND_CH_SWAP_ENABLE
        audio_gain_close_demo(fm_dec->gain);
#endif
#endif

        audio_mixer_ch_close(&fm_dec->mix_ch);
#if TCFG_PCM_ENC2TWS_ENABLE
        if (fm_dec->pcm_dec.dec_no_out_sound) {
            fm_dec->pcm_dec.dec_no_out_sound = 0;
            localtws_enc_api_close();
        }
#endif


#if SYS_DIGVOL_GROUP_EN
        sys_digvol_group_ch_close("music_fm");
#endif // SYS_DIGVOL_GROUP_EN


        // 先关闭各个节点，最后才close数据流
        if (fm_dec->stream) {
            audio_stream_close(fm_dec->stream);
            fm_dec->stream = NULL;
        }
    }



}





/*----------------------------------------------------------------------------*/
/**@brief    fm解码资源等待
   @param    *wait: 句柄
   @param    event: 事件
   @return   0：成功
   @note     用于多解码打断处理
*/
/*----------------------------------------------------------------------------*/
static int fm_wait_res_handler(struct audio_res_wait *wait, int event)
{
    int err = 0;
    log_i("fm_wait_res_handler, event:%d\n", event);
    if (event == AUDIO_RES_GET) {
        // 启动解码
        err = fm_dec_start();
    } else if (event == AUDIO_RES_PUT) {
        // 被打断
        __fm_dec_close();
    }

    return err;
}
/*----------------------------------------------------------------------------*/
/**@brief    暂停/启动 fm解码mix ch输出
   @param    pause : 1:暂停   0：启动
   @return   NULL
   @note
*/
/*----------------------------------------------------------------------------*/
void fm_dec_pause_out(u8 pause)
{
    if (!fm_dec) {
        return;
    }

    audio_mixer_ch_pause(&fm_dec->mix_ch, pause);
    audio_decoder_resume_all(&decode_task);
}

/*----------------------------------------------------------------------------*/
/**@brief    打开fm解码
   @param    source: 音频源
   @param    sample_rate: 采样率
   @return   0：成功
   @return   非0：失败
   @note
*/
/*----------------------------------------------------------------------------*/
int fm_dec_open(u8 source, u32 sample_rate)
{
    int err;
    struct fm_dec_hdl *dec;
    dec = zalloc(sizeof(*dec));
    if (!dec) {
        return -ENOMEM;
    }
    fm_dec = dec;

    dec->id = rand32();

    dec->source = source;

    dec->pcm_dec.ch_num = 2;
    dec->pcm_dec.output_ch_num = audio_output_channel_num();
    dec->pcm_dec.output_ch_type = audio_output_channel_type();
    dec->pcm_dec.sample_rate = sample_rate;

    dec->wait.priority = 2;
    dec->wait.preemption = 0;
    dec->wait.snatch_same_prio = 1;
    dec->wait.handler = fm_wait_res_handler;

    clock_add(DEC_FM_CLK);


#if TCFG_DEC2TWS_ENABLE
    // 设置localtws重播接口
    localtws_globle_set_dec_restart(fm_dec_push_restart);
#endif

    err = audio_decoder_task_add_wait(&decode_task, &dec->wait);
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    关闭fm解码
   @param
   @return
   @note
*/
/*----------------------------------------------------------------------------*/
void fm_dec_close(void)
{
    if (!fm_dec) {
        return;
    }

    __fm_dec_close();
    fm_dec_relaese();
    clock_set_cur();
    log_i("fm dec close \n\n ");
}

/*----------------------------------------------------------------------------*/
/**@brief    fm解码重新开始
   @param    id: 文件解码id
   @return   0：成功
   @return   非0：失败
   @note
*/
/*----------------------------------------------------------------------------*/
int fm_dec_restart(int id)
{
    if ((!fm_dec) || (id != fm_dec->id)) {
        return -1;
    }
    u8 source = fm_dec->source;
    u32 sample_rate = fm_dec->pcm_dec.sample_rate;
    fm_dec_close();
    int err = fm_dec_open(source, sample_rate);
    return err;
}

/*----------------------------------------------------------------------------*/
/**@brief    推送fm解码重新开始命令
   @param
   @return   true：成功
   @return   false：失败
   @note
*/
/*----------------------------------------------------------------------------*/
int fm_dec_push_restart(void)
{
    if (!fm_dec) {
        return false;
    }
    int argv[3];
    argv[0] = (int)fm_dec_restart;
    argv[1] = 1;
    argv[2] = (int)fm_dec->id;
    os_taskq_post_type(os_current_task(), Q_CALLBACK, ARRAY_SIZE(argv), argv);
    return true;
}





#endif
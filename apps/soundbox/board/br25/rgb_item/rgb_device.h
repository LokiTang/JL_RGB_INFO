#ifndef  __RGB_DEVICE_H__
#define  __RGB_DEVICE_H__

#include "rgb_item/rgb_driver.h"

//------------------------------用户开关-----------------------
#define SYS_DAC_ENERGY_GET              1               //是否获取dac能量
#define SYS_TYPE_DAC_ENERGY             0               //打印dac获取到的能量
#define BREATHE_LIKE_RAINBOW            1               //彩虹呼吸灯
#define GRADIENT_INCRE_DECRE            0               //渐变效果正反序
#define TYPE_SYS_ASSERT_EN              0               //打印函数本体

typedef struct rgb_device
{
    u8 color_r; //r的初始颜色阈值
    u8 color_g; //g的初始颜色阈值
    u8 color_b; //b的初始颜色阈值

} __RGB_DEVICE__;

#define  LED_NUM_MAX        16
#define  HSV_VECTOR_MAX     360
#define  MAX_BRIGHTNESS     100
#define  DAC_NUM_MAX        1000

//h阈值的偏移量
#define  HUE_DATA_OFFSET   (__HSV_DEVICE__) \
        {                   \
         .multiple_hue = 0, \
         .hue_offset = 1,   \
         .current_led = 0   \
        }

#define  TIMER_DATA_OFFSET    {   \
         .timer = 0    }

typedef struct hsv_device
{
    u16 h;  //h的初始颜色阈值
    u8 s;   //s的初始颜色阈值
    u8 v;   //v的初始颜色阈值

    u16 multiple_hue;   //h的阈值总和
    u16 hue_offset;     //h阈值的偏移量

    u8 breath_value;       //呼吸的阈值   

    u16 current_led;   

} __HSV_DEVICE__;

typedef struct dac_energy_get
{
    u32 energy;     //获取到的能量

} __DAC_ENERGY__;

//定时器信息
typedef struct timer_ret
{
    u32 timer;
} __TIMER_INFO__;

#if GRADIENT_INCRE_DECRE
//用户自定义枚举
typedef enum {
    INCREMENT,
    DECREMENT
} Direction;
#endif

#define sys_assert  \
            printf
#if SYS_DAC_ENERGY_GET
    #define get_dac_energy  \
                audio_dac_energy_get
#endif
//add below here

#define __spi_init__    \
            led_spi_init()
#define __clear_spi_buf__   \
            clear_buffer()
#define __timer_interrupt__ \
            clear_timer()

/*******************************INTERNAL CITE AREA*****************************/
//彩虹灯效跟随DAC采集旋转
void item_rainbow_cycle_with_energy(void);
//四个灯为一个颜色旋转-固定
void item_rainbow_cycle_with_fourth_fir(void);
void item_rainbow_cycle_with_fourth_sec(void);
//彩色星空-缩水版disco
void item_rainbow_cycle_with_star_infer(void);
//彩色星空-增强版disco
void item_rainbow_cycle_with_star_super(void);
//渐变灯效
void item_color_transform_with_gradient(void);
//呼吸灯效
void item_color_breathing_with_duration(void);
//追光灯效
void item_color_cycle_chasing_light(void);
//追光效果，带拖尾不灭灯
void item_color_cycle_chasing_with_tail(void);
//单灯变换追光
void item_color_cycle_chasing_with_single(void);


//四个灯为一个颜色旋转-流动
void item_rainbow_cycle_with_fourth_move(void);
//灯效总函数
void color_overall_pattern_items_steer(u8 steer);

/*******************************EXTERNL CITE AREA*****************************/
//发送spi数据
extern void user_rgb_send_buf(u8 r, u8 g, u8 b, u32 index);
//采集系统dac能量
extern int audio_dac_energy_get(void);
//系统spi初始化
extern void led_spi_init(void);


typedef enum{
    ITEM_RAINBOW_CYCLE,
    ITEM_FOURTH_CYCLE_FIR,
    ITEM_TRAN_GRADIENT,
    ITEM_TRAN_BREATHE,
    ITEM_FOURTH_CYCLE_SEC,
    ITEM_TRAN_DISCO_INFER,
    ITEM_TRAN_DISCO_SUPER,
    ITEM_CHASING_LIGHT_FIR,
    ITEM_CHASING_SINGLE,
    ITEM_CHASING_LIGHT_SEC,
    ITEM_FOURTH_CYCLE_MOVE,
    ITEM_TEMP_MODE,
}__ITEMS_STEER__;

#endif
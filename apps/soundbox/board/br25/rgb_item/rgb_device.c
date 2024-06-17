#include "rgb_item/rgb_device.h"
#include "rgb_item/rgb_driver.h"

__RGB_DEVICE__ rgb_info;
__HSV_DEVICE__ hsv_info = HUE_DATA_OFFSET;

__DAC_ENERGY__ energy_info;
__HSV_DEVICE__ rgb_array[LED_NUM_MAX];

__TIMER_INFO__ timer_info = TIMER_DATA_OFFSET;

#if GRADIENT_INCRE_DECRE
    Direction hue_direction = INCREMENT;
#endif
//初始化阈值
static u8 hsv_array[] = {95, 100};

static u8 color_vector[][3] =   \
{
    {0xFF, 0X0,  0X0},          //red
    {0xFF, 0xFF, 0X0},          //yellow
    {0X0,  0xFF, 0X0},          //green
    {0xFF, 0X0,  0xFF},         //purple
    {0X0,  0X0,  0xFF},         //blue
    {0X0,  0xFF, 0xFF},         //cyan
    {0xFF, 0xFF, 0xFF},         //white
    {0X0,  0X0,  0X0},          //dark
};

static u16 fourth_color_vector[] = {
    {0x00FF, 0X00F0, 0X00E0, 0X00A0}
};

//七彩流光灯 播放音乐时快速变换
void item_rainbow_cycle_with_energy(void)
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif
    
#if SYS_DAC_ENERGY_GET
    energy_info.energy = get_dac_energy() / DAC_NUM_MAX;
    if(!energy_info.energy)energy_info.energy++;
#endif
    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        rgb_array[i].h = (hsv_info.multiple_hue + i * (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
        memcpy(&rgb_array[i].s,hsv_array+1,sizeof(rgb_array[i].s));
        memcpy(&rgb_array[i].v,hsv_array,sizeof(rgb_array[i].v));
    }

#if SYS_DAC_ENERGY_GET
    //根据DAC采集的能量旋转
    hsv_info.multiple_hue += energy_info.energy;
#else 
    hsv_info.multiple_hue += hsv_info.hue_offset;
#endif
    for (u8 i = 0; i < LED_NUM_MAX; i++)
    {
        __RGB_HSV_MATCH( &rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b,rgb_array[i].h, rgb_array[i].s, rgb_array[i].v);
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }
#if SYS_TYPE_DAC_ENERGY
    sys_assert("********dac=%d\n********",energy_info.energy);
#endif
}

//四个灯为一组颜色旋转-固定
void item_rainbow_cycle_with_fourth_fir(void)
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif

#if SYS_DAC_ENERGY_GET
    energy_info.energy = get_dac_energy() / DAC_NUM_MAX;
    if(!energy_info.energy)energy_info.energy++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        rgb_array[i].h = (hsv_info.multiple_hue + i * (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
        memcpy(&rgb_array[i].s,hsv_array+1,sizeof(rgb_array[i].s));
        memcpy(&rgb_array[i].v,hsv_array,sizeof(rgb_array[i].v));
    }
#if SYS_DAC_ENERGY_GET
    hsv_info.multiple_hue += energy_info.energy;
#else 
    hsv_info.multiple_hue++;
#endif
    for (u8 i = 0; i < LED_NUM_MAX; i += 4) {
        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[i / 4].h, rgb_array[i / 4].s, rgb_array[i / 4].v);
        
        for (u8 j = 0; j < 4; j++) {
            user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i + j);
        }
    }
}

//彩色星空-增强版disco
void item_rainbow_cycle_with_star_super(void)
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif

#if SYS_DAC_ENERGY_GET
    energy_info.energy = get_dac_energy() / DAC_NUM_MAX;
    if (!energy_info.energy) energy_info.energy++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        rgb_array[i].h = (hsv_info.multiple_hue + (i % 4) * (HSV_VECTOR_MAX / 4) + i * (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
        memcpy(&rgb_array[i].s, hsv_array + 1, sizeof(rgb_array[i].s));
        memcpy(&rgb_array[i].v, hsv_array, sizeof(rgb_array[i].v));
    }

#if SYS_DAC_ENERGY_GET
    hsv_info.multiple_hue += energy_info.energy;
#else
    hsv_info.multiple_hue++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[i].h, rgb_array[i].s, rgb_array[i].v);
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }
}

//彩色星空-缩水版disco
void item_rainbow_cycle_with_star_infer(void)
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif
    
#if SYS_DAC_ENERGY_GET
    energy_info.energy = get_dac_energy() / DAC_NUM_MAX;
    if (!energy_info.energy) energy_info.energy++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        rgb_array[i].h = (hsv_info.multiple_hue + i * (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
        memcpy(&rgb_array[i].s, hsv_array + 1, sizeof(rgb_array[i].s));
        memcpy(&rgb_array[i].v, hsv_array, sizeof(rgb_array[i].v));
    }

#if SYS_DAC_ENERGY_GET
    hsv_info.multiple_hue += energy_info.energy;
#else
    hsv_info.multiple_hue++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        u8 color_index = (i / 4 + i % 4) % LED_NUM_MAX;

        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[color_index].h, rgb_array[color_index].s, rgb_array[color_index].v);
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }
}

//四个灯为一个颜色旋转-固定-颜色跟丰富
void item_rainbow_cycle_with_fourth_sec(void)
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif

#if SYS_DAC_ENERGY_GET
    energy_info.energy = get_dac_energy() / DAC_NUM_MAX;
    if (!energy_info.energy) energy_info.energy++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i += 4) {
        rgb_array[i / 4].h = (hsv_info.multiple_hue + i * (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
        memcpy(&rgb_array[i / 4].s, hsv_array + 1, sizeof(rgb_array[i / 4].s));
        memcpy(&rgb_array[i / 4].v, hsv_array, sizeof(rgb_array[i / 4].v));
    }

#if SYS_DAC_ENERGY_GET
    hsv_info.multiple_hue += energy_info.energy;
#else
    hsv_info.multiple_hue++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i += 4) {
        u8 color_index = i / 4;
        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[color_index].h, rgb_array[color_index].s, rgb_array[color_index].v);

        for (u8 j = 0; j < 4; j++) {
            u8 led_index = (i + j) % LED_NUM_MAX;
            user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, led_index);
        }
    }
}

//渐变灯效
void item_color_transform_with_gradient(void)        
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("hsv_info.multiple_hue : %d",hsv_info.multiple_hue);
#endif
    memcpy(&hsv_info.s,hsv_array+1,sizeof(hsv_info.s));
    memcpy(&hsv_info.v,hsv_array,sizeof(hsv_info.v)); 
    
#if GRADIENT_INCRE_DECRE
    if (hue_direction == INCREMENT) {
        hsv_info.multiple_hue += hsv_info.hue_offset;
        if (hsv_info.multiple_hue >= (HSV_VECTOR_MAX-1)) {
            hsv_info.multiple_hue = (HSV_VECTOR_MAX-1);
            hue_direction = DECREMENT;
        }
    } else {  
        hsv_info.multiple_hue -= hsv_info.hue_offset;
        if (hsv_info.multiple_hue <= INCREMENT) {
            hsv_info.multiple_hue = INCREMENT;
            hue_direction = INCREMENT;
        }
    }
    for (u8 i = 0; i < LED_NUM_MAX; i++) 
    {
        __RGB_HSV_MATCH( &rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b,hsv_info.multiple_hue, hsv_info.s, hsv_info.v);
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }
#else
    hsv_info.multiple_hue += hsv_info.hue_offset; 

    for (u8 i = 0; i < LED_NUM_MAX; i++) 
    {
        __RGB_HSV_MATCH( &rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b,hsv_info.multiple_hue, hsv_info.s, hsv_info.v);
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }

    //这里不能等于HSV_VECTOR_MAX，否则会跳灯
    hsv_info.multiple_hue %= (HSV_VECTOR_MAX-1);
#endif
}

//呼吸灯效
void item_color_breathing_with_duration(void)          
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif
    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        rgb_array[i].h = (i * (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
        memcpy(&rgb_array[i].s, hsv_array + 1, sizeof(rgb_array[i].s)); 
        rgb_array[i].v = hsv_info.breath_value; 
    }

    #if BREATHE_LIKE_RAINBOW
    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[i].h, rgb_array[i].s, rgb_array[i].v);
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }
    #else
    __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[hsv_info.breath_value].h, rgb_array[hsv_info.breath_value].s, rgb_array[hsv_info.breath_value].v);
    color_rainbow_init(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, hsv_info.breath_value);
    #endif
    hsv_info.breath_value += hsv_info.hue_offset;

    if (hsv_info.breath_value >= 100) {
        hsv_info.hue_offset = -1;
    }

    if (hsv_info.breath_value <= 0) {
        hsv_info.hue_offset = 1;
        hsv_info.multiple_hue +=  hsv_info.hue_offset % LED_NUM_MAX;
    }
}

//追光灯效
void item_color_cycle_chasing_light(void)
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif
#if SYS_DAC_ENERGY_GET
    energy_info.energy = get_dac_energy() / DAC_NUM_MAX;
    if (!energy_info.energy) energy_info.energy++;
#endif

    static u8 current_position = 0;
    static u8 current_color_index = 0;

    // 更新 multiple_hue 到新的颜色
    if (current_position == 0) {
        current_color_index = (current_color_index + 1) % (LED_NUM_MAX / 3); // 每 15 个灯变换一次颜色
        hsv_info.multiple_hue = (hsv_info.multiple_hue + (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
    }

    // 计算当前颜色
    rgb_array[current_color_index].h = hsv_info.multiple_hue;
    memcpy(&rgb_array[current_color_index].s, hsv_array + 1, sizeof(rgb_array[current_color_index].s));
    memcpy(&rgb_array[current_color_index].v, hsv_array, sizeof(rgb_array[current_color_index].v));

#if SYS_DAC_ENERGY_GET
    hsv_info.multiple_hue += energy_info.energy;
#else
    hsv_info.multiple_hue++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        if (i == current_position || i == (current_position + 1) % LED_NUM_MAX || i == (current_position + 2) % LED_NUM_MAX) {
            __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[current_color_index].h, rgb_array[current_color_index].s, rgb_array[current_color_index].v);
        } else {
            rgb_info.color_r = 0,   \
            rgb_info.color_g = 0,   \
            rgb_info.color_b = 0;
        }
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }

    // 更新当前位置
    current_position = (current_position + 1) % LED_NUM_MAX;
}

//闪电-随机
void hsv_stimulate_color_electronic_func(void)              
{
    u8 start_led = rand() % LED_NUM_MAX;
    u8 end_led = rand() % LED_NUM_MAX;

    u16 hue_step = (end_led > start_led) ? (HSV_VECTOR_MAX / (end_led - start_led)) : (HSV_VECTOR_MAX / (LED_NUM_MAX - start_led + end_led));

    for (u8 i = start_led; i != end_led; i = (i + 1) % LED_NUM_MAX) {
        rgb_array[i].h = (i - start_led) * hue_step;
        rgb_array[i].s = 100;               
        rgb_array[i].v = rand() % (MAX_BRIGHTNESS + 1); 
    }

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[i].h, rgb_array[i].s, rgb_array[i].v);
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }
}

//追光效果，带拖尾不灭灯
void item_color_cycle_chasing_with_tail(void)        
{
    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        rgb_array[i].h = hsv_info.multiple_hue;
        memcpy(&rgb_array[i].s, hsv_array + 1, sizeof(rgb_array[i].s)); 
        memcpy(&rgb_array[i].v, hsv_array, sizeof(rgb_array[i].v));     
    }

    __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[hsv_info.current_led].h, rgb_array[hsv_info.current_led].s, rgb_array[hsv_info.current_led].v);
    user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, hsv_info.current_led);

    hsv_info.multiple_hue += hsv_info.hue_offset % HSV_VECTOR_MAX;

    hsv_info.current_led = (hsv_info.current_led+1) % LED_NUM_MAX;
}

//单灯变换追光
void item_color_cycle_chasing_with_single(void)
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif
    
    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        if (i == hsv_info.current_led) {
            rgb_array[i].h = rand() % HSV_VECTOR_MAX;
            rgb_array[i].s = hsv_array[0];  
            rgb_array[i].v = hsv_array[1];  
        } else {
            rgb_array[i].v = 0;  
        }
    }

    hsv_info.current_led = (hsv_info.current_led+1) % LED_NUM_MAX;

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[i].h, rgb_array[i].s, rgb_array[i].v);
        user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, i);
    }
}

//四个灯为一个颜色旋转-流动
void item_rainbow_cycle_with_fourth_move(void) 
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif
    static u8 offset = 0;   //记录偏移量，四个灯为一组旋转流动模式

#if SYS_DAC_ENERGY_GET
    energy_info.energy = get_dac_energy() / DAC_NUM_MAX;
    if(!energy_info.energy)energy_info.energy++;
#endif 

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        rgb_array[i].h = (hsv_info.multiple_hue + i * (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
        memcpy(&rgb_array[i].s,hsv_array+1,sizeof(rgb_array[i].s));
        memcpy(&rgb_array[i].v,hsv_array,sizeof(rgb_array[i].v));
    }

    for (u8 i = 0; i < LED_NUM_MAX; i += 4) {
        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[i / 4].h, rgb_array[i / 4].s, rgb_array[i / 4].v);
        
        for (u8 j = 0; j < 4; j++) {
            u8 led_index = (i + j + offset) % LED_NUM_MAX;
            user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, led_index);
        }
    }       
#if SYS_DAC_ENERGY_GET
    hsv_info.multiple_hue += energy_info.energy;
    offset = (offset + 1) % LED_NUM_MAX;
#else                                 
    hsv_info.multiple_hue++;
    offset = (offset + 1) % LED_NUM_MAX;
#endif
}

//四个灯为一个颜色旋转-流动-颜色更丰富
void item_rainbow_cycle_with_fourth_sec_move(void)
{
#if TYPE_SYS_ASSERT_EN
    sys_assert("%s", __func__);
#endif
    static u8 offset = 0;   //记录偏移量，四个灯为一组旋转流动模式

#if SYS_DAC_ENERGY_GET
    energy_info.energy = get_dac_energy() / DAC_NUM_MAX;
    if (!energy_info.energy) energy_info.energy++;
#endif

    for (u8 i = 0; i < LED_NUM_MAX; i++) {
        rgb_array[i].h = (hsv_info.multiple_hue + i * (HSV_VECTOR_MAX / LED_NUM_MAX)) % HSV_VECTOR_MAX;
        memcpy(&rgb_array[i].s,hsv_array+1,sizeof(rgb_array[i].s));
        memcpy(&rgb_array[i].v,hsv_array,sizeof(rgb_array[i].v));
    }

    for (u8 i = 0; i < LED_NUM_MAX; i += 4) {
        u8 color_index = i / 4;
        __RGB_HSV_MATCH(&rgb_info.color_r, &rgb_info.color_g, &rgb_info.color_b, rgb_array[color_index].h, rgb_array[color_index].s, rgb_array[color_index].v);

        for (u8 j = 0; j < 4; j++) {
            u8 led_index = (i + j + offset) % LED_NUM_MAX;
            user_rgb_send_buf(rgb_info.color_r, rgb_info.color_g, rgb_info.color_b, led_index);
        }
    }

#if SYS_DAC_ENERGY_GET
    hsv_info.multiple_hue += energy_info.energy;
    offset = (offset + 1) % LED_NUM_MAX;
#else                                 
    hsv_info.multiple_hue++;
    offset = (offset + 1) % LED_NUM_MAX;
#endif
}

//-------------------------------end here------------------------
//清除现有的定时器
void clear_timer(void)
{
    if(timer_info.timer)
    {
        sys_timer_del(timer_info.timer);
        timer_info.timer = 0;
    }
}
//灯效总函数
void color_overall_pattern_items_steer(u8 steer)
{

//spi初始化之后清空buf里面的数据
__spi_init__;
__clear_spi_buf__;
__timer_interrupt__;

    switch (steer)
    {
    case ITEM_RAINBOW_CYCLE:
        //七彩流光灯
        timer_info.timer = sys_timer_add(NULL,item_rainbow_cycle_with_energy,15);      
        break;
    case ITEM_FOURTH_CYCLE_FIR:
        //四个灯为一组-模式1
        timer_info.timer = sys_timer_add(NULL,item_rainbow_cycle_with_fourth_fir,20);
        break;
    case ITEM_TRAN_GRADIENT: 
        //渐变灯效
        timer_info.timer = sys_timer_add(NULL,item_color_transform_with_gradient,30);
        break;
    case ITEM_TRAN_BREATHE:
        //呼吸灯效
        timer_info.timer = sys_timer_add(NULL,item_color_breathing_with_duration,20);
        break;
    case ITEM_FOURTH_CYCLE_SEC:
        //四个灯为一组-模式2
        timer_info.timer = sys_timer_add(NULL,item_rainbow_cycle_with_fourth_sec,20);
        break;
    case ITEM_TRAN_DISCO_INFER:
        //彩色星空-缩水版disco
        timer_info.timer = sys_timer_add(NULL,item_rainbow_cycle_with_star_infer,20);
        break;
    case ITEM_TRAN_DISCO_SUPER:
        //彩色星空-增强版disco
        timer_info.timer = sys_timer_add(NULL,item_rainbow_cycle_with_star_super,20);
        break;
    case ITEM_CHASING_LIGHT_FIR:
        //追光
        timer_info.timer = sys_timer_add(NULL,item_color_cycle_chasing_light,80);
        break;
    case ITEM_CHASING_SINGLE:
        //单灯变换追光
        timer_info.timer = sys_timer_add(NULL,item_color_cycle_chasing_with_single,80);
        break;
    case ITEM_CHASING_LIGHT_SEC:
        //追光带拖尾，不灭灯
        timer_info.timer = sys_timer_add(NULL,item_color_cycle_chasing_with_tail,80);
        break;    
    case ITEM_FOURTH_CYCLE_MOVE:
        //四个灯为一组-流动
        timer_info.timer = sys_timer_add(NULL,item_rainbow_cycle_with_fourth_move,80);
        break;
    case ITEM_TEMP_MODE:
        timer_info.timer = sys_timer_add(NULL,item_rainbow_cycle_with_fourth_sec_move,80);
        break;
    default:
        break;
    }
}

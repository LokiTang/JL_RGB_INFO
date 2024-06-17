// #include"rgb_device.h"
#include"rgb_item/rgb_driver.h"


void RGBtoHSV(u8 r, u8 g, u8 b, u16 *h, u8 *s, u8 *v) 
{
    double min, max, delta;

    min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    *v = max / 255.0; // 计算亮度

    delta = max - min;
    if (max != 0) {
        *s = (delta / max); // 计算饱和度
    } else {
        // r = g = b = 0
        *s = 0;
        *h = -1;
        return;
    }

    if (r == max)
        *h = (g - b) / delta; // 计算色相
    else if (g == max)
        *h = 2 + (b - r) / delta;
    else
        *h = 4 + (r - g) / delta;

    *h *= 60; // 将角度转化为度数
    if (*h < 0)
        *h += 360;
    
}
//第二种，shufi765
#if 0
void HSVtoRGB(u8 *r, u8 *g, u8 *b, u16 h, u8 s, u8 v) {
    u8 i;
    u8 f, p, q, t;

    if (s == 0) {
        *r = *g = *b = v;
        return;
    }

    h /= 60;
    i = h % 6;
    f = h / 6.0 - h / 6;
    p = v * (1 - s);
    q = v * (1 - f * s);
    t = v * (1 - (1 - f) * s);

    switch (i) {
    case 0:
        *r = v;
        *g = t;
        *b = p;
        break;
    case 1:
        *r = q;
        *g = v;
        *b = p;
        break;
    case 2:
        *r = p;
        *g = v;
        *b = t;
        break;
    case 3:
        *r = p;
        *g = q;
        *b = v;
        break;
    case 4:
        *r = t;
        *g = p;
        *b = v;
        break;
    default:
        *r = v;
        *g = p;
        *b = q;
        break;
    }
}
#endif

#if 0
void HSVtoRGB(u16 h, u8 s, u8 v, u8 *r, u8 *g, u8 *b) 
{
    int i;
    double f, p, q, t;

    if (s == 0) {
        *r = *g = *b = v * 255;
        return;
    }

    h /= 60;
    i = (int)h;
    f = h - i;
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));

    switch (i) {
        case 0:
            *r = v * 255;
            *g = t * 255;
            *b = p * 255;
            break;
        case 1:
            *r = q * 255;
            *g = v * 255;
            *b = p * 255;
            break;
        case 2:
            *r = p * 255;
            *g = v * 255;
            *b = t * 255;
            break;
        case 3:
            *r = p * 255;
            *g = q * 255;
            *b = v * 255;
            break;
        case 4:
            *r = t * 255;
            *g = p * 255;
            *b = v * 255;
            break;
        default:
            *r = v * 255;
            *g = p * 255;
            *b = q * 255;
            break;
    }
}

#elif 1
//常用
//颜色模型  HSV  转  RGB
void HSVtoRGB(u8 *r, u8 *g, u8 *b, u16 h, u8 s, u8 v)
{
	// R,G,B from 0-255, H from 0-360, S,V from 0-100
	int i;
	float RGB_min, RGB_max;
	RGB_max = v*2.55f;
	RGB_min = RGB_max*(100 - s) / 100.0f;

	i = h / 60;
	int difs = h % 60; // factorial part of h

					   // RGB adjustment amount by hue 
	float RGB_Adj = (RGB_max - RGB_min)*difs / 60.0f;

	switch (i) {
	case 0:
		*r = RGB_max;
		*g = RGB_min + RGB_Adj;
		*b = RGB_min;
		break;
	case 1:
		*r = RGB_max - RGB_Adj;
		*g = RGB_max;
		*b = RGB_min;
		break;
	case 2:
		*r = RGB_min;
		*g = RGB_max;
		*b = RGB_min + RGB_Adj;
		break;
	case 3:
		*r = RGB_min;
		*g = RGB_max - RGB_Adj;
		*b = RGB_max;
		break;
	case 4:
		*r = RGB_min + RGB_Adj;
		*g = RGB_min;
		*b = RGB_max;
		break;
	default:		// case 5:
		*r = RGB_max;
		*g = RGB_min;
		*b = RGB_max - RGB_Adj;
		break;
	}
}
#endif


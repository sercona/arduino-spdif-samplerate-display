#ifndef _OLED_BITMAPS_H_
#define _OLED_BITMAPS_H_

#include <U8glib.h>
//#define SCREEN_180 1

extern U8GLIB_SH1106_128X64 u8g;

// input selector icons
#define input_select_digital_width  32
#define input_select_digital_height 20
extern unsigned char input_select_digital_icon_bits[];

#define input_select_analog_width  32
#define input_select_analog_height 20
extern unsigned char input_select_analog_icon_bits[];

// speaker muted icon
#define speaker_muted_width 32
#define speaker_muted_height 20
extern unsigned char speaker_muted_icon_bits[];

// speaker un-muted icon
#define speaker_playing_width  32 //37
#define speaker_playing_height 20 //37
extern unsigned char speaker_playing_icon_bits[];

// alarm clock icon
#define alarm_icon_1_width 32
#define alarm_icon_1_height 37
extern unsigned char alarm_icon_1_bits[];


// public functions
extern void oled_draw_logo_loop(void);

#endif // _OLED_BITMAPS_H_


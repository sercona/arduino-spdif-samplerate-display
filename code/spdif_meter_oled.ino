/*
   spdif_meter_oled.ino

   Using common 'hobby' oled 128x64 i2c display and, optionally, a push button for input selector.
   
   Pin5 on the atmega arduino is enabled as a software 'frequency counter'.  Feed a level-shifted
   (3.3v from i2s to arduino's 5v) and buffered word-clock (LR clock) signal into pin5 and this
   code will map that sensed frequency into the nearest (if in tolerance) SPDIF sample rate value.

   (c) 2015-2017 Bryan Levin (Sercona Audio)
*/

// system includes
#include "Arduino.h"

#include <Wire.h>
#include <EEPROM.h>


// system-level add-ons
#include <U8glib.h>


// build-time options
//#define USE_INPUT_SELECT
//#define VERBOSE


// local includes
#include "oled_bitmaps.h"
#include "FreqCounter.h"



/*
   arduino pin defs
*/


// this allows serial uart sharing.  ftdi (etc) is via 1k resistors to the arduino chip tx,rx
//   and the 74125 buffers the xbee and disconnects it from the arduino tx,rx lines during
//   software upload.  when the app runs on the arduino, it re-enables the tx,rx lines on the 74125
//   and the pc is expected to be quiet and the xbee will have full i/o priority on the physical uart.
#define PIN_74125_ENA            4
#define PIN_freq_counter         5   // frequency counter feature (DO NOT CHANGE THIS PIN!)
#define PIN_button_press        11  // used as digital-in to detect the single front panel button
#define PIN_binary_select_out   12
#define PIN_LED13               13
#define PIN_I2C_RESERVED1      (A4)
#define PIN_I2C_RESERVED2      (A5)



// EEPROM LOCATIONS

#define NV_MAGIC_addr      0x00
#define NV_MAGIC_val       0x42

#define NV_INPUT_SEL_addr  0x01
#define NV_INPUT_SEL_coax  'C'
#define NV_INPUT_SEL_opto  'O'
#define NV_INPUT_SEL_usb   'U'

#define NV_BRIGHTNESS_addr 0x02
#define NV_BRIGHTNESS_default 3



/*
   globals
*/

char cur_digital_input[9];
char cur_sample_rate_str[16];
char str_buf[64];

// create an instance of the OLED display class
U8GLIB_SH1106_128X64 u8g(U8G_I2C_OPT_NO_ACK);	// Display which does not send ACK
extern void oled_draw_logo_loop (void);




char current_input_port;
char current_input_freq;

int key;
int last_key_state;
int led_brightness;
boolean display_current_on;

// Frequency input is digital pin 5
float F0 = 50329;  // (1/(2*pi*sqrt(LC)))
unsigned long frq, last_frq;


#define SERIAL_UART_BUF_LEN 80
char serial_uart_buffer[SERIAL_UART_BUF_LEN];
byte uart_buffer_idx;
boolean serial_line_ready;




// forwards
void displayFreq(long fin);
void restart_freq_counter(void);
void oled_clear_screen(void);




// set default values
void
init_eeprom (void)
{
  EEPROM.write(NV_MAGIC_addr, NV_MAGIC_val);
  EEPROM.write(NV_INPUT_SEL_addr, NV_INPUT_SEL_usb);  // 'U'
  EEPROM.write(NV_BRIGHTNESS_addr, NV_BRIGHTNESS_default);

  strcpy(cur_digital_input, "USB");
  current_input_port = NV_INPUT_SEL_usb;

  return;
}


// usb input
void
draw_usb (void)
{
  if (display_current_on != false) {
  }
  return;
}

// coax input
void
draw_coax (void)
{
  if (display_current_on != false) {
  }
  return;
}

// opto input
void
draw_opto (void)
{
  if (display_current_on != false) {
  }
  return;
}


void
redraw_input_port_name (void)
{
  if (current_input_port == NV_INPUT_SEL_coax) {
    draw_coax();
    
  } else if (current_input_port == NV_INPUT_SEL_opto) {
    draw_opto();
    
  } else if (current_input_port == NV_INPUT_SEL_usb) {
    draw_usb();
  }

  return;
}


void
change_input_to_coax (void)
{
  digitalWrite(PIN_binary_select_out, 0);
  EEPROM.write(NV_INPUT_SEL_addr, NV_INPUT_SEL_coax);

  // avoid redrawing the same string (causes unnecessary flicker)
  if (current_input_port != NV_INPUT_SEL_coax) {
    draw_coax();
  }

  current_input_port = NV_INPUT_SEL_coax;
  strcpy(cur_digital_input, "Coax");

  return;
}


void
change_input_to_opto (void)
{
  digitalWrite(PIN_binary_select_out, 1);
  EEPROM.write(NV_INPUT_SEL_addr, NV_INPUT_SEL_opto);

  // avoid redrawing the same string (causes unnecessary flicker)
  if (current_input_port != NV_INPUT_SEL_opto) {
    draw_opto();
  }

  current_input_port = NV_INPUT_SEL_opto;
  strcpy(cur_digital_input, "Opto");

  return;
}


void
change_input_to_usb (void)
{
  digitalWrite(PIN_binary_select_out, 1);
  EEPROM.write(NV_INPUT_SEL_addr, NV_INPUT_SEL_usb);

  // avoid redrawing the same string (causes unnecessary flicker)
  if (current_input_port != NV_INPUT_SEL_usb) {
    draw_usb();
  }

  current_input_port = NV_INPUT_SEL_usb;
  strcpy(cur_digital_input, "USB");

  return;
}


void
read_eeprom (void)
{
  char temp_val;

  // how bright?
  led_brightness = EEPROM.read(NV_BRIGHTNESS_addr);

  // this is done only at eeprom_read time to *force* the routines 'change_input_to_foo' to do a screen update
  current_input_port = 0;

  // which input?
  temp_val = EEPROM.read(NV_INPUT_SEL_addr);
  if (temp_val == NV_INPUT_SEL_usb) {
    current_input_port = 0;  // this is done only at eeprom_read time to force the routine, below, to do a screen update
    change_input_to_usb();

  } else if (temp_val == NV_INPUT_SEL_opto) {
    change_input_to_opto();

  } else if (temp_val == NV_INPUT_SEL_coax) {
    change_input_to_coax();
  }

  return;
}


void
update_samplerate_display (unsigned long freq)
{
  if ( (freq >= 31000) && (freq <= 33000) ) {
    if (current_input_freq != 32) {
      current_input_freq = 32;
      strcpy(cur_sample_rate_str, "  32");
    }

  } else if ( (freq >= 43000) && (freq <= 45000) ) {
    if (current_input_freq != 44) {
      current_input_freq = 44;
      strcpy(cur_sample_rate_str, " 44.1");
    }

  } else if ( (freq >= 47000) && (freq <= 49000) ) {
    if (current_input_freq != 48) {
      current_input_freq = 48;
      strcpy(cur_sample_rate_str, "  48");
    }

  } else if ( (freq >= 87000) && (freq <= 89000) ) {
    if (current_input_freq != 88) {
      current_input_freq = 88;
      strcpy(cur_sample_rate_str, " 88.2");
    }

  } else if ( (freq >= 95000) && (freq <= 97000) ) {
    if (current_input_freq != 96) {
      current_input_freq = 96;
      strcpy(cur_sample_rate_str, "  96");
    }

  } else if ( (freq >= 174000) && (freq <= 178000) ) {
    if (current_input_freq != 176) {
      current_input_freq = 176;
      strcpy(cur_sample_rate_str, "176.4");
    }

  } else if ( (freq >= 189000) && (freq <= 196000) ) {
    if (current_input_freq != 192) {
      current_input_freq = 192;
      strcpy(cur_sample_rate_str, " 192");
    }
  }

  return;
}


void
restart_freq_counter (void)
{
  FreqCounter::f_comp = 106;

  // set a short gate time and compensate for it later
  FreqCounter::start(250);  // 1/4 the rate (and we will left-shift twice to mult by 2 again)

  frq = 0;
  last_frq = 0;

  return;
}


void
setup (void)
{
  Wire.begin();

  pinMode(PIN_freq_counter, INPUT);         // d5 frequency counter feature (DO NOT CHANGE THIS PIN!)
  //pinMode(PIN_button_press, INPUT_PULLUP);  // d11
  //pinMode(PIN_binary_select_out, OUTPUT);   // d12 (output of spdif 'switch')

  //pinMode(PIN_74125_ENA, OUTPUT);  // xbee select on 74125 (important, since the xbee shares the usb tx/rx lines!)
  //digitalWrite(PIN_74125_ENA, 1);  // disable the xbee (74125 tri-state buffer; gnd to enable; line normally pulled-up)
  //digitalWrite(PIN_LED13, 0);  // turn off led13
  delay(10);


  // show an opening logo and give the user time to hit/connect the demo-mode jumper
  oled_clear_screen();
  oled_draw_logo_loop();
  delay(3000);  // for the display logo loop


  // first-time thru?  init eeprom on brand new chips
  if (EEPROM.read(NV_MAGIC_addr) != NV_MAGIC_val) {
    init_eeprom();
  }

  //digitalWrite(PIN_74125_ENA, 0);  // enable the xbee (74125 tri-state buffer; gnd to enable; line normally pulled-up)


  // this is our 'cache' to save redraws of same values on the led displays
  current_input_freq = 0;


  //init freq counter api
  restart_freq_counter();
  last_frq = 0;

  //alpha4.begin(0x70);  // pass in the address for the alpha display
  //numeric4.begin(0x72);  // pass in the address for the 7-seg display
  //alpha4.clear();
  //numeric4.clear();
  //alpha4.writeDisplay();
  //numeric4.writeDisplay();

  display_current_on = true;


  // copy values into working ram
  read_eeprom();

  if (led_brightness == 0) {
    // turn off display at zero
    //alpha4.clear();
    //numeric4.clear();
    display_current_on = false;

  } else {
    //alpha4.setBrightness(led_brightness);
    //numeric4.setBrightness(led_brightness);
    display_current_on = true;
  }

  display_current_on = true;  // debug
  last_key_state = (-1);


  return;
}


void
toggle_select_button (void)
{
  if (current_input_port == NV_INPUT_SEL_usb) {
    change_input_to_opto();

  } else if (current_input_port == NV_INPUT_SEL_opto) {
    change_input_to_coax();

  } else {
    change_input_to_usb();
  }

  return;
}


volatile unsigned long junk1;  // volatile, so that the compiler does not 'optimize' my software loop ;)

void
my_delay (unsigned long dly)
{

  for (unsigned long i = 0; i < (dly * 20); i++) {
    // do nothing useful but just take up time
    junk1 = i * 3141 / 234;
  }

  return;
}


void
draw_main_page (void)
{
#ifdef USE_INPUT_SELECT
  // input selector DIGITAL icon (temporary)
  u8g.drawXBMP(2, 0,
               input_select_digital_width,
               input_select_digital_height,
               input_select_digital_icon_bits);
#endif

  // spdif samplerate, in big fonts
  u8g.setFont(u8g_font_courB18);
  u8g.setFontPosTop();

  if (current_input_freq != 0) {   // draw the number IF we have lock-on
    // sample rate (numeric, floating point)
    u8g.drawStr(15, 27, cur_sample_rate_str);
    //u8g.setFont(u8g_font_6x10);  // small font
    //u8g.drawStr(93, 44, "kHz");

  } else {
//#ifdef VERBOSE
    u8g.setFont(u8g_font_6x10);  // small font
    u8g.drawStr(31, 34, "(No Signal)");
//#endif
  }

#ifdef VERBOSE
  // divider line (we always have that)
  u8g.drawLine(2, 51, 124 /*102*/, 51);
#endif

#ifdef USE_INPUT_SELECT
  // which input is active right now?
  sprintf(str_buf, "%s", cur_digital_input);
  u8g.drawStr(43, 13, str_buf);
#endif


#ifdef VERBOSE
  // remind the user what he's looking at
  u8g.drawStr(2, 62, "SPDIF Sample Rate");  // below scribed line
#endif

  return;
}


void
oled_draw_main_page_loop (void)
{
  u8g.firstPage();

  do {
    draw_main_page();
  } while (u8g.nextPage());

  return;
}


void
oled_clear_screen (void)
{
  u8g.firstPage();

  do {
  } while (u8g.nextPage());

  return;
}


void
loop (void)
{
  /*
     check on the ready-status of the freq counter ISR
  */

  if (FreqCounter::f_ready != 0) {

    // save the measured value
    frq = FreqCounter::f_freq << 2;  // shifted left since we took only quarter of a second of sample

    /*
       did the value change since last time?
    */

    if (frq != last_frq) {   // values changed: we have to update the screen
      update_samplerate_display(frq);
      last_frq = frq;   // save for next check
    } // measured value changed


    // restart for next time
    restart_freq_counter();

  } // if freq counter had a value for us


  // update the oled display
  oled_draw_main_page_loop();


  /*
     scan front panel button to see if input should be changed
  */

#ifdef NOTUSED
// multi-read it (helps debounce)
  key = digitalRead(PIN_button_press);
  delay(5);
  key += digitalRead(PIN_button_press);
  delay(5);
  key += digitalRead(PIN_button_press);

  // key was really pressed, so change modes
  if (key == 0) {
    toggle_select_button();  // this updates the display, the relay and eeprom
    my_delay(500);
  }
#endif

} // loop


// end main


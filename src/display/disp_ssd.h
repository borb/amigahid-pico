/**
 * ssd1306 display handling
 *
 * please see disp_ssd.c for a more comprehensive readme.
 */

#ifndef _DISPLAY_DISP_SSD_H
#define _DISPLAY_DISP_SSD_H

void disp_ssd_init(void);

void (*disp_write)(uint8_t x, uint8_t y, char *message);

#endif // _DISPLAY_DISP_SSD_H

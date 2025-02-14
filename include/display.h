#ifndef DISPLAY_H
#define DISPLAY_H

#include "hardware/i2c.h"
#include "ssd1306.h"
#include "globals.h"
#include "internet.h"
#include "joystick.h"
#include "button.h"
#include "neopixel.h"

// Pinos e módulo I2C
#define I2C_PORT i2c1 // Módulo I2C
#define PINO_SCL 14 // Pino SCL
#define PINO_SDA 15 // Pino SDA

// Pinos e módulo I2C
#define I2C_PORT i2c1
#define PINO_SCL 14 
#define PINO_SDA 15 

// Configuração do display OLED
extern ssd1306_t disp; // Display OLED

void init_display();
void desenha_menu();
void print_texto(char *msg, uint pos_x, uint pos_y, uint scale);
void print_retangulo(int x1, int y1, int x2, int y2);
void print_texto_scroll(const char *msg, int offset_x, int offset_y, uint scale);
void trim(char *str);
void clear_display();
void draw_smile();
int menu_oled();

#endif // DISPLAY_H
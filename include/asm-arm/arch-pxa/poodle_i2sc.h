/*
 * linux/drivers/sound/poodle_i2sc.h
 *
 * I2C & I2S control header file
 *
 *      
 * Copyright (C) 2002  SHARP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Change Log
 *
 */
#ifndef _POODLE_I2SC_H_
#define _POODLE_I2SC_H_

#define TRUE  1
#define FALSE 0

int i2c_open(unsigned char);
int i2c_close(unsigned char);

int i2s_open(unsigned char,unsigned int);
int i2s_close(unsigned char);

int i2c_byte_write(unsigned char,unsigned char,unsigned char);

int i2c_init(void);
void i2c_suspend(void);
void i2c_resume(void);


#endif

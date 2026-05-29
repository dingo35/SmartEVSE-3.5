/*
;	 Project:       Smart EVSE
;
;
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/
#include <vector>

#ifndef __GLCD_H
#define __GLCD_H

#define GLCD_MERGE 0b00001000
#define GLCD_HIRES_FONT
#define GLCD_FULL_CHARSET
#define GLCD_ALIGN_LEFT   0
#define GLCD_ALIGN_CENTER 1
#define GLCD_ALIGN_RIGHT  2

extern void GLCDHelp(void);
extern void GLCD(void);
extern void GLCDMenu(unsigned char Buttons);
extern void GLCD_init(void);
extern bool GridRelayOpen;
// BMP rendered by createImageFromGLCDBuffer()
// 1-bit monochrome BMP, 62-byte header + pixel data.
static constexpr int      BMP_WIDTH     = 128;
static constexpr int      BMP_HEIGHT    = 64;
static constexpr uint32_t BMP_ROW_SIZE  = ((BMP_WIDTH + 31) / 32) * 4;       // rows padded to a multiple of 4 bytes
static constexpr size_t   BMP_IMAGE_SIZE = 62 + (BMP_ROW_SIZE * BMP_HEIGHT); // header + pixels
extern const uint8_t* createImageFromGLCDBuffer(size_t &outSize);

#if SMARTEVSE_VERSION >= 40
#include <SPI.h>
extern void glcd_clrln(unsigned char ln, unsigned char data);
extern SPIClass LCD_SPI2;
#endif

#endif // #ifndef __GLCD_H

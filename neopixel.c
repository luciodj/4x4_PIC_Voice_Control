/*
\file   neopixel.c

\brief  A NeoPixel RGB LED driver sharing the SPI port

(c) 2018 Microchip Technology Inc. and its subsidiaries.

Subject to your compliance with these terms, you may use Microchip software and any
derivatives exclusively with Microchip products. It is your responsibility to comply with third party
license terms applicable to your use of third party software (including open source software) that
may accompany Microchip software.

THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY
IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS
FOR A PARTICULAR PURPOSE.

IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP
HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO
THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT
OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS
SOFTWARE.
*/
#include <string.h>
#include <stdlib.h>
#include "mcc_generated_files/mcc.h"
#include "mcc_generated_files/spi2.h"
#include "neopixel.h"
#include "math.h"

#ifndef FCY
#define FCY (_XTAL_FREQ/2)
#endif


#define V       64
#define RED     V, 0, 0
#define BLUE    0, 0, V
#define GREEN   0, V, 0
#define YELLOW  V, V, 0

#define SMOOTH  5  // ms
#define ROWS    4
#define LEDxROW 4
#define STEPS   100

// NOTE: since the SPI bus is shared between the WINC and the NeoPixel driver
//       the WINC communication must be slowed down in order to avoid commands
//       being intercepted by the RGB LED panel. A 50us delay has been inserted
//       in nm_bus_wrapper_mega.c in function spi_rw()

// display matrix
uint8_t array[ROWS * LEDxROW * 3];
int16_t threshold[ROWS];

// define a 1 and 0 duration pulses
#define WS_1    0xFC // 0.8us
#define WS_0    0xF0 // 0.4us

void SendByte(uint8_t b){
    uint8_t i;
    for(i=0; i<8; i++){
        if (b & 0x80) SPI2_Exchange8bit(WS_1);
        else SPI2_Exchange8bit(WS_0);
        b <<= 1;
    }
}

void SendPixel(uint8_t r, uint8_t g, uint8_t b){
    SendByte(g);
    SendByte(r);
    SendByte(b);
}

void NEO_fill(uint8_t r, uint8_t g, uint8_t b){
    uint8_t i;
    uint8_t *pi = array;
    for(i=0; i<ROWS*LEDxROW; i++){
        *pi++ = g;
        *pi++ = r;
        *pi++ = b;
    }
}

void NEO_display(void){
    uint8_t i;
    uint8_t *pi = array;
    uint8_t  save_ipl;
    // Disable IOCI interrupt temporarily
    SET_AND_SAVE_CPU_IPL(save_ipl, 7);
    for(i=0; i<ROWS*LEDxROW; i++){
        SendByte(*pi++);
        SendByte(*pi++);
        SendByte(*pi++);
    }
    // Re-Enable IOCI interrupt
    RESTORE_CPU_IPL(0);
    DELAY_microseconds(500);
}

void NEO_migrate(uint8_t r, uint8_t g, uint8_t b){
    float fcr = array[1], fcg = array[0], fcb = array[2];
    float fr = r-fcr, fg = g-fcg, fb = b-fcb;

    uint16_t x;
    uint8_t row, p;
    // compute thresholds
    for(x=0; x<ROWS; x++){
        threshold[x] = -STEPS / ROWS * x;
    }
    // migration loop
    for(x=0; x<STEPS*2; x++){
        // update thresholds
        for(row=0; row<ROWS; row++){
            threshold[row]++;
            // update matrix by rows
            if ((threshold[row] > 0) && (threshold[row] <= STEPS)){
                // update row
                float ratio = threshold[row] * 1.0 / STEPS;
                uint8_t *pi = &array[row * LEDxROW * 3];
                for(p=0; p<LEDxROW; p++){
                    *pi++ = floor(fcg+ fg * ratio);
                    *pi++ = floor(fcr+ fr * ratio);
                    *pi++ = floor(fcb+ fb * ratio);
                }
            }
        }

        NEO_display();
        DELAY_milliseconds(SMOOTH);
    }
}

void NEO_init(void){
    SPI2_Initialize();
    NEO_migrate(GREEN);
    NEO_migrate(0,0,0);
//    NEO_fill(GREEN);
//    NEO_display();
}

// ============================================================
//  cc1101.h — minimal CC1101 driver (receive only)
// ============================================================
#pragma once
#include <Arduino.h>

#define CC_CS    12
#define CC_MISO  10
#define CC_SW1   47     // antenna switch: SW1/SW0 = 1/0 315, 1/1 433, 0/1 868/915
#define CC_SW0   48

extern uint8_t g_ccVersion;            // chip's reply (for diagnostics)

bool    ccInit();                      // reset + set up OOK receive; false = no chip
uint8_t ccStrobe(uint8_t s);           // command (SIDLE 0x36, SRX 0x34, SPWD 0x39 ...)
void    ccWrite(uint8_t a, uint8_t v);
uint8_t ccReadStatus(uint8_t a);       // status registers 0x30..0x3D
int     ccRssi();                      // current level, dBm
void    ccAntenna(float mhz);          // switch antenna for the band
void    ccSetFreq(float mhz);
void    ccSetBw(uint8_t mdmcfg4);      // RX bandwidth (F5 ≈ 58 kHz, A5 ≈ 270 kHz)
int     ccMeasure(float mhz);          // retune, enter RX, measure RSSI
void    ccSleep();

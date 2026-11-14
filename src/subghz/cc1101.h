// ============================================================
//  cc1101.h — минимальный драйвер CC1101 (только приём)
// ============================================================
#pragma once
#include <Arduino.h>

#define CC_CS    12
#define CC_MISO  10
#define CC_SW1   47     // переключатель антенны: SW1/SW0 = 1/0 315, 1/1 433, 0/1 868/915
#define CC_SW0   48

extern uint8_t g_ccVersion;            // что ответил чип (для диагностики)

bool    ccInit();                      // сброс + настройка на приём OOK; false — чипа нет
uint8_t ccStrobe(uint8_t s);           // команда (SIDLE 0x36, SRX 0x34, SPWD 0x39 ...)
void    ccWrite(uint8_t a, uint8_t v);
uint8_t ccReadStatus(uint8_t a);       // регистры состояния 0x30..0x3D
int     ccRssi();                      // текущий уровень, dBm
void    ccAntenna(float mhz);          // переключить антенну под диапазон
void    ccSetFreq(float mhz);
void    ccSetBw(uint8_t mdmcfg4);      // ширина полосы приёма (F5 ≈ 58 кГц, A5 ≈ 270 кГц)
int     ccMeasure(float mhz);          // перестроиться, войти в приём, измерить RSSI
void    ccSleep();

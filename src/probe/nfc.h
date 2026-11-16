// ============================================================
//  nfc.h — раздел NFC: «что это за карта?»
//
//  Только чтение и только то, что карта сама говорит о себе всем:
//  тип (по ATQA/SAK), начало UID, для меток NTAG/Ultralight — открытые
//  NDEF-записи (ссылка, текст). Банковскую карту узнаём одним вопросом
//  «ты платёжная?» (SELECT PPSE) и больше НИЧЕГО не читаем: ни номера,
//  ни имени, ни истории. Ничего не пишем, ничего не эмулируем.
//
//  PN532 по I2C (0x24, шина с батареей: SDA 8, SCL 18), сброс — GPIO45.
//  Включается, только пока раздел открыт или планшет подписан на "nfc".
//
//  nfc_model.cpp — классификация и разбор NDEF (чистый код, есть в симуляторе),
//  nfc.cpp        — драйвер PN532, раздел меню, JSON,
//  nfc_screens.cpp — экран.
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

#define NFC_NDEF_N   3
#define NFC_NDEF_LEN 64
#define NFC_HIST     4

struct NfcCard {
  char     fam[28];                   // «MIFARE Classic 1K», «Bank card (EMV)», ...
  char     hint[40];                  // «transit / access card - likely»
  uint8_t  uid[10];
  uint8_t  uidLen;
  uint16_t atqa;
  uint8_t  sak;
  bool     felica, bank, isoDep;
  char     ndef[NFC_NDEF_N][NFC_NDEF_LEN];   // открытые записи метки (ссылка / текст)
  uint8_t  nNdef;
  uint32_t t;                         // когда приложили (millis)
};

enum { NFC_OFF = 0, NFC_SCAN = 1, NFC_NOREADER = 2 };

struct NfcModel {
  int      state = NFC_OFF;
  uint32_t fw = 0;                    // версия прошивки PN532 (IC.Ver.Rev)
  bool     have = false, present = false;
  NfcCard  last = {};
  uint32_t lastSeen = 0;              // карта всё ещё у палки
  uint32_t taps = 0;
  uint32_t gen = 0;                   // растёт на каждое прикладывание / ошибку (для планшета)
  NfcCard  hist[NFC_HIST];
  int      histN = 0;
};
extern NfcModel g_nfc;

// тип карты по тому, что она ответила (ulVer — байт объёма из GET_VERSION или -1)
void nfcClassify(NfcCard& c, int ulVer);
// NDEF из памяти метки (страницы с 4-й): записи-строки в c.ndef
void nfcParseNdef(const uint8_t* mem, int n, NfcCard& c);
// UID для показа: первые 2 байта, остальное скрыто ("04A2••" в JSON)
void nfcUidMasked(const NfcCard& c, char* out, size_t n, bool utf8Dots);
void nfcCommit(const NfcCard& c, uint32_t now);   // новая карта -> модель

bool nfcStart();
void nfcStop();
void nfcTick(uint32_t now);
void nfcJson(String& o);              // событие SSE "nfc"

void drawNfc(uint32_t now);
void iconCard(int x, int y, uint16_t c, uint16_t bg);

extern const Section nfcSection;

// ============================================================
//  nfc.h — NFC section: "what card is this?"
//
//  Read only, and only what a card tells everyone about itself:
//  type (by ATQA/SAK), start of the UID, for NTAG/Ultralight tags the open
//  NDEF records (link, text). A bank card gets one question,
//  "are you a payment card?" (SELECT PPSE), and NOTHING else is read: no number,
//  name or history. Nothing is written, nothing emulated.
//
//  PN532 over I2C (0x24, battery bus: SDA 8, SCL 18), reset on GPIO45.
//  Enabled only while the section is open or the tablet is subscribed to "nfc".
//
//  nfc_model.cpp — classification and NDEF parsing (pure code, also in the simulator),
//  nfc.cpp        — PN532 driver, menu section, JSON,
//  nfc_screens.cpp — screen.
// ============================================================
#pragma once
#include <Arduino.h>
#include "core/section.h"

#define NFC_NDEF_N   3
#define NFC_NDEF_LEN 64
#define NFC_HIST     4

struct NfcCard {
  char     fam[48];                   // «MIFARE Classic 1K», «Bank card (EMV)», ...
  char     hint[96];                  // «transit / access card - likely»
  uint8_t  uid[10];
  uint8_t  uidLen;
  uint16_t atqa;
  uint8_t  sak;
  bool     felica, bank, isoDep;
  char     ndef[NFC_NDEF_N][NFC_NDEF_LEN];   // open tag records (link / text)
  uint8_t  nNdef;
  uint32_t t;                         // when the card was presented (millis)
};

enum { NFC_OFF = 0, NFC_SCAN = 1, NFC_NOREADER = 2 };

struct NfcModel {
  int      state = NFC_OFF;
  uint32_t fw = 0;                    // PN532 firmware version (IC.Ver.Rev)
  bool     have = false, present = false;
  NfcCard  last = {};
  uint32_t lastSeen = 0;              // card still at the stick
  uint32_t taps = 0;
  uint32_t gen = 0;                   // grows with each tap / error (for the tablet)
  NfcCard  hist[NFC_HIST];
  int      histN = 0;
};
extern NfcModel g_nfc;

// card type from its reply (ulVer = capacity byte from GET_VERSION, or -1)
void nfcClassify(NfcCard& c, int ulVer);
// NDEF from tag memory (pages from 4th): record strings go to c.ndef
void nfcParseNdef(const uint8_t* mem, int n, NfcCard& c);
// UID for display: first 2 bytes, the rest hidden ("04A2••" in JSON)
void nfcUidMasked(const NfcCard& c, char* out, size_t n, bool utf8Dots);
void nfcCommit(const NfcCard& c, uint32_t now);   // new card -> model

bool nfcStart();
void nfcStop();
void nfcTick(uint32_t now);
void nfcJson(String& o);              // SSE event "nfc"

void drawNfc(uint32_t now);
void iconCard(int x, int y, uint16_t c, uint16_t bg);

extern const Section nfcSection;

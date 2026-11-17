// ============================================================
//  serout.h — single queue for everything written to the PC over USB.
//  Lines go out whole and in order (PC-monitor "KEY ...", "AUD ..." and PC-app "@E ..."
//  never interleave); writes never block: if the PC stops reading, the queue stops growing.
// ============================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

// Queue a line (newline added). droppable: may be dropped when the queue is long
// (audio/spectrum frames). Returns false if it did not fit.
bool   serOutLine(const char* s, size_t n, bool droppable = false);
bool   serOutLine(const char* s);
// build a line from parts without extra copies: begin -> part... -> end
bool   serOutBegin(size_t total, bool droppable);
void   serOutPart(const char* s, size_t n);
void   serOutEnd();
size_t serOutQueued();                 // bytes waiting to be sent
void   serOutPump();                   // every loop(): write as much as the port accepts

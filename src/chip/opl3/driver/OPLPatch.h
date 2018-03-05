/*
 * Default patches are (C) The Fat Man
 * Original file (C) 1994 Microsoft Corporation
 * File structure (C) 2013 James Alan Nguyen
 */

#pragma once
#include <array>
#include <stdint.h>

typedef uint8_t  BYTE;
typedef uint16_t WORD;

enum {
   NUMOPS = 4,
};

#pragma pack(push)
#pragma pack(1)
struct Operator
{
   BYTE    bAt20;              /* flags which are send to 0x20 on fm */
   BYTE    bAt40;              /* flags seet to 0x40 */
   /* the note velocity & midi velocity affect total level */
   BYTE    bAt60;              /* flags sent to 0x60 */
   BYTE    bAt80;              /* flags sent to 0x80 */
   BYTE    bAtE0;              /* flags send to 0xe0 */
};

struct Patch
{
   Operator op[NUMOPS];        /* operators */
   BYTE    bAtA0[2];           /* send to 0xA0, A3 */
   BYTE    bAtB0[2];           /* send to 0xB0, B3 */
   /* use in a patch, the block should be 4 to indicate
   normal pitch, 3 => octave below, etc. */
   BYTE    bAtC0[2];           /* sent to 0xc0, C3 */
   BYTE    bOp;                /* see PATCH_??? */
   //BYTE    bDummy;             /* place holder */
   BYTE    bRhythmMap;         /* see RHY_CH_??? */
};
#pragma pack(pop)

struct MelMap
{
   BYTE bPreset;
   short wBaseTranspose, wSecondTranspose;
   short wPitchEGAmt;
   WORD wPitchEGTime;
   short wBaseFineTune, wSecondFineTune;
   BYTE bRetrigDly;
   BYTE bReservedPadding[8];
};

struct PercMap
{
   BYTE bPreset;
   BYTE bBaseNote;
   BYTE bPitchEGAmt;
};

extern const std::array<MelMap, 128> gbDefaultMelMap;
extern const std::array<PercMap, 128> gbDefaultPercMap;
extern const std::array<Patch, 256> glpDefaultPatch;

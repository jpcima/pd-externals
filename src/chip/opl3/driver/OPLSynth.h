// ==============================================================================
//
// Copyright (c) 1996-2000 Microsoft Corporation.  All rights reserved.
//
// Extensions (C) 2013 James Alan Nguyen
// http://www.codingchords.com
//
// Puredata interface (C) 2018 Jean Pierre Cimalando
//
// ==============================================================================

#pragma once
#include "OPLPatch.h"
#include "../nukedopl/opl3.h"
#include <m_pd.h>
#include <vector>
#include <stdint.h>

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD, ULONG;
typedef int32_t  LONG;

enum {
   NUMPATCHES  = 256,
   DRUMCHANNEL = 9, //midi channel 10
};

static inline DWORD AsULMUL(DWORD a, DWORD b) { return a * b; }
static inline DWORD AsLSHL(DWORD a, DWORD b) { return a << b; }
static inline DWORD AsULSHR(DWORD a, DWORD b) { return a >> b; }

// Linear interpolation equation
static inline double lin_intp(double x, double xmin, double xmax, double ymin, double ymax)
   { return ymin+(ymax-ymin)*(x-xmin)/(xmax-xmin); }

// indexed FM registers
enum {
   AD_LSI        = (0x000),
   AD_LSI2       = (0x101),
   AD_TIMER1     = (0x001),
   AD_TIMER2     = (0x002),
   AD_MASK       = (0x004),
   AD_CONNECTION = (0x104),
   AD_NEW        = (0x105),
   AD_NTS        = (0x008),
   AD_MULT       = (0x020),
   AD_MULT2      = (0x120),
   AD_LEVEL      = (0x040),
   AD_LEVEL2     = (0x140),
   AD_AD         = (0x060),
   AD_AD2        = (0x160),
   AD_SR         = (0x080),
   AD_SR2        = (0x180),
   AD_FNUMBER    = (0x0a0),
   AD_FNUMBER2   = (0x1a0),
   AD_BLOCK      = (0x0b0),
   AD_BLOCK2     = (0x1b0),
   AD_DRUM       = (0x0bd),
   AD_FEEDBACK   = (0x0c0),
   AD_FEEDBACK2  = (0x1c0),
   AD_WAVE       = (0x0e0),
   AD_WAVE2      = (0x1e0),
};

/* transformation of linear velocity value to
logarithmic attenuation */

/* typedefs for MIDI patches */
enum {
   PATCH_1_4OP = 0,        /* use 4-operator patch */
   PATCH_2_2OP = 1,        /* use two 2-operator patches */
   PATCH_1_2OP = 2,        /* use one 2-operator patch */
};

enum {
   NUM2VOICES = 18,
   NUM4VOICES = 6, //9
   NUMMIDICHN = 16,
};

/* MIDI */

struct Voice
{
   BYTE    bVoiceID;               /* used to identify note allocations */
   BYTE    bNote;                  /* note played */
   BYTE    bChannel;               /* channel played on */
   BYTE    bPatch;                 /* what patch is the note,
                                   drums patch = drum note + 128 */
   BYTE    bOn;                    /* TRUE if note is on, FALSE if off */
   BYTE    bVelocity;              /* velocity */
   DWORD   dwTime;                 /* time that was turned on/off;
                                   0 time indicates that its not in use */
   //DWORD   dwOrigPitch[2];         /* original pitch, for pitch bend */
   BYTE    bBlock[2];              /* value sent to the block */
   BYTE    bSusHeld;               /* turned off, but held on by sustain */

   // for EG/LFO
   DWORD  dwStartTime;
   long   dwLFOVal;
   long   dwDetuneEG;

   // for portamento
   BYTE   bPrevNote;
   DWORD dwPortaSampTime;
   //BYTE   bPortaSampCnt;
   DWORD dwPortaSampCnt;

   short wCoarseTune;
   short wFineTune;
   //double dfPortaRatio;
};


/* a bit of tuning information */
enum {
   FSAMP = 49716 // (3579545.0 / 72.0) /* sampling frequency */
};

/* operator offset location */
static constexpr WORD gw2OpOffset[ NUM2VOICES ][ 2 ] =
{
   { 0x000,0x003 },
   { 0x001,0x004 },
   { 0x002,0x005 },
   { 0x008,0x00b },
   { 0x009,0x00c },
   { 0x00a,0x00d },
   { 0x010,0x013 },
   { 0x011,0x014 },
   { 0x012,0x015 },

   { 0x100,0x103 },
   { 0x101,0x104 },
   { 0x102,0x105 },
   { 0x108,0x10b },
   { 0x109,0x10c },
   { 0x10a,0x10d },
   { 0x110,0x113 },
   { 0x111,0x114 },
   { 0x112,0x115 },
};

static constexpr BYTE gb4OpVoices[] =
{
   0, 1, 2, 9, 10, 11
};

// Rhythm mode channels
// note: any other value is assumed a normal patch.
enum {
   RHY_CH_BD  = 0x06,
   RHY_CH_SD  = 0x07,
   RHY_CH_TOM = 0x08,
   RHY_CH_HH  = 0x09, //0x07
   RHY_CH_CY  = 0x0A, //0x08
};

static constexpr BYTE gbRhyOpMask[] =
{
   0x03, // BD ch7
   0x02, // SD ch8 op0
   0x01, // TOM ch8 op1
   0x01, // HH
   0x02  // CY
};

// MIDI mode constants
enum {
   MIDIMODE_GM1 = 0x01,
   MIDIMODE_GM2 = 0x02,
   MIDIMODE_GS  = 0x03,
   MIDIMODE_XG  = 0x04,
};

static constexpr BYTE gbMaliceXIdentifier[8] =
   {'M','a','l','i','c','e','X',' '};

static constexpr BYTE gbVelocityAtten[64] =
{
   40, 37, 35, 33, 31, 29, 27, 25, 24, 22, 21, 20, 19, 18, 17, 16,
   16, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9,  9,  8,  8,
   7,  7,  6,  6,  6,  5,  5,  5,  4,  4,  4,  4,  3,  3,  3,  3,
   2,  2,  2,  2,  2,  1,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0
};

static constexpr BYTE offsetSlot[] =
{
   0,  1,  2,  3,  4,  5,  8,  9,  10,
   11, 12, 13, 16, 17, 18, 19, 20, 21
};

class OPLSynth
{
private:
   opl3_chip m_Miniport{};

   // midi stuff
   Voice   m_Voice[NUM2VOICES]; /* info on what voice is where */
   DWORD   m_dwCurTime;        /* for note on/off */
   DWORD   m_dwCurSample;      /* for software eg/lfo generators */
   /* volume */
   WORD    m_wSynthAttenL;     /* in 1.5dB steps */
   WORD    m_wSynthAttenR;     /* in 1.5dB steps */
   char    m_bMasterCoarseTune;      /* TODO Master Tuning level*/
   double  m_dwMasterTune;     /* Master Tune */

   std::vector<BYTE> m_noteHistory[NUMMIDICHN];
   std::vector<BYTE> m_sostenutoBuffer[NUMMIDICHN];
   BYTE    m_bRPNCount[NUMMIDICHN];
   BYTE    m_bNRPNCount[NUMMIDICHN];

   /* support for volume property */
   LONG    m_MinVolValue;      // Minimum value for volume controller
   LONG    m_MaxVolValue;      // Maximum value for volume controller
   ULONG   m_VolStepDelta;     // Correlation between controller and actual decibels
   LONG    m_SavedVolValue[2]; // Saved value for volume controller

   /* channel volumes */
   BYTE    m_bChanAtten[NUMMIDICHN];       /* attenuation of each channel, in .75 db steps */
   BYTE    m_bStereoMask[NUMMIDICHN];      /* mask for left/right for stereo midi files */

   //short   m_iBend[NUMMIDICHN];     /* bend for each channel */
   long    m_iBend[NUMMIDICHN];       /* bend for each channel */
   BYTE    m_iExpThres[NUMMIDICHN];   /* 0 to 127 expression value */
   BYTE    m_curVol[NUMMIDICHN];      /* Volume control */
   BYTE    m_RPN[NUMMIDICHN][2];      /* RPN WORD */
   BYTE    m_NRPN[NUMMIDICHN][2];     /* NRPN WORD */
   BYTE    m_bDataEnt[NUMMIDICHN][2]; /* Data Entry MSB/LSB */
   BYTE    m_iBendRange[NUMMIDICHN];  /* Bend range as dictated by CC100=0, CC101=0, CC6=n, where n= +/- range of semitones */
   BYTE    m_bModWheel[NUMMIDICHN];   /* Modulation wheel setting */
   BYTE    m_bCoarseTune[NUMMIDICHN]; /* -64 to +63 semitones coarse tuning */
   BYTE    m_bFineTune[NUMMIDICHN];   /* -1 to +1 semitones tuning */

   WORD    m_wMonoMode;                  /* Flag for bend mode, bitmasked (LSB=ch1) */
   WORD    m_wDrumMode;                  /* Flag for drum mode, bitmasked (LSB=ch1) */
   WORD    m_wPortaMode;                 /* Flag for bporta mode, bitmasked (LSB=ch1) */
   BYTE    m_bPortaTime[NUMMIDICHN];     /* Number of getsample requests before reaching note */
   BYTE    m_bLastVoiceUsed[NUMMIDICHN]; /* Needed for legato in mono mode */
   BYTE    m_bLastNoteUsed[NUMMIDICHN];  /* Needed for portamento */

   BYTE    m_bBankSelect[NUMMIDICHN][2]; /* Currently set bank selects */
   BYTE    m_bPatch[NUMMIDICHN];         /* patch number mapped to */
   BYTE    m_bSustain[NUMMIDICHN];       /* Is sustain in effect on this channel? */
   BYTE    m_bSostenuto[NUMMIDICHN];     /* Sostenuto pedal depressed */

   /*ADSR modifiers*/
   BYTE    m_bAttack[NUMMIDICHN];      /*Scaled to modify carrier instrument AR*/
   BYTE    m_bDecay[NUMMIDICHN];       /*Scaled to modify carrier instrument DR*/
   BYTE    m_bRelease[NUMMIDICHN];     /*Scaled to modify carrier instrument RR*/
   BYTE    m_bBrightness[NUMMIDICHN];  /*Scaled to modify modulator instrument TL*/

   BYTE    b4OpVoiceSet;               /*Bitvector to indicate bits 0-6 as channel flags for 4-op voice mode.*/
   BYTE    m_MIDIMode;                 /*System Exclusive MIDI command mode (TODO: dynamically set defaults)*/
   BYTE    m_bSysexDeviceId;           /*Device ID*/

   /*bank defaults*/
   std::array<MelMap, 128> gbMelMap = gbDefaultMelMap;
   std::array<PercMap, 128> gbPercMap = gbDefaultPercMap;
   std::array<Patch, 256> glpPatch = glpDefaultPatch;

   void Opl3_ChannelVolume(BYTE bChannel, WORD wAtten);
   void Opl3_SetPan(BYTE bChannel, BYTE bPan);
   void Opl3_PitchBend(BYTE bChannel, long iBend);
   void Opl3_NoteOn(BYTE bPatch,BYTE bNote, BYTE bChannel, BYTE bVelocity,long iBend);
   void Opl3_NoteOff(BYTE bPatch,BYTE bNote, BYTE bChannel, BYTE bSustain);
   void Opl3_UpdateBankSelect(BYTE bSigBit, BYTE bChannel, BYTE val);
   WORD Opl3_FindSecondVoice(BYTE bFirstVoice, BYTE bVoiceID);
   void Opl3_AllNotesOff(void);
   void Opl3_ChannelNotesOff(BYTE bChannel);
   WORD Opl3_FindFullSlot(BYTE bNote, BYTE bChannel);
   //WORD Opl3_CalcFAndB (DWORD dwPitch);
   WORD Opl3_MIDINote2FNum(double note, BYTE bChannel, long dwLFOVal);
   void Opl3_ProcessDataEntry(BYTE bChannel);
   void Opl3_Set4OpFlag(BYTE bVoice, bool bSetFlag, BYTE bOp);
   //DWORD Opl3_CalcBend (DWORD dwOrig, short iBend);
   BYTE Opl3_CalcVolume (BYTE bOrigAtten, BYTE bChannel,BYTE bVelocity, BYTE bOper, BYTE bMode);
   BYTE Opl3_CalcStereoMask (BYTE bChannel);
   WORD Opl3_FindEmptySlot(BYTE bPatch, BYTE bChannel);
   WORD Opl3_FindEmptySlot4Op(BYTE bPatch, BYTE bChannel);
   void Opl3_SetVolume(BYTE bChannel);
   void Opl3_FMNote(WORD wNote, Patch *lpSN, BYTE bChannel, WORD wNote2);
   void Opl3_SetSustain(BYTE bChannel, BYTE bSusLevel);
   void Opl3_CutVoice(BYTE bVoice, BYTE bIsInstantCut);
   void Opl3_SetPortamento(BYTE bChannel, BYTE bPortaTime);
   void Opl3_CalcPatchModifiers(Patch *lpSN, BYTE bChannel);
   void Opl3_BoardReset(void);
   bool Opl3_IsPatchEmpty(BYTE bPatch);
   void Opl3_LFOUpdate(BYTE bVoice);
   void ProcessGSSysEx(const Bit8u *bufpos, DWORD len);
   void ProcessXGSysEx(const Bit8u *bufpos, DWORD len);
   void ProcessMaliceXSysEx(const Bit8u *bufpos, DWORD len);
   Patch& Opl3_GetPatch(BYTE bBankMSB, BYTE bBankLSB, BYTE bPatch);

public:
   void Opl3_SoftCommandReset(void);
   void WriteMidiData(DWORD dwData);
   bool Init(unsigned rate);
   void GetSample(t_float *left, t_float *right, unsigned len);
   void PlaySysex(const Bit8u *bufpos, DWORD len);
   inline void Opl3_ChipWrite(WORD idx, BYTE val);
   ~OPLSynth() { close(); }
   void close();
};

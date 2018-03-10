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

#include "OPLSynth.h"
#include <jsl/byte_order>
#include <algorithm>
#include <string.h>
#include <math.h>

void
   OPLSynth::
   WriteMidiData(DWORD dwData)
{
   BYTE    bMsgType,bChannel, bVelocity, bNote;
   DWORD   dwTemp;

   bMsgType = (BYTE) dwData & (BYTE)0xf0;
   bChannel = (BYTE) dwData & (BYTE)0x0f;
   bNote = (BYTE) ((WORD) dwData >> 8) & (BYTE)0x7f;
   bVelocity = (BYTE) (dwData >> 16) & (BYTE)0x7f;

   switch (bMsgType)
   {
   case 0x90:      /* turn key on, or key off if VELOCITY == 0 */
      if (bVelocity)
      {
         //if (bChannel == DRUMCHANNEL) // TODO: change to dynamically assignable drum channels
         if ((m_wDrumMode & (1<<bChannel)) || bChannel == DRUMCHANNEL)
         {
            //if(bNote>=35 && bNote<88)
            {
               //Opl3_NoteOn((BYTE)(gbPercMap[bNote - 35].bPreset+35+128),gbPercMap[bNote - 35].bBaseNote,bChannel,bVelocity,m_iBend[bChannel]);
               Opl3_NoteOn((BYTE)(gbPercMap[bNote].bPreset+128),gbPercMap[bNote].bBaseNote,bChannel,bVelocity,m_iBend[bChannel]);
            }
         }
         else
         {
            Opl3_NoteOn((BYTE)m_bPatch[bChannel],bNote,bChannel,bVelocity,m_iBend[bChannel]);
         }
         break;
      } // if bVelocity.
      //NOTE: no break specified here. On an else case we want to continue through and turn key off

   case 0x80:
      /* turn key off */
      //  we don't care what the velocity is on note off
      //if (bChannel == DRUMCHANNEL)
      if ((m_wDrumMode & (1<<bChannel)) || bChannel == DRUMCHANNEL)
      {
         //if(bNote>=35 && bNote<88)
         {
            //Opl3_NoteOff((BYTE)(gbPercMap[bNote - 35].bPreset+35+128),gbPercMap[bNote - 35].bBaseNote, bChannel, 0);
            Opl3_NoteOff((BYTE)(gbPercMap[bNote].bPreset+128),gbPercMap[bNote].bBaseNote, bChannel, 0);
         }
      }
      else
      {
         Opl3_NoteOff ((BYTE) m_bPatch[bChannel],bNote, bChannel, m_bSustain[ bChannel ]);
      }
      break;

   case 0xb0:
      /* change control */
      switch (bNote)
      {

      case 0: // Bank Select MSB
         Opl3_UpdateBankSelect(1, bChannel, bVelocity);
         break;

      case 1: // Modulation Wheel
         m_bModWheel[bChannel] = bVelocity;
         break;

      case 5: // Portamento Time
         Opl3_SetPortamento(bChannel, bVelocity);
         break;

      case 6: // Data Entry (CC98-101 dependent)
         m_bDataEnt[bChannel][1] = bVelocity;
         Opl3_ProcessDataEntry(bChannel);
         break;

      case 7:
         /* change channel volume */
         //Opl3_ChannelVolume(bChannel,gbVelocityAtten[bVelocity >> 1]);
         m_curVol[bChannel] = bVelocity;
         Opl3_ChannelVolume(bChannel,bVelocity);
         break;

      case 8:
         break;

      case 10:
         /* change the pan level */
         Opl3_SetPan(bChannel, bVelocity);
         break;

      case 11:
         m_iExpThres[bChannel] = bVelocity;
         /* set expression threshold. should influence bChannel.gbVelocityAtten[curVol>>1] range */
         Opl3_ChannelVolume(bChannel,m_curVol[bChannel]);
         break;

      case 32:  // Bank Select LSB
         Opl3_UpdateBankSelect(0, bChannel, bVelocity);
         break;

      case 38:  // Data Entry LSB
         // TODO handle for MidiPlay-compatible register invocation
         m_bDataEnt[bChannel][0] = bVelocity;
         Opl3_ProcessDataEntry(bChannel);
         break;

      case 64:  // Sustain Pedal
         /* Change the sustain level */
         Opl3_SetSustain(bChannel, bVelocity);
         break;

      case 65:  // Portamento Pedal
         m_wPortaMode = (bVelocity > 0x3F) ?
            m_wPortaMode | (1<<bChannel) :
            m_wPortaMode & ~(1<<bChannel) ;

         // TODO: check if porta pedal off updates currently gliding notes?

         break;

      case 66:  // Sostenuto PEdal
         m_bSostenuto[bChannel] = bVelocity;

         if (bVelocity > 0x3F && m_sostenutoBuffer[bChannel].size() == 0)
            m_sostenutoBuffer[bChannel] = m_noteHistory[bChannel]; // memowise copy of held notes at this time instance
         else
         {
            while (m_sostenutoBuffer[bChannel].size() > 0)
            {
               BYTE bSosNote = *m_sostenutoBuffer[bChannel].rbegin();
               m_sostenutoBuffer[bChannel].pop_back();
               Opl3_NoteOff(m_bPatch[bChannel], bSosNote, bChannel, m_bSustain[bChannel]);
            }
         }
         break;

      case 71:  // "Harmonic content"
         // TODO: Modify op0 FB parameter.
         break;

      case 72:  // Release
         // TODO: read adjustment depending on algorithm
         m_bRelease[bChannel] = bVelocity;
         for (int i = 0; i < NUM2VOICES; ++i)
         {
            if (m_Voice[i].bChannel == bChannel)
            {
               char bOffset = (char)lin_intp(m_bRelease[bChannel], 0, 127, (-16), 16);

               if (!bOffset)
                  continue;

               for (int j = 0; j < 2; ++j)
               {
                  WORD wOffset = gw2OpOffset[ i ][ j ] ;
                  BYTE bInst = glpPatch[m_Voice[i].bPatch].op[j].bAt80;
                  char bTemp = bInst & 0xF;

                  if (glpPatch[m_Voice[i].bPatch].bAtC0[0] & 0x01)
                     continue;

                  bInst &= ~0xF;
                  bTemp -= bOffset;
                  bInst |= (bTemp > 0xF) ? 0xF : (bTemp >= 0) ? bTemp : 0;
                  Opl3_ChipWrite( 0x80 + wOffset, bInst);
               }
            }
         }
         break;

      case 73:  // Attack
         // TODO: read adjustment depending on algorithm
         m_bAttack[bChannel] = bVelocity;
         for (int i = 0; i < NUM2VOICES; ++i)
         {
            if (m_Voice[i].bChannel == bChannel)
            {
               char bOffset = (char)lin_intp(m_bAttack[bChannel], 0, 127, (-16), 16);
               WORD wOffset;
               BYTE bInst = glpPatch[m_Voice[i].bPatch].op[1].bAt60;
               char bTemp = ((bInst & 0xF0)>>4);

               if (!bOffset)
                  continue;

               for (int j = 0; j < 2; ++j)
               {
                  if (j == 0 && !((bInst & 0xF0)>>4))
                     continue;

                  wOffset = gw2OpOffset[ i ][ j ] ;
                  bInst = glpPatch[m_Voice[i].bPatch].op[j].bAt60;
                  bTemp = ((bInst & 0xF0)>>4);

                  bInst &= ~0xF0;
                  bTemp -= bOffset;
                  bInst |= (bTemp > 0xF) ? 0xF : (bTemp >= 0) ? (bTemp<<4) : 0;
                  Opl3_ChipWrite( 0x60 + wOffset, bInst) ;
               }
            }
         }
         break;

      case 74:  // "brightness"
         // TODO: read adjustment depending on algorithm (?)
#ifdef USE_EXPERIMENTAL
         m_bBrightness[bChannel] = bVelocity;
         for (int i = 0; i < NUM2VOICES; ++i)
         {
            if (m_Voice[i].bChannel == bChannel)
            {
               WORD wOffset = gw2OpOffset[ i ][ 0 ] ;
               BYTE bInst = glpPatch[m_Voice[i].bPatch].op[0].bAt40;
               char bTemp = bInst & 0x3F;
               char bOffset = (char)lin_intp(m_bBrightness[bChannel], 0, 127, (-32), 32);
               bInst &= ~0x3F;
               bTemp -= bOffset;
               bInst |= (bTemp > 0x3F) ? 0x3F : (bTemp >= 0) ? bTemp : 0;
               Opl3_ChipWrite( 0x40 + wOffset, bInst) ;
            }
         }
#endif // USE_EXPERIMENTAL

         break;

      case 75:  // Decay
         // TODO: modify DR or SR setting on carrier
         break;

      case 98:  // NRPN LSB
      case 99:  // NRPN MSB
         m_NRPN[bChannel][(bNote & 0x01)] = bVelocity;
         m_bRPNCount[bChannel] = 0;
         ++m_bNRPNCount[bChannel];
         break;

      case 100: // RPN LSB
      case 101: // RPN MSB
         m_RPN[bChannel][(bNote & 0x01)] = bVelocity;
         ++m_bRPNCount[bChannel];
         break;

      case 121: // Reset all controllers
         Opl3_ChannelNotesOff(bChannel);
         for (int i = 0; i < NUMMIDICHN; ++i)
         {
            m_iExpThres[i] = 127;
            m_iBend[i] = 0;
            m_bSustain[i] = 0;
            m_bRPNCount[i] = 0;
            m_bModWheel[i] = 0;
            m_bStereoMask[i] = 0xFF;
         }
         //Opl3_SoftCommandReset(); // TODO do not do full reset
         break;

      case 126: // Mono mode on
         Opl3_ChannelNotesOff(bChannel);
         m_wMonoMode |= (1<<bChannel);  // latch bit flag for channel mono mode
         m_bLastVoiceUsed[bChannel] = bChannel; // Assign to midi-voice channel 1:1; last two channels only used if overflow for poly mode.
         break;

      case 127: // Poly mode on
         Opl3_ChannelNotesOff(bChannel);
         m_wMonoMode &= ~(1<<bChannel);  // unset bit flag for channel mono mode
         break;

      default:
         if (bNote >= 120)        /* Channel mode messages */
         {
            Opl3_ChannelNotesOff(bChannel);
         }
         //  else unknown controller
      };
      break;

   case 0xc0: // Program change
      //if (bChannel != DRUMCHANNEL)
      if (!(m_wDrumMode & (1<<bChannel)))
      {
         m_bPatch[ bChannel ] = bNote ;
      }
      break;

   case 0xe0:  // pitch bend
   {
      LONG iBend;
      dwTemp = ((WORD)bNote << 0) | ((WORD)bVelocity << 7);
      iBend = (LONG)dwTemp - 0x2000;
      Opl3_PitchBend(bChannel, iBend);
      break;
   }
   };
   return;
}

// ========================= opl3 specific methods ============================

// ==========================================================================
// Opl3_AllNotesOff - turn off all notes
// ==========================================================================
void
   OPLSynth::
   Opl3_AllNotesOff()
{
   BYTE i;

   for (i = 0; i < NUM2VOICES; i++)
   {
      if (m_Voice[i].bSusHeld)
         Opl3_CutVoice(i, false);
      else
         Opl3_NoteOff(m_Voice[i].bPatch, m_Voice[i].bNote, m_Voice[i].bChannel, 0);
  }
}

// ==========================================================================
//  void Opl3_NoteOff
//
//  Description:
//     This turns off a note, including drums with a patch
//     # of the drum note + 128, but the first drum instrument is at MIDI note _35_.
//
//  Parameters:
//     BYTE bPatch
//        MIDI patch
//
//     BYTE bNote
//        MIDI note
//
//     BYTE bChannel
//        MIDI channel
//
//  Return Value:
//     Nothing.
//
//
// ==========================================================================
void
   OPLSynth::
   Opl3_NoteOff(BYTE bPatch, BYTE bNote, BYTE bChannel, BYTE bSustain)
{
   Patch        *lpPS = &glpPatch[bPatch] ;
   WORD         wTemp, wTemp2 ;
   BYTE         b4Op = (BYTE)(lpPS->bOp != PATCH_1_2OP);

   const std::vector<BYTE>::iterator
      &sosBegin = m_sostenutoBuffer[bChannel].begin(),
      &sosEnd = m_sostenutoBuffer[bChannel].end(),
      &it1 = std::find(sosBegin, sosEnd, bNote);

   // If exist on sostenuto list, do not do anything
   if (it1 != m_sostenutoBuffer[bChannel].end()/* && m_bSostenuto[bChannel] > 0x3F*/)
      return;

   // Find the note slot
   wTemp = Opl3_FindFullSlot( bNote, bChannel ) ;

   // Remove note instance irrespective of any status
   const std::vector<BYTE>::iterator &it2 = std::find(
      m_noteHistory[bChannel].begin(),
      m_noteHistory[bChannel].end(),
      bNote);
   if (it2 != m_noteHistory[bChannel].end() && (*it2) == bNote)
      m_noteHistory[bChannel].erase(it2);

   if (wTemp != 0xffff)
   {
      // Set voice to sustain unless mono-legato and more than one note is still being held
      if (bSustain && (!(m_wMonoMode & (1<<bChannel)) || !m_noteHistory[bChannel].size()))
      {
         // This channel is sustained, don't really turn the note off,
         // just flag it.
         m_Voice[wTemp].bSusHeld = 1;
         if (b4Op)
         {
            wTemp2 = (lpPS->bOp == PATCH_2_2OP) ?
               Opl3_FindSecondVoice((BYTE)wTemp, m_Voice[wTemp].bVoiceID) :
               wTemp + 3;
            wTemp2 = (wTemp2 < NUM2VOICES) ? wTemp2 : (WORD)~0;

            if (wTemp2 != (WORD)~0)
               m_Voice[wTemp2].bSusHeld = 1;
         }
         return;
      }

      // if 2x2op / 4-op patch
      switch(lpPS->bOp)
      {
         case PATCH_1_4OP: // note cut on second voice is not necessary but need to be verified.
            wTemp2 = wTemp+3;
            wTemp2 = (wTemp2 < NUM2VOICES) ? wTemp2 : (WORD)~0;

            if ((m_wMonoMode & (1<<bChannel)) > 0 &&
                m_noteHistory[bChannel].size() > 0)
            {
               int prevNote = (*m_noteHistory[bChannel].rbegin());
               m_noteHistory[bChannel].pop_back();
               if (!(m_wDrumMode & (1<<bChannel)) && bChannel != DRUMCHANNEL)
                  Opl3_NoteOn(bPatch, prevNote, bChannel, m_Voice[wTemp].bVelocity, m_iBend[bChannel]);
            }
            else
            {
               Opl3_CutVoice((BYTE)wTemp, false);

               if (wTemp2 != (WORD)~0)
               {
                  /*if ((lpPS->note.bAtC0[1] & 0x1))
                     Opl3_CutVoice((BYTE)wTemp2, false);
                  else*/
                  {
                     m_Voice[wTemp2].bOn = false;
                     m_Voice[wTemp2].bSusHeld = false;
                     m_Voice[wTemp2].dwTime = m_dwCurTime;
                  }
               }
            }
            break;

         case PATCH_2_2OP:
            // obtain voice with identical binding ID
            wTemp2 = Opl3_FindSecondVoice((BYTE)wTemp, m_Voice[wTemp].bVoiceID);

            if ((m_wMonoMode & (1<<bChannel)) > 0 &&
                m_noteHistory[bChannel].size() > 0)
            {
               int prevNote = (*m_noteHistory[bChannel].rbegin());
               m_noteHistory[bChannel].pop_back();
               if (!(m_wDrumMode & (1<<bChannel)) && bChannel != DRUMCHANNEL)
                  Opl3_NoteOn(bPatch, prevNote, bChannel, m_Voice[wTemp].bVelocity, m_iBend[bChannel]);
               break;
            }
            else
            {
               Opl3_CutVoice((BYTE)wTemp, false);

               if (wTemp2 != (WORD)~0)
               {
                  Opl3_CutVoice((BYTE)wTemp2, false);
                  break;
               }
            }
         // fall through

         case PATCH_1_2OP:
            // shut off the note portion
            // we have the note slot, turn it off.
            if ((m_wMonoMode & (1<<bChannel)) > 0 &&
                m_noteHistory[bChannel].size() > 0)
            {
               int prevNote = (*m_noteHistory[bChannel].rbegin());
               m_noteHistory[bChannel].pop_back();
               if (!(m_wDrumMode & (1<<bChannel)) && bChannel != DRUMCHANNEL)
                  Opl3_NoteOn(bPatch, prevNote, bChannel, m_Voice[wTemp].bVelocity, m_iBend[bChannel]);
            }
            else
               Opl3_CutVoice((BYTE)wTemp, false);
            break;
      }

   }
}


void
   OPLSynth::
   Opl3_UpdateBankSelect(BYTE bSigBit, BYTE bChannel, BYTE val)
{
   m_bBankSelect[bChannel][bSigBit] = val;

   // Update drum mode setting
   // TODO: set conditional to disable this feature
   m_wDrumMode = (m_bBankSelect[bChannel][0] == 0 && m_bBankSelect[bChannel][1] == 0x7F) ?
      m_wDrumMode | (1<<bChannel) :
      m_wDrumMode &~(1<<bChannel) ;
}

// ==========================================================================
//  WORD Opl3_FindSecondVoice
//
//  Description:
//     This finds a slot which shares the same voice ID.
//     Needed particularly for 4op patches.
//
//  Parameters:
//     BYTE bFirstVoice
//        Voice number of the first pair
//
//     BYTE bVoiceID
//        Voice ID to find
//
//  Return Value:
//     WORD
//        Second Voice number, or ~0 if not found.
//
//
// ==========================================================================
WORD
   OPLSynth::
   Opl3_FindSecondVoice(BYTE bFirstVoice, BYTE bVoiceID)
{
   for (WORD i = 0; i < NUM2VOICES; i++)
   {
      if (i == bFirstVoice) continue;

      if (bVoiceID == m_Voice[ i ].bVoiceID)
         return i;

      // couldn't find it
   }
   return (WORD)~0;
}


// ==========================================================================
//  WORD Opl3_FindFullSlot
//
//  Description:
//     This finds a slot with a specific note, and channel.
//     If it is not found then 0xFFFF is returned.
//
//  Parameters:
//     BYTE bNote
//        MIDI note number
//
//     BYTE bChannel
//        MIDI channel #
//
//  Return Value:
//     WORD
//        note slot #, or 0xFFFF if can't find it
//
//
// ==========================================================================
WORD
   OPLSynth::
   Opl3_FindFullSlot
   (
   BYTE            bNote,
   BYTE            bChannel
   )
{
   WORD  i ;

   for (i = 0; i < NUM2VOICES; i++)
   {
      if ((bChannel == m_Voice[ i ].bChannel)
         && (bNote == m_Voice[ i ].bNote)
         && (m_Voice[ i ].bOn)
         && !(m_Voice[ i ].bSusHeld))
      {
         return ( i ) ;
      }
      // couldn't find it
   }
   return ( 0xFFFF ) ;
}

//------------------------------------------------------------------------
//  void Opl3_FMNote
//
//  Description:
//     Turns on an FM-synthesizer note.
//
//  Parameters:
//     WORD wNote
//        the note number from 0 to NUMVOICES
//
//     Patch *lpSN
//        structure containing information about what
//        is to be played.
//
//     BYTE bChannel
//        Current MIDI channel of note (needed for mono legato mode)
//
//     WORD wNote2
//        the note number from 0 to NUMVOICES (2nd pair)
//        (ignored if NS->bOp == PATCH_1_2OP)
//
//  Return Value:
//     Nothing.
//------------------------------------------------------------------------
void
   OPLSynth::
   Opl3_FMNote(WORD wNote, Patch * lpSN, BYTE bChannel, WORD wNote2)
{
   WORD            i ;
   WORD            wOffset ;
   Operator        *lpOS ;
   BYTE            b4Op = (BYTE)((lpSN->bOp != PATCH_1_2OP) && wNote2 != (WORD)~0);

   // Do not send note off to allow for mono mode legato
   if ( !(m_wMonoMode & (1<<bChannel)) || (m_wDrumMode & (1<<bChannel) || bChannel == DRUMCHANNEL))
   {
      // write out a note off, just to make sure...
      wOffset = wNote;
      if (wNote >= (NUM2VOICES / 2))
         wOffset += (0x100 - (NUM2VOICES / 2));

      Opl3_ChipWrite(AD_BLOCK + wOffset, 0 ) ;

      // needed for 2x2op patches
      if (b4Op && lpSN->bOp == PATCH_2_2OP)
      {
         wOffset = wNote2;
         if (wNote2 >= (NUM2VOICES / 2))
            wOffset += (0x100 - (NUM2VOICES / 2));

         Opl3_ChipWrite(AD_BLOCK + wOffset, 0 ) ;
      }
   }

   // TODO: Switch between 4-op mode enables.

   // writing the operator information

   //for (i = 0; i < (WORD)((wNote < NUM4VOICES) ? NUMOPS : 2); i++)
   for (i = 0; i < NUMOPS; i++)
   {
      if (i == 2 && !b4Op)
         break;

      lpOS = &lpSN -> op[ i ] ;
      wOffset =
         (i < 2) ? gw2OpOffset[ wNote ][ i ] :
         gw2OpOffset[ wNote2 ][ (i-2) ] ;

      Opl3_ChipWrite( 0x20 + wOffset, lpOS -> bAt20) ;
      Opl3_ChipWrite( 0x40 + wOffset, lpOS -> bAt40) ;
      Opl3_ChipWrite( 0x60 + wOffset, lpOS -> bAt60) ;
      Opl3_ChipWrite( 0x80 + wOffset, lpOS -> bAt80) ;
      Opl3_ChipWrite( 0xE0 + wOffset, lpOS -> bAtE0) ;

   }

   // write out the voice information
   wOffset = (wNote < 9) ? wNote : (wNote + 0x100 - 9) ;
   Opl3_ChipWrite(0xa0 + wOffset, lpSN -> bAtA0[ 0 ] ) ;
   Opl3_ChipWrite(0xc0 + wOffset, lpSN -> bAtC0[ 0 ] ) ;

   // Note on...
   Opl3_ChipWrite(0xb0 + wOffset,
      (BYTE)(lpSN -> bAtB0[ 0 ] | 0x20) ) ;

   if (b4Op && (lpSN->bOp == PATCH_2_2OP /*|| (lpSN->bOp == PATCH_1_4OP && (lpSN->bAtC0[1] & 0x1))*/))
   {
      wOffset = (wNote2 < 9) ? wNote2 : (wNote2 + 0x100 - 9) ;
      Opl3_ChipWrite(0xa0 + wOffset, lpSN -> bAtA0[ 1 ] ) ;
      Opl3_ChipWrite(0xc0 + wOffset, lpSN -> bAtC0[ 1 ] ) ;

      Opl3_ChipWrite(0xb0 + wOffset,
         (BYTE)(lpSN -> bAtB0[ 1 ] | 0x20) ) ;
   }

} // end of Opl3_FMNote()

//=======================================================================
//  WORD Opl3_NoteOn
//
//  Description:
//     This turns on a note, including drums with a patch # of the
//     drum note + 0x80.  The first GM drum instrument is mapped to note 35 instead of zero, though, so
//     we expect 0 as the first drum patch (acoustic kick) if note 35 comes in.
//
//  Parameters:
//     BYTE bPatch
//        MIDI patch
//
//     BYTE bNote
//        MIDI note
//
//     BYTE bChannel
//        MIDI channel
//
//     BYTE bVelocity
//        velocity value
//
//     short iBend
//        current pitch bend from -32768 to 32767
//
//  Return Value:
//     WORD
//        note slot #, or 0xFFFF if it is inaudible
//=======================================================================
void
   OPLSynth::
   Opl3_NoteOn(BYTE bPatch, BYTE bNote, BYTE bChannel, BYTE bVelocity, long iBend)
{
   WORD             wTemp, i, j, wTemp2 = ~0 ;
   BYTE             b4Op, /*bTemp, */bMode, bStereo, bRhyPatch;
   Patch            *lpPS, NS ;
   short            wBaseFineTune     = 0,
                    wBaseCoarseTune   = 0,
                    wSecondFineTune   = 0,
                    wSecondCoarseTune = 0;
   bool             bIsMonoMode = ((m_wMonoMode & (1<<bChannel))>0);
   //DWORD            dwBasicPitch, dwPitch[ 2 ] ;

   // Increment voice allocation ID (needed for pairing operator pairs for 2x2op patches)
   static BYTE      bVoiceID = 0;

   if (bPatch < 128)
   {
      wBaseFineTune     = gbMelMap[bPatch].wBaseFineTune,
      wBaseCoarseTune   = gbMelMap[bPatch].wBaseTranspose,
      wSecondFineTune   = gbMelMap[bPatch].wSecondFineTune,
      wSecondCoarseTune = gbMelMap[bPatch].wSecondTranspose;
   }

   // Get a pointer to the patch
   lpPS = &glpPatch[bPatch] ;
   // Find out the basic pitch according to our
   // note value.  This may be adjusted because of
   // pitch bends or special qualities for the note.

   /*dwBasicPitch = gdwPitch[ bNote % 12 ] ;
   bTemp = bNote / (BYTE) 12 ;
   if (bTemp > (BYTE) (60 / 12))
      dwBasicPitch = AsLSHL( dwBasicPitch, (BYTE)(bTemp - (BYTE)(60/12)) ) ;
   else if (bTemp < (BYTE) (60/12))
      dwBasicPitch = AsULSHR( dwBasicPitch, (BYTE)((BYTE) (60/12) - bTemp) ) ;*/

   // if blank patch, ignore completely
   if (Opl3_IsPatchEmpty(bPatch))
      return;

   // Copy the note information over and modify
   // the total level and pitch according to
   // the velocity, midi volume, and tuning.

   memcpy( &NS, lpPS, sizeof(Patch));

   // Check if 4op patch and is not a rhythm mode patch
   bRhyPatch = !(NS.bRhythmMap < RHY_CH_BD || NS.bRhythmMap > RHY_CH_CY);
   b4Op = (BYTE)(NS.bOp != PATCH_1_2OP && !bRhyPatch);

   // Init note
   for (j = 0; j < 2; j++)
   {
      long lPitchSet =
           (j==0 && bPatch < 128) ? (wBaseCoarseTune*8192)+(wBaseFineTune*4096/100)
         : (j==1 && bPatch < 128) ? (wSecondCoarseTune*8192)+(wSecondFineTune*4096/100)
         : 0;

      wTemp = Opl3_MIDINote2FNum(bNote, bChannel, lPitchSet);
      NS.bAtA0[ j ] = (BYTE) wTemp ;
      NS.bAtB0[ j ] = (BYTE) 0x20 | (BYTE) (wTemp >> 8) ;
   }
   // Modify level for each operator, but only
   // if they are carrier waves

   bMode = (BYTE) ((NS.bAtC0[ 0 ] & 0x01) * 2 + 4) ;

   for (i = 0; i < NUMOPS; i++)
   {
      if (!b4Op && i >= 2)
         break;

      wTemp = (BYTE)
         Opl3_CalcVolume(  (BYTE)(NS.op[ i ].bAt40 & (BYTE) 0x3f),
         bChannel,
         bVelocity,
         (BYTE) i,
         bMode ) ;
      NS.op[ i ].bAt40 = (NS.op[ i ].bAt40 & (BYTE)0xc0) | (BYTE) wTemp ;
   }

   // Do stereo panning, but cutting off a left or
   // right channel if necessary...

   bStereo = Opl3_CalcStereoMask( bChannel ) ;
   NS.bAtC0[ 0 ] &= bStereo ;

   // Check if mono mode is set.
   wTemp = 0;
   if (bIsMonoMode)
   {
      // Is last voice used if it indeed is used by this channel
      // else find a new slot.
      // TODO: special case needed for PATCH_1_4OP

      //wTemp = m_Voice[m_bLastVoiceUsed[bChannel]].bChannel;
      wTemp = (m_Voice[m_bLastVoiceUsed[bChannel]].bChannel == bChannel) ? m_bLastVoiceUsed[bChannel] :
              (NS.bOp == PATCH_1_4OP) ? Opl3_FindEmptySlot4Op(bPatch, bChannel) : Opl3_FindEmptySlot(bPatch, bChannel);

      // If a rhythm mode patch
      if (bRhyPatch)
      {
         switch (NS.bRhythmMap)
         {
            case RHY_CH_BD:
               break;

            case RHY_CH_SD:
               break;

            case RHY_CH_TOM:
               break;

            case RHY_CH_HH:
               break;

            case RHY_CH_CY:
               break;
         }
      }

      // safety measure for pure 4-op patches
      /*else if (NS.bOp == PATCH_1_4OP && ((wTemp > 2 && wTemp < 9) || wTemp > 11))
      {
         bool is4OpVoice = false;

         for (i = 0; i < NUM4VOICES; ++i)
         {
            if (gb4OpVoices[i] == wTemp)
            {
               is4OpVoice = true;
               break;
            }
         }

         if (!is4OpVoice)
         {
            wTemp = Opl3_FindEmptySlot4Op(bPatch, bChannel);
         }
      }
      */
      // Check if last channel used else find one.
      /*else if (NS.bOp != PATCH_1_4OP)
      {
         wTemp = (wTemp == bChannel) ? wTemp :
                 //(NS.bOp == PATCH_1_4OP) ? Opl3_FindEmptySlot4Op(bPatch, bChannel) :
                 Opl3_FindEmptySlot(bPatch, bChannel);
         //m_Voice[wTemp].bPrevNote = m_Voice[wTemp].bNote;
      }*/

      // If percussion voice not ended, kill it
      //if ((m_wDrumMode & (1<<bChannel)) > 0 || bChannel == DRUMCHANNEL)
      //{
      //   if (m_Voice[wTemp].bOn || m_Voice[wTemp].bSusHeld)
      //   {
      //      Opl3_CutVoice(wTemp, true);
      //   }
      //}
      // TODO: configuration flag for locking to 1:1 channel mapping.
   }

   else
   {
      // Find an empty slot, and use it...
      wTemp = (NS.bOp == PATCH_1_4OP /*&& (NS.bAtC0[1] & 0x1) == 0*/) ?
         Opl3_FindEmptySlot4Op(bPatch, bChannel) : Opl3_FindEmptySlot(bPatch, bChannel);
   }

   // Portamento
   // If mono mode and incomplete, take current position
   if ((m_Voice[wTemp].bOn || m_Voice[wTemp].bSusHeld) && (m_wPortaMode & (1<<bChannel)) > 0
      && m_Voice[wTemp].dwPortaSampCnt > 0 && bIsMonoMode)
   {
      DWORD curSampRange = m_Voice[wTemp].dwPortaSampTime,
            curSampCnt = m_Voice[wTemp].dwPortaSampCnt;

      m_Voice[wTemp].bPrevNote = (BYTE)floor(0.5 + (double)lin_intp(
         (curSampRange - curSampCnt),
         0,
         curSampRange,
         m_Voice[wTemp].bPrevNote,
         m_Voice[wTemp].bNote));
   }
   else
      m_Voice[wTemp].bPrevNote = m_bLastNoteUsed[bChannel] ; //m_Voice[wTemp].bNote;

   m_Voice[wTemp].dwPortaSampTime = (DWORD)floor(0.5 + pow(((double)m_bPortaTime[bChannel]*0.4), 1.5)); //m_bPortaTime[bChannel];
   m_Voice[wTemp].dwPortaSampCnt = m_Voice[wTemp].dwPortaSampTime; //m_bPortaTime[bChannel];
   if ((bNote == m_bLastNoteUsed[bChannel] /*&& !m_Voice[bChannel].bOn && !m_Voice[bChannel].bSusHeld*/)
      || m_bLastNoteUsed[bChannel] == (BYTE)0xFF || (m_wPortaMode & (1<<bChannel)) == 0)
      m_Voice[wTemp].dwPortaSampCnt = 0;

   // Remove note from queue if held
   if (m_Voice[ wTemp ].bOn && !bIsMonoMode)
   {
      Opl3_NoteOff (
         m_Voice[ wTemp ].bPatch,
         m_Voice[ wTemp ].bNote,
         m_Voice[ wTemp ].bChannel,
         (BYTE)0
      );
   }

   m_Voice[ wTemp ].bNote = bNote ;
   m_Voice[ wTemp ].bChannel = bChannel ;
   m_Voice[ wTemp ].bPatch = bPatch ;
   m_Voice[ wTemp ].bVelocity = bVelocity ;
   m_Voice[ wTemp ].bOn = true ;
   //m_Voice[ wTemp ].dwOrigPitch[0] = dwPitch[ 0 ] ;  // not including bend
   //m_Voice[ wTemp ].dwOrigPitch[1] = dwPitch[ 1 ] ;  // not including bend
   m_Voice[ wTemp ].bBlock[0] = NS.bAtB0[ 0 ] ;
   m_Voice[ wTemp ].bBlock[1] = NS.bAtB0[ 1 ] ;
   m_Voice[ wTemp ].bSusHeld = 0;
   m_Voice[ wTemp ].dwTime = ++m_dwCurTime ;
   m_Voice[ wTemp ].dwLFOVal = 0;
   m_Voice[ wTemp ].dwDetuneEG = 0;
   m_Voice[ wTemp ].wCoarseTune = wBaseCoarseTune;
   m_Voice[ wTemp ].wFineTune = wBaseFineTune;

   if ((((m_wMonoMode & (1<<bChannel)) > 0 && m_noteHistory[bChannel].size() == 0) ||
        (m_wDrumMode & (1<<bChannel) || bChannel == DRUMCHANNEL))
       || !(m_wMonoMode & (1<<bChannel)))
      m_Voice[ wTemp ].dwStartTime = m_dwCurSample;

   if (b4Op)
   {
      switch(NS.bOp)
      {
         case PATCH_2_2OP:
            wTemp2 = ((m_wMonoMode & (1<<bChannel)) > 0) ? // Get corresponding voice last used
               Opl3_FindSecondVoice((BYTE)wTemp, m_Voice[m_bLastVoiceUsed[bChannel]].bVoiceID) :
               ~0;
            wTemp2 = (wTemp2 != (WORD)~0) ? wTemp2 : Opl3_FindEmptySlot( bPatch, bChannel );
            Opl3_Set4OpFlag((BYTE)wTemp, false, PATCH_2_2OP);
            if (wTemp2 != (WORD)~0) {
                Opl3_Set4OpFlag((BYTE)wTemp2, false, PATCH_2_2OP);

               //if ((m_wMonoMode & (1<<bChannel)) > 0)
               //{
               //   // If percussion voice not ended, kill it
               //   if ((m_wDrumMode & (1<<bChannel)) > 0 || bChannel == DRUMCHANNEL)
               //   {
               //      if (m_Voice[wTemp2].bOn || m_Voice[wTemp2].bSusHeld)
               //      {
               //         Opl3_CutVoice(wTemp2, true);
               //      }
               //   }
               //}

               // Portamento 2nd voice
               if ((m_Voice[wTemp2].bOn || m_Voice[wTemp2].bSusHeld) && (m_wPortaMode & (1<<bChannel)) > 0
                  && m_Voice[wTemp2].dwPortaSampCnt > 0 && (m_wMonoMode & (1<<bChannel)) > 0)
               {
                  DWORD curSampRange = m_Voice[wTemp2].dwPortaSampTime,
                        curSampCnt = m_Voice[wTemp2].dwPortaSampCnt;

                  m_Voice[wTemp2].bPrevNote = (BYTE)floor(0.5 + (double)lin_intp(
                     (curSampRange - curSampCnt),
                     0,
                     curSampRange,
                     m_Voice[wTemp2].bPrevNote,
                     m_Voice[wTemp2].bNote));
               }
               else
                  m_Voice[wTemp2].bPrevNote = m_bLastNoteUsed[bChannel] ;//m_Voice[wTemp2].bNote;

               m_Voice[wTemp2].dwPortaSampTime = (DWORD)floor(0.5 + pow(((double)m_bPortaTime[bChannel]*0.4), 1.5)); //m_bPortaTime[bChannel];
               m_Voice[wTemp2].dwPortaSampCnt = m_Voice[wTemp2].dwPortaSampTime; //m_bPortaTime[bChannel];
               if ((bNote == m_bLastNoteUsed[bChannel] /*&& !m_Voice[bChannel].bOn && !m_Voice[bChannel].bSusHeld*/)
                  || (m_wPortaMode & (1<<bChannel)) == 0 || m_bLastNoteUsed[bChannel] == (BYTE)0xFF)
                  m_Voice[wTemp2].dwPortaSampCnt = 0;
            }

            break;

         case PATCH_1_4OP:
            /*if ((NS.bAtC0[1] & 0x1) == 0)*/
            {
               wTemp2 = wTemp + 3; // TODO check if correct voice distance
               wTemp2 = (wTemp2 < NUM2VOICES) ? wTemp2 : (WORD)~0;

               // set 4-op mode for this channel
               Opl3_Set4OpFlag((BYTE)wTemp, true, PATCH_1_4OP);

               break;
            }
            /*else
            {
               wTemp2 = ((m_wMonoMode & (1<<bChannel)) > 0) ? // Get corresponding voice last used
               Opl3_FindSecondVoice((BYTE)wTemp, m_Voice[m_bLastVoiceUsed[bChannel]].bVoiceID) :
               ~0;
               wTemp2 = (wTemp2 != (WORD)~0) ? wTemp2 : Opl3_FindEmptySlot( bPatch, bChannel );

               Opl3_Set4OpFlag((BYTE)wTemp, false);
               break;
            }*/
      }

      if (wTemp2 != (WORD)~0)
      {
         // Remove note from queue if held
         if (m_Voice[ wTemp2 ].bOn && !bIsMonoMode)
         {
            Opl3_NoteOff (
               m_Voice[ wTemp2 ].bPatch,
               m_Voice[ wTemp2 ].bNote,
               m_Voice[ wTemp2 ].bChannel,
               (BYTE)0
            );
         }

         m_Voice[ wTemp2 ].bNote = bNote ;
         m_Voice[ wTemp2 ].bChannel = bChannel ;
         m_Voice[ wTemp2 ].bPatch = bPatch ;
         m_Voice[ wTemp2 ].bVelocity = bVelocity ;
         m_Voice[ wTemp2 ].bOn = true ;
         //m_Voice[ wTemp2 ].dwOrigPitch[0] = dwPitch[ 0 ] ;  // not including bend
         //m_Voice[ wTemp2 ].dwOrigPitch[1] = dwPitch[ 1 ] ;  // not including bend
         m_Voice[ wTemp2 ].bBlock[0] = NS.bAtB0[ 0 ] ;
         m_Voice[ wTemp2 ].bBlock[1] = NS.bAtB0[ 1 ] ;
         m_Voice[ wTemp2 ].bSusHeld = 0;
         m_Voice[ wTemp2 ].dwTime = ++m_dwCurTime ;
         m_Voice[ wTemp2 ].dwLFOVal = 0;
         m_Voice[ wTemp2 ].dwDetuneEG = 0;
         m_Voice[ wTemp2 ].wCoarseTune = wSecondCoarseTune;
         m_Voice[ wTemp2 ].wFineTune = wSecondFineTune;
         if ((((m_wMonoMode & (1<<bChannel)) > 0 && m_noteHistory[bChannel].size() == 0) ||
              (m_wDrumMode & (1<<bChannel) || bChannel == DRUMCHANNEL))
          || !(m_wMonoMode & (1<<bChannel)))
            m_Voice[ wTemp2 ].dwStartTime = m_dwCurSample;
      }
   }
   else
      Opl3_Set4OpFlag((BYTE)wTemp, false, PATCH_1_2OP);

   // Send data
   Opl3_CalcPatchModifiers(&NS, bChannel);
   Opl3_FMNote(wTemp, &NS, bChannel, wTemp2 ) ; // TODO refactor functionality to insert second operator

   m_bLastVoiceUsed[bChannel] = (BYTE)wTemp; // save voice ref
   m_Voice[ wTemp ].bVoiceID = ++bVoiceID;
   m_bLastNoteUsed[bChannel] = bNote;
   if (b4Op)
   {
      if (wTemp2 != (WORD)~0)
         m_Voice[ wTemp2 ].bVoiceID = bVoiceID;
   }

   // Add to note history as most recent note
   m_noteHistory[bChannel].push_back(bNote);

   // If on sostenuto list, add to it (needed for note off)
   if (std::find(
      m_sostenutoBuffer[bChannel].begin(),
      m_sostenutoBuffer[bChannel].end(),
      bNote) != m_sostenutoBuffer[bChannel].end())
   m_sostenutoBuffer[bChannel].push_back(bNote);

} // end of Opl3_NoteOn()

void
   OPLSynth::
   Opl3_Set4OpFlag(BYTE bVoice, bool bSetFlag, BYTE bOp)
{
   BYTE cur4OpVoiceSet = b4OpVoiceSet;

   for (int i = 0; i < NUM4VOICES; ++i)
   {
      if (gb4OpVoices[i] == (BYTE)bVoice)
      {
         // Unset voice if not 4op
         /*if (bOp != PATCH_1_4OP && (cur4OpVoiceSet & (1<<i)) > 0)
            Opl3_CutVoice(gb4OpVoices[i], true);*/

         // Unset relevant voices if 4op
         /*else if (bOp == PATCH_1_4OP && (cur4OpVoiceSet & (i<<1)) > 0)
         {
            //if (m_Voice[bVoice].bOn)
            Opl3_CutVoice(gb4OpVoices[i], true);

            //if (m_Voice[bVoice+3].bOn)
            Opl3_CutVoice(gb4OpVoices[i]+3, true);
         }*/

         // Update local register
         b4OpVoiceSet = (bSetFlag) ?
            (b4OpVoiceSet | (1<<i)) :
            (b4OpVoiceSet & ~(1<<i)) ;

         // Write 4op enable/disable register to chip
         if (cur4OpVoiceSet != b4OpVoiceSet)
            Opl3_ChipWrite(AD_CONNECTION, (BYTE)(b4OpVoiceSet)) ;

         break;
      }

      // this will always be to unset since FindEmptySlot4Op only looks for first pair
      else if (gb4OpVoices[i]+3 == (BYTE)bVoice/* && !bSetFlag*/)
      {
         // Cut base voice to be safe (?)
         if (bOp != PATCH_1_4OP && (cur4OpVoiceSet & (1<<i)) > 0)
            Opl3_CutVoice(gb4OpVoices[i], true);

         // Update local register
         b4OpVoiceSet &= ~(1<<i);

         // Write 4op enable/disable register to chip
         if (cur4OpVoiceSet != b4OpVoiceSet)
            Opl3_ChipWrite(AD_CONNECTION, (BYTE)(b4OpVoiceSet)) ;
         break;
      }
   }
}

bool
   OPLSynth::
   Opl3_IsPatchEmpty(BYTE bPatch)
{
   Patch *lpPS = &glpPatch[bPatch];
   DWORD isEmpty = 0;

   for (BYTE i = 0; i < NUMOPS; ++i)
   {
       isEmpty += (lpPS->op[i].bAt20 + lpPS->op[i].bAt40
                 + lpPS->op[i].bAt60 + lpPS->op[i].bAt80
                 + lpPS->op[i].bAtE0);
   }

   //isEmpty += (lpPS->bAtA0[0] + lpPS->bAtB0[0] + lpPS->bAtC0[0]);
   //isEmpty += (lpPS->bAtA0[1] + lpPS->bAtB0[1] + lpPS->bAtC0[1]);

   return (isEmpty == 0);
}

void
   OPLSynth::
   Opl3_CalcPatchModifiers(Patch *lpSN, BYTE bChannel)
{
   char bTemp;
   char bOffset;

   //Attack
   bOffset = (char)lin_intp(m_bAttack[bChannel], 0, 127, (-16), 16);
   if (bOffset)
   {
      for (int i = 0; i < NUMOPS; i+=2)
      {
         if (i == 2 && lpSN->bOp == PATCH_1_2OP)
            break;

         bTemp = ((lpSN->op[1+i].bAt60 & 0xF0)>>4);
         lpSN->op[1+i].bAt60 &= ~0xF0;
         bTemp = (char)bTemp - bOffset;
         lpSN->op[1+i].bAt60 |= (bTemp > 0xF) ? 0xF0 : (bTemp > 0) ? (bTemp<<4) : 0;

         // If AM mode (assuming 2op)
         if (((lpSN->bAtC0[0+(i%2)]) & 0x1) == 1)
         {
            bTemp = ((lpSN->op[0].bAt60 & 0xF0)>>4);
            lpSN->op[0+i].bAt60 &= ~0xF0;
            bTemp = (char)bTemp - bOffset;
            lpSN->op[0+i].bAt60 |= (bTemp > 0xF) ? 0xF0 : (bTemp > 0) ? (bTemp<<4) : 0;
         }
      }
   }

   //Release
   bOffset = (char)lin_intp(m_bRelease[bChannel], 0, 127, (-16), 16);
   if (bOffset)
   {
      for (int i = 0; i < NUMOPS; ++i)
      {
         if (i == 2 && lpSN->bOp == PATCH_1_2OP)
            break;

         bTemp = lpSN->op[i].bAt80 & 0xF;
         lpSN->op[i].bAt80 &= ~0xF;
         bTemp = (char)bTemp - bOffset;
         lpSN->op[i].bAt80 |= (bTemp > 0xF) ? 0xF : (bTemp > 0) ? bTemp : 0;
      }
   }

   //Brightness
   bOffset = (char)lin_intp(m_bBrightness[bChannel], 0, 127, (-32), 32);
   if (bOffset)
   {
      for (int i = 0; i < NUMOPS; i+=2)
      {
         if (i == 2 && lpSN->bOp == PATCH_1_2OP)
            break;

         bTemp = lpSN->op[0+i].bAt40 & 0x3F;  // retain output level (0 = loud, 0x3f = quiet)
         lpSN->op[0+i].bAt40 &= ~0x3F;        // clear old output level
         bTemp = (char)bTemp - bOffset;
         lpSN->op[0+i].bAt40 |= (bTemp > 0x3F) ? 0x3F : (bTemp > 0) ? bTemp : 0;


      }
   }
}

void
   OPLSynth::
   Opl3_ProcessDataEntry(BYTE bChannel)
{
   WORD rpn  = ((WORD)(m_RPN[bChannel][0])|(m_RPN[bChannel][1] << 8)) & (WORD)(0x7F7F),
        nrpn = ((WORD)(m_NRPN[bChannel][0])|(m_NRPN[bChannel][1] << 8)) & (WORD)(0x7F7F);
   //DWORD dwTemp;
   BYTE val = m_bDataEnt[bChannel][1];  // TODO repurpose?

   if (m_bRPNCount[bChannel] >= 2)
   {
      m_bRPNCount[bChannel] = 2;

      switch (rpn)
      {
         case (WORD)0x0000:   // Pitch Bend Range extension
            // Calculate base bend value then apply
            ///////////m_iBendRange[bChannel] = (!m_iBendRange[bChannel]) ? 2 : m_iBendRange[bChannel];
            //dwTemp = (m_iBendRange[bChannel]) ? ((long)m_iBend[bChannel] / m_iBendRange[bChannel]) : 0;
            m_iBendRange[bChannel] = val & 0x7f;
            //dwTemp = (dwTemp > 0) ? dwTemp * m_iBendRange[bChannel] : ((long)m_iBend[bChannel] * m_iBendRange[bChannel]);
            //m_iBend[bChannel] = (long) (dwTemp);

            // Update pitch bend
            Opl3_PitchBend(bChannel, m_iBend[bChannel]);
            break;

         case (WORD)0x0001:   // Fine tune
            m_bFineTune[bChannel] = val & 0x7f;

            // Update pitch bend
            Opl3_PitchBend(bChannel, m_iBend[bChannel]);
            break;

         case (WORD)0x0002:   // Coarse tune
            m_bCoarseTune[bChannel] = val & 0x7f;

            // Update pitch bend
            Opl3_PitchBend(bChannel, m_iBend[bChannel]);
            break;
      }
   }

   else if (m_bNRPNCount[bChannel] >= 2)
   {
      m_bNRPNCount[bChannel] = 2;

      switch (nrpn)
      {
         case 0x0040:
         case 0x0041:
         {
            BYTE bInsNum = m_NRPN[bChannel][0],  // TODO determine by bank as well
                 bSelOp  = val,
                 bOpRegVal    = m_bDataEnt[bChannel][0];

            (void)bInsNum; (void)bSelOp; (void)bOpRegVal;
         }
      }
   }
}

//=======================================================================
//Opl3_CalcFAndB - Calculates the FNumber and Block given a frequency.
//
//inputs
//       DWORD   dwPitch - pitch
//returns
//        WORD - High byte contains the 0xb0 section of the
//                        block and fNum, and the low byte contains the
//                        0xa0 section of the fNumber
//=======================================================================
//WORD
//   OPLSynth::
//   Opl3_CalcFAndB(DWORD dwPitch)
//{
//   BYTE    bBlock;
//
//   /* bBlock is like an exponential to dwPitch (or FNumber) */
//   for (bBlock = 1; dwPitch >= 0x400; dwPitch >>= 1, bBlock++)
//      ;
//
//   if (bBlock > 0x07)
//      bBlock = 0x07;  /* we cant do anything about this */
//
//   /* put in high two bits of F-num into bBlock */
//   return ((WORD) bBlock << 10) | (WORD) dwPitch;
//}

//=======================================================================
//Opl3_CalcBend - This calculates the effects of pitch bend
//        on an original value.
//
//inputs
//        DWORD   dwOrig - original frequency
//        short   iBend - from -32768 to 32768, -2 half steps to +2
//returns
//        DWORD - new frequency
//=======================================================================
//DWORD
//   OPLSynth::
//   Opl3_CalcBend (DWORD dwOrig, long iBend)
//{
//   /* do different things depending upon positive or
//   negative bend */
//   DWORD dw;
//
//   if (iBend > 0)
//   {
//      dw = (DWORD)((iBend * (LONG)(256.0 * (EQUAL * EQUAL - 1.0))) >> 8);
//      dwOrig += (DWORD)(AsULMUL(dw, dwOrig) >> 15);
//   }
//   else if (iBend < 0)
//   {
//      dw = (DWORD)(((-iBend) * (LONG)(256.0 * (1.0 - 1.0 / EQUAL / EQUAL))) >> 8);
//      dwOrig -= (DWORD)(AsULMUL(dw, dwOrig) >> 15);
//   }
//
//   return dwOrig;
//}

//=======================================================================
// Opl3_CalcVolume - This calculates the attenuation for an operator.
//
//inputs
//        BYTE    bOrigAtten - original attenuation in 0.75 dB units
//        BYTE    bChannel - MIDI channel
//        BYTE    bVelocity - velocity of the note
//        BYTE    bOper - operator number (from 0 to 3)
//        BYTE    bMode - voice mode (from 0 through 7 for
//                                modulator/carrier selection)
//returns
//        BYTE - new attenuation in 0.75 dB units, maxing out at 0x3f.
//=======================================================================
BYTE
   OPLSynth::
   Opl3_CalcVolume(BYTE bOrigAtten,BYTE bChannel,BYTE bVelocity,BYTE bOper,BYTE bMode)
{
   BYTE        bVolume;
   WORD        wTemp;
   WORD        wMin;

   switch (bMode) {
   case 0:
      bVolume = (BYTE)(bOper == 3);
      break;
   case 1:
      bVolume = (BYTE)((bOper == 1) || (bOper == 3));
      break;
   case 2:
      bVolume = (BYTE)((bOper == 0) || (bOper == 3));
      break;
   case 3:
      bVolume = (BYTE)(bOper != 1);
      break;
   case 4:
      bVolume = (BYTE)((bOper == 1) || (bOper == 3));
      break;
   case 5:
      bVolume = (BYTE)(bOper >= 1);
      break;
   case 6:
      bVolume = (BYTE)(bOper <= 2);
      break;
   case 7:
      bVolume = true;
      break;
   default:
      bVolume = false;
      break;
   };
   if (!bVolume)
      return bOrigAtten; /* this is a modulator wave */

   wMin =(m_wSynthAttenL < m_wSynthAttenR) ? m_wSynthAttenL : m_wSynthAttenR;
   wTemp = bOrigAtten +
      ((wMin << 1) +
      m_bChanAtten[bChannel] +
      gbVelocityAtten[bVelocity >> 1]);
   return (wTemp > 0x3f) ? (BYTE) 0x3f : (BYTE) wTemp;
}

// ===========================================================================
// Opl3_ChannelNotesOff - turn off all notes on a channel
// ===========================================================================
void
   OPLSynth::
   Opl3_ChannelNotesOff(BYTE bChannel)
{
   int i;

   m_noteHistory[bChannel].clear();
   m_sostenutoBuffer[bChannel].clear();

   for (i = 0; i < NUM2VOICES; i++)
   {
      if ((m_Voice[ i ].bOn) && (m_Voice[ i ].bChannel == bChannel))
      {
         //Opl3_NoteOff(m_Voice[i].bPatch, m_Voice[i].bNote,m_Voice[i].bChannel, 0) ;
         Opl3_CutVoice(i, true);
      }
   }
}

// ===========================================================================
/* Opl3_ChannelVolume - set the volume level for an individual channel.
*
* inputs
*      BYTE    bChannel - channel number to change
*      WORD    wAtten  - attenuation in 1.5 db units
*
* returns
*      none
*/
// ===========================================================================
void
   OPLSynth::
   Opl3_ChannelVolume(BYTE bChannel, WORD wAtten)
{
   //m_bChanAtten[bChannel] = (BYTE)wAtten;
   m_bChanAtten[bChannel] = (BYTE)gbVelocityAtten[(BYTE)(((wAtten & 0x7F) >> 1) * ((float)m_iExpThres[bChannel]/0x7F))];

   Opl3_SetVolume(bChannel);
}

// ===========================================================================
//  void Opl3_SetVolume
//
//  Description:
//     This should be called if a volume level has changed.
//     This will adjust the levels of all the playing voices.
//
//  Parameters:
//     BYTE bChannel
//        channel # of 0xFF for all channels
//
//  Return Value:
//     Nothing.
//
// ===========================================================================
void
   OPLSynth::
   Opl3_SetVolume(BYTE bChannel)
{
   WORD            i, j, wTemp, wOffset ;
   Patch           *lpPS;
   Operator        opSt;
   BYTE            bMode, bStereo, bOffset ;
   char            bTemp;

   // Loop through all the notes looking for the right
   // channel.  Anything with the right channel gets
   // its pitch bent.
   for (i = 0; i < NUM2VOICES; i++)
   {
      if ((m_Voice[ i ].bChannel == bChannel) || (bChannel == 0xff))
      {
         // Get a pointer to the patch
         lpPS = &(glpPatch[ m_Voice[ i ].bPatch ]);

         // Modify level for each operator, IF they are carrier waves...
         bMode = (BYTE) ( (lpPS->bAtC0[0] & 0x01) * 2 + 4);

         for (j = 0; j < 2; ++j)
         {
            if (j == 0 && lpPS->bOp == PATCH_1_4OP)
               continue;

            // Copy to buffer
            memcpy( &opSt, &(lpPS->op[j]), sizeof( Operator ) ) ;

            wTemp = (BYTE) Opl3_CalcVolume(
               //(BYTE) (lpPS -> op[j].bAt40 & (BYTE) 0x3f),
               (BYTE) (opSt.bAt40 & (BYTE) 0x3f),
               m_Voice[i].bChannel, m_Voice[i].bVelocity,
               (BYTE) j,            bMode ) ;

            // Brightness update  TODO: refactor
            bOffset = (char)lin_intp(m_bBrightness[bChannel], 0, 127, (-32), 32);
            if (bOffset != 0 && j==0)
            {
               bTemp = lpPS->op[j].bAt40 & 0x3F;  // retain output level (0 = loud, 0x3f = quiet)
               opSt.bAt40 &= ~0x3F;        // clear old output level
               bTemp = (char)wTemp - bOffset;
               wTemp = (bTemp > (BYTE)0x3F) ? (WORD)0x3F : (bTemp > (BYTE)0) ? (WORD)bTemp : (WORD)0;
            }

            opSt.bAt40 = (opSt.bAt40 & (BYTE)0xc0) | (BYTE) wTemp;

            // Write new value.
            wOffset = gw2OpOffset[ i ][ j ] ;
            Opl3_ChipWrite(
               0x40 + wOffset,
               //(BYTE) ((lpPS -> op[j].bAt40 & (BYTE)0xc0) | (BYTE) wTemp) ) ;
               (BYTE) (opSt.bAt40) ) ;
         }

         // Do stereo pan, but cut left or right channel if needed.
         bStereo = Opl3_CalcStereoMask( m_Voice[ i ].bChannel ) ;
         wOffset = i;
         if (i >= (NUM2VOICES / 2))
            wOffset += (0x100 - (NUM2VOICES / 2));
         Opl3_ChipWrite(0xc0 + wOffset, (BYTE)(lpPS -> bAtC0[ 0 ] & bStereo) ) ;
      }
   }
} // end of Opl3_SetVolume

// ===========================================================================
// Opl3_SetPan - set the left-right pan position.
//
// inputs
//      BYTE    bChannel - channel number to alter
//      BYTE    bPan     - 0-47 for left, 81-127 for right, or somewhere in the middle.
//
// returns - none
//
//  As a side note, I think it's odd that (since 64 = CENTER, 127 = RIGHT and 0 = LEFT)
//  there are 63 intermediate gradations for the left side, but 62 for the right.
// ===========================================================================
void
   OPLSynth::
   Opl3_SetPan(BYTE bChannel, BYTE bPan)
{
   /* change the pan level */
   if (bPan > (64 + 16))
      m_bStereoMask[bChannel] = 0xef;      /* let only right channel through */
   else if (bPan < (64 - 16))
      m_bStereoMask[bChannel] = 0xdf;      /* let only left channel through */
   else
      m_bStereoMask[bChannel] = 0xff;      /* let both channels */

   /* change any curently playing patches */
   Opl3_SetVolume(bChannel);
}


// ===========================================================================
//  void Opl3_PitchBend
//
//  Description:
//     This pitch bends a channel.
//
//  Parameters:
//     BYTE bChannel
//        channel
//
//     short iBend
//        values from -32768 to 32767, being -2 to +2 half steps
//
//  Return Value:
//     Nothing.
// ===========================================================================
void
   OPLSynth::
   Opl3_PitchBend(BYTE bChannel, long iBend)
{
   WORD   i, wTemp[ 2 ], wOffset, j ;
//   DWORD  dwNew ;

   // Remember the current bend value
   m_iBend[ bChannel ] = iBend;

   // Loop through all the notes looking for
   // the correct channel.  Anything with the
   // correct channel gets its pitch bent...
   for (i = 0; i < NUM2VOICES; i++)
   {
      if (m_Voice[ i ].bChannel == bChannel/* && (m_Voice [ i ].bOn || m_bSustain[bChannel])*/)
      {
         j = 0 ;
         //wTemp[ j ] = Opl3_CalcFAndB( Opl3_CalcBend( m_Voice[ i ].dwOrigPitch[ j ], iBend ) ) ;
         wTemp[ j ] = Opl3_MIDINote2FNum(m_Voice[ i ].bNote, bChannel,
            (m_Voice[ i ].dwLFOVal + m_Voice[ i ].dwDetuneEG + (8192*m_Voice[ i ].wCoarseTune) + (40.96*m_Voice[ i ].wFineTune))
         );
         m_Voice[ i ].bBlock[ j ] =
            (m_Voice[ i ].bBlock[ j ] & (BYTE) 0xe0) |
            (BYTE) (wTemp[ j ] >> 8) ;

         wOffset = i;
         if (i >= (NUM2VOICES / 2))
            wOffset += (0x100 - (NUM2VOICES / 2));

         Opl3_ChipWrite(AD_BLOCK + wOffset, m_Voice[ i ].bBlock[ 0 ] ) ;
         Opl3_ChipWrite(AD_FNUMBER + wOffset, (BYTE) wTemp[ 0 ] ) ;
      }
   }
} // end of Opl3_PitchBend

// ===========================================================================
//  void Opl3_MIDINote2FNum
//
//  Description:
//     Obtains FNumber from MIDI note + current bend values
//     Special thanks to ValleyBell for MidiPlay sources for adaption
//
//  Parameters:
//     double note   - MIDI note number (promoted from BYTE)
//     BYTE bChannel - channel
//     long dwLFOVal - current magnitude of the modulation waveform
//
//  Return Value:
//     ((WORD) BlockVal << 10) | (WORD) keyVal;
// ===========================================================================
WORD
   OPLSynth::
   Opl3_MIDINote2FNum(double note, BYTE bChannel, long dwLFOVal)
{
	double freqVal, curNote, RPNTune;
	signed char BlockVal;
	WORD keyVal;
//	signed long CurPitch;

   /*TODO: keep for later, may add other features */
	//CurPitch = //MMstTuning + TempMid->TunePb + TempMid->Pitch + TempMid->ModPb;

   RPNTune = ((signed)m_bCoarseTune[bChannel] - 64);
   RPNTune += lin_intp(m_bFineTune[bChannel], 0, 127, (-1), 1);

	curNote = (double)(note + m_bMasterCoarseTune + RPNTune + (m_iBend[bChannel] * m_iBendRange[bChannel] + dwLFOVal) / 8192.0); //Note + CurPitch / 8192.0;
	freqVal = 440.0 * pow(2.0, (curNote - 69) / 12.0);
	//BlockVal = ((signed short int)CurNote / 12) - 1;
	BlockVal = ((signed short int)(curNote + 6) / 12) - 2;
	if (BlockVal < 0x00)
		BlockVal = 0x00;
	else if (BlockVal > 0x07)
		BlockVal = 0x07;
	//KeyVal = (unsigned short int)(FreqVal * pow(2, 20 - BlockVal) / CHIP_RATE + 0.5);
	keyVal = (WORD)(freqVal * (1ULL << (20 - BlockVal)) / FSAMP + 0.5);
	if (keyVal > 0x03FF)
		keyVal = 0x03FF;

	return (BlockVal << 10) | keyVal;	// << (8+2)
}


// ===========================================================================
//  Opl3_CalcStereoMask - This calculates the stereo mask.
//
//  inputs
//            BYTE  bChannel - MIDI channel
//  returns
//            BYTE  mask (for register 0xc0-c8) for eliminating the
//                  left/right/both channels
// ===========================================================================
BYTE
   OPLSynth::
   Opl3_CalcStereoMask(BYTE bChannel)
{
   WORD        wLeft, wRight;

   /* figure out the basic levels of the 2 channels */
   wLeft = (m_wSynthAttenL << 1) + m_bChanAtten[bChannel];
   wRight = (m_wSynthAttenR << 1) + m_bChanAtten[bChannel];

   /* if both are too quiet then mask to nothing */
   if ((wLeft > 0x3f) && (wRight > 0x3f))
      return 0xcf;

   /* if one channel is significantly quieter than the other than
   eliminate it */
   if ((wLeft + 8) < wRight)
      return (BYTE)(0xef & m_bStereoMask[bChannel]);   /* right is too quiet so eliminate */
   else if ((wRight + 8) < wLeft)
      return (BYTE)(0xdf & m_bStereoMask[bChannel]);   /* left too quiet so eliminate */
   else
      return (BYTE)(m_bStereoMask[bChannel]);  /* use both channels */
}

//------------------------------------------------------------------------
//  WORD Opl3_FindEmptySlot
//
//  Description:
//     This finds an empty note-slot for a MIDI voice.
//     If there are no empty slots then this looks for the oldest
//     off note.  If this doesn't work then it looks for the oldest
//     on-note of the same patch.  If all notes are still on then
//     this finds the oldests turned-on-note.
//
//  Parameters:
//     BYTE bPatch
//        MIDI patch that will replace it.
//
//  Return Value:
//     WORD
//        note slot #
//
//
//------------------------------------------------------------------------
WORD
   OPLSynth::
   Opl3_FindEmptySlot(BYTE bPatch, BYTE bChannel)
{
   BYTE   bChnVoiceCnt = 0;
   WORD   i, foundOldestOff, foundOldestCurCh, foundOldestOn;
   DWORD  dwOldestOff, dwOldestCurCh, dwOldestOn ;

   // Now, look for a slot of the oldest off-note
   dwOldestOff = 0xffffffff ;
   dwOldestCurCh = 0xffffffff ;
   dwOldestOn = 0xffffffff ;
   foundOldestOff = 0xffff ;
   foundOldestCurCh = 0xffff;
   foundOldestOn = 0xffff;

   for (i = 0; i < NUM2VOICES; i++)
   {
      if (!m_Voice[ i ].dwTime)
         return ( i ) ;

      if (!m_Voice[ i ].bOn && m_Voice[ i ].dwTime < dwOldestOff)
      {
         dwOldestOff = m_Voice[ i ].dwTime ;
         foundOldestOff = i ;
      }

      if ((m_Voice[ i ].bOn || m_Voice[ i ].bSusHeld) && m_Voice[ i ].bChannel == bChannel && m_Voice[ i ].dwTime < dwOldestCurCh)
      {
         dwOldestCurCh = m_Voice[ i ].dwTime;
         foundOldestCurCh = i;
         ++bChnVoiceCnt;
      }

      if (m_Voice[ i ].dwTime < dwOldestOn)
      {
         dwOldestOn = m_Voice[ i ].dwTime;
         foundOldestOn = i;
      }
   }

   if (foundOldestOff != 0xffff)
      return ( foundOldestOff ) ;

   if (foundOldestCurCh != 0xffff && bChnVoiceCnt > (glpPatch[bPatch].bOp == PATCH_2_2OP ? 4 : 2))
      return ( foundOldestCurCh ) ;

   return foundOldestOn;

} // end of Opl3_FindEmptySlot()

//------------------------------------------------------------------------
//  WORD Opl3_FindEmptySlot4Op
//
//  Description:
//     This finds an empty note-slot for a 4-op MIDI voice.
//     If there are no empty slots then this looks for the oldest
//     off note.  If this doesn't work then it looks for the oldest
//     on-note of the same patch.  If all notes are still on then
//     this finds the oldests turned-on-note.
//
//  Parameters:
//     BYTE bPatch
//        MIDI patch that will replace it.
//
//  Return Value:
//     WORD
//        note slot #
//
//
//------------------------------------------------------------------------
WORD
   OPLSynth::
   Opl3_FindEmptySlot4Op(BYTE bPatch, BYTE bChannel)
{
   BYTE   bChnVoiceCnt = 0;
   WORD   i, n, foundOldestOff, foundOldestCurCh, foundOldestOn;
   DWORD  dwOldestOff, dwOldestCurCh, dwOldestOn ;

   dwOldestOff   = 0xffffffff ;
   dwOldestCurCh = 0xffffffff ;
   dwOldestOn    = 0xffffffff ;
   foundOldestOff   = 0xffff ;
   foundOldestCurCh = 0xffff;
   foundOldestOn    = 0xffff;

   for (n = 0; n < NUM4VOICES; n++)
   {
      i = gb4OpVoices[ n ];

      if (!m_Voice[ i ].dwTime)
         return ( i ) ;

      if (!m_Voice[ i ].bOn && m_Voice[ i ].dwTime < dwOldestOff)
      {
         dwOldestOff = m_Voice[ i ].dwTime ;
         foundOldestOff = i ;
      }

      if ((m_Voice[ i ].bOn || m_Voice[ i ].bSusHeld) && m_Voice[ i ].bChannel == bChannel && m_Voice[ i ].dwTime < dwOldestCurCh)
      {
         dwOldestCurCh = m_Voice[ i ].dwTime;
         foundOldestCurCh = i;
         ++bChnVoiceCnt;
      }

      if (m_Voice[ i ].dwTime < dwOldestOn)
      {
         dwOldestOn = m_Voice[ i ].dwTime;
         foundOldestOn = i;
      }
   }

   if (foundOldestOff != 0xffff)
      return ( foundOldestOff ) ;

   if (foundOldestCurCh != 0xffff && bChnVoiceCnt > 2)
      return ( foundOldestCurCh ) ;

   return foundOldestOn;



   /*
   // First, look for a slot with a time == 0
   for (i = 0;  i < NUM4VOICES; i++)
      if (!m_Voice[ gb4OpVoices[ i ] ].dwTime)
         return ( gb4OpVoices[ i ] ) ;

   // Now, look for a slot of the oldest off-note
   dwOldest = 0xffffffff ;
   found = 0xffff ;

   for (i = 0; i < NUM4VOICES; i++)
      if ((!m_Voice[ gb4OpVoices[ i ] ].bOn)
       && (m_Voice[ gb4OpVoices[ i ] ].dwTime < dwOldest))
      {
         dwOldest = m_Voice[ gb4OpVoices[ i ] ].dwTime ;
         found = gb4OpVoices[ i ] ;
      }

   if (found != 0xffff)
      return ( found ) ;

   // Custom:  Search for oldest note on same channel, but only allocate if there
   //          are more than 2 notes currently held on same channel
   found = 0;
   dwOldest = 0xFFFF;
   for (i = 0; i < NUM4VOICES; ++i)
      if (m_Voice[ gb4OpVoices[ i ] ].dwTime < dwOldest && m_Voice[ gb4OpVoices[ i ] ].bChannel == bChannel)
      {
         dwOldest = m_Voice[ gb4OpVoices[ i ] ].dwTime;
         found = gb4OpVoices[ i ] ;
         if (m_Voice[ gb4OpVoices[ i ] ].bOn && !m_Voice[ gb4OpVoices [ i ] ].bSusHeld) ++bChnVoiceCnt;
      }

   if (found != 0xFFFF && bChnVoiceCnt > 2)
      return ( found );


   //// Now, look for a slot of the oldest note with
   //// the same patch
   //dwOldest = 0xffffffff ;
   //found = 0xffff ;
   //for (i = 0; i < NUM4VOICES; i++)
   //   if ((m_Voice[ gb4OpVoices[ i ] ].bPatch == bPatch) && (m_Voice[ gb4OpVoices[ i ] ].dwTime < dwOldest))
   //   {
   //      dwOldest = m_Voice[ gb4OpVoices[ i ] ].dwTime ;
   //      found = gb4OpVoices[ i ] ;
   //   }
   //if (found != 0xffff)
   //   return ( found ) ;

   // Now, just look for the oldest voice
   found = 0 ;
   dwOldest = m_Voice[ gb4OpVoices[ found ] ].dwTime ;
   for (i = 0; i < NUM4VOICES; i++)
      if (m_Voice[ gb4OpVoices[ i ] ].dwTime < dwOldest)
      {
         dwOldest = m_Voice[ gb4OpVoices[ i ] ].dwTime ;
         found = gb4OpVoices[ i ] ;
      }

   return ( found ) ;*/

} // end of Opl3_FindEmptySlot4Op()

//------------------------------------------------------------------------
//  WORD Opl3_SetSustain
//
//  Description:
//     Set the sustain controller on the current channel.
//
//  Parameters:
//     BYTE bSusLevel
//        The new sustain level
//
//
//------------------------------------------------------------------------
void
   OPLSynth::
   Opl3_SetSustain(BYTE bChannel,BYTE bSusLevel)
{
   BYTE i;

   if (m_bSustain[ bChannel ] && !bSusLevel)
   {
      // Sustain has just been turned off for this channel
      // Go through and turn off all notes that are being held for sustain
      //
      for (i = 0; i < NUM2VOICES; i++)
      {
         if ((bChannel == m_Voice[ i ].bChannel) &&
            m_Voice[ i ].bSusHeld)
         {
            // this is not guaranteed to cut repeated sustained notes
            //Opl3_NoteOff(m_Voice[i].bPatch, m_Voice[i].bNote, m_Voice[i].bChannel, 0);

            Opl3_CutVoice(i, false);
         }
      }
   }
   m_bSustain[ bChannel ] = bSusLevel;
}

void
   OPLSynth::
   Opl3_SetPortamento(BYTE bChannel, BYTE bPortaTime)
{
   if ((m_wPortaMode & (1<<bChannel)) > 0)
   {
      BYTE bOldPortaTime = m_bPortaTime[bChannel],
           i;
      DWORD dwPortaScaledTime = (DWORD)floor(0.5 + pow(((double)bPortaTime*0.4), 1.5));

      m_bPortaTime[bChannel] = bPortaTime;

      // Set current porta pitch to new value if smaller
      for (i = 0; i < NUM2VOICES; ++i)
      {
         // Rescale counter to align with current pitch as long it has had a note on
         if (m_Voice[i].bChannel == bChannel && m_Voice[i].dwTime > 0)
         {
            DWORD dwOldScaledTime = (DWORD)floor(0.5 + pow(((double)bOldPortaTime*0.4), 1.5));

            m_Voice[i].dwPortaSampTime = dwPortaScaledTime; //bPortaTime;
            m_Voice[i].dwPortaSampCnt = (DWORD)floor(0.5 + lin_intp(m_Voice[i].dwPortaSampCnt,
               0, dwOldScaledTime/*bOldPortaTime*/, 0, dwPortaScaledTime/*bPortaTime*/));
         }
      }
   } else
      m_bPortaTime[bChannel] = bPortaTime;
}

void
   OPLSynth::
   Opl3_BoardReset()
{
   BYTE i;

   /* ---- silence the chip -------- */

   /* tell the FM chip to use 4-operator mode, and
   fill in any other random variables */
   Opl3_ChipWrite(AD_NEW, 0x01);
   Opl3_ChipWrite(AD_MASK, 0x60);
   Opl3_ChipWrite(AD_CONNECTION, 0x00);
   Opl3_ChipWrite(AD_NTS, 0x00);

   /* turn off the drums, and use high vibrato/modulation */
   Opl3_ChipWrite(AD_DRUM, 0xc0);

   /* turn off all the oscillators */
   for (i = 0; i <= 0x15; i++)
   {
      if ((i & 0x07) <= 0x05)
      {
         Opl3_ChipWrite(AD_LEVEL + i, 0x3f);
         Opl3_ChipWrite(AD_LEVEL2 + i, 0x3f);
      }
   };

   /* turn off all the voices */
   for (i = 0; i <= 0x08; i++)
   {
      Opl3_ChipWrite(AD_BLOCK + i, 0x00);
      Opl3_ChipWrite(AD_BLOCK2 + i, 0x00);
   };
}

//------------------------------------------------------------------------
//  void Opl3_CutNote
//
//  Description:
//     Routine to note off or note cut the FM voice channel
//
//  Parameters:
//     BYTE bVoice
//        The selected FM voice
//
//     BYTE bIsInstantCut
//        Flag to indicate whether the note-off should be instant.
//
//------------------------------------------------------------------------
void
   OPLSynth::
   Opl3_CutVoice(BYTE bVoice, BYTE bIsInstantCut)
{
   WORD wOffset = bVoice, wOpOffset;
   Patch *lpNS = &(glpPatch[ m_Voice[bVoice].bPatch]);
   BYTE bOp = lpNS->bOp;

   if (bVoice >= (NUM2VOICES / 2))
      wOffset += (0x100 - (NUM2VOICES / 2));

   // set op1 sustain to high
   if (bIsInstantCut)
   {
      if (bOp == PATCH_1_4OP)
      {
         BYTE alg = (lpNS->bAtC0[0] & 0x01) | ((lpNS->bAtC0[1] & 0x01)<<1);

         //Check carriers for 4op patch
         switch(alg)
         {
            case 3:
               wOpOffset = gw2OpOffset[ bVoice ][ 0 ] ;
               Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high
               wOpOffset = gw2OpOffset[ bVoice +3 ][ 0 ] ;
               Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high
               wOpOffset = gw2OpOffset[ bVoice +3 ][ 1 ] ;
               Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high
               break;

            case 2:
               wOpOffset = gw2OpOffset[ bVoice ][ 1 ] ;
               Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high
               wOpOffset = gw2OpOffset[ bVoice +3 ][ 1 ] ;
               Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high
               break;

            case 1:
               wOpOffset = gw2OpOffset[ bVoice ][ 0 ] ;
               Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high
               //fall through

            case 0:
               wOpOffset = gw2OpOffset[ bVoice +3 ][ 1 ] ;
               Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high
               break;
         }


      }
      else
      {
         wOpOffset = gw2OpOffset[ bVoice ][ 1 ] ; // assuming 2op
         Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high

         // if AM mode
         if (lpNS->bAtC0[0] & 0x01)
         {
            wOpOffset = gw2OpOffset[ bVoice ][ 0 ] ; // assuming 2op
            Opl3_ChipWrite( 0x80 + wOpOffset, 0xFF) ; // set SR to high
         }
      }
   }

   Opl3_ChipWrite(AD_BLOCK + wOffset,
      (BYTE)(m_Voice[ bVoice ].bBlock[ 0 ] & 0x1f) ) ;

   // Note this...
   m_Voice[ bVoice ].bOn = false ;
   m_Voice[ bVoice ].bBlock[ 0 ] &= 0x1f ;
   m_Voice[ bVoice ].bBlock[ 1 ] &= 0x1f ;
   m_Voice[ bVoice ].dwTime = ++m_dwCurTime ;
   m_Voice[ bVoice ].bSusHeld = false ;
}

bool
   OPLSynth::
   Init(unsigned rate)
{
   // init some members
   this->m_dwCurTime = 1;    /* for note on/off */
   /* volume */
   this->m_wSynthAttenL = 0;        /* in 1.5dB steps */
   this->m_wSynthAttenR = 0;        /* in 1.5dB steps */

   this->m_MinVolValue  = 0xFFD0C000;    //  minimum -47.25(dB) * 0x10000
   this->m_MaxVolValue  = 0x00000000;    //  maximum  0    (dB) * 0x10000
   this->m_VolStepDelta = 0x0000C000;    //  steps of 0.75 (dB) * 0x10000

   this->m_bMasterCoarseTune = 0;

   this->m_dwMasterTune = 0;

   this->m_MIDIMode = MIDIMODE_XG;
   this->m_bSysexDeviceId = 0x10;

   for (int i = 0; i < NUMMIDICHN; ++i)
   {
      this->m_noteHistory[i].reserve(256);
      this->m_sostenutoBuffer[i].reserve(256);
   }

   OPL3_Reset(&m_Miniport, rate);

   Opl3_BoardReset();
   Opl3_SoftCommandReset();

   return true;
}

void
   OPLSynth::
   Opl3_LFOUpdate(BYTE bVoice)
{
   WORD  wTemp, wOffset;
   long  newLFOVal = m_Voice[bVoice].dwLFOVal,
         newDetuneEG = m_Voice[bVoice].dwDetuneEG;
   DWORD timeDiff = m_dwCurSample-m_Voice[bVoice].dwStartTime;
   double timeLapse = 0.00025*3.14159265358979323846*(double)(timeDiff),
          noteDiff = 0;

   // Portamento update first
   if (m_Voice[bVoice].dwPortaSampCnt > 0)
   {
      const double PORTA_DEC_RATE = 3.333;

      //--m_Voice[bVoice].bPortaSampCnt;
      m_Voice[bVoice].dwPortaSampCnt = ((m_Voice[bVoice].dwPortaSampCnt-PORTA_DEC_RATE) <= 0) ?
         0 : (DWORD)floor(0.5 + m_Voice[bVoice].dwPortaSampCnt-PORTA_DEC_RATE);

      noteDiff = (double)lin_intp(
         (m_Voice[bVoice].dwPortaSampTime - m_Voice[bVoice].dwPortaSampCnt),
         0,
         m_Voice[bVoice].dwPortaSampTime,
         m_Voice[bVoice].bPrevNote,
         m_Voice[bVoice].bNote
      );

      /*noteDiff = m_Voice[bVoice].dwNoteFactor*(m_Voice[bVoice].dwPortaSampTime
         - m_Voice[bVoice].bPortaSampCnt) - 1;*/
   }

   // 100Hz sine wave with half semitone magnitude by default
   // (mod*32)sin((1/100)*FSAMP * curSample)
   if (m_bModWheel[m_Voice[bVoice].bChannel] > 0)
      newLFOVal = (long)floor(0.5 + (m_bModWheel[m_Voice[bVoice].bChannel]*32)*sin(timeLapse));

   // Linear envelope generator hack  (TODO: improve)
   if ((m_wDrumMode & (1<<m_Voice[bVoice].bChannel)) > 0 &&
       (m_Voice[bVoice].bOn || m_Voice[bVoice].bSusHeld)) {   // only continue it if the note is held
       wTemp = gbPercMap[m_Voice[bVoice].bPatch-128].bPitchEGAmt & 0xFF;
       if (wTemp > 0)
       {
           newDetuneEG = (long)((signed char)wTemp * 0.25 * (timeDiff));
       }
   }

   if (newLFOVal == m_Voice[bVoice].dwLFOVal && newDetuneEG == m_Voice[bVoice].dwDetuneEG
      && m_Voice[bVoice].dwPortaSampCnt == 0 && noteDiff == 0)
      return;

   m_Voice[bVoice].dwLFOVal = newLFOVal;
   m_Voice[bVoice].dwDetuneEG = newDetuneEG;

   // Hax
   newLFOVal += newDetuneEG + (8192*m_Voice[ bVoice ].wCoarseTune*100) + (40.96*m_Voice[ bVoice ].wFineTune);

   wTemp = Opl3_MIDINote2FNum((noteDiff)?noteDiff:m_Voice[ bVoice ].bNote,
      m_Voice[bVoice].bChannel, newLFOVal);
   m_Voice[ bVoice ].bBlock[ 0 ] =
      (m_Voice[ bVoice ].bBlock[ 0 ] & (BYTE) 0xe0) |
      (BYTE) (wTemp >> 8) ;

   wOffset = bVoice;
   if (bVoice >= (NUM2VOICES / 2))
      wOffset += (0x100 - (NUM2VOICES / 2));

   // Depends if voice is enabled or not
   if (m_Voice[bVoice].bOn == false)
      wTemp &= ~(1<<5);

   Opl3_ChipWrite(AD_BLOCK + wOffset, m_Voice[ bVoice ].bBlock[ 0 ] ) ;
   Opl3_ChipWrite(AD_FNUMBER + wOffset, (BYTE) wTemp ) ;

} // end of Opl3_PitchBend

void
   OPLSynth::
   GetSample(t_float *left, t_float *right, unsigned len)
{
   BYTE i;

   // Perform stepping of pitch LFO
   //for (i = 0; i < len; ++i)
   //{
   //   if (i%(FSAMP/100)==0)
   //   {
   // Step through each channel
   for (i = 0; i < NUM2VOICES; i++)
   {
      // Only execute once initialized
      if (m_Voice[i].bChannel != (BYTE)~0)
         Opl3_LFOUpdate(i);
   }
      //}
   //}

   m_dwCurSample += len;

   for (unsigned i = 0; i < len; ++i) {
       Bit16s outp[2];
       OPL3_GenerateResampled(&m_Miniport, outp);
       left[i] = outp[0] * (1/(t_float)32768);
       right[i] = outp[1] * (1/(t_float)32768);
   }
}


void
   OPLSynth::
   PlaySysex(const Bit8u *bufpos, DWORD len)
{
   bool IsResetSysex = false;
   const BYTE resetArray[6][12] =
   {
      {0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7},
      {0xF0, 0x7E, 0x7F, 0x09, 0x02, 0xF7},
      {0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7},
      {0xF0, 0x41, 0x10, 0x42, 0x12, 0x00, 0x00, 0x7F, 0x00, 0x01, 0xF7},
      {0xF0, 0x41, 0x10, 0x42, 0x12, 0x00, 0x00, 0x7F, 0x01, 0x00, 0xF7},
      {0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7},
   };

   /*
    * F0 - Exclusive status
    * xx - Manufacturer ID
    * 1n - Device Number
    * xx - Model ID
    * aa - Address High
    * aa - Address Mid
    * aa - Address Low
    * dd - Data
    * ...
    * dd - Data
    * F7 - End of Exclusive
    */

   //std::string SysExVal((char*)bufpos);

   // For reference: http://homepage2.nifty.com/mkmk/midi_lab/exref.htm
   //            and http://www.bandtrax.com.au/sysex.htm
   //GM               F0 7E 7F 09 01 F7
   //GM-2             F0 7E 7F 09 02 F7
   //SC55(GS)/TG300B  F0 41 10 42 12 40 00 7F 00 41 F7
   //SC88(GS) Mode1   F0 41 10 42 12 00 00 7F 00 01 F7
   //SC88(GS) Mode2   F0 41 10 42 12 00 00 7F 01 00 F7
   //XG               F0 43 10 4C 00 00 7E 00 F7

   // Ignore Sysex Continued or non-standard sysex events
   if (len < 4 || bufpos[0] != 0xF0 || bufpos[len-1] != 0xF7)
      return;

   // Check reset
   if ((len == 6  && (memcmp(&resetArray[0][4], bufpos, len-4)==0 ||
                      memcmp(&resetArray[1][4], bufpos, len-4)==0)) ||
       (len == 11 && (memcmp(&resetArray[2][4], bufpos, len-4)==0 ||
                     memcmp(&resetArray[4][4], bufpos, len-4)==0 ||
                     memcmp(&resetArray[5][4], bufpos, len-4)==0)) ||
       (len == 9  && (memcmp(&resetArray[5][4], bufpos, len-4)==0))
      )
      IsResetSysex = true;


   /*
    * General MIDI System Exclusive (MODE 1)
    *  - Do not allow bank switching at all (melodic or drum)
    *  - CC71-74 disabled
    */

   /*
    * General MIDI Level 2 System Exclusive (MODE 2)
    *  - Allow bank switching
    *  - CC71-74 enabled
    *  - Drum bank select 127 disabled - must use secondary sysex
    */

   /*
    * Rolands GS System Exclusive (MODE 3)
    *  - Allow bank switching
    *  - Drum bank select 127 disabled - must use secondary sysex
    *  - CC71-74 enabled (TG300b-mode compatibility)
    *  -
    */

   /*
    * Yamaha XG System Exclusive (MODE 4) - default
    *  - Allow bank switching
    *  - Allow bank-switchable MIDI channels for drum bank
    *  - CC71-74 enabled
    *  - Drum bank select enabled
    */

   //TODO: common reset routines to all channels
   if (IsResetSysex)
   {
      m_MIDIMode =
         (len == 6) ? bufpos[4] :  // will correspond to either MIDIMODE_GM1 or MIDIMORDE_GM2
         (len == 11) ? MIDIMODE_GS :
         (len == 9) ? MIDIMODE_XG :
         0x00;

      Opl3_SoftCommandReset();
   }

   else
   {
      switch(bufpos[1])
      {
         case 0x7E:  // Universal
         case 0x7F:
            // MIDI Master Volume
            //F0 7F 7F 04 01 xx(lsb) yy(msb) F7

            // GM-2 MIDI Master Fine Tuning
            //F0 7F devID 04 03 lsb msb F7 (default== 00 40, max = 7F 7F)

            // GM-2 MIDI Master Coarse Tuning
            //F0 7F devID 04 04 00 msb F7 (default=40. from 00 to 7F)
            break;

         case 0x41:  // Roland
            ProcessGSSysEx(bufpos, len);
            break;

         case 0x43:  // Yamaha
            ProcessXGSysEx(bufpos, len);
            break;

         case 0x7D:  // Non commercial
            ProcessMaliceXSysEx(bufpos, len);
            break;
      }
   }
}

void
   OPLSynth::
   ProcessGSSysEx(const Bit8u *bufpos, DWORD len)
{
   // TODO handle equivalent GM-2/XG commands?

}

void
   OPLSynth::
   ProcessXGSysEx(const Bit8u *bufpos, DWORD len)
{
   if (len >= 5 && (bufpos[4] == 0x00 || bufpos[4] == 0x30))
   {
      switch((len >= 7) ? bufpos[6] : 0xff)
      {
         case 0x00:
            // Master Tuning  (aaH = 1000H + bbH x 0100H + ccH * 0010H + ddH * 0001H)-0400H (in 0.1 cent units)
            //F0 43 1n 4C 00 00 00 aa bb cc dd F7 (default: 00 04 00 00)
            if (bufpos[4]==0x30)
            {

            }

            // MIDI Master Tuning
            //F0 43 1n 27 30 00 00 0m(sb) 0l(sb) cc(ignored) F7  (default=00 40)
            else
            {

            }
            break;

         // Master Transpose (melodic notes only, non-realtime)
         //F0 43 1n 4c 00 00 06 xx F7 (40=default)
         case 0x06:
            if (len < 8)
               break;
            m_bMasterCoarseTune = bufpos[7] - 0x40;
            // do not update?
            break;

         // Master Volume
         //F0 43 1n 4C 00 00 04 xx F7 (default=7F, from 00 to 7F)
         case 0x04:
            break;

         // Master Attenuator
         //F0 43 1n 4C 00 00 05 xx F7 (default=00, from 00 to 7F) - opposite of above?
         case 0x05:
            break;


      }
   }

   // Multipart data, nn=channel (00 to 0F)
   else if (len >= 8 && (bufpos[4] == 0x08))
   {
      BYTE bChannel = bufpos[5] & 0xF,
           bVal     = bufpos[7] & 0x7F;

      switch (bufpos[6])
      {
         // Bank Select MSB
         //F0 43 1n 4C 08 nn 01 xx F7
         case 0x01:
            Opl3_UpdateBankSelect(1, bChannel, bVal);
            break;

         // Bank Select LSB
         //F0 43 1n 4C 08 nn 02 xx F7
         case 0x02:
            Opl3_UpdateBankSelect(0, bChannel, bVal);
            break;

         // Program Change
         //F0 43 1n 4C 08 nn 03 xx F7
         case 0x03:
            m_bPatch[bChannel] = bVal;
            break;

         // Mono / Poly
         //F0 43 1n 4C 08 nn 05 xx F7 (00 or 01, default=01-poly)
         case 0x05:
            m_wMonoMode = (bVal == 0) ?
               m_wMonoMode |  (1<<bChannel) :
               m_wMonoMode & ~(1<<bChannel);

            break;

         // Part mode (lazy implementation, enable drum mode for nonzero value)
         //F0 43 1n 4C 08 nn 07 xx F7 (00-nomral, 01...nn=drum)
         case 0x07:
            m_wDrumMode = (bVal == 0) ?
               m_wDrumMode & ~(1<<bChannel) :
               m_wDrumMode |  (1<<bChannel);

            break;

         // Note Shift (coarse tune.  lazy implementation: percussion will detune instead of shift notes and update instantaneously)
         //F0 43 1n 4C 08 nn 08 xx F7 (default=40)
         case 0x08:
            m_bCoarseTune[bChannel] = bVal;
            Opl3_PitchBend(bChannel, m_iBend[bChannel]);
            break;

         // Detune (fine tune)
         //F0 43 1n 4C 08 nn 09/0A xx xx F7  (default=08 00.  range=00 00 to 0F 0F)

         // Volume
         //F0 43 1n 4C 08 nn 0B xx F7 (default=64, from 00 to 7F)
         case 0x0B:
            m_curVol[bChannel] = bVal;
            Opl3_ChannelVolume(bChannel, bVal);
            break;

         // Velocity Sensitivity (TODO?)
         //F0 43 1n 4C 08 nn 0D xx F7 (default=40, from 00 to 7F)

         // Pan
         //F0 43 1n 4C 08 nn 0E xx F7 (default=40, from 00 to 7F)

         // Vibrato Rate
         //F0 43 1n 4C 08 nn 15 xx F7 (default=40, from 00 to 7F)

         // Vibrato Depth
         //F0 43 1n 4C 08 nn 16 xx F7 (default=40, from 00 to 7F)

         // Vibrato Delay
         //F0 43 1n 4C 08 nn 17 xx F7 (default=40, from 00 to 7F)

         // LPF Cutoff (Brightness)
         //F0 43 1n 4C 08 nn 18 xx F7 (default=40, from 00 to 7F)

         // Attack
         //F0 43 1n 4C 08 nn 1A xx F7 (default=40, from 00 to 7F)

         // Decay  (NRPN 1H/64H)
         //F0 43 1n 4C 08 nn 1B xx F7 (default=40, from 00 to 7F)

         // Release
         //F0 43 1n 4C 08 nn 1C xx F7 (default=40, from 00 to 7F)

         // Portamento switch
         //F0 43 1n 4C 08 nn 67 xx F7 (default=00, from 00 to 01)
         case 0x67:
            m_wPortaMode = (bVal > 0x3F) ?
               m_wPortaMode | (1<<bChannel) :
               m_wPortaMode & ~(1<<bChannel) ;
            break;

         // Portamento time
         //F0 43 1n 4C 08 nn 68 xx F7 (default=00, from 00 to 7F)
         case 0x68:
            Opl3_SetPortamento(bChannel, bVal);
            break;

         // Pitch EG init level (not supported)
         //F0 43 1n 4C 08 nn 69 xx F7 (default=40, from 00 to 7F)

         // Pitch EG attack time (not supported) 6A

         // Pitch EG Release level (not supported) 6B

         // Pitch EG Release Time (not supported) 6C


      }

      // Drum part commands

      // Drum Note Pitch Coarse (not supported) (3n = drum part n, rr = drum note number)
      //F0 43 1n 4C 3n rr 00 xx F7 (default=40, from 00 to 7F)

      // Drum Note Pitch File (not supported)
      //F0 43 1n 4C 3n rr 01 xx F7 (default=40, from 00 to 7F)
   }
}

static void
   MemCopy2x4To8(BYTE *dst, const BYTE *src, DWORD dstlen)
{
   // Combine consecutive nibble pairs into bytes
   //(to decode sysex payload which can only use 7 bits of each byte)
   for (DWORD i = 0; i < dstlen; ++i)
   {
      DWORD lsn = src[2*i+0] & 0xf;
      DWORD msn = src[2*i+1] & 0xf;
      dst[i] = (msn << 4) | lsn;
   }
}

void
   OPLSynth::
   ProcessMaliceXSysEx(const Bit8u *bufpos, DWORD len)
{
   // Check the message is for our device ID
   if (bufpos[2] != m_bSysexDeviceId)
       return;

   switch ((len >= 7) ? bufpos[4] : 0xff)
   {
   case 0x01:
      // Request
      //F0 7D [device-id] 00 01 [data...] [checksum] F7
      bufpos += 5; len -= 7;

      if (len < 8 || memcmp(bufpos, gbMaliceXIdentifier, 8))
         break;
      bufpos += 8; len -= 8;

      // TODO maybe handle at some point
      (void)bufpos; (void)len;
      break;
   case 0x02:
      // Send
      //F0 7D [device-id] 00 02 [data...] [checksum] F7
      bufpos += 5; len -= 7;

      if (len < 8 || memcmp(bufpos, gbMaliceXIdentifier, 8))
         break;
      bufpos += 8; len -= 8;

      const char *tag = (const char *)bufpos;
      if (len < 8)
         break;
      bufpos += 8; len -= 8;

      if (!memcmp(tag, "OP3PATCH", 8))
      {
         if (len < 4)
             break;
         BYTE insno;
         BYTE patchlen;
         MemCopy2x4To8(&insno, bufpos, 1);
         bufpos += 2; len -= 2;
         MemCopy2x4To8(&patchlen, bufpos, 1);
         bufpos += 2; len -= 2;

         if (patchlen < 28 || len/2 < patchlen)
             break;
         Patch patch = {};
         if (patchlen > sizeof(Patch))
            patchlen = sizeof(Patch);
         MemCopy2x4To8((BYTE *)&patch, bufpos, patchlen);

         // Check validity
         if (patch.bOp > PATCH_1_2OP)
             break;

         // Set the patch
         glpPatch[insno] = patch;
      }
      else if (!memcmp(tag, "OP3MLMAP", 8))
      {
          if (len < 3)
             break;
         BYTE insno = *bufpos & 0x7f;
         bufpos += 1; len -= 1;
         BYTE maplen;
         MemCopy2x4To8(&maplen, bufpos, 1);
         bufpos += 2; len -= 2;

         if (len/2 < maplen)
            break;
         MelMap map = {};
         if (maplen > sizeof(MelMap))
            maplen = sizeof(MelMap);
         MemCopy2x4To8((BYTE *)&map, bufpos, maplen);

         // Byte swap fields, file to native
         map.wBaseTranspose = jsl::int_of_be16(map.wBaseTranspose);
         map.wSecondTranspose = jsl::int_of_be16(map.wSecondTranspose);
         map.wPitchEGAmt = jsl::int_of_be16(map.wPitchEGAmt);
         map.wPitchEGTime = jsl::int_of_be16(map.wPitchEGTime);
         map.wBaseFineTune = jsl::int_of_be16(map.wBaseFineTune);
         map.wSecondFineTune = jsl::int_of_be16(map.wSecondFineTune);

         // Check validity
         /**/

         // Set it
         gbMelMap[insno] = map;
      }
      else if (!memcmp(tag, "OP3PCMAP", 8))
      {
         if (len < 3)
            break;
         BYTE insno = *bufpos & 0x7f;
         bufpos += 1; len -= 1;
         BYTE maplen;
         MemCopy2x4To8(&maplen, bufpos, 1);
         bufpos += 2; len -= 2;

         if (len/2 < maplen)
            break;
         PercMap map = {};
         if (maplen > sizeof(PercMap))
            maplen = sizeof(PercMap);
         MemCopy2x4To8((BYTE *)&map, bufpos, maplen);

         // Check validity
         if (map.bBaseNote >= 128)
             break;

         // Set it
         gbPercMap[insno] = map;
      }
      break;
   }
}

void
   OPLSynth::
   Opl3_SoftCommandReset()
{
   for (int i = 0; i < NUM2VOICES; ++i)
   {
      Opl3_CutVoice(i, true);
      //m_Voice[i].bPatch = 0;
      //m_Voice[i].dwTime = 0;
      memset(&m_Voice[i], 0, sizeof(Voice));
      m_Voice[i].bChannel = ~0;
   }

   for (int i = 0; i < NUMMIDICHN; ++i)
   {
      m_iBend[i] = 0;          // central value
      m_iBendRange[i] = 2;     // -/+2 semitones
      m_curVol[i] = 100;       // CC7 = 100
      m_iExpThres[i] = 127;    // CC11 = max
      m_bChanAtten[i] = 4;     // default attenuation value
      m_bLastNoteUsed[i] = ~0;
      m_bLastVoiceUsed[i] = ~0;
      m_bLastNoteUsed[i] = ~0;
      m_bPatch[i] = 0;
      m_bSustain[i] = 0;
      m_bSostenuto[i] = 0;
      m_bPortaTime[i] = 0;
      m_bStereoMask[i] = 0xff;
      m_bModWheel[i] = 0;
      m_bRelease[i] = 64;
      m_bAttack[i] = 64;
      m_bBrightness[i] = 64;
      m_noteHistory[i].clear();
      m_sostenutoBuffer[i].clear();
      m_bCoarseTune[i] = 64;
      m_bFineTune[i] = 64;
      m_bRPNCount[i] = 0;
      memset(m_RPN[i], -1, sizeof(WORD));
      memset(m_NRPN[i], -1, sizeof(WORD));
      memset(m_bDataEnt[i], -1, sizeof(WORD));
      memset(m_bBankSelect[i], ((i==9)?(127<<8):0), sizeof(WORD));
   }

   b4OpVoiceSet = 0;         // disable 4op mode by default
   Opl3_ChipWrite(AD_CONNECTION, (BYTE)(b4OpVoiceSet));

   m_wDrumMode = (1<<9);     // Ch10 Drums
   m_wMonoMode = 0;          // Set all channels to polyphonic mode
   m_wPortaMode = 0;
   m_dwCurTime = 0;
}

inline void
   OPLSynth::
   Opl3_ChipWrite(WORD idx, BYTE val)
{
   // Write to software chip
   OPL3_WriteReg(&m_Miniport, idx, val);
}

void
   OPLSynth::
   close()
{
   // Reset board
   Opl3_BoardReset();
}

#include <stdint.h>
#include "multiplexer.h"
#include "HV5812.h"
#include <Arduino.h>

void multiplexer(uint8_t fieldMux[]) {
  static unsigned long currMuxTime;
  static unsigned long prevMuxTime;
  static long contentSreg;

  static uint8_t muxGate;
  static uint8_t muxCnt;

  
    currMuxTime = millis(); 
    if(currMuxTime-prevMuxTime >= muxInt) {
      prevMuxTime = currMuxTime;   
      contentSreg = ((seg_7[fieldMux[muxGate+3]] << 8) | seg_7[fieldMux[muxGate]] | (1<<gate[muxGate]));
      muxCnt++;
      if (muxCnt < 100) {
 
        if (muxGate == 1) {
          contentSreg |= TAG_DP;
        }
        if (muxGate == 2) {
          contentSreg |= MONAT_DP;
        }
      }
      else {
        if (muxCnt > 200) {
          muxCnt = 0;     
        }
      }
      shiftHV5812( contentSreg);
      muxGate++;
      if (muxGate > 2) {
        muxGate = 0;
      }
      contentSreg = 0;
    }
  }


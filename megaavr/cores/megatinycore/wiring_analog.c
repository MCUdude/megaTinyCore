/*
  wiring_analog.c - analog input and output
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA

  Modified 28 September 2010 by Mark Sproul
*/

#include "wiring_private.h"
#include "pins_arduino.h"
#include "Arduino.h"



inline __attribute__((always_inline)) void check_valid_digital_pin(pin_size_t pin) {
  if(__builtin_constant_p(pin)) {
    if (pin >= NUM_TOTAL_PINS && pin != NOT_A_PIN)
    // Exception made for NOT_A_PIN - code exists which relies on being able to pass this and have nothing happen.
    // While IMO very poor coding practice, these checks aren't here to prevent lazy programmers from intentionally
    // taking shortcuts we disapprove of, but to call out things that are virtually guaranteed to be a bug.
    // Passing -1/255/NOT_A_PIN to the digital I/O functions is most likely intentional.
      badArg("digital I/O function called  is constant, but not a valid pin");
  }
}

inline __attribute__((always_inline)) void check_valid_analog_pin(pin_size_t pin) {
    if(__builtin_constant_p(pin)) {
    #ifdef ADC_LOWLAT_bm
      badCall("analog functionality not yet available for 2-series");
    #else
      #ifdef ADC_DAC0
        if (pin != ADC_DAC0 && pin != ADC_INTREF && pin != ADC_TEMPERATURE)
      #else
        if (pin != ADC_INTREF && pin != ADC_TEMPERATURE)
      #endif
      {
        pin = digitalPinToAnalogInput(pin);
        if (pin == NOT_A_PIN) {
          badArg("analogRead called with constant pin that is not a valid analog pin");
        }
      }
    #endif
  }
}

inline __attribute__((always_inline)) void check_valid_analog_ref(uint8_t mode) {
  if (__builtin_constant_p(mode)) {
    #ifdef ADC_LOWLAT_bm
      badCall("analog functionality not yet available for 2-series");
    #else
      #if defined(EXTERNAL)
        if (!(mode == EXTERNAL || mode == VDD || mode== INTERNAL0V55 || mode== INTERNAL1V1 || mode== INTERNAL1V5|| mode== INTERNAL2V5|| mode== INTERNAL4V34))
          badArg("analogReference called with argument that is not a valid analog reference");
      #else
        if (!(mode == VDD || mode== INTERNAL0V55 || mode== INTERNAL1V1 || mode== INTERNAL1V5|| mode== INTERNAL2V5|| mode== INTERNAL4V34))
          badArg("analogReference called with argument that is not a valid analog reference");
      #endif
    #endif
  }
}

inline __attribute__((always_inline)) void check_valid_dac_ref(uint8_t mode) {
  if (__builtin_constant_p(mode)) {
    if (mode >= 5)
      badArg("DACReference called with argument that is not a valid DAC reference - VDD is not a valid reference tinyAVR parts!");
  }
}

inline __attribute__((always_inline)) void check_valid_duty_cycle(int16_t val) {
  if (__builtin_constant_p(val)) {
    if (val < 0 || val >255)
      badArg("analogWrite duty cycle called with a constant not between 0 and 255");
  }
}

#ifdef ADC_LOWLAT_bm
  // 2-series part with the crazy ADC
  // need a variable to store the resolution - details will be changed
  static uint8_t _analog_resolution=10;

#endif


void analogReference(uint8_t mode) {
#ifdef ADC_LOWLAT_bm
  return badCall("analog functionality not yet available for 2-series");
#else
  check_valid_analog_ref(mode);
  switch (mode) {
    #if defined(EXTERNAL)
      case EXTERNAL:
    #endif
    case VDD:
      VREF.CTRLB &= ~VREF_ADC0REFEN_bm; //Turn off force-adc-reference-enable
      ADC0.CTRLC = (ADC0.CTRLC & ~(ADC_REFSEL_gm)) | mode | ADC_SAMPCAP_bm; //per datasheet, recommended SAMPCAP=1 at ref > 1v - we don't *KNOW* the external reference will be >1v, but it's probably more likely...
      // VREF.CTRLA does not need to be reconfigured, as the voltage references only supply their specified voltage when requested to do so by the ADC.
      break;
    case INTERNAL0V55:
      VREF.CTRLA =  VREF.CTRLA & ~(VREF_ADC0REFSEL_gm); //These bits are all 0 for 0.55v reference, so no need to do the mode << VREF_ADC0REFSEL_gp here;
      ADC0.CTRLC = (ADC0.CTRLC & ~(ADC_REFSEL_gm | ADC_SAMPCAP_bm)) | INTERNAL; //per datasheet, recommended SAMPCAP=0 at ref < 1v
      VREF.CTRLB |= VREF_ADC0REFEN_bm; //Turn off force-adc-reference-enable
      break;
    case INTERNAL1V1:
    case INTERNAL2V5:
    case INTERNAL4V34:
    case INTERNAL1V5:
      VREF.CTRLA = (VREF.CTRLA & ~(VREF_ADC0REFSEL_gm)) | (mode << VREF_ADC0REFSEL_gp);
      ADC0.CTRLC = (ADC0.CTRLC & ~(ADC_REFSEL_gm)) | INTERNAL | ADC_SAMPCAP_bm; //per datasheet, recommended SAMPCAP=1 at ref > 1v
      break;
  }
#endif
}

#ifdef DAC0
void DACReference(uint8_t mode) {
  check_valid_dac_ref(mode);
  if (mode < 5) {
    VREF.CTRLA = mode | (VREF.CTRLA & (~VREF_DAC0REFSEL_gm));
  }
}
#else
void DACReference(__attribute__ ((unused))uint8_t mode) {
  badCall("DACreference is not available - this part does not have a DAC");
}
#endif


int analogRead(uint8_t pin) {
#ifdef ADC_LOWLAT_bm
  //badCall("analog functionality not yet available for 2-series");
  return -1;
#else
  check_valid_analog_pin(pin);


  #ifdef ADC_DAC0
  if (pin != ADC_DAC0 && pin != ADC_INTREF && pin != ADC_TEMPERATURE)
  #else
  if (pin != ADC_INTREF && pin != ADC_TEMPERATURE)
  #endif
  {
    pin = digitalPinToAnalogInput(pin);
    if (pin == NOT_A_PIN) {
      return -1;
    }
  }

  /* Reference should be already set up */
  /* Select channel */
  ADC0.MUXPOS = (pin << ADC_MUXPOS_gp);

  /* Start conversion */
  ADC0.COMMAND = ADC_STCONV_bm;

  /* Wait for result ready */
  while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));

  /* Combine two bytes */
  return ADC0.RES;
#endif
}

//analogReadResolution() has two legal values you can pass it, 8 or 10 on these parts. According to the datasheet, you can clock the ADC faster if you set it to 8.
//like the pinswap functions, if the user passes bogus values, we set it to the default and return false.

inline void analogReadResolution(uint8_t res) {
#ifdef ADC_LOWLAT_bm
  return badCall("analog functionality not yet available for 2-series");
#else
  if (!__builtin_constant_p(res))
    badArg("analogReadResolution must only be passed constant values");
  #ifdef ADC_LOWLAT_bm
    if (res !=8 && res != 10 && res != 12)
      badArg("analogReadResolution called with invalid argument - valid options are 8, 12, or 10 (compatibility mode).");
    _analog_resolution=res; //just set that variable? *shrug* is just a standin for now.
  #else
    if (res !=8 && res != 10)
      badArg("analogReadResolution called with invalid argument - valid options are 8 or 10.");
    if (res == 8) {
      ADC0.CTRLA |= ADC_RESSEL_bm;
    } else {
      ADC0.CTRLA &= ~ADC_RESSEL_bm;
    }
  #endif
#endif
}


// PWM output only works on the pins with
// hardware support.  These are defined in the variant
// pins_arduino.h file.  For the rest of the pins, we default
// to digital output.
void analogWrite(uint8_t pin, int val) {
  check_valid_digital_pin(pin);
  check_valid_duty_cycle(val);
  uint8_t bit_mask  = digitalPinToBitMask(pin);
  if (bit_mask == NOT_A_PIN) {
    return;
  }
  // Set pin output because that's what Arduino does
  // Moved this way down to the end, why enable output before we have the pin doing the right thing?
  // pinMode(pin, OUTPUT);

  /* Get timer */
  /* megaTinyCore only - assumes only TIMERA0, TIMERD0, or DACOUT
   * can be returned here, all have only 1 bit set, so we can use
   * PeripheralControl as a mask to see if they have taken over
   * any timers with minimum overhead - critical on these parts
   * Since nothing that will show up here can have more than one
   * one bit set, binary and will give 0x00 if that bit is cleared
   * which is NOT_ON_TIMER.
   */
  uint8_t digital_pin_timer =  digitalPinToTimer(pin) & PeripheralControl;
  /* end megaTinyCore-specific section */

  uint8_t *timer_cmp_out;
  /* Find out Port and Pin to correctly handle port mux, and timer. */
  switch (digital_pin_timer) {
    case TIMERA0:
      if (val <= 0) { /* if zero or negative drive digital low */
        digitalWrite(pin, LOW);
      } else if (val >= 255) { /* if max or greater drive digital high */
        digitalWrite(pin, HIGH);
      } else {
        /* Calculate correct compare buffer register */
        #ifdef __AVR_ATtinyxy2__
        if (bit_mask == 0x80) {
          bit_mask = 1;  //on the xy2, WO0 is on PA7
        }
        #endif
        uint8_t offset=0;
        if (bit_mask > 0x04) { // HCMP
          bit_mask <<= 1;      // mind the gap
          offset = 1;          // used to offset the
        }
        if      (bit_mask & 0x44) offset += 4;
        else if (bit_mask & 0x22) offset += 2;
        timer_cmp_out = ((uint8_t *)(&TCA0.SPLIT.LCMP0)) + (offset);
        (*timer_cmp_out) = (val);
        TCA0.SPLIT.CTRLB |= bit_mask;
      }
      break;
      // End of TCA case

  #if defined(DAC0)
    case DACOUT:
    {
      DAC0.DATA = val;
      DAC0.CTRLA = 0x41; //OUTEN=1, ENABLE=1
      break;
    }
  #endif
  #if (defined(TCD0) && defined(USE_TIMERD0_PWM))
    case TIMERD0:
      {
      #ifndef NO_GLITCH_TIMERD0
        if (val < 1) { /* if zero or negative drive digital low */
          digitalWrite(pin, LOW);
        } else if (val > 254) { /* if max or greater drive digital high */
          digitalWrite(pin, HIGH);
        } else {
      #endif
        // Calculation of values to write to CMPxSET
        // val is 1~254, so 255-val is 1~254. so we double it and subtract 1, now we have odd number between 1 and 507, corresponding to an even number of counts.
        // Timer counts to 509, 510 counts including 0... so this gives us our promised 1/255~254/255 duty cycles, not 2/511th~509/511th...

        // Now, if NO_GLITCH_TIMERD0 is defined, val can be anything...
        #if defined(NO_GLITCH_TIMERD0)
          uint8_t set_inven=0;
          if (val < 1) {
            val=0; //this will "just work", we'll set it to the maximum, it will never match, and will stay LOW
          } else if (val > 254) {
            val=0; //here we *also* set it to 0 so it would stay LOW
            set_inven=1; //but we invert the pin output with INVEN!
          }
        #endif

        uint8_t oldSREG=SREG;
        cli(); //interrupts off... wouldn't due to have this mess interrupted and messed with...
        while (!(TCD0.STATUS & (TCD_ENRDY_bm | TCD_CMDRDY_bm ))); //if previous sync/enable in progress, wait for it to finish.
        // with interrupts off since an interrupt could trip these...
        //set new values
        uint8_t fc_mask;
        if (bit_mask == 2) {  //PIN_PC1
          TCD0.CMPBSET = ((255 - val) << 1)-1;
          fc_mask = 0x80;
        } else {        //PIN_PC0
          TCD0.CMPASET = ((255 - val) << 1)-1;
          fc_mask = 0x40;
        }

        if (!(TCD0.FAULTCTRL & fc_mask)) {
          //if it's not active, we need to activate it... which produces a glitch in the PWM

          TCD0.CTRLA &= ~TCD_ENABLE_bm; //stop the timer
          _PROTECTED_WRITE(TCD0.FAULTCTRL, TCD0.FAULTCTRL | fc_mask);
          while (!(TCD0.STATUS & TCD_ENRDY_bm)); // wait until we can re-enable it
          TCD0.CTRLA |= TCD_ENABLE_bm; //re-enable it
        } else {
          TCD0.CTRLE = TCD_SYNCEOC_bm; //Synchronize
        }

        #if defined(NO_GLITCH_TIMERD0)
          // We only support control of the TCD0 PWM functionality on PIN_PC0 and PIN_PC1 (on 20 and 24 pin parts )
          // so if we're here, we're acting on either PC0 or PC1.
          if (set_inven == 0){
            // we are not setting invert to make the pin HIGH when not set; either was 0 (just set CMPxSET > CMPBCLR)
            // or somewhere in between.
            if (bit_mask == 1){
              PORTC.PIN0CTRL &= ~(PORT_INVEN_bm);
            } else {
              PORTC.PIN1CTRL &= ~(PORT_INVEN_bm);
            }
          } else {
            // we *are* turning off PWM while forcing pin high - analogwrite(pin,255) was called on TCD0 PWM pin...
            if (bit_mask == 1){
              PORTC.PIN0CTRL |= PORT_INVEN_bm;
            } else {
              PORTC.PIN1CTRL |= PORT_INVEN_bm;
            }
          }
        #endif
        SREG=oldSREG;
      }
      break;
    #endif
    //end of TCD0 code

    /* If non timer pin, or unknown timer definition.  */
    /* do a digital write  */
    case NOT_ON_TIMER:
    default:
      if (val < 128) {
        digitalWrite(pin, LOW);
      } else {
        digitalWrite(pin, HIGH);
      }
      break;
  } //end of switch/case
  // Now that everything is said and done, we've set the pin high or low as if it's not a PWM pin, or told the timer to give it PWM if it is - this is a better timwe to finally turn on the output drivers.
  // True, it is at most 1-2 PWM timer ticks under typical settings, it's probably at least 1 tick, maybe several at 1 MHz (haven't timed analogWrite lately)
  pinMode(pin, OUTPUT);
} // end of analogWrite

void takeOverTCA0() {
  TCA0.SPLIT.CTRLA = 0;          // Stop TCA0
  PeripheralControl &= ~TIMERA0; // Mark timer as user controlled
                                 // Reset TCA0
  /* Okay, seriously? The datasheets and io headers disagree here */
  TCA0.SPLIT.CTRLESET = TCA_SPLIT_CMD_RESET_gc | 0x03;
}

#if defined(TCD0)
void takeOverTCD0() {
  TCD0.CTRLA = 0;                     // Stop TCD0
  _PROTECTED_WRITE(TCD0.FAULTCTRL,0); // Turn off all outputs
  PeripheralControl &= ~TIMERD0;      // Mark timer as user controlled
}
#endif

/* --COPYRIGHT--,BSD
 * Copyright (c) 2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//*****************************************************************************
//  MSP430FR235x Demo - ADC, Sample A1, internal 1.5V Ref, TB1.1 Trig, Set P1.2 if A1>0.5V
//
//  Description: This example works on Repeat-Single-Channel Mode.
//  A1 is sampled 16/second (ACLK/1024) with reference to 1.5V.
//  Timer_B is run in upmode and TB1.1B is used to automatically trigger
//  ADC conversion. Internal oscillator times sample (16x) and conversion(13x).
//  Inside ADC_ISR if A1 > 0.5V, P1.2 is set, else reset. Normal mode is LPM3.
//  ACLK = XT1 = 32768Hz, MCLK = SMCLK = default DCODIV ~1MHz.
//  //* An external watch crystal on XIN XOUT is required for ACLK *//
//
//                MSP430FR2355
//             -----------------
//         /|\|                 |
//          | |                 |
//          --|RST              |
//            |                 |
//        >---|P1.1/A1      P1.2|--> LED
//
//
//   Cash Hao
//   Texas Instruments Inc.
//   November 2016
//   Built with IAR Embedded Workbench v6.50.0 & Code Composer Studio v6.2.0
//******************************************************************************
//         Team Rocket Firmware
//
// P1.1_UCB0CLK_ACLK_OA0A_COMP0.1_A1 ---> SWITCH output signal (to nichrome heating)
// P5.2_TB2CLK_A10 ---> ADC input signal (from thermistor)
//
// Description: will probably use repeated-single-channel mode for ADC. A10 is sampled
// [at a certain frequency, maybe lowest possible to save energy?] with reference to 2.5V
// [our voltage input range is 0V~2.5V]. 
// [12-bit conversion]
// Inside ADC_ISR if A10 < [targetTemp], P1.1 is set, else cleared. 
// [need to also select an ADC clock source: use auxiliary clock ACLK (suitable for low frequency)]
// deep sleep mode 
//
// ******************************************************************************
//
// pseudo code:
// 1. stop WDT (done)
// 2. configure GPIO: set P1.1 to output direction, clear P1.1 (done); set P5.2 to input direction
// 3. configure ADC A10 pin
// 4. configure XT1CLK oscillator
// 5. disable the GPIO power-on default high-impedance mode to activate previously configured port settings
// 6. set ACLK = XT1
// 7. configure ADC: ADCON, repeat single channel, 12 bit resolution, A10 ADC input select, Vref = 2.5V, enable ADC conv complete interrupt
// 8. configure reference: unlock PMM registers, enable 2.5V internal reference, delay for reference setting, ADC enable
// 9. ADC ISR: if ADCMEM0 < TARGET set P1.1 else clear P1.1


#include <msp430.h>
#include <stdbool.h>
#include <stdint.h>

// #define TARGET // find out voltage at 50 deg celcius

uint16_t currentTemp; // declare variable for current temperature

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;                                 // Stop WDT

    // Configure GPIO
    P1DIR |= BIT1;                                            // Set P1.1 to output direction
    P1OUT &= ~BIT1;                                           // Clear P1.1

    // Configure ADC A1 pin
    // P1SEL0 |= BIT1;
    // P1SEL1 |= BIT1;
    
    // configure ADC A10 pin (P5.2)
    P5SEL0 |= BIT2;
    P5SEL1 |= BIT2;

    // Configure XT1 oscillator
    P2SEL1 |= BIT6 | BIT7;                                    // P2.6~P2.7: crystal pins

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;

    CSCTL4 = SELA__XT1CLK;                                    // Set ACLK = XT1; MCLK = SMCLK = DCO
    do
    {
        CSCTL7 &= ~(XT1OFFG | DCOFFG);                        // Clear XT1 and DCO fault flag
        SFRIFG1 &= ~OFIFG;
    }while (SFRIFG1 & OFIFG);                                 // Test oscillator fault flag

    // Configure ADC
    ADCCTL0 |= ADCON | ADCMSC;                                // ADCON
    ADCCTL1 |= ADCSHP | ADCSHS_2 | ADCCONSEQ_2;               // repeat single channel; TB1.1 trig sample start
    ADCCTL2 &= ~ADCRES;                                       // clear ADCRES in ADCCTL
    ADCCTL2 |= ADCRES_2;                                      // 12-bit conversion results
    ADCMCTL0 |= ADCINCH_1 | ADCSREF_1;                        // A1 ADC input select; Vref=1.5V
    ADCIE |= ADCIE0;                                          // Enable ADC conv complete interrupt

    // Configure reference
    PMMCTL0_H = PMMPW_H;                                      // Unlock the PMM registers
    PMMCTL2 |= INTREFEN | REFVSEL_0;                          // Enable internal 1.5V reference
    __delay_cycles(400);                                      // Delay for reference settling

    ADCCTL0 |= ADCENC;                                        // ADC Enable


    // can get rid of this following part
    // ADC conversion trigger signal - TimerB1.1 (32ms ON-period)
    TB1CCR0 = 1024-1;                                         // PWM Period
    TB1CCR1 = 512-1;                                          // TB1.1 ADC trigger
    TB1CCTL1 = OUTMOD_4;                                      // TB1CCR0 toggle
    TB1CTL = TBSSEL__ACLK | MC_1 | TBCLR;                     // ACLK, up mode

    __bis_SR_register(LPM0_bits | GIE);                       // Enter LPM3 w/ interrupts
}

// ADC interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC_VECTOR
__interrupt void ADC_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC_VECTOR))) ADC_ISR (void)
#else
#error Compiler not supported!
#endif
{
    switch(__even_in_range(ADCIV,ADCIV_ADCIFG))
    {
        case ADCIV_NONE:
            break;
        case ADCIV_ADCOVIFG:
            break;
        case ADCIV_ADCTOVIFG:
            break;
        case ADCIV_ADCHIIFG:
            break;
        case ADCIV_ADCLOIFG:
            break;
        case ADCIV_ADCINIFG:
            break;
        case ADCIV_ADCIFG:
            if (ADCMEM0 < 0x555)                             // ADCMEM = A0 < 0.5V?
                P1OUT &= ~BIT2;                              // Clear P1.2 LED off
            else
                P1OUT |= BIT2;                               // Set P1.2 LED on
            ADCIFG = 0;
            break;                                           // Clear CPUOFF bit from 0(SR)
        default:
            break;
    }
}

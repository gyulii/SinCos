
/* Hasznos link a szogszamitas matematikai levezetesenek megertesehez:
https://www.ti.com/lit/ug/tidua05a/tidua05a.pdf?ts=1611819649961&ref_url=https%253A%252F%252Fwww.google.com%252F

Link az iqmath leirasahoz:
https://www.ti.com/lit/ug/sprugg9/sprugg9.pdf?ts=1624959457335&ref_url=https%253A%252F%252Fwww.google.com%252F  */

//#define NDEBUG
#define ToRemove 0



#include <math.h>

#define   MATH_TYPE      IQ_MATH
#define GLOBAL_Q 19                 //Tesztfuggveny alapjan kell az Q19 vagy annal kisebb.
#include "IQmathLib.h"

/*Angle calculation */

typedef struct angles
{
    volatile _iq angle_in_fixed_fine;
    volatile _iq angle_in_fixed;
    volatile float angle_fine; // Fine Resolution Angle Calculation in radian
    volatile float angle_coarse; // Coarse Resolution Angle Calculation in degree
    volatile float angle; // Interpolated High-Resolution Angle Calculation in degree
    volatile float angle_fine_quadrant;

}angle_t;

// VÁLTOZÓ DEFINIÁLÁS
#ifndef NDEBUG
volatile Uint16  ConversionCount;
#endif


volatile angle_t angles;


#ifndef NDEBUG
volatile Uint16  Voltage1[300];
volatile Uint16  Voltage2[300];
#endif


/* SEGEDVALTOZOK FINE ANGLE SZAMOLASHOZ */
volatile float g_float_temp = 0;
int shifted_channel_A,shifted_channel_B;

#if 0
int teszt_shifted_channel_A,teszt_shifted_channel_B;
#endif


#ifndef NDEBUG
float tarolo[300];
float tarolo_coarse[300];
float tarolo_QEP[300];
float tarolo_fix[300];
#endif

/* QEP Globals */

volatile int32  g_qepCounter;

/*   ADC Globals       */

volatile Uint16 g_AdcChanel_A;
volatile Uint16 g_AdcChanel_B;
volatile Uint16 g_adc_avg;

/*Adc average calibration*/

volatile Uint16  g_min_value_actual = 50000;
volatile Uint16  g_min_value_result = 55000;
volatile Uint16  g_max_value_actual = 500;
volatile Uint16  g_max_value_result = 1000;

#if 0
/* iq-hoz letrehozott valtozok */
float teszt_g_max_value_actual=1;
float teszt_g_max_value_result=2;
#endif


void adc_reinit_for_next_measurment()
{
    AdcRegs.ADCTRL2.bit.RST_SEQ1 = 1; // Reset SEQ1
    AdcRegs.ADCST.bit.INT_SEQ1_CLR = 1; // Clear INT SEQ1 bit
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
}

int AdcReadValue_Channel_A (void)
{
    return (AdcMirror.ADCRESULT0);
}

int AdcReadValue_Channel_B (void)
{
   return (AdcMirror.ADCRESULT1);
}

void adc_config()
{
    AdcRegs.ADCTRL3.bit.SMODE_SEL = 0x1;
    AdcRegs.ADCMAXCONV.all = 0x0001; // Setup 2 conv's on SEQ1
    AdcRegs.ADCCHSELSEQ1.bit.CONV00 = 0x6; // Setup ADCINA3 as 1st SEQ1 conv.
    AdcRegs.ADCTRL2.bit.EPWM_SOCA_SEQ1 = 1;    // Enable SOCA from ePWM to start SEQ1
    AdcRegs.ADCTRL2.bit.INT_ENA_SEQ1 = 1; // Enable SEQ1 interrupt (every EOS)
}


void epwm_config()
{
    // Assumes ePWM1 clock is already enabled in InitSysCtrl();
    EPwm1Regs.ETSEL.bit.SOCAEN = 1; // Enable SOC on A group
    EPwm1Regs.ETSEL.bit.SOCASEL = 2; // Enable event time-base counter equal to period
    EPwm1Regs.ETPS.bit.SOCAPRD = 1; // Generate pulse on 1st event
    EPwm1Regs.TBPRD = 6000; // Set period for ePWM1 -> 50 kHz Sample
    EPwm1Regs.TBCTL.bit.CTRMODE = 0; // count up and start
}


void QepGpioInit(void)
{
   EALLOW;
   /* Pull up*/
    GpioCtrlRegs.GPAPUD.bit.GPIO24 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO25 = 0;
    GpioCtrlRegs.GPAPUD.bit.GPIO26 = 0;
    /* Clk enable */
    GpioCtrlRegs.GPAQSEL2.bit.GPIO24 = 0;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO25 = 0;
    GpioCtrlRegs.GPAQSEL2.bit.GPIO26 = 0;
    /*  Function define */
    GpioCtrlRegs.GPAMUX2.bit.GPIO24 = 2;
    GpioCtrlRegs.GPAMUX2.bit.GPIO25 = 2;
    GpioCtrlRegs.GPAMUX2.bit.GPIO26 = 2;
    EDIS;
}

void find_adc_min_value()
{
    g_min_value_actual = AdcReadValue_Channel_A();
    if (g_min_value_actual < g_min_value_result)
    {
        g_min_value_result = g_min_value_actual;
    }
}

void find_adc_max_value()
{
    g_max_value_actual = AdcReadValue_Channel_A();
    if (g_max_value_actual > g_max_value_result)
    {
        g_max_value_result = g_max_value_actual;
    }
}

Uint16 find_adc_avg()
{
    int32 tmp_max = g_max_value_result;
    int32 tmp_min = g_min_value_result;
    int32 temp_valtozo = tmp_max + tmp_min;
    temp_valtozo = temp_valtozo>>1;
    return temp_valtozo;
}

/* Megtalalja az olvasott ertekek alapjan az ADC nullatmeneti erteket (g_adc_avg) ami a kesobbi kiertekeleshez kell  */

void adc_zero_crossing_find()
{
    find_adc_min_value();
    find_adc_max_value();
    g_adc_avg = find_adc_avg();
}



void QepInit(void)
{
# if ToRemove
    EQep2Regs.QPOSCNT = 78; // test init
    EQep2Regs.QPOSCMP = 500;
#endif


    EQep2Regs.QUPRD=150000000;    //Unit timer period, clk -> Sysclock
    EQep2Regs.QDECCTL.bit.QSRC=0;    //quadrature mode
    EQep2Regs.QEPCTL.bit.FREE_SOFT=2; // emulation kikapcsolasa
    EQep2Regs.QEPCTL.bit.PCRM=0; // Reset on COMP R
    EQep2Regs.QEPCTL.bit.UTE=1; // Unit timer enable
    EQep2Regs.QEPCTL.bit.QCLM=1; // Position counter (QPOSLAT), capture timer (QCTMRLAT)  and capture period (QCPRDLAT) values are latched ON TIMEOUT
    EQep2Regs.QPOSMAX=0xffffffff;
    EQep2Regs.QEPCTL.bit.QPEN=1; // eQEP position counter is enabled
    EQep2Regs.QEINT.bit.IEL = 1; // INDEX EVENT INT ENABLE
#if ToRemove
    EQep2Regs.QEINT.bit.UTO = 1; // TIMEOUT TIMER INT ENABLE
#endif

/*
    EQep2Regs.QCAPCTL.bit.UPPS=2;       // 1/32 alacsony sebeseghez jo lehet
    EQep2Regs.QCAPCTL.bit.CCPS=1;       // SYS/ 2exp7
    EQep2Regs.QCAPCTL.bit.CEN=0;
*/
# if ToRemove
    EQep2Regs.QPOSCTL.bit.PCE = 1 ; // enable position compare unit
    EQep2Regs.QEINT.bit.PCM = 1 ;   // enable pos compare interrupt
#endif
}

int QepReadDir(void)
{
    return EQep2Regs.QEPSTS.bit.QDF;
}

int QepReadCounter(void)
{
    return EQep2Regs.QPOSCNT;
}


void QEP_reinit_for_next_interrupt()
{
    EQep2Regs.QCLR.bit.IEL = 1; // Index INT FLAG
    EQep2Regs.QCLR.bit.INT = 1; // CLEAR INT FLAG
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP5; // INT A
}

_iq calculate_atan()
{
    shifted_channel_A = g_AdcChanel_A - g_adc_avg; //kozepertek 0-ba tolasa
    shifted_channel_B = g_AdcChanel_B - g_adc_avg; //kozepertek 0-ba tolasa

#if ToRemove
    float res = (float) (shifted_channel_B) / (float) (shifted_channel_A);
#endif

    _iq res = _IQdiv((_IQ(shifted_channel_B)),_IQ(shifted_channel_A));
    return _IQatan(res);

}


#if ToRemove
/* Megvizsgalja, hogy melyik a leheto legnagyobb atang-nal hasznalt arany ami elofordul,
 igy megallapithato, hogy melyik _iq fuggvenyt kell hasznalni a megfelelo mukodes erdekeben. */
void iq_max_arany_teszt(){
    teszt_shifted_channel_A = g_AdcChanel_A - g_adc_avg;
    teszt_shifted_channel_B = g_AdcChanel_B - g_adc_avg;
    teszt_g_max_value_actual = teszt_shifted_channel_B/teszt_shifted_channel_A;
        if (teszt_g_max_value_actual > teszt_g_max_value_result)
        {
            teszt_g_max_value_result = teszt_g_max_value_actual;
        }
}
#endif

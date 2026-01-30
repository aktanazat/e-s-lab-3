// Lab 3 - IR Remote Control Texting Over UART

#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "hw_nvic.h"
#include "rom.h"
#include "rom_map.h"
#include "interrupt.h"
#include "pin_mux_config.h"
#include "prcm.h"
#include "gpio.h"
#include "systick.h"
#include "uart_if.h"

#define SYSCLKFREQ 80000000ULL
#define TICKS_TO_US(ticks) \
    ((((ticks) / SYSCLKFREQ) * 1000000ULL) + \
    ((((ticks) % SYSCLKFREQ) * 1000000ULL) / SYSCLKFREQ))

#define SYSTICK_RELOAD_VAL 8000000UL

#define IR_GPIO_PORT GPIOA1_BASE
#define IR_GPIO_PIN  0x10

#define MAX_PULSES 100
volatile unsigned long g_pulseWidths[MAX_PULSES];
volatile int g_pulseIdx = 0;
volatile int g_pulseReady = 0;
volatile int g_systickExpired = 0;

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif

static inline void SysTickReset(void) {
    HWREG(NVIC_ST_CURRENT) = 1;
    g_systickExpired = 0;
}

static void BoardInit(void) {
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);
    PRCMCC3200MCUInit();
}

static void SysTickHandler(void) {
    g_systickExpired = 1;
    if (g_pulseIdx > 0) {
        g_pulseReady = 1;
    }
}

static void GPIOIntHandler(void) {
    unsigned long ulStatus = MAP_GPIOIntStatus(IR_GPIO_PORT, true);
    MAP_GPIOIntClear(IR_GPIO_PORT, ulStatus);

    if (ulStatus & IR_GPIO_PIN) {
        unsigned long tickVal = MAP_SysTickValueGet();
        unsigned long pulseWidth = TICKS_TO_US(SYSTICK_RELOAD_VAL - tickVal);

        if (g_pulseIdx < MAX_PULSES && !g_systickExpired) {
            if (g_pulseIdx > 50 && pulseWidth > 10000) {
                g_pulseReady = 1;
                return;
            }
            g_pulseWidths[g_pulseIdx++] = pulseWidth;
        }
        SysTickReset();
    }
}

static void IRInit(void) {
    MAP_SysTickPeriodSet(SYSTICK_RELOAD_VAL);
    MAP_SysTickIntRegister(SysTickHandler);
    MAP_SysTickIntEnable();
    MAP_SysTickEnable();

    MAP_GPIOIntRegister(IR_GPIO_PORT, GPIOIntHandler);
    MAP_GPIOIntTypeSet(IR_GPIO_PORT, IR_GPIO_PIN, GPIO_BOTH_EDGES);
    MAP_GPIOIntClear(IR_GPIO_PORT, MAP_GPIOIntStatus(IR_GPIO_PORT, false));
    MAP_IntEnable(INT_GPIOA1);
    MAP_GPIOIntEnable(IR_GPIO_PORT, IR_GPIO_PIN);
}

static void decodePulses(void) {
    if (g_pulseIdx < 40) return;

    int i;
    int startIdx = 0;

    for (i = 0; i < g_pulseIdx - 1; i++) {
        if (g_pulseWidths[i] > 3000 && g_pulseWidths[i] < 4000 &&
            g_pulseWidths[i+1] > 1400 && g_pulseWidths[i+1] < 1800) {
            startIdx = i + 2;
            break;
        }
    }

    if (startIdx == 0) return;

    unsigned long decoded1 = 0;
    unsigned long decoded2 = 0;
    int bitCount = 0;

    for (i = startIdx; i + 1 < g_pulseIdx && bitCount < 48; i += 2) {
        unsigned long space = g_pulseWidths[i + 1];
        int bit = (space > 800) ? 1 : 0;
        if (bitCount < 32) {
            if (bit) decoded1 |= (1UL << (31 - bitCount));
        } else {
            if (bit) decoded2 |= (1UL << (31 - (bitCount - 32)));
        }
        bitCount++;
    }

    Report("Bits: %d  Code: 0x%08lX %08lX\n\r", bitCount, decoded1, decoded2);
}

void main(void) {
    BoardInit();
    PinMuxConfig();
    InitTerm();
    ClearTerm();
    IRInit();

    Report("Lab 3 - IR Remote (TV Code 1003)\n\r");
    Report("Press buttons to capture codes...\n\r");

    while (1) {
        if (g_pulseReady) {
            decodePulses();
            g_pulseIdx = 0;
            g_pulseReady = 0;
            g_systickExpired = 0;
            SysTickReset();
        }
    }
}

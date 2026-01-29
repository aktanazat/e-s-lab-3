// Lab 3 - IR Remote Control Texting Over UART

#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "rom.h"
#include "rom_map.h"
#include "spi.h"
#include "uart.h"
#include "interrupt.h"
#include "pin_mux_config.h"
#include "utils.h"
#include "prcm.h"
#include "gpio.h"
#include "systick.h"
#include <stdint.h>
#include <string.h>

#include "uart_if.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1351.h"

#define SPI_IF_BIT_RATE  100000

// SysTick macros from prelab
#define SYSCLKFREQ 80000000ULL
#define TICKS_TO_US(ticks) \
    ((((ticks) / SYSCLKFREQ) * 1000000ULL) + \
    ((((ticks) % SYSCLKFREQ) * 1000000ULL) / SYSCLKFREQ))
#define US_TO_TICKS(us) ((SYSCLKFREQ / 1000000ULL) * (us))

// SysTick reload for 100ms timeout
#define SYSTICK_RELOAD_VAL 8000000UL

// IR receiver on PIN_03 = GPIO12 = GPIOA1 bit 4
#define IR_GPIO_PORT GPIOA1_BASE
#define IR_GPIO_PIN  0x10

// Pulse width buffer
#define MAX_PULSES 100
volatile unsigned long g_pulseWidths[MAX_PULSES];
volatile int g_pulseIdx = 0;
volatile int g_pulseReady = 0;
volatile int g_systickExpired = 0;

#if defined(ccs)
extern void (* const g_pfnVectors[])(void);
#endif

char MessageTx[64];
char MessageRx[64];
int MsgTxIdx = 0;

static inline void SysTickReset(void) {
    HWREG(NVIC_ST_CURRENT) = 1;
    g_systickExpired = 0;
}

static void BoardInit(void) {
#ifndef USE_TIRTOS
#if defined(ccs)
    MAP_IntVTableBaseSet((unsigned long)&g_pfnVectors[0]);
#endif
#endif
    MAP_IntMasterEnable();
    MAP_IntEnable(FAULT_SYSTICK);
    PRCMCC3200MCUInit();
}

static void SPIInit(void) {
    MAP_SPIReset(GSPI_BASE);
    MAP_SPIFIFOEnable(GSPI_BASE, SPI_TX_FIFO | SPI_RX_FIFO);
    MAP_SPIConfigSetExpClk(GSPI_BASE, MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE, SPI_MODE_MASTER, SPI_SUB_MODE_0,
                     (SPI_SW_CTRL_CS | SPI_4PIN_MODE | SPI_TURBO_OFF |
                      SPI_CS_ACTIVELOW | SPI_WL_8));
    MAP_SPIEnable(GSPI_BASE);
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
    unsigned long ulStatus = MAP_GPIOIntStatus(IR_GPIO_PORT, false);
    MAP_GPIOIntClear(IR_GPIO_PORT, ulStatus);
    MAP_IntEnable(INT_GPIOA1);
    MAP_GPIOIntEnable(IR_GPIO_PORT, IR_GPIO_PIN);
}

static void UART1Init(void) {
    MAP_UARTConfigSetExpClk(UARTA1_BASE, MAP_PRCMPeripheralClockGet(PRCM_UARTA1),
                UART_BAUD_RATE, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                UART_CONFIG_PAR_NONE));
    MAP_UARTEnable(UARTA1_BASE);
}

static void sendMessage(char *str, int len) {
    int i;
    for (i = 0; i < len; i++) {
        MAP_UARTCharPut(UARTA1_BASE, str[i]);
    }
    MAP_UARTCharPut(UARTA1_BASE, '\n');
}

// Decode pulse widths to determine button pressed
// Returns: 0-9 for digits, 10=DELETE, 11=MUTE, -1=unknown
static int decodePulses(void) {
    if (g_pulseIdx < 10) return -1;

    // Build bit pattern from pulse widths
    // Typical IR: short pulse (~500-600us) = 0, long pulse (~1600-1700us) = 1
    unsigned long decoded = 0;
    int bitCount = 0;
    int i;

    Report("Pulses captured: %d\n\r", g_pulseIdx);
    for (i = 0; i < g_pulseIdx && i < 20; i++) {
        Report("  [%d]: %lu us\n\r", i, g_pulseWidths[i]);
    }

    // Skip leader pulse, decode data pulses
    for (i = 2; i < g_pulseIdx && bitCount < 32; i++) {
        unsigned long pw = g_pulseWidths[i];
        if (pw > 300 && pw < 5000) {
            if (pw > 1000) {
                decoded |= (1UL << (31 - bitCount));
            }
            bitCount++;
        }
    }

    Report("Decoded: 0x%08lX (%d bits)\n\r", decoded, bitCount);
    return -1;  // Return -1 for now, will map after capturing patterns
}

void main(void) {
    memset(MessageTx, 0, sizeof(MessageTx));
    memset(MessageRx, 0, sizeof(MessageRx));

    BoardInit();
    PinMuxConfig();
    SPIInit();
    Adafruit_Init();
    InitTerm();
    ClearTerm();
    UART1Init();
    IRInit();

    Report("Lab 3 - IR Remote (TV Code 1003)\n\r");
    Report("Waiting for IR signal on PIN_03...\n\r");

    fillScreen(BLACK);
    drawString(0, 0, "TX:", WHITE, BLACK, 1);
    drawString(0, 64, "RX:", WHITE, BLACK, 1);

    while (1) {
        // Check for received UART data
        if (MAP_UARTCharsAvail(UARTA1_BASE)) {
            int i = 0;
            while (MAP_UARTCharsAvail(UARTA1_BASE) && i < 63) {
                MessageRx[i++] = MAP_UARTCharGet(UARTA1_BASE);
            }
            MessageRx[i] = '\0';
            Report("RX: %s\n\r", MessageRx);
            fillRect(0, 74, 128, 20, BLACK);
            drawString(0, 74, MessageRx, GREEN, BLACK, 1);
        }

        // Check if IR transmission complete
        if (g_pulseReady) {
            int button = decodePulses();

            // Reset for next transmission
            g_pulseIdx = 0;
            g_pulseReady = 0;
            g_systickExpired = 0;
            SysTickReset();
        }
    }
}

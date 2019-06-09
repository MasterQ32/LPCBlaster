#include <cstdint>
#include <cstddef>
#include <lpc17xx.h>
#include <attributes.h>

#include "serial.hpp"

// Deklaraion der Default Interrupt-Handler
[[noreturn]] static void IntDefaultHandler(void) INTERRUPT;

extern int main();

typedef void (*ISR_Handler)(void);

static ISR_Handler vector_table[] ALIGNED(1024) SECTION(".isr_vector") USED  =
{
    nullptr,                                // We manually cheat the stack to the end of the "normal" RAM
    IntDefaultHandler,                      // The reset handler
    IntDefaultHandler,                      // The NMI handler
    IntDefaultHandler,                      // The hard fault handler
    IntDefaultHandler,                      // The MPU fault handler
    IntDefaultHandler,                      // The bus fault handler
    IntDefaultHandler,                      // The usage fault handler
    (ISR_Handler)0xEFFFE782,                // Reserved
    nullptr,                                // Reserved
    nullptr,                                // Reserved
    nullptr,                                // Reserved
    IntDefaultHandler,                      // SVCall handler
    IntDefaultHandler,                      // Debug monitor handler
    nullptr,                                // Reserved
    IntDefaultHandler,                      // The PendSV handler
    IntDefaultHandler,                      // The SysTick handler

    IntDefaultHandler,                      // 16 Watchdog
    IntDefaultHandler,                      // 17 Timer0
    IntDefaultHandler,                      // 18 Timer1
    IntDefaultHandler,                      // 19 Timer2
    IntDefaultHandler,                      // 20 Timer3
    IntDefaultHandler,                           // 21 UART0
    IntDefaultHandler,                      // 22 UART1
    IntDefaultHandler,                      // 23 UART2
    IntDefaultHandler,                      // 24 UART3
    IntDefaultHandler,                      // 25 PWM 1
    IntDefaultHandler,                      // 26 I2C0
    IntDefaultHandler,                      // 27 I2C1
    IntDefaultHandler,                      // 28 I2C2
    IntDefaultHandler,                      // 29 SPI
    IntDefaultHandler,                      // 30 SSP0
    IntDefaultHandler,                      // 31 SSP1
    IntDefaultHandler,                      // 32 PLL0
    IntDefaultHandler,                      // 33 RTC
    IntDefaultHandler,                      // 34 Ext0
    IntDefaultHandler,                      // 35 Ext1
    IntDefaultHandler,                      // 36 Ext2
    IntDefaultHandler,                      // 37 Ext3 / GPIO
    IntDefaultHandler,                      // 38 ADC
    IntDefaultHandler,                      // 39 BOD
    IntDefaultHandler,                      // 40 USB
    IntDefaultHandler,                      // 41 CAN
    IntDefaultHandler,                      // 42 GPDMA
    IntDefaultHandler,                      // 43 I2S
    IntDefaultHandler,                      // 44 Ethernet
    IntDefaultHandler,                      // 45 RITINT
    IntDefaultHandler,                      // 46 Motor PWM
    IntDefaultHandler,                      // 47 QuadEncoder
    IntDefaultHandler,                      // 48 PLL1 (USB)
    IntDefaultHandler,                      // 49 USB Activity
    IntDefaultHandler                       // 50 CAN Activity
};

typedef void (*constructor)(void);
typedef void (*destructor)(void);

extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

extern "C" destructor start_dtors;
extern "C" destructor end_dtors;

void cpp_call_global_ctors()
{
	for (constructor * i = &start_ctors; i != &end_ctors; i++)
		(*i)();
}

void cpp_call_global_dtors()
{
	for (destructor * i = &start_dtors; i != &end_dtors; i++)
		(*i)();
}


void NORETURN __cxa_pure_virtual()
{
	Serial::tx("Pure virtual function call!\r\n");
	while(1);
}

static void IntDefaultHandler(void)
{
	Serial::tx("Unhandled interrupt!\r\n");
	while(1);
}

// this is the entry point of the bootloader.
// it is called by the LPCBlaster application.
extern "C" NORETURN void LPCBlasterEntry() USED ALIGNED(4);
extern "C" NORETURN void LPCBlasterEntry()
{
	// Offset für ISR-Vektortabelle setzen.
	SCB->VTOR = reinterpret_cast<uint32_t>(vector_table);

	// SVC-Priorität niedriger als alle Fault Handler
	SCB->SHP[7] = 1;

	// Fault-Handler aktivieren
	SCB->SHCSR = 0x00070000;

	// C++ "hochfahren"
	cpp_call_global_ctors();

	// Aufruf Hauptprogramm
	main();

	cpp_call_global_dtors();

	while(1);
}

#include "serial.hpp"
#include <lpc17xx.h>
#include <attributes.h>

#include "system.hpp"

/*
void Serial::init(uint32_t baudrate)
{
	LPC_SC->PCONP |= (1<<3); // PCUART0
	LPC_SC->PCLKSEL0 &= ~0xC0U; //
	LPC_SC->PCLKSEL0 |=  0x00U; // UART0 PCLK = SysClock / 4

	LPC_UART0->LCR = 0x83; // enable DLAB, 8N1
	LPC_UART0->FCR = 0x00; // disable any fifoing

	uint32_t constexpr pclk   = F_CPU / 4;
	uint32_t const     regval = (pclk / (16 * baudrate));

	LPC_UART0->DLL = (regval >> 0x00) & 0xFF;
	LPC_UART0->DLM = (regval >> 0x08) & 0xFF;

	LPC_UART0->LCR &= ~0x80; // disable DLAB
}
*/

void Serial::tx(char ch)
{
	while(!(LPC_UART0->LSR & (1<<5))); // Wait for Previous transmission
	LPC_UART0->THR = ch;               // Load the data to be transmitted
}

void Serial::tx(char const * msg)
{
	while(*msg)
		tx(*msg++);
}

void Serial::tx(const void * data, size_t length)
{
	char const * buf = reinterpret_cast<char const *>(data);
	for(size_t i = 0; i < length; i++)
		tx(buf[i]);
}

bool Serial::available ()
{
	return (LPC_UART0->LSR & (1<<0));
}

char Serial::rx()
{
	char ch;
	while(!(LPC_UART0->LSR & (1<<0)));  // Wait till the data is received
	ch = LPC_UART0->RBR;                // Read received data
	return ch;
}

static Serial::InterruptHandler custom_isr = nullptr;

static void serial_isr() INTERRUPT;
static void serial_isr()
{
	uint32_t const iir = LPC_UART0->IIR;

	switch(iir & 0x0E)
	{
		case 0x08: // Received byte
		{
			char c = LPC_UART0->RBR;
			custom_isr(c);
			break;
		}
	}
}

void Serial::enable_interrupt(InterruptHandler isr)
{
	custom_isr = isr;
	NVIC_SetHandler(UART0_IRQn, reinterpret_cast<uint32_t>(serial_isr));
	NVIC_EnableIRQ(UART0_IRQn);

	LPC_UART0->IER = 0x01; // enable receive interrupt
}

void Serial::disable_interrupt()
{
	NVIC_DisableIRQ(UART0_IRQn);
}

#ifndef SERIAL_HPP
#define SERIAL_HPP

#include <cstdint>
#include <cstddef>

namespace Serial
{
	using InterruptHandler = void (*)(char c);
	// void init(uint32_t baudrate);

	void tx(char ch);
	void tx(char const * msg);
	void tx(void const * data, size_t length);

	bool available ();
	char rx();

	void enable_interrupt(InterruptHandler isr);
	void disable_interrupt();
};

#endif // SERIAL_HPP

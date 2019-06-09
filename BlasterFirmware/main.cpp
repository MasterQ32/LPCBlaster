#include <cstdint>
#include <cstddef>
#include <attributes.h>
#include <hal/gpio.hpp>
#include <hal/iap.hpp>
#include "serial.hpp"
#include "modules/modules.hpp"
#include "sysctrl.hpp"

static sysctrl::SerialHandler serialHandler;

static sysctrl::state current_state;

sysctrl::state sysctrl::go(sysctrl::SerialHandler h, sysctrl::state initial_state)
{
	current_state = initial_state;
	serialHandler = h;
	return initial_state;
}

void sysctrl::acknowledge()
{
	Serial::tx('\006');
}

sysctrl::state sysctrl::return_to_main(bool suppress_ack)
{
	if(not suppress_ack)
		acknowledge();
	return -1;
}

sysctrl::state sysctrl::return_to_main(ErrorCode code)
{
	Serial::tx('\025');
	Serial::tx(uint8_t(code));
	return -1;
}

// fancy thing is:
// we get our CPU and UART set up already from the ISP!
int main()
{
	Serial::tx("LPCBlaster ready.\r\n");

	current_state = -1;
	while(true)
	{
		if(current_state == -1)
			system_main::begin();

		char c = Serial::rx();
		current_state = serialHandler(current_state, uint8_t(c));
	}
}

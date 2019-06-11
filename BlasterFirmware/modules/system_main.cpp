#include "system_main.hpp"
#include "modules.hpp"
#include "sysctrl.hpp"
#include "serial.hpp"
#include <hal/iap.hpp>

#include <lpc17xx.h>

static sysctrl::state rcv(sysctrl::state, uint8_t c)
{
	switch(c)
	{
		case 'L': return data_loader::begin();
		case 'Z': return zero_memory::begin();
		case 'R': return readback_memory::begin();
		case 'E': return erase_sectors::begin_partial();
		case 'F': return erase_sectors::begin_full();
		case 'K': NVIC_SystemReset(); break;
		case 'X': iap::reinvoke_isp(); break;
		default: return sysctrl::return_to_main(ErrorCode::UnknownCommand, c);
	}
	return 0;
}

sysctrl::state system_main::begin()
{
	return sysctrl::go(&rcv, 0);
}

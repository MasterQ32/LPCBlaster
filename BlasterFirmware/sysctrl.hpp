#pragma once

#include <cstdint>

#include "errorcode.hpp"

extern char ahbram[32768];

namespace sysctrl
{
	using state = int;

	using SerialHandler = int(*)(state state, uint8_t byte);

	state go(SerialHandler, state initial_state);

	void acknowledge();

	state return_to_main(bool suppress_ack = false);

	state return_to_main(ErrorCode err);
}

#include "zero_memory.hpp"

#include "sysctrl.hpp"

#include <cstring>

namespace
{
	enum State {
		ReadOffset0 = 0,
		ReadOffset1,
		ReadLength0,
		ReadLength1,
	};

	uint16_t offset;
	uint16_t length;

	sysctrl::state rcv(sysctrl::state state, uint8_t val)
	{
		switch(State(state))
		{
			case ReadOffset0:
				offset = val;
				return ReadOffset1;

			case ReadOffset1:
				offset |= uint16_t(val) << 8;
				return ReadLength0;

			case ReadLength0:
				length = val;
				return ReadLength1;

			case ReadLength1:
				length |= uint16_t(val) << 8;
				if(length > 0) {
					if(uint32_t(offset) + length > sizeof(ahbram))
						return sysctrl::return_to_main(ErrorCode::OutOfRange);
					memset(&ahbram[offset], 0, length);
					return sysctrl::return_to_main();
				}
				else {
					return sysctrl::return_to_main(ErrorCode::InvalidLength);
				}
		}
		return sysctrl::return_to_main(ErrorCode::UnkownState);
	}
}

sysctrl::state zero_memory::begin()
{
	length = 0;
	offset = 0;
	return sysctrl::go(&rcv, ReadOffset0);
}

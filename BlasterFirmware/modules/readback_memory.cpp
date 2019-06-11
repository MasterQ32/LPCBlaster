#include "readback_memory.hpp"
#include "serial.hpp"

namespace
{
	enum State {
		ReadOffset0 = 0,
		ReadOffset1,
		ReadOffset2,
		ReadOffset3,
		ReadLength0,
		ReadLength1,
		ReadLength2,
		ReadLength3,
	};

	uint32_t offset;
	uint32_t length;

	sysctrl::state rcv(sysctrl::state state, uint8_t val)
	{
		switch(State(state))
		{
			case ReadOffset0:
				offset = val;
				return ReadOffset1;

			case ReadOffset1:
				offset |= uint32_t(val) << 8;
				return ReadOffset2;

			case ReadOffset2:
				offset |= uint32_t(val) << 16;
				return ReadOffset3;

			case ReadOffset3:
				offset |= uint32_t(val) << 24;
				return ReadLength0;

			case ReadLength0:
				length = val;
				return ReadLength1;

			case ReadLength1:
				length |= uint32_t(val) << 8;
				return ReadLength2;

			case ReadLength2:
				length |= uint32_t(val) << 16;
				return ReadLength3;

			case ReadLength3:
				length |= uint32_t(val) << 24;
				if(length > 0) {
					uint32_t end;
					if(__builtin_add_overflow(offset, length, &end))
						return sysctrl::return_to_main(ErrorCode::OutOfRange);

					sysctrl::acknowledge();

					uint8_t const * memory = reinterpret_cast<uint8_t const *>(offset);

					uint16_t checksum = 0;
					for(size_t i = 0; i < length; i++) {
						uint8_t b = memory[i];
						checksum += b;
						Serial::tx(b);
					}

					Serial::tx(checksum & 0xFFU);
					Serial::tx((checksum & 0xFF00U) >> 8);

					return sysctrl::return_to_main(true);
				}
				else {
					return sysctrl::return_to_main(ErrorCode::InvalidLength);
				}

		}
		return sysctrl::return_to_main(ErrorCode::UnknownState);
	}
}

sysctrl::state readback_memory::begin()
{
	offset = 0;
	length = 0;
	return sysctrl::go(&rcv, ReadOffset0);
}

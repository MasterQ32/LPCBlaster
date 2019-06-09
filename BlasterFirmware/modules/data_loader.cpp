#include "data_loader.hpp"
#include "sysctrl.hpp"
#include "serial.hpp"

namespace
{
	enum State {
		ReadOffset0 = 0,
		ReadOffset1,
		ReadLength0,
		ReadLength1,
		ReadData,
		ReadChecksum0,
		ReadChecksum1,
	};

	uint16_t offset;
	uint16_t length;
	uint16_t local_checksum, remote_checksum;

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
					return ReadData;
				}
				else {
					return sysctrl::return_to_main(ErrorCode::InvalidLength);
				}

			case ReadData:
				ahbram[offset++] = val;
				local_checksum += val;
				length -= 1;
				if(length == 0)
					return ReadChecksum0;
				else
					return ReadData;

			case ReadChecksum0:
				remote_checksum = val;
				return ReadChecksum1;

			case ReadChecksum1:
				remote_checksum |= uint16_t(val) << 8;
				if(remote_checksum != local_checksum)
					return sysctrl::return_to_main(ErrorCode::InvalidChecksum);
				else
					return sysctrl::return_to_main();
		}
		return sysctrl::return_to_main(ErrorCode::UnkownState);
	}
}

sysctrl::state data_loader::begin()
{
	remote_checksum = 0;
	local_checksum = 0;
	length = 0;
	offset = 0;
	return sysctrl::go(&rcv, ReadOffset0);
}

#ifndef SYSTEM_MAIN_HPP
#define SYSTEM_MAIN_HPP

#include "sysctrl.hpp"

// commands:
// L:load_memory(offset:u16, length:u16, data:u8[length], checksum:u16)
// Z:zero_memory(offset:u16, length:u16)
// R:readback_memory(offset:u32, length:u32) → { data:u8[length], checksum:u16 }
// E:erase(sectorCnt:u8,sectorList:u8[sectorCnt])
// F:full_erase()
// W:erase_and_write(flash_offset:u32,work_offset:u16,length:u16)
// K:[[noreturn]] reset_system()
// X:[[noreturn]] exit_to_isp()

// every command either returns
//   ACK ('\006')
// or
//   NAK ('\025') followed by an error code (see sysctrl.hpp/ErrorCode)
// after completion or before starting to send any data.

namespace system_main
{
	sysctrl::state begin();
}

#endif // SYSTEM_MAIN_HPP

#ifndef ERROR_HPP
#define ERROR_HPP

#include <cstdint>

enum class ErrorCode : uint8_t
{
	UnknownState     = 0x00,
	InvalidLength   = 0x01,
	InvalidChecksum = 0x02,
	OutOfRange      = 0x03,
	NotAligned      = 0x04,
	IAPFailure      = 0x05,
	UnknownCommand  = 0x06,
};

#endif // ERROR_HPP

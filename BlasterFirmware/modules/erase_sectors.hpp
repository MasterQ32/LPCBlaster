#ifndef ERASE_SECTORS_HPP
#define ERASE_SECTORS_HPP

#include "sysctrl.hpp"

namespace erase_sectors
{
	sysctrl::state begin_partial();
	sysctrl::state begin_full();
};

#endif // ERASE_SECTORS_HPP

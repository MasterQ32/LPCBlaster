#ifndef SECTOR_TABLE_HPP
#define SECTOR_TABLE_HPP

#include <cstdint>

struct Sector
{
	uint32_t start_address;
	uint32_t length;
};

extern const Sector sector_table[30];

#endif // SECTOR_TABLE_HPP

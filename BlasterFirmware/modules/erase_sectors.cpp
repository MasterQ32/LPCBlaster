#include "erase_sectors.hpp"
#include "sector_table.hpp"
#include "system.hpp"

#include <iterator>
#include <hal/iap.hpp>

namespace
{
	enum State {
		ReadSectorCount,
		ReadSectorList
	};

	uint8_t sectorCount;

	uint8_t sectors[30];
	uint8_t index;

	bool errorState;

	sysctrl::state rcv(sysctrl::state state, uint8_t byte)
	{
		switch(State(state))
		{
			case ReadSectorCount:
				sectorCount = byte;
				if(sectorCount == 0)
					return sysctrl::return_to_main(ErrorCode::InvalidLength);
				if(sectorCount > 30)
					return sysctrl::return_to_main(ErrorCode::OutOfRange);
				return ReadSectorList;

			case ReadSectorList:
			{
				sectors[index++] = byte;
				errorState |= (byte >= 30);
				if(index >= sectorCount)
				{
					// sort sector table
					for(size_t i = 0; i < sectorCount - 1; i++) {
						for(size_t j = i + 1; j < sectorCount; j++) {
							if(sectors[i] > sectors[j]) {
								std::swap(sectors[i], sectors[j]);
							}
						}
					}

					auto const find_range = [](size_t i) -> size_t
					{
						while(i+1 < sectorCount) {
							if(sectors[i+1] != sectors[i] + 1)
								return i;
							i += 1;
						}
						return sectorCount - 1;
					};

					size_t start = 0;
					while(start < sectorCount)
					{
						size_t end = find_range(start);

						auto const prep_err = iap::prepare_sector(sectors[start], sectors[end]);
						if(prep_err != iap::CMD_SUCCESS)
							return sysctrl::return_to_main(ErrorCode::IAPFailure, 1);

						auto const erase_err = iap::erase_sectors(sectors[start], sectors[end], F_CPU / 1000);
						if(erase_err != iap::CMD_SUCCESS)
							return sysctrl::return_to_main(ErrorCode::IAPFailure, 2);

						start = end + 1;
					}

					return sysctrl::return_to_main();
				}
				return ReadSectorList;
			}
		}
		return sysctrl::return_to_main(ErrorCode::UnknownState);
	}
}

sysctrl::state erase_sectors::begin_partial()
{
	errorState = false;
	index = 0;
	return sysctrl::go(&rcv, ReadSectorCount);
}

sysctrl::state erase_sectors::begin_full()
{
	auto const prep_err = iap::prepare_sector(0, std::size(sector_table) - 1);
	if(prep_err != iap::CMD_SUCCESS)
		return sysctrl::return_to_main(ErrorCode::IAPFailure);

	auto const erase_err = iap::erase_sectors(0, std::size(sector_table) - 1, F_CPU / 1000);
	if(erase_err != iap::CMD_SUCCESS)
		return sysctrl::return_to_main(ErrorCode::IAPFailure);

	return sysctrl::return_to_main();
}

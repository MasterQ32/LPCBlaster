#include "erase_and_write.hpp"
#include "sector_table.hpp"
#include "system.hpp"
#include <hal/iap.hpp>
#include <optional>

namespace
{
	enum State {
		ReadFlashOffset0 = 0,
		ReadFlashOffset1,
		ReadFlashOffset2,
		ReadFlashOffset3,
		ReadWorkOffset0,
		ReadWorkOffset1,
		ReadLength0,
		ReadLength1,
	};

	uint32_t flash_offset;
	uint16_t work_offset;
	uint16_t length;

	static inline std::optional<uint32_t> find_sector_for_address(uint32_t address)
	{
		for(size_t i = 0; i < std::size(sector_table); i++)
		{
			if(address < sector_table[i].start_address)
				continue;
			if(address >= sector_table[i].start_address + sector_table[i].length)
				continue;
			return uint32_t(i);
		}
		return std::nullopt;
	}

	sysctrl::state rcv(sysctrl::state state, uint8_t val)
	{
		switch(State(state))
		{
			case ReadFlashOffset0:
				flash_offset = val;
				return ReadFlashOffset1;

			case ReadFlashOffset1:
				flash_offset |= uint32_t(val) << 8;
				return ReadLength0;

			case ReadFlashOffset2:
				flash_offset |= uint32_t(val) << 16;
				return ReadLength0;

			case ReadFlashOffset3:
				flash_offset |= uint32_t(val) << 24;
				return ReadWorkOffset0;

			case ReadWorkOffset0:
				work_offset = val;
				return ReadWorkOffset1;

			case ReadWorkOffset1:
				work_offset |= uint16_t(val) << 8;
				return ReadLength0;

			case ReadLength0:
				length = val;
				return ReadLength1;

			case ReadLength1:
				length |= uint16_t(val) << 8;
				if(length > 0) {
					if(uint32_t(work_offset) + length > sizeof(ahbram))
						return sysctrl::return_to_main(ErrorCode::OutOfRange, 1);

					uint32_t end;
					if(__builtin_add_overflow(flash_offset, uint32_t(length), &end))
						return sysctrl::return_to_main(ErrorCode::OutOfRange, 2);

					if((flash_offset & 0xFFU) != 0)
						return sysctrl::return_to_main(ErrorCode::NotAligned, 1);

					if((work_offset & 0x3U) != 0)
						return sysctrl::return_to_main(ErrorCode::NotAligned, 2);

					if((length & 0xFFU) != 0)
						return sysctrl::return_to_main(ErrorCode::NotAligned, 3);

					uint32_t end_address = (flash_offset + length + 0xFFU - 1) & ~0xFFU;

					auto const first_sector = find_sector_for_address(flash_offset);
					auto const last_sector = find_sector_for_address(end_address);

					if(not first_sector or not last_sector)
						return sysctrl::return_to_main(ErrorCode::OutOfRange, 3);

					auto const prep1_err = iap::prepare_sector(*first_sector, *last_sector);
					if(prep1_err != iap::CMD_SUCCESS)
						return sysctrl::return_to_main(ErrorCode::IAPFailure, 1);

					auto const erase_err = iap::erase_sectors(*first_sector, *last_sector, F_CPU / 1000);
					if(erase_err != iap::CMD_SUCCESS)
						return sysctrl::return_to_main(ErrorCode::IAPFailure, 2);

					uint32_t offset = 0;
					while(offset < length)
					{
						auto len = std::min<uint32_t>(4096, uint32_t(length) - offset);
						if(len < 512)
							len = 256;
						else if(len < 1024)
							len = 512;
						else if(len < 4096)
							len = 1024;

						auto const first_sector = find_sector_for_address(flash_offset + offset);
						auto const last_sector = find_sector_for_address(flash_offset + offset + len - 1);

						if(not first_sector or not last_sector)
							return sysctrl::return_to_main(ErrorCode::OutOfRange, 4);

						auto const prep2_err = iap::prepare_sector(*first_sector, *last_sector);
						if(prep2_err != iap::CMD_SUCCESS)
							return sysctrl::return_to_main(ErrorCode::IAPFailure, 3);


						auto const copy_error = iap::copy_ram_to_flash(
							reinterpret_cast<uint32_t*>(flash_offset + offset),
							reinterpret_cast<uint32_t*>(work_offset + offset),
							len,
							F_CPU / 1000
						);
						if(copy_error != iap::CMD_SUCCESS)
							return sysctrl::return_to_main(ErrorCode::IAPFailure, 5);

						offset += len;
					}

					return sysctrl::return_to_main();
				}
				else {
					return sysctrl::return_to_main(ErrorCode::InvalidLength);
				}
		}
		return sysctrl::return_to_main(ErrorCode::UnknownState);
	}
}

sysctrl::state erase_and_write::begin()
{
	return sysctrl::go(&rcv, ReadFlashOffset0);
}

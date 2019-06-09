#include "elfloader.hpp"

#include <QFile>
#include <QDebug>
#include <elf.h>
#include <cstdio>

template<typename T>
static T read(QByteArray const & src, uint32_t offset)
{
	static_assert(std::is_pod_v<T>);
	assert(offset + sizeof(T) <= src.size());
	T result;
	memcpy(&result, src.data() + offset, sizeof(T));
	return result;
}

std::optional<std::tuple<QByteArray, uint32_t> > ELFLoader::load_binary(const QString & fileName)
{
	QByteArray elf;
	{
		QFile file(fileName);
		if(not file.open(QFile::ReadOnly))
			return std::nullopt;
		elf = file.readAll();
	}

	auto const file_header = read<Elf32_Ehdr>(elf, 0);

	// verify it's an ELF file
	if(file_header.e_ident[EI_MAG0] != ELFMAG0) return std::nullopt;
	if(file_header.e_ident[EI_MAG1] != ELFMAG1) return std::nullopt;
	if(file_header.e_ident[EI_MAG2] != ELFMAG2) return std::nullopt;
	if(file_header.e_ident[EI_MAG3] != ELFMAG3) return std::nullopt;

	// verify it's ELF32 little
	if(file_header.e_ident[EI_CLASS]   != ELFCLASS32) return std::nullopt;
	if(file_header.e_ident[EI_DATA]    != ELFDATA2LSB) return std::nullopt;
	if(file_header.e_ident[EI_VERSION] != EV_CURRENT) return std::nullopt;

	if(file_header.e_type != ET_EXEC) return std::nullopt;
	if(file_header.e_machine != EM_ARM) return std::nullopt;
	if(file_header.e_version != EV_CURRENT) return std::nullopt;

	uint32_t start_address = 0x10001000;
	QByteArray binary;

	fprintf(stderr, "Sections:\n");
	for(size_t i = 0; i < file_header.e_shnum; i++)
	{
		auto const shdr = read<Elf32_Shdr>(elf, file_header.e_shoff + file_header.e_shentsize * i);

		fprintf(stderr, "name=%u\ttype=%04X\tflags=%04X\taddr=%04X\toffset=%04X\tsize=%04X\tlink=%04X\tinfo=%04X%\n",
			shdr.sh_name,
			shdr.sh_type,
			shdr.sh_flags,
			shdr.sh_addr,
			shdr.sh_offset,
			shdr.sh_size,
			shdr.sh_link,
			shdr.sh_info
		);
		fflush(stderr);

		if(shdr.sh_type == SHT_PROGBITS or shdr.sh_type == SHT_NOBITS)
		{
			assert(shdr.sh_addr >= start_address);

			uint32_t const binary_offset = shdr.sh_addr - start_address;

			binary.resize(std::max<int>(binary.size(), binary_offset + shdr.sh_size));

			if(shdr.sh_type == SHT_PROGBITS)
			{
				memcpy(
					binary.data() + binary_offset,
					elf.data() + shdr.sh_offset,
					shdr.sh_size
				);
			}
			else
			{
				memset(
					binary.data() + binary_offset,
					0,
					shdr.sh_size
				);
			}

		}
	}

	return std::make_tuple(std::move(binary), file_header.e_entry);
}

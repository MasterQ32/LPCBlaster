#ifndef ELFLOADER_HPP
#define ELFLOADER_HPP

#include <tuple>
#include <optional>
#include <QString>
#include <QByteArray>

namespace ELFLoader
{
	std::optional<std::tuple<QByteArray, uint32_t>> load_binary(QString const & fileName);
};

#endif // ELFLOADER_HPP

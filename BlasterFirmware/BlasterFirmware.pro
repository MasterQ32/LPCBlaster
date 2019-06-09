TEMPLATE=app
CONFIG += c++17

CORTEX_M3_OPTIMIZE=s
CORTEX_M3 = no_hal_init
CORTEX_M3_LINKERSCRIPT = $$quote($$PWD/linker.ld)

include(/home/felix/projects/lowlevel/cortex-m3-template/cortex-m3.pri)

SOURCES += \
  main.cpp \
  modules/data_loader.cpp \
  modules/erase_sectors.cpp \
  modules/readback_memory.cpp \
  modules/system_main.cpp \
  modules/zero_memory.cpp \
  sector_table.cpp \
  serial.cpp \
  sysinit.cpp

DISTFILES += \
  linker.ld

HEADERS += \
  errorcode.hpp \
  modules/data_loader.hpp \
  modules/erase_sectors.hpp \
  modules/modules.hpp \
  modules/readback_memory.hpp \
  modules/system_main.hpp \
  modules/zero_memory.hpp \
  sector_table.hpp \
  serial.hpp \
  sysctrl.hpp \
  system.hpp

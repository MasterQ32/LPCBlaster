TEMPLATE = subdirs

contains(QMAKE_PLATFORM, arm_baremetal): SUBDIRS += BlasterFirmware
contains(QMAKE_PLATFORM, linux): SUBDIRS += LPCBlaster

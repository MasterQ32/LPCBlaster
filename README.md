# LPCBlaster
My approach for an ISP programming tool for the LPC17xx family.

## Core Idea
The ISP of the LPC17xx family is an ASCII based protocol with
a lot of parsing and data overhead. My idea was to reduce the
overhead by using binary transmission and batch programming modes.

The ISP allows these two useful things for the idea:
- Loading data into RAM
- Starting code at an arbitrary adress

Load a small program into RAM (~2kB) and start it. This program does
a low-bandwidth ISP protocol which utilizes the whole RAM of the
chip to do faster programming:

1. Load up to 32kB of data into the RAM
2. Erase and write a list of sectors in batch (1 32kB sector or 8 4kB sectors)
3. Repeat 1, 2 until whole program is transferred

## LPCBlaster Protocol

The protocol used for ISP programming is binary and uses a packet based
approach. Packet types are determined by the first byte transmitted and
have variable length.

After reception, the controller responds with either ACK (0x06) or NAK (0x15)
plus a one byte error code.

All integers used in the protocol are encoded little endian and have their width
encoded in their name (`u8`, `u16`, `u32`).

### Load Memory
`L:load_memory(offset:u16, length:u16, data:u8[length], checksum:u16)`

Loads a block of data into the 32k work buffer. `offset` is a position in
the work buffer, `length` is the number of bytes that should be transferred.
`data` is a sequence of `length` bytes and `checksum` is the 16 bit sum of all
bytes in `data`.

### Zero Memory
`Z:zero_memory(offset:u16, length:u16)`

Sets the specified memory area to zero. `offset` is a position in the 32k work
buffer, `length` is the number of bytes that should be cleared.

### Readback Memory
`R:readback_memory(offset:u32, length:u32) → { data:u8[length], checksum:u16 }`

Reads data from the controllers memory and sends it to the host.
The data is sent after the `ACK` so the host knows if the command is successful
or not.

After the data, a simple two-byte checksum is transferred to verify if the
data was transmitted correctly.

`offset` is the memory address where the read should start. `length` is the
number of bytes that should be transmitted.
`offset` must not be aligned to a word boundary but can be arbitrarily set.

### Erase Sectors
`E:erase(count:u8,sectors:u8[count])`

Erases the given sectors. `count` is the number of sectors to be cleared,
`sectors` is an array of `count` bytes, each specifiying the sector number
to be erased.

### Full Flash Erase
`F:full_erase()`

Erases all sectors in the flash.

### Erase and Write
`W:erase_and_write(flash_offset:u32,work_offset:u16,length:u16)`

Erases all sectors touched by the memory span between `flash_offset` and `flash_offset + length - 1`. Then copies bytes starting at `work_offset` from
the work buffer to the flash.

### Reset Controller
`K:[[noreturn]] reset_system()`

Resets the controller by using `NVIC_SystemReset()`. This restarts the system.

### Return To Builtin ISP
`X:[[noreturn]] exit_to_isp()`

This command returns the control to the builtin ISP handler and allows
using it's commands to do further tasks.

### Error List
Each command may return `NAK` followed by an error code. These may be one of those:

| Error Code | Description                                                     |
|-----------'|-----------------------------------------------------------------|
|     `0x00` | _Unknown state_: The device encountered an invalid state.       |
|     `0x01` | _Invalid length_: Length was zero.                              |
|     `0x02` | _Invalid checksum:_ There was a transmission error.             |
|     `0x03` | _Out Of Range_: `offset`+`length` would read/write out of range.|
|     `0x04` | _Not Aligned_: A parameter was required to be aligned, but was not.|
|     `0x05` | _IAP Failure_: There was an error during an IAP operation.      |

#!/bin/sh

i2cset -y 3 0x51 0x00 0x00
i2cset -y 3 0x51 0x01 0x03
i2cset -y 3 0x51 0x02 0x00
i2cset -y 3 0x51 0x03 0x01
i2cset -y 3 0x51 0x04 0x01
i2cset -y 3 0x51 0x05 0x00
i2cset -y 3 0x51 0x06 0x00

i2cget -y 3 0x51 0 b  
i2cget -y 3 0x51 1 b  
i2cget -y 3 0x51 2 b  
i2cget -y 3 0x51 3 b  
i2cget -y 3 0x51 4 b  
i2cget -y 3 0x51 5 b  
i2cget -y 3 0x51 6 b  
# LPS22HH_FakeIt
Example of using DocTest and FakeIt to build integration tests for the STM32 B-U585I-IOT02A board.

# Build and flash the board

For a Linux host with the arm-none-eabi toolchain and openocd tools installed :

```bash
cmake --preset Debug
cmake --build --preset Debug
arm-none-eabi-objcopy -O binary ./build/Debug/LPS22HH_FakeIt.elf ./build/Debug/LPS22HH_FakeIt.bin
openocd -f interface/stlink.cfg -f target/stm32u5x.cfg -c "program ./build/Debug/LPS22HH_FakeIt.bin 0x08000000 verify reset exit"
```

# Build and run integration tests

```bash
cd tests
cmake --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```


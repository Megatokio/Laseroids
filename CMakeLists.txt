cmake_minimum_required(VERSION 3.13)
#set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(CMAKE_CXX_STANDARD 14)
include(pico_sdk_import.cmake)
project(XY2Scanner)
pico_sdk_init()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DRELEASE -DNDEBUG")

add_executable(Laseroids
	utilities.cpp
	demos.cpp
	charset1.cpp
	ssd1306.cpp
	OledDisplay.cpp
	AdcLoadSensor.cpp
	DS3231.cpp
	FlashDrive.cpp
	Laseroids.cpp
	HiScore.cpp
	XY2.cpp
	main.cpp
	)

pico_generate_pio_header(Laseroids ${CMAKE_CURRENT_LIST_DIR}/XY2-100.pio)

pico_enable_stdio_usb(Laseroids 1)
pico_enable_stdio_uart(Laseroids 1)
target_link_libraries(Laseroids pico_stdlib hardware_rtc pico_multicore hardware_pio hardware_dma)
target_link_libraries(Laseroids hardware_i2c hardware_adc hardware_irq hardware_pwm hardware_flash)
target_link_libraries(Laseroids hardware_interp)
pico_add_extra_outputs(Laseroids)


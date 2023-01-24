run:
	gcc src/main.c src/bme280.c src/crc16.c src/pid.c src/uart.c  -I ../ -lwiringPi -o bin && ./bin /dev/i2c-1
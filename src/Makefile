all: upload serial

IP = si5351.local

html:
	mkdir -vp webclient/dist/
	gcc webclient/bin2c.c -o webclient/dist/bin2c.exe
	./webclient/dist/bin2c.exe webclient/web/index.html ./src/index_html_data.h index_html_data

build_old: html
	pio run -e esp01 

upload_old: html
	pio run -e esp01  -t upload --upload-port /dev/ttyUSB0


build: html
	pio run -e esp01_1m

upload: html
	pio run -e esp01_1m  -t upload --upload-port /dev/ttyUSB0


ota: build
	curl --progress-bar -F "image=@.pio/build/esp01_1m/firmware.bin" $(IP)/update

ota_old: build
	curl --progress-bar -F "image=@.pio/build/esp01/firmware.bin" $(IP)/update


serial:
	miniterm /dev/ttyUSB0 115200

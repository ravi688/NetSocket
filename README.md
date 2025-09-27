# NetSocket
Cross platform Networking Programming library which works on Linux and Windows both. 

## Building
```
build_master_meson wrap install spdlog
build_master meson setup build
build_master meson compile -C build
```
## Running test
```
python client_server_test.py build
```

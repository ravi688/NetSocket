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
## Examples
1. https://github.com/ravi688/NetSocket/blob/main/source/main.client.cpp
2. https://github.com/ravi688/NetSocket/blob/main/source/main.server.cpp
3. https://github.com/ravi688/NetSocket/blob/main/source/main.client_async_socket.cpp
4. https://github.com/ravi688/NetSocket/blob/main/source/main.server_async_socket.cpp

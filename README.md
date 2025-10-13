# NetSocket
Cross platform Networking Programming library which works on Linux and Windows both. 

## Installing dependencies
### Packages for MINGW64
```
pacman -S mingw-w64-x86_64-zlib
pacman -S mingw-w64-x86_64-mbedtls
```
### Packages for Linux
```
sudo apt install libssl-dev
sudo apt install libmbedtls-dev
```
### Install meson wraps
```
build_master_meson wrap install spdlog
```
### Build and install IXWebSocket
```
git clone https://github.com/ravi688/IXWebSocket.git
cd IXWebSocket
cmake -S . -B build_tls -GNinja -DUSE_TLS=1 -DCMAKE_INSTALL_PREFIX=/mingw64 -DCMAKE_BUILD_TYPE=Release
cmake --build build_tls
cmake --install build_tls
```

## Building
```
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

# A C++ library for serial ports, that takes care of Concurrency

## Contributors

* Emil Fresk

---

## License

Licensed under the LGPL-v3 license, see LICENSE file for details.

---

Port of the serial pipe class form C#, dependent of the "serial" library.
* Remember to add the path: `export LD_LIBRARY_PATH=/usr/local/lib/`
* Simple to use, see example file.
* Dependency on https://github.com/wjwwood/serial
* Compile example: `g++ example.cpp -o test -std=c++14 -pthread -lserial`

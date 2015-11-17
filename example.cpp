#include <iostream>
#include "serialpipe.h"

using namespace std;

void test(const std::vector<uint8_t> &data);

int main()
{
    /* Create serial pipe */
    SerialPipe::SerialPipe sp("/dev/ttyUSB0", 115200);

    /* Register a callback */
    sp.registerCallback(test);

    /* Open the serial port */
    sp.openPort();

    /* Transmit some data... */
    sp.serialTransmit("test test test");

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    return 0;
}

/* Print incomming data... */
void test(const std::vector<uint8_t> &data)
{
    cout << "1: ";
    for (const uint8_t &v : data)
    {
        cout << v;
    }

    cout << endl;
}


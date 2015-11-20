/****************************************************************************
*
* Copyright (C) 2015 Emil Fresk.
* All rights reserved.
*
* This file is part of the SerialPipe library.
*
* GNU Lesser General Public License Usage
* This file may be used under the terms of the GNU Lesser
* General Public License version 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE included in the
* packaging of this file.  Please review the following information to
* ensure the GNU Lesser General Public License version 3.0 requirements
* will be met: http://www.gnu.org/licenses/lgpl-3.0.html.
*
* If you have questions regarding the use of this file, please contact
* Emil Fresk at emil.fresk@gmail.com.
*
****************************************************************************/

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

    cin.get();

    /* Close port */
    sp.closePort();

    return 0;
}

/* Print incomming data... */
void test(const std::vector<uint8_t> &data)
{
    cout << "Data received: ";
    for (const uint8_t &v : data)
    {
        cout << v;
    }

    cout << endl;
}


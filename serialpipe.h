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

#ifndef _SERIALPIPE_H
#define _SERIALPIPE_H

/* Data includes */
#include <vector>
#include <cstdint>
#include <string>

/* Serial include (https://github.com/wjwwood/serial) */
#include "serial/serial.h" // Needs to be updated based on project

/* Threading includes */
#include <thread>
#include <mutex>
#include <condition_variable>

namespace SerialPipe {

typedef std::function<void(const std::vector<uint8_t> &)> serial_callback;

/**
 * @brief   A serial library that takes care of the sending and receiving of
 *          serial data through threads and callbacks.
 *          Written for the C++14 standard.
 */
class SerialPipe
{
private:

    /** @brief Internal holder for a callback, including function pointer and
     *         ID for deletion late. This has the drawback of allowing a max of
     *         MAX_INT number of registrations / unregistrations. */
    struct serial_callback_holder
    {
        serial_callback_holder(int _id, serial_callback _cb)
            : id(_id), callback(_cb) { }

        int id;
        serial_callback callback;
    };

    /** @brief Holder for the serialport object. */
    serial::Serial *_serial;

    /** @brief Mutex for the serialport. */
    std::mutex _serial_lock;

    /** @brief Mutex for the ID counter. */
    std::mutex _idlock;

    /** @brief Vector holding the registered callbacks. */
    std::vector< serial_callback_holder > callbacks;

    /** @brief ID counter for the removal of subscriptions. */
    int _id;

    /** @brief Selector if the serial data is binary or ASCII with '\n'
     *         termination. */
    bool _string_data;

    /** @brief The holder of the transmit and receive threads. */
    std::thread _tx_thread, _rx_thread;

    /** @brief Data queue for transmit. */
    std::vector<uint8_t> _tx_buffer;

    /** @brief Condition variable for the transmit buffer. */
    std::condition_variable _tx_dowork;

    /** @brief Data queue for receive. */
    std::vector<uint8_t> _rx_buffer;

    /** @brief Mutexes for send/receive queue access. */
    std::mutex _tx_buffer_lock, _rx_buffer_lock;

    /** @brief Shutdown flag for the workers. */
    volatile bool shutdown;

    /** @brief Worker function for the read thread. */
    void readSerialWorker()
    {
        while (!shutdown)
        {
            /* Check so the port is open, else sleep and check again. */
            if (!_serial->isOpen())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            else
            {
                /* Read data based on the setup. */
                if (_string_data)
                {
                    std::string data;
                    int size = 0;

                    /* Read the string and process if any data is received. */
                    try {
                        size = _serial->readline(data);
                    }
                    catch (serial::SerialException e)
                    {
                        closePort();
                    }

                    if (size > 0)
                    {
                        std::lock_guard<std::mutex> locker(_rx_buffer_lock);

                        /* Move the data to the buffer. */
                        _rx_buffer.insert(_rx_buffer.begin(),
                                          data.begin(), data.end());

                        /* Run the callbacks and clear the buffer. */
                        executeCallbacks(_rx_buffer);
                        _rx_buffer.clear();
                    }
                }
                else
                {
                    std::lock_guard<std::mutex> locker(_rx_buffer_lock);
                    int size = _serial->read(_rx_buffer, 1000);

                    if (size > 0)
                    {
                        /* Run the callbacks and clear the buffer. */
                        executeCallbacks(_rx_buffer);
                        _rx_buffer.clear();
                    }
                }
            }
        }
    }

    /** @brief Worker function for the write thread. */
    void writeSerialWorker()
    {
        while (!shutdown)
        {
            /* Check so the port is open, else sleep and check again. */
            if (!_serial->isOpen())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            else
            {
                std::unique_lock<std::mutex> locker(_tx_buffer_lock);

                _tx_dowork.wait(locker, [&]() {
                    return (!_tx_buffer.empty() || shutdown);
                });

                /* Write the buffer to the serial. */
                if (_serial->isOpen())
                {
                    try {
                        _serial->write(_tx_buffer);
                        _tx_buffer.clear();
                    }
                    catch (serial::SerialException e)
                    {
                        closePort();
                    }
                }
            }
        }
    }

    /** @brief Execute callbacks with data. */
    void executeCallbacks(const std::vector<uint8_t> &payload)
    {
        for (auto &cb : callbacks)
            cb.callback(payload);
    }

public:

    /**
     * @brief   Constructor for the SerialPipe. Setups the serial
     *          port, calculates the inter-byte timeout and starts the
     *          transmission and reception threads.
     *
     * @note    It does not automatically open the serial port.
     *
     * @param[in] port  Port name: "/dev/ttyXX" for Linux, "COMX" for Windows.
     * @param[in] baudrate  Sets the baudrate of the port.
     * @param[in] timeout_ms    Sets the timeout for the communication.
     * @param[in] string_data   Sets if the data is binary or string.
     */
    SerialPipe(std::string port, unsigned int baudrate,
               unsigned int timeout_ms = 100, bool string_data = true)
        : _string_data(string_data)
    {
        std::lock_guard<std::mutex> locker(_serial_lock);

        _id = 0;
        shutdown = false;

        /* Calculate the inter-byte timeout based on 10 characters. */
        unsigned int inter_byte_timeout = (10*1000*10) / baudrate;

        /* If it was less than 1 ms, round up to 1 ms. */
        if (inter_byte_timeout == 0)
            inter_byte_timeout = 1;

        /* Connect to the serial port and start the workers. */
        serial::Timeout timeout(inter_byte_timeout, timeout_ms, 0,
                                timeout_ms, 0);

        /* Set the input port to "" in order to not automatically open. */
        _serial = new serial::Serial("", baudrate, timeout); // RAII
        _serial->setPort(port);

        _rx_thread = std::thread(&SerialPipe::readSerialWorker, this);
        _tx_thread = std::thread(&SerialPipe::writeSerialWorker, this);
    }

    /**
     * @brief   The destructor will gracefully stop the transaction threads
     *          and closes the serial port.
     *
     * @note    Automatically closes the serial port.
     */
    ~SerialPipe()
    {
        /* Set the shutdown flag and signal. */
        shutdown = true;
        _tx_dowork.notify_one();

        /* Wait for the threads to gracefully exit. */
        _rx_thread.join();
        _tx_thread.join();

        /* Close the serial port and delete it. */
        {
            std::lock_guard<std::mutex> locker(_serial_lock);

            /* If the serial is open, close it and then delete. */
            if (_serial->isOpen())
                _serial->close();

            delete _serial; // RAII
        }
    }


    /**
     * @brief   Register a callback for data received.
     *
     * @param[in]   The function to register.
     * @note        Shall be of the form void(const std::vector<uint8_t> &).
     *
     * @return      Returnb the ID of the callback, is used for unregistration.
     */
    int registerCallback(serial_callback callback)
    {
        std::lock_guard<std::mutex> locker(_idlock);

        /* Add the callback to the list. */
        callbacks.emplace_back(serial_callback_holder(_id, callback));

        return _id++;
    }

    /**
     * @brief   Unregister a callback from the queue.
     *
     * @param[in]   The ID supplied from @p registerCallback.
     *
     * @return      Return true if the ID was deleted.
     */
    bool unregisterCallback(int id)
    {
        std::lock_guard<std::mutex> locker(_idlock);

        /* Delete the callback with correct ID, a little ugly. */
        for (unsigned int i = 0; i < callbacks.size(); i++)
        {
            if (callbacks[i].id == id)
            {
                callbacks.erase(callbacks.begin() + i);

                return true;
            }
        }

        /* No match, return false. */
        return false;
    }

    /**
     * @brief   Transmit a data packet over the serial (individual bytes).
     *
     * @param[in]   A vector of bytes (uint8_t).
     */
    void serialTransmit(const std::vector<uint8_t> &data)
    {
        std::unique_lock<std::mutex> locker(_tx_buffer_lock);

        _tx_buffer.insert(_tx_buffer.end(), data.begin(), data.end());
        _tx_dowork.notify_one();
    }

    /**
     * @brief   Transmit a string packet over the serial.
     *
     * @param[in]   A string.
     */
    void serialTransmit(const std::string &data)
    {
        std::unique_lock<std::mutex> locker(_tx_buffer_lock);

        _tx_buffer.insert(_tx_buffer.end(), data.begin(), data.end());
        _tx_dowork.notify_one();
    }

    /**
     * @brief   Closes the serial port if it is open.
     */
    void closePort()
    {
        std::lock_guard<std::mutex> locker(_serial_lock);

        if (_serial->isOpen())
            _serial->close();
    }

    /**
     * @brief   Opens the serial port if it is closed.
     */
    void openPort()
    {
        std::lock_guard<std::mutex> locker(_serial_lock);

        if (!_serial->isOpen())
            _serial->open();
    }
};

} // namespace SerialPipe

#endif


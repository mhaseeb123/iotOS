/*
 * The MIT License (MIT)
 * Copyright (c) 2019 Muhammad Haseeb, Usman Tariq
 *      COP5614: Operating Systems | SCIS FIU
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SENSORS_H_
#define SENSORS_H_

 /* Include necessary headers */
#include <iostream>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <mutex>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include "rpc/server.h"
#include "rpc/client.h"

/* Status Codes */
#define SUCCESS                          0
#define ERR_NULL_PARAM                  -1000
#define ERR_THREAD_NUM                  -1001
#define ERR_UNINIT_LOCK                 -1002
#define ERR_TIMER_CREATION_FAILED       -1003
#define ERR_THREAD_CREATION_FAILED      -1004
#define ERR_INVLD_ARGS                  -1005
#define ERR_INVLD_IP                    -1006
#define ERR_DEV_NOT_REGD                -1007

/* Defined by the Gateway */
#define ERR_SENSOR_NOT_REGISTERED        4

/* Macros */
#define HOME                             1 << 0
#define AWAY                             1 << 1
#define EXIT                             0

/* Typedefs */
typedef int STATUS;
typedef unsigned int MODE;
typedef struct itimerval TIMER;
typedef std::mutex LOCK;

/* Function Prototypes */
std::string getIPAddress();
long long query_state(int device_id);
STATUS change_state(int device_id, int new_state);

#endif /* SENSORS_H_ */

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

#ifndef GATEWAY_H_
#define GATEWAY_H_

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
#include <chrono>
#include <ctime>
#include <dirent.h>
#include "rpc/server.h"
#include "rpc/client.h"

#define TEMP           0
#define MOTION         1
#define BULB           2
#define OUTLET         3

#define SUCCESS        0
#define FAILURE        1

#define OFF            0
#define ON             1

#define HOMETIMER      2000
#define TEMPSCALE      1000

typedef int STATUS;
typedef unsigned int MODE;
typedef struct itimerval TIMER;
typedef std::mutex LOCK;

#define HOME           1 << 0
#define AWAY           1 << 1
#define EXIT           0

/* Function Prototypes */
void change_mode(int inmode);
int registerf(std::string ltype, std::string lname);
void printHeader();
void askMode();
void printstuff();
long long query_state(int device_id);
void UpdateTimer(int msecs);
void DisableTimer();
int change_state(int device_id, int state);
void TimerExpired();
void text_message(int sig);
void *UserEntry(void *arg);
void *HeatManage(void *arg);
void *BulbManage(void *arg);
void change_mode(int lmode);
STATUS test_system();
STATUS main();

#endif /* GATEWAY_H_ */

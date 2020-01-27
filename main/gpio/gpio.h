/** @file gpio.h
 * 
 * @brief tcp2rtu gateway
 *
 * @par       
 * COPYRIGHT NOTICE: (c) 2020 Maksym Krasovskyi.  All rights reserved.
 */ 

#define GPIO_NUMBER 15

#define ID 0
#define MODE 1
#define PULL_U 2
#define PULL_D 3
#define MODE_MAX 4

#ifndef GPIO_H
#define GPIO_H
// {id, mode 0/1 (0 - input, 1 - output), pullup, pulldown, maxmode}
static int gpioConfig[GPIO_NUMBER][5];
int gpioDefaultConfig[GPIO_NUMBER][5] = {
    {2, 0, 0, 0, 1},
    {4, 0, 0, 0, 1},
    {5, 0, 0, 0, 1},
    {12, 0, 0, 0, 1},
    {13, 0, 0, 0, 1},
    {14, 0, 0, 0, 1},
    {15, 0, 0, 0, 1},
    {18, 0, 0, 0, 1},
    {23, 0, 0, 0, 1},
    {32, 0, 0, 0, 1},
    {33, 0, 0, 0, 1},
    {34, 0, 0, 0, 0},
    {35, 0, 0, 0, 0},
    {36, 0, 0, 0, 0},
    {39, 0, 0, 0, 0}
};

#endif /* GPIO_H */



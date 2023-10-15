/*
 * Babelfish
 *
 * Copyright (C) 2023 Vladimir Vukicevic
 * 
 */

#ifndef __BABELFISH_H__
#define __BABELFISH_H__

#include <pico/stdlib.h>

#include "babelfish_hw.h"
#include "events.h"
#include "host.h"
#include "debug.h"

extern uint8_t const ascii_to_hid[128][2];
extern uint8_t const hid_to_ascii[128][2];

#endif
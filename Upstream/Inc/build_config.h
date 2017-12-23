/*
 * build_config.h
 *
 *  Created on: Jun 20, 2017
 *      Author: Robert Fisk
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef INC_BUILD_CONFIG_H_
#define INC_BUILD_CONFIG_H_


#define CONFIG_MASS_STORAGE_ENABLED
#define CONFIG_MASS_STORAGE_WRITES_PERMITTED

#define CONFIG_KEYBOARD_ENABLED
#define CONFIG_KEYBOARD_BOT_DETECT_ENABLED

#define CONFIG_MOUSE_ENABLED
#define CONFIG_MOUSE_BOT_DETECT_ENABLED


//Configure keyboard bot detection here:
#ifdef CONFIG_KEYBOARD_BOT_DETECT_ENABLED
    //-----------------------------------------------------------
    //Adjust this threshold first to tune keyboard bot detection. Lower values = more sensitive
    #define KEYBOARD_BOTDETECT_TEMPORARY_LOCKOUT_BIN_THRESHOLD  4
    //-----------------------------------------------------------

    #define KEYBOARD_BOTDETECT_FAST_BIN_WIDTH_MS                10      //10ms per bin
    #define KEYBOARD_BOTDETECT_SLOW_BIN_WIDTH_MS                20      //20ms per bin
    #define KEYBOARD_BOTDETECT_FAST_BIN_COUNT                   30      //30 bins at 10ms = 300ms fast coverage
    #define KEYBOARD_BOTDETECT_SLOW_BIN_COUNT                   50      //50 bins at 20ms = 1 sec slow coverage, wrapped

    #define KEYBOARD_BOTDETECT_FAST_BIN_DRAIN_DIVIDER           2
    #define KEYBOARD_BOTDETECT_SLOW_BIN_DRAIN_DIVIDER           4
#endif

//Configure mouse bot detection here:
#ifdef CONFIG_MOUSE_BOT_DETECT_ENABLED
    //-----------------------------------------------------------
    //Adjust this threshold first to tune mouse bot detection. Lower values = more sensitive
    #define MOUSE_BOTDETECT_TEMPORARY_LOCKOUT_VELOCITY_THRESHOLD  4
    //-----------------------------------------------------------

    #define MOUSE_BOTDETECT_VELOCITY_HISTORY_SIZE               12
    #define MOUSE_BOTDETECT_VELOCITY_MATCH_BASE                 256
    #define MOUSE_BOTDETECT_VELOCITY_MATCH_ERROR                6
    #define MOUSE_BOTDETECT_MOVE_DELAY_LIMIT                    5

#endif

//Configuration common to all bot detectors
#if defined CONFIG_KEYBOARD_BOT_DETECT_ENABLED || defined CONFIG_MOUSE_BOT_DETECT_ENABLED
    #define BOTDETECT_TEMPORARY_LOCKOUT_TIME_MS                 4000
    #define BOTDETECT_TEMPORARY_LOCKOUT_FLASH_TIME_MS           60000   //Flash fault LED for 60 seconds after temporary lockout
#endif


#endif /* INC_BUILD_CONFIG_H_ */
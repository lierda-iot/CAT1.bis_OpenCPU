/**
 * @file liot_keypad_demo.c
 * @brief Keypad demo implementation for Lierda modules
 *
 * This demo demonstrates how to use the keypad controller (KPC) functionality
 * on Lierda modules. It shows how to initialize the keypad, configure row and column pins,
 * register callback functions, and process key press events.
 *
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 1.1
 * @date 2025-08-27
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */
#include <stdio.h>
#include "liot_keypad.h"
#include "liot_os.h"
#include "lierda_app_main.h"



/*
   This project demonstrates the usage of KPC, the keypad layout is:

  +---------+  +---------+  +---------+  +---------+                ----+     +-----
  |         |  |         |  |         |  |         |                    |     |
  |    1    |--|    2    |--|    3    |--|    A    |--------- ROW0      |     |
  |         |  |         |  |         |  |         |                    |     |
  +---------+  +---------+  +---------+  +---------+                    +-----+
       |            |            |            |
  +---------+  +---------+  +---------+  +---------+                ----------+     +-----
  |         |  |         |  |         |  |         |                          |     |
  |    4    |--|    5    |--|    6    |--|    B    |--------- ROW1            |     |
  |         |  |         |  |         |  |         |                          |     |
  +---------+  +---------+  +---------+  +---------+                          +-----+
       |            |            |            |
  +---------+  +---------+  +---------+  +---------+                ----------------+     +-----
  |         |  |         |  |         |  |         |                                |     |
  |    7    |--|    8    |--|    9    |--|    C    |--------- ROW2                  |     |
  |         |  |         |  |         |  |         |                                |     |
  +---------+  +---------+  +---------+  +---------+                                +-----+
       |            |            |            |
  +---------+  +---------+  +---------+  +---------+                ----------------------+     +-----
  |         |  |         |  |         |  |         |                                      |     |
  |   Func  |--|    0    |--|   Menu  |--|    D    |--------- ROW3                        |     |
  |         |  |         |  |         |  |         |                                      |     |
  +---------+  +---------+  +---------+  +---------+                                      +-----+
       |            |            |            |                         ^
       |            |            |            |                         |

      COL4         COL3         COL2         COL0                 row scan waveform
 */


/* NT26FCNB10WNA & NT26FCNB00WNA */
/* Module PIN:16  Pad Name:GPIO27   paddr:52  */
#define LIERDA_KEYPAD_ROW_0_MUDULE_PIN      (16)

/* Module PIN:25  Pad Name:GPIO26   paddr:51 */
#define LIERDA_KEYPAD_ROW_1_MUDULE_PIN      (25)

/* Module PIN:106  Pad Name:GPIO25   paddr:50 */
#define LIERDA_KEYPAD_ROW_2_MUDULE_PIN      (106)

/* Module PIN:20  Pad Name:GPIO24   paddr:49 */
#define LIERDA_KEYPAD_ROW_3_MUDULE_PIN      (20)

/* Module PIN:29  Pad Name:GPIO13   paddr:28 */
#define LIERDA_KEYPAD_COLUMN_0_MUDULE_PIN   (29)

/* Module PIN:28  Pad Name:GPIO12   paddr:27 */
#define LIERDA_KEYPAD_COLUMN_1_MUDULE_PIN   (28)

/* Module PIN:56  Pad Name:GPIO7   paddr:22 */
#define LIERDA_KEYPAD_COLUMN_2_MUDULE_PIN   (56)

/* Module PIN:58  Pad Name:GPI14   paddr:29 */
#define LIERDA_KEYPAD_COLUMN_3_MUDULE_PIN   (58)

#define LIERDA_FUNC_KEY_CODE               (0xFE)

#define LIERDA_MENU_KEY_CODE               (0xFD)

/**
 * @brief Key code mapping table
 *
 * This 2D array maps the physical key positions (row/column) to their corresponding
 * character codes. The mapping follows the 4x4 keypad layout described above.
 */
const uint8_t LierdakeyCodes[LIOT_KEYPAD_ROW_LENGTH][LIOT_KEYPAD_COL_LENGTH] =
{
    {'A',	'3',            		'2',        '1'   },
    {'B',	'6',            		'5',        '4'   },
    {'C',	'9',            		'8',        '7'   },
    {'D',	LIERDA_FUNC_KEY_CODE,   '0',        LIERDA_MENU_KEY_CODE},
};

/**
 * @brief Keypad callback function
 *
 * This callback function is called whenever a key event occurs (press, release, repeat).
 * It logs the row, column, and press state of the key event.
 *
 * @param cb Keypad report event structure containing row, column, and value information
 */
void ql_keypad_callback(liot_kpc_report_event_t cb)
{   
    liot_trace("row:%d col:%d pressd:%d", cb.row, cb.column, cb.value);
}

/**
 * @brief Keypad demo main thread
 *
 * This is the main thread function for the keypad demonstration. It performs the following operations:
 * 1. Initializes the keypad with row and column pin configurations
 * 2. Registers the keypad callback function
 * 3. Enters a loop to continuously check for key events
 * 4. Processes different key events based on key codes:
 *    - Function key: Toggles a flag when pressed
 *    - Menu key: Handles both short press and long press (2.5s)
 *    - Regular keys: Outputs the key character, with optional prefix when Function is active
 *
 * @param arvg Thread argument (unused)
 */
void liot_keypad_demo_thread(void *arvg)
{
	uint32_t funcKeyPressCount = 0, menuKeyPressCount = 0;
	uint8_t pressed,id,get_row,get_col;
    // Row pin configuration array
    uint8_t row[LIOT_KEYPAD_ROW_LENGTH] = {
        LIERDA_KEYPAD_ROW_0_MUDULE_PIN,
        LIERDA_KEYPAD_ROW_1_MUDULE_PIN,
        LIERDA_KEYPAD_ROW_2_MUDULE_PIN,
        LIERDA_KEYPAD_ROW_3_MUDULE_PIN};

    // Column pin configuration array
    uint8_t col[LIOT_KEYPAD_COL_LENGTH] = {
        LIERDA_KEYPAD_COLUMN_0_MUDULE_PIN,
        LIERDA_KEYPAD_COLUMN_1_MUDULE_PIN,
        LIERDA_KEYPAD_COLUMN_2_MUDULE_PIN,
        LIERDA_KEYPAD_COLUMN_3_MUDULE_PIN};

    liot_rtos_task_sleep_ms(1000);
	liot_trace("task Lierda_keyPAD.");
    // Initialize keypad with callback function and pin configurations
    liot_keypad_init(ql_keypad_callback, row, col);
    // Main loop to process key events
    while(1)
    {
        // Check for key state changes
        if(liot_keypad_state(&pressed, &id)==LIOT_KEYPAD_SUCCESS)
        {
            // Calculate row and column from key ID
            get_row = id/LIOT_KEYPAD_COL_LENGTH;
        	get_col = id%LIOT_KEYPAD_COL_LENGTH;
            switch(LierdakeyCodes[get_row][get_col])
            {
                case LIERDA_FUNC_KEY_CODE:
                {
                    if(pressed == LIOT_KPC_REPORT_KEY_PRESS)
                    {
                        // Toggle function key state
                        funcKeyPressCount++;
                    }
                }
                break;

                case LIERDA_MENU_KEY_CODE:
                {
                    if(pressed == LIOT_KPC_REPORT_KEY_REPEAT)
                    {
                        // Count repeat events for long press detection
                        menuKeyPressCount++;

                        // Detect long press after 2.5s (50 * 10ms intervals)
                        if(menuKeyPressCount == 50)
                        {
                            liot_trace("Menu key long press\r\n");
                        }

                    }
                    else if(pressed == LIOT_KPC_REPORT_KEY_PRESS)
                    {
                        // Handle short press
                        liot_trace("Menu key press\r\n");
                        menuKeyPressCount = 0;
                    }
                    else
                    {
                        // Reset counter on key release
                        menuKeyPressCount = 0;
                    }
                }
                break;

                default:
                {
                    // Handle regular key events
                    if(pressed != LIOT_KPC_REPORT_KEY_RELEASE)
                    {
                        // Check if Function key is active (odd count means active)
                        if(funcKeyPressCount & 1)
                        {
                            // Output key with 'F' prefix when Function is active
                            liot_trace("F%c\r\n", LierdakeyCodes[get_row][get_col]);
                        }
                        else
                        {
                            // Output regular key character
                            liot_trace("%c\r\n", LierdakeyCodes[get_row][get_col]);
                        }
                    }
                }
                break;
            }
        }
		liot_rtos_task_sleep_ms(10);
    }
}




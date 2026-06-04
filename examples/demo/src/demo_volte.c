/**
 * @file liot_volte_demo.c
 * @brief VoLTE (Voice over LTE) demo implementation
 *
 * This demo demonstrates how to use the VoLTE functionality on the CAT1 module.
 * It includes IMS registration, voice call setup, call management, and handling
 * of various VoLTE events through callback functions.
 *
 * @author ljz email:ciot_iot_support@lierda.com
 * @version 2.3
 * @date 2025-08-27
 *
 * @copyright Copyright (c) 2023 Zhejiang Lierda Internet of Things Technology Co., Ltd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lierda_app_main.h"
#include "liot_os.h"
#include "liot_volte.h"
#include "liot_voice_call.h"
#include "liot_type.h"

// Define structure for voice call events
typedef struct
{
	uint32 id;	   // Event identifier
	uint32 param1; // First parameter
	void *param2;  // Second parameter
} liot_volte_event_t;

// Define maximum size and number of the event queue
#define DEMO_VOLTE_MAX_SIZE (sizeof(liot_volte_event_t))
#define DEMO_VOLTE_MAX_NUM 10
// Enable auto-answer for incoming calls
#define VOLTE_AUTO_ANSWER_ENABLE 1
// Declare message queue and timer
static liot_queue_t volte_msg_queue = NULL;
liot_timer_t change_volume_timer = NULL;

/**
 * @brief Voice call event callback function
 *
 * @param sim       SIM card index, value: 0-1
 * @param event_id  Event identifier
 * @param ctx       Context parameter containing event-specific data
 */
void user_voice_call_event_callback(uint8_t sim, int event_id, void *ctx)
{
	liot_volte_event_t vc_event;

	vc_event.id = event_id;
	vc_event.param1 = sim;
	if (NULL != ctx) // Check if context is valid
	{
		vc_event.param2 = ctx;
	}
	// Release event to the message queue
	liot_rtos_queue_release(volte_msg_queue, DEMO_VOLTE_MAX_SIZE, (uint8_t *)&vc_event, LIOT_NO_WAIT);
}

/**
 * @brief Set volume timer callback function
 *
 * This function is called periodically by the timer to adjust the volume.
 * It cycles the volume from 0 to 100 in steps of 20.
 *
 * @param ctx No argument required (unused)
 *
 * @note This function is currently commented out and not used in the demo.
 */
void liot_set_volume_timer_callback(void *ctx)
{
	static uint8_t volte_volume = 0;

	volte_volume += 20;
	if (volte_volume > 100)
	{
		volte_volume = 0;
	}
	liot_trace("liot_set_volume_timer_callback:%d", volte_volume);
	liot_volte_volume_set(0, volte_volume);
}

/**
 * @brief Application main thread for VoLTE demo
 *
 * This is the main task function for the VoLTE demonstration. It performs the following operations:
 * 1. Registers voice call event callback
 * 2. Creates message queue for event handling
 * 3. Configures VoLTE settings (codec type, IMS registration, usage mode, domain preference)
 * 4. Sets up auto-answer for incoming calls (if enabled)
 * 5. Processes events from the message queue in a loop:
 *    - IMS registration indication
 *    - Incoming call indication
 *    - Call connection indication
 *    - Call disconnection indication
 *    - Incoming number indication
 *    - Audio sample rate indication
 *
 * @param param No argument required (unused)
 */
void liot_volte_demo_task(void *param)
{
	liot_volte_errcode_e err = LIOT_VOLTE_SUCCESS;
	LiotOSStatus_t ret = LIOT_SUCCESS;
	uint8_t nSim = 0;
	liot_volte_event_t msg;
	uint8_t total = 0;
	uint8_t vdp = 0;
	uint8_t usage = 0;
	uint8_t ims_reg = 0;
	uint8_t telStr[20] = {0};

	liot_vc_info_s vc_info[LIOT_VC_MAX_NUM] = {0};

	liot_trace("liot_volte_demo_task enter");

	// Register voice call event callback
	liot_voice_call_callback_register(user_voice_call_event_callback);
	// Create message queue for event handling
	liot_rtos_queue_create(&volte_msg_queue, DEMO_VOLTE_MAX_SIZE, DEMO_VOLTE_MAX_NUM);
	// Set codec type to 8311
	liot_volte_codec_type_set(0, 1);

	// Enable IMS registration required for VoLTE
	err = liot_volte_ims_reg_set(nSim, LIOT_VOLTE_ENABLE_REG_INFO);
	if (err != LIOT_VOLTE_SUCCESS)
	{
		liot_trace("liot_volte_ims_reg_set FAIL");
	}
	else
	{
		liot_trace("liot_volte_ims_reg_set OK");
	}
	// Configure device to prioritize Data Centric
	err = liot_volte_usage_set(nSim, 1);
	if (err != LIOT_VOLTE_SUCCESS)
	{
		liot_trace("data centric set FAIL,err = %d", err);
	}
	else
	{
		liot_trace("data centric set OK");
	}
	// Confirm current service mode
	err = liot_volte_usage_get(nSim, &usage);
	if (err != LIOT_VOLTE_SUCCESS)
	{
		liot_trace("data centric get FAIL,err = %d", err);
	}
	else
	{
		liot_trace("data centric get OK:%d", usage);
	}
	// Set domain preference to PS for VoLTE support
	err = liot_volte_vdp_set(nSim, 4);
	if (err != LIOT_VOLTE_SUCCESS)
	{
		liot_trace("get IMS PS Voice preferred FAIL,err=%d", err);
	}
	else
	{
		liot_trace("get IMS PS Voice preferred OK");
	}
	// Verify current voice domain preference
	err = liot_volte_vdp_get(nSim, &vdp);
	if (err != LIOT_VOLTE_SUCCESS)
	{
		liot_trace("get IMS PS Voice preferred FAIL,err=%d", err);
	}
	else
	{
		liot_trace("get IMS PS Voice preferred OK:%d", vdp);
	}
#if VOLTE_AUTO_ANSWER_ENABLE // Set auto-answer
	err = liot_voice_auto_answer(nSim, 5);
	if (err != LIOT_VOLTE_SUCCESS)
	{
		liot_trace("liot_voice_auto_answer parm err,err=%d", err);
	}
	else
	{
		liot_trace("liot_voice_auto_answer OK");
	}
#endif

	// Main loop to process events from queue
	while (1)
	{
		memset(&msg, 0, sizeof(msg));
		ret = liot_rtos_queue_wait(volte_msg_queue, (uint8_t *)&msg, DEMO_VOLTE_MAX_SIZE, LIOT_WAIT_FOREVER);
		if (ret != LIOT_SUCCESS)
		{
			continue;
		}
		switch (msg.id)
		{
			case LIOT_VC_IMSREG_IND:
			{
				// Handle IMS registration status
				liot_volte_ims_info_s *ims_info = (liot_volte_ims_info_s *)msg.param2;
				liot_trace("nSim=%d, ims reg info : %d", msg.param1, ims_info->reg_info);
				// If registered successfully, proceed with making a call
				if (ims_info->reg_info == LIOT_VOLTE_REG_SUCCESS)
				{

					// Get local phone number via IMS
					err = liot_voice_get_phone_num(nSim, telStr);
					if (err != LIOT_VOLTE_SUCCESS)
					{
						liot_trace("liot_voice_get_phone_num FAIL");
					}
					else
					{
						liot_trace("liot_voice_get_phone_num OK, phone number:%s", telStr);
					}

					// make a call
					liot_trace("start call");
					err = liot_voice_call_start(nSim, "10086");
					if (err != LIOT_VOLTE_SUCCESS)
					{
						liot_trace("liot_voice_call_start FAIL,err=%d", err);
						// Check current IMS registration state
						err = liot_volte_ims_reg_get(nSim, &ims_reg);
						if (err != LIOT_VOLTE_SUCCESS)
						{
							liot_trace("liot_volte_ims_reg_get FAIL");
						}
						else
						{
							liot_trace("liot_volte_ims_reg_get OK, ims reg:%d", ims_reg);
							if (ims_reg == 0)
							{
								liot_volte_ims_set(nSim, 1);
							}
						}
					}
					else
					{
						liot_trace("liot_voice_call_start OK");
					}
				}
				else
				{
					liot_trace("IMS protocol registration failed");
				}
			}
			break;

			case LIOT_VC_RING_IND:
			{
// Answer incoming call manually after delay
#if !VOLTE_AUTO_ANSWER_ENABLE
				liot_rtos_task_sleep_s(10);
				err = liot_voice_call_answer(nSim);
				if (err != LIOT_VOLTE_SUCCESS)
				{
					liot_trace("liot_voice_call_answer FAIL");
				}
				else
				{
					liot_trace("liot_voice_call_answer OK");
				}
#else
				liot_trace("incoming call...");
#endif
			}
			break;

			case LIOT_VC_CONNECT_IND:
			{
				// Call connected - check current call information
				liot_trace("CONNECT");
				err = liot_voice_call_clcc(nSim, &total, vc_info);
				if (err != LIOT_VOLTE_SUCCESS)
				{
					liot_trace("liot_voice_call_clcc FAIL");
				}
				else
				{
					liot_trace("total number = %d", total);
					for (uint8_t i = 0; i < total; i++)
					{
						liot_trace("index:%d direction:%d status:%d mpty:%d number:%s",
								vc_info[i].idx, vc_info[i].direction, vc_info[i].status, vc_info[i].multiparty, vc_info[i].number);
					}
				}
				// Start periodic volume adjustment timer
				liot_rtos_timer_create(&change_volume_timer, 1, liot_set_volume_timer_callback, NULL);
				liot_rtos_timer_start(change_volume_timer, 2000);
	// Test code for active hang-up
#if 0
				liot_rtos_task_sleep_s(10);
				err = liot_voice_call_end(nSim);
				if(err != LIOT_VOLTE_SUCCESS){
					liot_trace("liot_voice_call_end FAIL");
				}else{
					liot_trace("liot_voice_call_end OK");
				}
#endif
			}
			break;

			case LIOT_VC_NOCARRIER_IND:
			{
				// Call disconnected - handle cleanup
				liot_trace("NOCARRIER");
				liot_rtos_timer_stop(change_volume_timer);
				liot_rtos_timer_delete(change_volume_timer);
				// Verify no ongoing calls
				err = liot_voice_call_clcc(nSim, &total, vc_info);
				if (err != LIOT_VOLTE_SUCCESS)
				{
					liot_trace("liot_voice_call_clcc FAIL");
				}
				else
				{
					liot_trace("total number = %d", total);
					for (uint8_t i = 0; i < total; i++)
					{
						liot_trace("index:%d direction:%d status:%d mpty:%d number:%s",
								vc_info[i].idx, vc_info[i].direction, vc_info[i].status, vc_info[i].multiparty, vc_info[i].number);
					}
				}
			}
			break;

			case LIOT_VC_INCOMING_NUMBER_IND:
			{
				// Display incoming caller ID
				liot_volte_incoming_num_s *incoming_num = (liot_volte_incoming_num_s *)msg.param2;
				liot_trace("nSim=%d, incoming num : %d, %s", msg.param1, incoming_num->dialNumStrLen, incoming_num->dialNumStr);
			}
			break;

			case LIOT_VC_SAMPLERATE_IND:
			{
				// Show current audio sampling rate
				liot_volte_samplerate_s *samplerate = (liot_volte_samplerate_s *)msg.param2;
				liot_trace("nSim=%d, lrclk : %d, mclk : %d", msg.param1, samplerate->lrclk, samplerate->mclk);
			}
			break;

			default:
			{
				// Unknown event
				liot_trace("event id = 0x%x", msg);
			}
				break;
		}
	}
	// Free allocated memory
	if (msg.param2 != NULL)
	{
		free(msg.param2);
		msg.param2 = NULL;
	}
	// Delete the task
	liot_rtos_task_delete(NULL);
}


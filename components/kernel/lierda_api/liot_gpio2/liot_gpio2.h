/**  @file
  liot_gpio2.h
  @brief GPIO2 API interface definitions for LIOT platform
*/

#ifndef _LIOT_GPIO2_H_
#define _LIOT_GPIO2_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "liot_osi_def.h"
#include "liot_type.h"

#define LIOT_GPIO_ERRCODE_BASE (LIOT_COMPONENT_BSP_GPIO << 16)
/*========================================================================
 *	Enum
 *========================================================================*/
/*!
 * @brief GPIO operation return codes.
 *
 * @details This enumeration lists all possible error codes returned by GPIO-related functions.
 */
typedef enum {
    L_GPIO_ERR_SUCCESS  = LIOT_SUCCESS,                    /*!< Operation was successful */
    L_GPIO_ERR_EXECUTE  = 1 | LIOT_GPIO_ERRCODE_BASE,      /*!< General execution error */
    L_GPIO_ERR_INVALID_PARAM,                              /*!< Invalid input parameter */
    L_GPIO_ERR_OPEN,                                       /*!< Failed to open GPIO */
    L_GPIO_ERR_CONFIG,                                     /*!< Configuration failed */
    L_GPIO_ERR_PULL_SET,                                   /*!< Pull resistor setup failed */
    L_GPIO_ERR_CALLBACK,                                   /*!< Callback registration failed */
    L_GPIO_ERR_LEVEL_TRIGGER                               /*!< Level trigger configuration failed */
} liot_gpioerr_e;

/*!
 * @brief GPIO pin identifier.
 *
 * @details This enumeration defines the available GPIO pins on the module. Each value represents a specific GPIO number,
 *          The GPIOs of EC716 and EC718 are different, and this should be noted. Please refer to the I/O multiplexing table for details.
 */
typedef enum
{
    L_GPIO_0 = 0,     /*!< GPIO pin 0 */
    L_GPIO_1,         /*!< GPIO pin 1 */
    L_GPIO_2,         /*!< GPIO pin 2 */
    L_GPIO_3,         /*!< GPIO pin 3 */
    L_GPIO_4,         /*!< GPIO pin 4 */
    L_GPIO_5,         /*!< GPIO pin 5 */
    L_GPIO_6,         /*!< GPIO pin 6 */
    L_GPIO_7,         /*!< GPIO pin 7 */
    L_GPIO_8,         /*!< GPIO pin 8 */
    L_GPIO_9,         /*!< GPIO pin 9 */
    L_GPIO_10,        /*!< GPIO pin 10 */
    L_GPIO_11,        /*!< GPIO pin 11 */
    L_GPIO_12,        /*!< GPIO pin 12 */
    L_GPIO_13,        /*!< GPIO pin 13 */
    L_GPIO_14,        /*!< GPIO pin 14 */
    L_GPIO_15,        /*!< GPIO pin 15 */
    L_GPIO_16,        /*!< GPIO pin 16 */
    L_GPIO_17,        /*!< GPIO pin 17 */
    L_GPIO_18,        /*!< GPIO pin 18 */
    L_GPIO_19,        /*!< GPIO pin 19 */
    L_GPIO_20,        /*!< GPIO pin 20 */
    L_GPIO_21,        /*!< GPIO pin 21 */
    L_GPIO_22,        /*!< GPIO pin 22 */
    L_GPIO_23,        /*!< GPIO pin 23 */
    L_GPIO_24,        /*!< GPIO pin 24 */
    L_GPIO_25,        /*!< GPIO pin 25 */
    L_GPIO_26,        /*!< GPIO pin 26 */
    L_GPIO_27,        /*!< GPIO pin 27 */
    L_GPIO_28,        /*!< GPIO pin 28 */
    L_GPIO_29,        /*!< GPIO pin 29 */
    L_GPIO_30,        /*!< GPIO pin 30 */
    L_GPIO_31,        /*!< GPIO pin 31 */
    L_GPIO_32,        /*!< GPIO pin 32 */
    L_GPIO_33,        /*!< GPIO pin 33 */
    L_GPIO_34,        /*!< GPIO pin 34 */
    L_GPIO_35,        /*!< GPIO pin 35 */
    L_GPIO_36,        /*!< GPIO pin 36 */
    L_GPIO_37,        /*!< GPIO pin 37 */
    L_GPIO_38,        /*!< GPIO pin 38 */
    L_GPIO_MAX       /*!< Maximum index for GPIO pin numbers (not a valid pin) */
} liot_gpio_e;

/*!
 * @brief GPIO direction configuration.
 *
 * @details This enumeration defines the two possible directions for a GPIO pin.
 */
typedef enum
{
    L_IO_INPUT,      /*!< Configure the GPIO as an input pin */
    L_IO_OUTPUT,     /*!< Configure the GPIO as an output pin */
} liot_gpiodir_e;

/*!
 * @brief GPIO logic level configuration.
 *
 * @details This enumeration defines the possible logic levels that a GPIO pin can be set to or read from.
 */
typedef enum {
    L_IO_LOW,       /*!< Logic low level (0V or GND) */
    L_IO_HIGH,      /*!< Logic high level (The value is related to the voltage level of the corresponding power domain that is set.) */
    L_IO_NONE       /*!< Unknown logic level*/
} liot_gpiolvl_e;

/*!
 * @brief GPIO interrupt trigger configuration.
 *
 * @details This enumeration defines the available interrupt trigger modes for a GPIO pin.
 */
typedef enum {
    L_INT_SIG_NONE      = 0U,
    L_INT_LEVEL_LOW     = 1U,        /*!< Trigger interrupt on level low */
    L_INT_LEVEL_HIGH    = 2U,       /*!< Trigger interrupt on level high */
    L_INT_EDGE_FALL     = 3U,        /*!< Trigger interrupt on signal edge falling */
    L_INT_EDGE_RISE     = 4U,        /*!< Trigger interrupt on signal edge rising */
    L_INT_EDGE_BOTH     = 5U,        /*!< Trigger interrupt on both rising and falling edges */
} liot_intsig_e;

/*!
 * @brief GPIO pull-up/pull-down configuration.
 *
 * @details This enumeration defines the available pull-up and pull-down modes for a GPIO pin.
 */
typedef enum {
    LIOT_PULL_DEFAULT = 0,            /*!< Use default pull mode determined by hardware/function */
    LIOT_FORCE_PULL_NONE,             /*!< Force no pull resistor (neither pull-up nor pull-down) */
    LIOT_FORCE_PULL_DOWN,             /*!< Force pull-down resistor enabled */
    LIOT_FORCE_PULL_UP                /*!< Force pull-up resistor enabled */
} liot_gpio_pull_mode_e;

/*!
 * @brief Wakeup pin configuration structure.
 *
 * @details This structure holds the configuration parameters for a wakeup pin,
 *          including pull resistor setting and edge detection mode.
 */
typedef struct {
    liot_gpio_pull_mode_e wakeup_pull;   /*!< Pull-up or pull-down configuration for the wakeup pin */
    liot_intsig_e wakeup_edge;   /*!< Edge detection mode for the wakeup pin */
} liot_wakeup_cfg_t;

/*!
 * @brief Wakeup source identifier.
 *
 * @details This enumeration defines the available wakeup sources that can be used to wake up the system from low-power mode,
 *          The specific availability status needs to be related to the actual hardware.
 */
typedef enum
{
    L_WAKEUPAD_0 = 0,       /*!< Wakeup source 0 */
    L_WAKEUPAD_1,           /*!< Wakeup source 1 */
    L_WAKEUPAD_2,           /*!< Wakeup source 2 */
    L_WAKEUPAD_3,           /*!< Wakeup source 3 */
    L_WAKEUPAD_4,           /*!< Wakeup source 4 */
    L_WAKEUPAD_5            /*!< Wakeup source 5 */
} liot_wakeuppad_e;

/*!
 * @brief GPIO voltage level configuration.
 *
 * @details This enumeration defines the available voltage levels that can be set for a GPIO pin.
 */
typedef enum
{
    // @ 1.8V level
	L_VOLT_1_65V = 0,
	L_VOLT_1_70V,
	L_VOLT_1_75V,
	L_VOLT_1_80V,
	L_VOLT_1_85V,
	L_VOLT_1_90V,
	L_VOLT_1_95V,
	L_VOLT_2_00V,

	// @ 2.8V level
	L_VOLT_2_65V = 8,
	L_VOLT_2_70V,
	L_VOLT_2_75V,
	L_VOLT_2_80V,
	L_VOLT_2_85V,
	L_VOLT_2_90V,
	L_VOLT_2_95V,
	L_VOLT_3_00V,

	// @ 3.3V level
	L_VOLT_3_05V = 16,
	L_VOLT_3_10V,
	L_VOLT_3_15V,
	L_VOLT_3_20V,
	L_VOLT_3_25V,
	L_VOLT_3_30V,
	L_VOLT_3_35V,
	L_VOLT_3_40V,
} liot_volt_e;

typedef enum
{
    L_DOMAIN_NORMAL = 0,
    L_DOMAIN_AON,
    L_DOMAIN_ALL
} liot_powerdomain_e;

/*!
 * @brief GPIO pin function multiplexing configuration.
 *
 * @details This enumeration defines the available function multiplexing options for a GPIO pin.
 */
typedef enum
{
    L_PIN_FUNC_0 = 0U,
    L_PIN_FUNC_1 = 1U,
    L_PIN_FUNC_2 = 2U,
    L_PIN_FUNC_3 = 3U,
    L_PIN_FUNC_4 = 4U,
    L_PIN_FUNC_5 = 5U,
    L_PIN_FUNC_6 = 6U,
    L_PIN_FUNC_7 = 7U,
    L_PIN_FUNC_UNKNOWN = 0xFF
} liot_pinfunc_e;

/*========================================================================
 *	struct
 *========================================================================*/
/*!
 * @brief GPIO interrupt callback structure.
 *
 * @details This structure contains the interrupt trigger type and the callback function
 *          to be executed when the interrupt occurs.
 */
typedef struct
{
    liot_intsig_e signal;
    void (*callback)(void* arg);
    void* arg;
} liot_intcb_t;

/*========================================================================
 *	api function
 *========================================================================*/
/**
 * @brief Initialize a GPIO pin with the specified configuration.
 * @details Initialize a GPIO pin with the specified configuration including direction,
 *          initial level, and interrupt settings.
 * @param gpio The GPIO number (module peripheral pin).
 * @param mode The direction of the GPIO pin: input or output.
 * @param level The logic level to set for the GPIO pin (only valid for output mode).
 * @param intcb Pointer to the interrupt callback structure (can be NULL if interrupt is not needed).
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_GpioInit(liot_gpio_e gpio, liot_gpiodir_e mode, liot_gpiolvl_e level, liot_intcb_t* intcb);

/**
 * @brief Initialize a GPIO pin with the specified configuration.
 * @details Initialize a GPIO pin with the specified configuration including direction,
 *          initial level, and interrupt settings.
 * @param modempin The physical pin number on the module.
 * @param mode The direction of the GPIO pin: input or output.
 * @param level The logic level to set for the GPIO pin (only valid for output mode).
 * @param intcb Pointer to the interrupt callback structure (can be NULL if interrupt is not needed).
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_GpioInitDirect(int modempin, liot_gpiodir_e mode, liot_gpiolvl_e level, liot_intcb_t* intcb);

/**
 * @brief Get the current logic level of a GPIO pin.
 *
 * @details This function reads the current state (high or low) of the specified GPIO pin.
 *          The result is valid only if the pin is configured as an input.
 *
 * @param gpio The GPIO number (module peripheral pin).
 *
 * @return The current logic level of the GPIO pin: L_IO_HIGH or L_IO_LOW.
 */
liot_gpiolvl_e Liot_GpioGetLevel(liot_gpio_e gpio);

/**
 * @brief Set the logic level (high or low) of a GPIO pin.
 *
 * @details This function sets the output state of a specified GPIO pin to either high or low. 
 *          The function has effect only if the pin is configured as an output. 
 *          Attempting to use this function on an input-configured pin may result in undefined behavior.
 *
 * @param gpio The GPIO number (module peripheral pin).
 * @param level The logic level to set: L_IO_HIGH or L_IO_LOW.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_GpioSetLevel(liot_gpio_e gpio, liot_gpiolvl_e level);

/**
 * @brief Enable GPIO interrupt functionality.
 *
 * @details This function enables the GPIO interrupt functionality for all configured GPIO pins.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_GpioIntEnable(void);

/**
 * @brief Disable GPIO interrupt functionality.
 *
 * @details This function disables the GPIO interrupt functionality for all GPIO pins.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_GpioIntDisable(void);

/**
 * @brief Control the AON power domain.
 *
 * @details This function powers on or off the AON (Always On) power domain,
 *          which is used to maintain certain functionalities in low-power mode.
 * @param enable true to enable AON power domain, false to disable.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_AonPowerCtl(bool enable);

/**
 * @brief Set the voltage level for a power domain.
 *
 * @details This function sets the voltage level for the specified power domain.
 *
 * @param domain The power domain to configure.
 * @param volt The desired voltage level for the power domain.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_SetVoltage(liot_powerdomain_e domain, liot_volt_e volt);

/**
 * @brief Set the function configuration for a module pin.
 *
 * @details This function configures the specified module pin with the desired function multiplexing value.
 *
 * @param modempin The physical pin number on the module.
 * @param func_sel The function multiplexing value to set for the pin.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_SetPinFunc(int modempin, liot_pinfunc_e func_sel);

/**
 * @brief Get the current function configuration of a module pin.
 *
 * @details This function retrieves the current function multiplexing value of the specified module pin.
 *
 * @param modempin The physical pin number on the module.
 *
 * @return The current function multiplexing value of the pin.
 */
liot_pinfunc_e Liot_GetPinFunc(int modempin);

/**
 * @brief Set the logic level (high or low) of a modem pin.
 *
 * @details This function sets the output state of a specified modem pin to either high or low. 
 *          The function has effect only if the pin is configured as an output. 
 *          Attempting to use this function on an input-configured pin may result in undefined behavior.
 *
 * @param modempin The modem pin number (module peripheral pin).
 * @param level The logic level to set: L_IO_HIGH or L_IO_LOW.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_SetPinLevel(int modempin, liot_gpiolvl_e level);

/**
 * @brief Get the current logic level of a modem pin.
 *
 * @details This function reads the current state (high or low) of the specified GPIO pin.
 *          The result is valid only if the pin is configured as an input.
 *
 * @param modempin The modem pin number (module peripheral pin).
 *
 * @return The current logic level of the GPIO pin: L_IO_HIGH or L_IO_LOW.
 */
liot_gpiolvl_e Liot_GetPinLevel(int modempin);

/**
 * @brief Configure and initialize a wakeup interrupt source.
 *
 * @details This function initializes a wakeup interrupt with the specified configuration,
 *          including interrupt trigger type. It also registers the corresponding interrupt callback function.
 *
 * @param wakeuppad The wakeup pad to configure.
 * @param cfg The configuration structure for the wakeup pin.
 * @param cb Pointer to the interrupt callback function.
 * @param arg Pointer to the context passed to the callback function.
 *
 * @note Wakeup interrupts can trigger even when not in sleep mode.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_WakeupIntInit(liot_wakeuppad_e wakeuppad, liot_wakeup_cfg_t cfg, void *cb, void *arg);

/**
 * @brief Deinitialize a wakeup interrupt source.
 *
 * @details This function disables and deinitializes the specified wakeup interrupt source.
 *
 * @param wakeuppad The wakeup pad to deinitialize.
 *
 * @return L_GPIO_ERR_SUCCESS on success, otherwise an error code.
 */
liot_gpioerr_e Liot_WakeupIntDeinit(liot_wakeuppad_e wakeuppad);

/**
 * @brief Get the current logic level of a wakeup pin.
 *
 * @details This function reads the current state (high or low) of the specified wakeup pin.
 *
 * @param wakeuppad The wakeup pad to read.
 *
 * @return The current logic level of the wakeup pad: L_IO_HIGH or L_IO_LOW.
 */
liot_gpiolvl_e Liot_WakeupPadGetLevel(liot_wakeuppad_e wakeuppad);

#ifdef __cplusplus
} /*"C" */
#endif

#endif
#include "liot_gpio2.h"
#include "liot_os.h"
#include "liot_pwm.h"
#include "liot_log.h"

#define MOTOR_P_PWM_NUM         LIOT_PWM_0
#define MOTOR_P_PIN             (22)
#define MOTOR_P_FUNC            (5)

#define MOTOR_N_PWM_NUM         LIOT_PWM_2
#define MOTOR_N_PIN             (54)
#define MOTOR_N_FUNC            (5)

void motor_init(void)
{
    Liot_SetPinFunc(MOTOR_P_PIN, MOTOR_P_FUNC);
    Liot_SetPinFunc(MOTOR_N_PIN, MOTOR_N_FUNC);

    liot_pwm_cfg_s ptr;
    ptr.clk_sel   = LIOT_CLK_RC26M;
    ptr.duty      = 0;
    ptr.period    = 100;
    ptr.prescaler = 25;
    liot_pwm_enable(MOTOR_P_PWM_NUM, &ptr);
    liot_pwm_enable(MOTOR_N_PWM_NUM, &ptr);
}

void motor_set_io_to_gpio(int pin)
{
    liot_pwm_cfg_s ptr;
    ptr.clk_sel   = LIOT_CLK_RC26M;
    ptr.duty      = 0;
    ptr.period    = 100;
    ptr.prescaler = 25;
    
    if(pin == MOTOR_P_PIN)
    {
        Liot_SetPinFunc(MOTOR_P_PIN, 0);
        Liot_SetPinFunc(MOTOR_N_PIN, MOTOR_N_FUNC);
        liot_pwm_close(MOTOR_P_PWM_NUM);
        liot_pwm_enable(MOTOR_N_PWM_NUM, &ptr);
        liot_pwm_open(MOTOR_N_PWM_NUM);
    }
    else
    {
        Liot_SetPinFunc(MOTOR_P_PIN, MOTOR_P_FUNC);
        Liot_SetPinFunc(MOTOR_N_PIN, 0);
        liot_pwm_close(MOTOR_N_PWM_NUM);
        liot_pwm_enable(MOTOR_P_PWM_NUM, &ptr);
        liot_pwm_open(MOTOR_P_PWM_NUM);
    }
}

void motor_set_speed(int speed)
{
    if(speed > 100) speed = 100;
    if(speed < -100) speed = -100;

    if(speed > 0)
    {
        motor_set_io_to_gpio(MOTOR_N_PIN);
        liot_pwm_set_duty_cycle(MOTOR_P_PWM_NUM, 100-speed);
    }
    else if(speed < 0)
    {
        motor_set_io_to_gpio(MOTOR_P_PIN);
        liot_pwm_set_duty_cycle(MOTOR_N_PWM_NUM, 100+speed);
    }
    else
    {
        liot_pwm_close(MOTOR_P_PWM_NUM);
        liot_pwm_close(MOTOR_N_PWM_NUM);
        Liot_SetPinFunc(MOTOR_P_PIN, 0);
        Liot_SetPinFunc(MOTOR_N_PIN, 0);
    }
}

void liot_motor_demo_task(void *argv)
{
    liot_rtos_task_sleep_ms(1000);

    liot_trace("motor init\n");
    motor_init();

    liot_trace("motor start\n");
    while(1)
    {
        liot_rtos_task_sleep_ms(1000);
        motor_set_speed(100);
        liot_rtos_task_sleep_ms(1000);
        motor_set_speed(-100);
        liot_rtos_task_sleep_ms(1000);
        motor_set_speed(100);
        liot_rtos_task_sleep_ms(1000);
        motor_set_speed(-100);
        liot_rtos_task_sleep_ms(1000);
        motor_set_speed(0);
    }

    liot_rtos_task_delete(NULL);
}
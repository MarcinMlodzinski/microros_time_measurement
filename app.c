#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <test_message/msg/test_message.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

#define RCCHECK(fn)                                                                      \
    {                                                                                    \
        rcl_ret_t temp_rc = fn;                                                          \
        if ((temp_rc != RCL_RET_OK))                                                     \
        {                                                                                \
            printf("Failed status on line %d: %d. Aborting.\n", __LINE__, (int)temp_rc); \
            vTaskDelete(NULL);                                                           \
        }                                                                                \
    }
#define RCSOFTCHECK(fn)                                                                    \
    {                                                                                      \
        rcl_ret_t temp_rc = fn;                                                            \
        if ((temp_rc != RCL_RET_OK))                                                       \
        {                                                                                  \
            printf("Failed status on line %d: %d. Continuing.\n", __LINE__, (int)temp_rc); \
        }                                                                                  \
    }

rcl_subscription_t subscriber;
rcl_publisher_t publisher;
test_message__msg__TestMessage data_msg; // data_msg.data

void appMain(void *arg)
{
    data_msg.data.capacity = 100;
    data_msg.data.data = (uint8_t *)malloc(data_msg.data.capacity * sizeof(uint8_t));
    data_msg.data.size = 0;

    for (uint8_t i = 0; i < 100; i++)
    {
        data_msg.data.data = i;
        data_msg.data.size++;
    }

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;

    // create init_options
    RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

    // create node
    rcl_node_t node;
    RCCHECK(rclc_node_init_default(&node, "data_publisher", "", &support));

    // create publisher
    RCCHECK(rclc_publisher_init_default(
        &publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(test_message, msg, TestMessage),
        "data_publisher"));

    // create subscriber
    // RCCHECK(rclc_subscription_init_default(
    // 	&subscriber,
    // 	&node,
    // 	ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    // 	"/microROS/int32_subscriber"));

    // create timer,
    // rcl_timer_t timer;
    // const unsigned int timer_timeout = 1000;
    // RCCHECK(rclc_timer_init_default(
    // 	&timer,
    // 	&support,
    // 	RCL_MS_TO_NS(timer_timeout),
    // 	timer_callback));

    // create executor
    // rclc_executor_t executor;
    // RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
    // RCCHECK(rclc_executor_add_timer(&executor, &timer));

    // msg.data = 0;
    // make message????

    // while (1)
    // {
    // rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
    // usleep(100000);
    rcl_publish(&publisher, &data_msg, NULL);
    // }

    // free resources
    RCCHECK(rcl_publisher_fini(&publisher, &node))
    RCCHECK(rcl_node_fini(&node))

    vTaskDelete(NULL);
}
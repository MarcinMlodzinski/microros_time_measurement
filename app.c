#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <test_message/msg/test_message.h>
#include <time_measurements/msg/time_measurements.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
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

// rcl_subscription_t subscriber;
rcl_publisher_t data_reliable_publisher, data_best_effort_publisher, times_publisher;
test_message__msg__TestMessage big_data_msg; // data_msg.data
test_message__msg__TestMessage small_data_msg;
time_measurements__msg__TimeMeasurements times_msg;

TaskHandle_t handler[5];

void threadWorkTask(void *arg)
{
    int i = 0;
    while (1)
    {
        i++;
    }
    vTaskDelete(NULL);
}

void threadWorkTaskMemory(void *arg)
{
    int32_t i = 0;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t my_handle;

    while (1)
    {
        ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
        err = nvs_get_i32(my_handle, "counter", &i);
        for (int j = 0; j < 10; j++)
        {
            printf("Updating restart counter in NVS ... ");
            i++;
            err = nvs_set_i32(my_handle, "counter", i);
            printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
            printf("Committing updates in NVS ... ");
            err = nvs_commit(my_handle);
            printf((err != ESP_OK) ? "Failed!\n" : "Done\n");
        }
        nvs_close(my_handle);
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

void initAdditionalTasks()
{
    xTaskCreate(
        threadWorkTask,   // Function that should be called
        "task1",          // Name of the task (for debugging)
        1000,             // Stack size (bytes)
        NULL,             // Parameter to pass
        tskIDLE_PRIORITY, // Task priority
        &handler[0]       // Task handle
    );
    xTaskCreate(
        threadWorkTask,   // Function that should be called
        "task2",          // Name of the task (for debugging)
        1000,             // Stack size (bytes)
        NULL,             // Parameter to pass
        tskIDLE_PRIORITY, // Task priority
        &handler[1]       // Task handle
    );
    xTaskCreate(
        threadWorkTask,   // Function that should be called
        "task3",          // Name of the task (for debugging)
        1000,             // Stack size (bytes)
        NULL,             // Parameter to pass
        tskIDLE_PRIORITY, // Task priority
        &handler[2]       // Task handle
    );
    xTaskCreate(
        threadWorkTask,   // Function that should be called
        "task4",          // Name of the task (for debugging)
        1000,             // Stack size (bytes)
        NULL,             // Parameter to pass
        tskIDLE_PRIORITY, // Task priority
        &handler[3]       // Task handle
    );
    xTaskCreate(
        threadWorkTaskMemory, // Function that should be called
        "task5",              // Name of the task (for debugging)
        8192,                 // Stack size (bytes)
        NULL,                 // Parameter to pass
        tskIDLE_PRIORITY,     // Task priority
        &handler[4]           // Task handle
    );
}

void sendOneSmall(rosidl_runtime_c__String name, rcl_publisher_t *publisher)
{
    for (int j = 0; j < 10; j++)
    {
        usleep(2000000);
        times_msg.measurement_type.size = name.size;
        times_msg.measurement_type.capacity = name.capacity;
        times_msg.measurement_type.data = name.data;
        RCSOFTCHECK(rmw_uros_sync_session(1000));
        times_msg.start_time = rmw_uros_epoch_nanos();
        rcl_publish(publisher, &small_data_msg, NULL);
        times_msg.stop_time = rmw_uros_epoch_nanos();
        rcl_publish(&times_publisher, &times_msg, NULL);
    }
}

void sendOneBig(rosidl_runtime_c__String name, rcl_publisher_t *publisher)
{
    for (int j = 0; j < 10; j++)
    {
        usleep(2000000);
        times_msg.measurement_type.size = name.size;
        times_msg.measurement_type.capacity = name.capacity;
        times_msg.measurement_type.data = name.data;
        RCSOFTCHECK(rmw_uros_sync_session(1000));
        times_msg.start_time = rmw_uros_epoch_nanos();
        rcl_publish(publisher, &big_data_msg, NULL);
        times_msg.stop_time = rmw_uros_epoch_nanos();
        rcl_publish(&times_publisher, &times_msg, NULL);
    }
}

void sendManySmall(rosidl_runtime_c__String name, rcl_publisher_t *publisher)
{
    for (int j = 0; j < 10; j++)
    {
        usleep(2000000);
        times_msg.measurement_type.size = name.size;
        times_msg.measurement_type.capacity = name.capacity;
        times_msg.measurement_type.data = name.data;
        RCSOFTCHECK(rmw_uros_sync_session(1000));
        times_msg.start_time = rmw_uros_epoch_nanos();
        for (int i = 0; i < 66000; ++i)
        {
            rcl_publish(publisher, &small_data_msg, NULL);
        }
        times_msg.stop_time = rmw_uros_epoch_nanos();
        rcl_publish(&times_publisher, &times_msg, NULL);
    }
}

void initData()
{
    big_data_msg.data.capacity = 66000;
    big_data_msg.data.data = (uint8_t *)malloc(big_data_msg.data.capacity * sizeof(uint8_t));
    big_data_msg.data.size = 0;

    for (int i = 0; i < 66000; i++)
    {
        big_data_msg.data.data[i] = 255;
        big_data_msg.data.size++;
    }

    small_data_msg.data.capacity = 1;
    small_data_msg.data.data = (uint8_t *)malloc(small_data_msg.data.capacity * sizeof(uint8_t));
    small_data_msg.data.size = 0;

    for (uint8_t i = 0; i < 1; i++)
    {
        small_data_msg.data.data[i] = 255;
        small_data_msg.data.size++;
    }
}

void appMain(void *arg)
{
    initData();

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;

    // create init_options
    RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

    // create node
    rcl_node_t node;
    RCCHECK(rclc_node_init_default(&node, "time_measurements", "", &support));

    // create publisher
    RCCHECK(rclc_publisher_init_default(
        &data_reliable_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(test_message, msg, TestMessage),
        "data_reliable"));

    RCCHECK(rclc_publisher_init_best_effort(
        &data_best_effort_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(test_message, msg, TestMessage),
        "data_best_effort"));

    RCCHECK(rclc_publisher_init_default(
        &times_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(time_measurements, msg, TimeMeasurements),
        "times"));

    rosidl_runtime_c__String name;

    name.size = 18;
    name.capacity = 19;
    name.data = "One small reliable";
    sendOneSmall(name, &data_reliable_publisher);

    name.size = 21;
    name.capacity = 22;
    name.data = "One small best effort";
    sendOneSmall(name, &data_best_effort_publisher);

    name.size = 16;
    name.capacity = 17;
    name.data = "One big reliable";
    sendOneBig(name, &data_reliable_publisher);

    name.size = 19;
    name.capacity = 20;
    name.data = "Many small reliable";
    sendManySmall(name, &data_reliable_publisher);

    name.size = 22;
    name.capacity = 23;
    name.data = "Many small best effort";
    sendManySmall(name, &data_best_effort_publisher);

    initAdditionalTasks(); ////////////////////////////////////////////////////////////////////

    name.size = 25;
    name.capacity = 26;
    name.data = "One small loaded reliable";
    sendOneSmall(name, &data_reliable_publisher);

    name.size = 28;
    name.capacity = 29;
    name.data = "One small loaded best effort";
    sendOneSmall(name, &data_best_effort_publisher);

    name.size = 23;
    name.capacity = 24;
    name.data = "One big loaded reliable";
    sendOneBig(name, &data_reliable_publisher);

    name.size = 26;
    name.capacity = 27;
    name.data = "Many small loaded reliable";
    sendManySmall(name, &data_reliable_publisher);

    name.size = 29;
    name.capacity = 30;
    name.data = "Many small loaded best effort";
    sendManySmall(name, &data_best_effort_publisher);

    // free resources

    vTaskDelete(handler[0]);
    vTaskDelete(handler[1]);
    vTaskDelete(handler[2]);
    vTaskDelete(handler[3]);
    vTaskDelete(handler[4]);
    RCCHECK(rcl_publisher_fini(&data_reliable_publisher, &node))
    RCCHECK(rcl_publisher_fini(&data_best_effort_publisher, &node))
    RCCHECK(rcl_publisher_fini(&times_publisher, &node))
    RCCHECK(rcl_node_fini(&node))

    vTaskDelete(NULL);
}
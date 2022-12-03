#include <zephyr.h>
#include <device.h>
#include <stdio.h>
#include <stdint.h>

#include <net/net_if.h>
#include <net/wifi_mgmt.h>
#include <net/net_event.h>

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <test_message/msg/test_message.h>
#include <time_measurements/msg/time_measurements.h>

#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <rmw_microros/rmw_microros.h>
#include <microros_transports.h>

#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <fs/nvs.h>

#define RCCHECK(fn)                                                                      \
    {                                                                                    \
        rcl_ret_t temp_rc = fn;                                                          \
        if ((temp_rc != RCL_RET_OK))                                                     \
        {                                                                                \
            printk("Failed status on line %d: %d. Aborting.\n", __LINE__, (int)temp_rc); \
        }                                                                                \
    }
#define RCSOFTCHECK(fn)                                                                    \
    {                                                                                      \
        rcl_ret_t temp_rc = fn;                                                            \
        if ((temp_rc != RCL_RET_OK))                                                       \
        {                                                                                  \
            printk("Failed status on line %d: %d. Continuing.\n", __LINE__, (int)temp_rc); \
        }                                                                                  \
    }

#define STACKSIZE 1024
#define PRIORITY 4

K_THREAD_STACK_DEFINE(thread1_stack_area, STACKSIZE);
static struct k_thread thread1_data;

K_THREAD_STACK_DEFINE(thread2_stack_area, STACKSIZE);
static struct k_thread thread2_data;

K_THREAD_STACK_DEFINE(thread3_stack_area, STACKSIZE);
static struct k_thread thread3_data;

K_THREAD_STACK_DEFINE(thread4_stack_area, STACKSIZE);
static struct k_thread thread4_data;

K_THREAD_STACK_DEFINE(thread5_stack_area, 4096);
static struct k_thread thread5_data;

static struct nvs_fs fs;

#define STORAGE_NODE DT_NODE_BY_FIXED_PARTITION_LABEL(storage)
#define FLASH_NODE DT_MTD_FROM_FIXED_PARTITION(STORAGE_NODE)

// Wireless management
static struct net_mgmt_event_callback wifi_shell_mgmt_cb;
static bool connected = 0;

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb,
                                    uint32_t mgmt_event, struct net_if *iface)
{
    if (NET_EVENT_IPV4_ADDR_ADD == mgmt_event)
    {
        printf("DHCP Connected\n");
        connected = 1;
    }
}

// micro-ROS
rcl_publisher_t data_reliable_publisher, data_best_effort_publisher, times_publisher;
test_message__msg__TestMessage big_data_msg; // data_msg.data
test_message__msg__TestMessage small_data_msg;
time_measurements__msg__TimeMeasurements times_msg;

void threadWorkTask(void *a, void *a2, void *a3)
{
    ARG_UNUSED(a);
    ARG_UNUSED(a2);
    ARG_UNUSED(a3);
    int i = 0;
    while (1)
    {
        i++;
    }
}

void threadWorkTaskMemory(void *arg, void *arg2, void *arg3)
{
    ARG_UNUSED(arg);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);
    int a = 0;
    (void)nvs_write(&fs, 1, &a, sizeof(a));
    while (1)
    {
        nvs_read(&fs, 1, &a, sizeof(a));
        for (int i = 0; i < 10; i++)
        {
            a++;
        }
        (void)nvs_write(&fs, 1, &a, sizeof(a));
        k_msleep(10);
    }

    struct flash_pages_info info;
    const struct device *flash_dev;
    flash_dev = DEVICE_DT_GET(FLASH_NODE);
    fs.offset = FLASH_AREA_OFFSET(storage);
    flash_get_page_info_by_offs(flash_dev, fs.offset, &info);
    fs.sector_size = info.size;
    fs.sector_count = 3U;

    nvs_init(&fs, flash_dev->name);
    (void)nvs_write(&fs, 1, &a, sizeof(a));
    while (1)
    {
        nvs_read(&fs, 1, &a, sizeof(a));
        for (int i = 0; i < 10; i++)
        {
            a++;
        }
        (void)nvs_write(&fs, 1, &a, sizeof(a));
        k_msleep(10);
    }
}

k_tid_t thread1;
k_tid_t thread2;
k_tid_t thread3;
k_tid_t thread4;
k_tid_t thread5;

void initAdditionalTasks()
{
    thread1 = k_thread_create(&thread1_data, thread1_stack_area,
                              K_THREAD_STACK_SIZEOF(thread1_stack_area),
                              threadWorkTask,
                              NULL, NULL, NULL,
                              PRIORITY, 0, K_NO_WAIT);
    thread2 = k_thread_create(&thread2_data, thread2_stack_area,
                              K_THREAD_STACK_SIZEOF(thread2_stack_area),
                              threadWorkTask,
                              NULL, NULL, NULL,
                              PRIORITY, 0, K_NO_WAIT);
    thread3 = k_thread_create(&thread3_data, thread3_stack_area,
                              K_THREAD_STACK_SIZEOF(thread3_stack_area),
                              threadWorkTask,
                              NULL, NULL, NULL,
                              PRIORITY, 0, K_NO_WAIT);
    thread4 = k_thread_create(&thread4_data, thread4_stack_area,
                              K_THREAD_STACK_SIZEOF(thread4_stack_area),
                              threadWorkTask,
                              NULL, NULL, NULL,
                              PRIORITY, 0, K_NO_WAIT);
    thread5 = k_thread_create(&thread5_data, thread5_stack_area,
                              K_THREAD_STACK_SIZEOF(thread5_stack_area),
                              threadWorkTaskMemory,
                              NULL, NULL, NULL,
                              PRIORITY, 0, K_NO_WAIT);
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
    big_data_msg.data.capacity = 65600;
    big_data_msg.data.data = (uint8_t *)malloc(big_data_msg.data.capacity * sizeof(uint8_t));
    big_data_msg.data.size = 0;

    for (int i = 0; i < 65600; i++)
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

void main(void)
{
    // Set custom transports
    rmw_uros_set_custom_transport(
        MICRO_ROS_FRAMING_REQUIRED,
        (void *)&default_params,
        zephyr_transport_open,
        zephyr_transport_close,
        zephyr_transport_write,
        zephyr_transport_read);

    // Init micro-ROS
    // ------ Wifi Configuration ------
    net_mgmt_init_event_callback(&wifi_shell_mgmt_cb,
                                 wifi_mgmt_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);

    net_mgmt_add_event_callback(&wifi_shell_mgmt_cb);

    struct net_if *iface = net_if_get_default();
    static struct wifi_connect_req_params cnx_params;

    cnx_params.ssid = "Orange_Swiatlowod_E480";
    cnx_params.ssid_length = strlen(cnx_params.ssid);
    cnx_params.channel = 0;
    cnx_params.psk = "jakiekolwiekhaslodointernetu123";
    cnx_params.psk_length = strlen(cnx_params.psk);
    cnx_params.security = WIFI_SECURITY_TYPE_PSK;

    if (net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &cnx_params, sizeof(struct wifi_connect_req_params)))
    {
        printf("Connection request failed\n");
    }
    else
    {
        printf("Connection requested\n");
    }

    while (!connected)
    {
        printf("Waiting for connection\n");
        usleep(10000);
    }
    printf("Connection OK\n");

    // ------ micro-ROS ------
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

    initData();

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

    k_thread_abort(thread1);
    k_thread_abort(thread2);
    k_thread_abort(thread3);
    k_thread_abort(thread4);
    k_thread_abort(thread5);
    RCCHECK(rcl_publisher_fini(&data_reliable_publisher, &node))
    RCCHECK(rcl_publisher_fini(&data_best_effort_publisher, &node))
    RCCHECK(rcl_publisher_fini(&times_publisher, &node))
    RCCHECK(rcl_node_fini(&node))
}

// K_THREAD_DEFINE(first, 128, threadWorkTask, NULL, NULL, NULL,
//                 4, 0, K_NO_WAIT);
// K_THREAD_DEFINE(second, 128, threadWorkTask, NULL, NULL, NULL,
//                 4, 0, K_NO_WAIT);
// K_THREAD_DEFINE(third, 128, threadWorkTask, NULL, NULL, NULL,
//                 4, 0, K_NO_WAIT);
// K_THREAD_DEFINE(fourth, 128, threadWorkTask, NULL, NULL, NULL,
//                 4, 0, K_NO_WAIT);

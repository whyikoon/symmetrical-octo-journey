/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "iot_import.h"
#include "iot_export.h"
#include "iot_export_linkkit.h"


/* Properties defined of the sample
{
  "data":"Hello World"  // data string, can be set by cloud or app, dataType is string
  "status":1,           // indicate the status, can be set by cloud or app, dataType is bool
}
*/

/* the sample will auto terminate if execution time expired */
#define SAMPLE_EXECUTION_TIME           20

/* this parameter is deprecated in this version, just set to 16 */
#define LINKKIT_MSG_BUF_MAX             16

/* get tsl from cloud if this macro is defined 1, or the local tls shall be provided */
#define LINKKIT_GET_TLS_FROM_CLOUD      1

/*  */
#define USER_EXAMPLE_YIELD_TIMEOUT_MS   200

/* indentifier of status property */
#define PROPERTY_ID_STATUS              "Status"

/* indentifier of data property */
#define PROPERTY_ID_DATA                "Data"

#define PROPERTY_ID_DATA_VALUE          "Hello,World!"

/* format of property post payload */
#define PROPERTY_PAYLOAD_FORMAT         "{\"Data\": \"%s\", \"Status\": %d}"


/* define print for app trace */
#define APP_TRACE(fmt, ...)  \
    do { \
        HAL_Printf("%s|%03d :: ", __func__, __LINE__); \
        HAL_Printf(fmt, ##__VA_ARGS__); \
        HAL_Printf("%s", "\r\n"); \
    } while(0)


/* app context type define */
typedef struct _app_context {
    int         device_id;
    char        prop_data[30];
    uint8_t     prop_status;
    uint8_t     cloud_connected;
    uint8_t     device_initialized;    
} app_context_t;

/* app context variable declare */
static app_context_t app_context;

/* 
 * Connect handle
 */
static int user_connected_event_handler(void)
{
    APP_TRACE("Cloud Connected");

    app_context.cloud_connected = 1;
    return 0;
}

/* 
 * Disconnect handle
 */
static int user_disconnected_event_handler(void)
{
    APP_TRACE("Cloud Disconnected");

    app_context.cloud_connected = 0;
    return 0;
}

/*
 * the handler of service which is defined by identifier, not property
 * alink method: thing.service.{tsl.service.identifier}
 */
static int user_service_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
        const char *request, const int request_len,
        char **response, int *response_len)
{
    APP_TRACE("Service Request Received, Devid: %d, Service ID: %.*s, Payload: %s", devid, serviceid_len, serviceid, request);
    
    /* view sdk example to get more detail */
    return 0;
}

/**
 * Received property set message from cloud
 */
static int user_property_set_event_handler(const int devid, const char *request, const int request_len)
{
    APP_TRACE("Property Set Received, Devid: %d, payload: %s\r\n", devid, request);

    /* you should use cJSON to parse the request to get specific property value, see sdk exampel */
    return 0;
}

/**
 * Received property get message from cloud handler, cloud may not support sending property_get message!!!
 */
static int user_property_get_event_handler(const int devid, const char *request, const int request_len, char **response,
        int *response_len)
{
    return SUCCESS_RETURN;
}

/*
 * Received property post reply message form cloud
 */
static int user_report_reply_event_handler(const int devid, const int msgid, const int code, const char *reply,
        const int reply_len)
{
    const char *reply_value = (reply == NULL) ? ("NULL") : (reply);
    const int reply_value_len = (reply_len == 0) ? (strlen("NULL")) : (reply_len);

    /* reply value many be meaningless if it's a property post reply */
    APP_TRACE("Property Post Reply Received, Devid: %d, Message ID: %d, Code: %d, Reply: %.*s\r\n", devid, msgid, code,
                  reply_value_len,
                  reply_value);
    return 0;
}

/**
 * Received event post reply message from cloud
 */
static int user_trigger_event_reply_event_handler(const int devid, const int msgid, const int code, const char *eventid,
        const int eventid_len, const char *message, const int message_len)
{
    /* reply value many be meaningless */
    APP_TRACE("Trigger Event Reply Received, Devid: %d, Message ID: %d, Code: %d, EventID: %.*s, Message: %.*s", devid,
                  msgid, code,
                  eventid_len,
                  eventid, message_len, message);

    return 0;
}

/**
 * Device complete all topic subscribe to server
 */
static int user_initialized(const int devid)
{
    APP_TRACE("Device Initialized, Devid: %d", devid);

    if (app_context.device_id == devid) {
        app_context.device_initialized = 1;
    }

    return 0;
}

/* app post all property ervery 5 second */
static int app_post_all_property(void)
{
    int res = 0;
    char payload[60] = {0};
    
    HAL_Snprintf(payload, sizeof(payload), PROPERTY_PAYLOAD_FORMAT, app_context.prop_data, app_context.prop_status);

    res = IOT_Linkkit_Report(app_context.device_id, ITM_MSG_POST_PROPERTY, (uint8_t*)payload, strlen(payload));
    if (res == FAIL_RETURN) {
        APP_TRACE("App post properties every 5 seconds fail\r\n");
        return res;
    }
    APP_TRACE("Property post successfully, Message ID: %d, payload: %s", res, payload);

    return res;
}

/* update system time */
static unsigned long long app_uptime_sec(void)
{
    static uint64_t start_time = 0;

    if (start_time == 0) {
        start_time = HAL_UptimeMs();
    }

    return (HAL_UptimeMs() - start_time) / 1000;
}

/* Linkkit sample main routine */
static int app_linkkit_sample(void) 
{
    int res;
    uint64_t now = 0;
    uint64_t prev_sec = 0;
    iotx_linkkit_dev_meta_info_t device_meta_info;

    /* init app data */
    memset(&app_context, 0, sizeof(app_context_t));
    memcpy(app_context.prop_data, PROPERTY_ID_DATA_VALUE, strlen(PROPERTY_ID_DATA_VALUE));
    app_context.prop_status = 1;    

    /* Register callback you would use */
    IOT_RegisterCallback(ITE_CONNECT_SUCC, user_connected_event_handler);
    IOT_RegisterCallback(ITE_DISCONNECTED, user_disconnected_event_handler);
    IOT_RegisterCallback(ITE_SERVICE_REQUST, user_service_request_event_handler);
    IOT_RegisterCallback(ITE_PROPERTY_SET, user_property_set_event_handler);
    IOT_RegisterCallback(ITE_PROPERTY_GET, user_property_get_event_handler);
    IOT_RegisterCallback(ITE_REPORT_REPLY, user_report_reply_event_handler);
    IOT_RegisterCallback(ITE_TRIGGER_EVENT_REPLY, user_trigger_event_reply_event_handler);
    IOT_RegisterCallback(ITE_INITIALIZE_COMPLETED, user_initialized);

    /* Config the server domain of your region, the MQTT_DOMAIN is defined in makefile */
    IOT_Ioctl(IOTX_IOCTL_SET_MQTT_DOMAIN, (void *)MQTT_DOMAIN);  

    if(0 != memcmp(ENDPOINT, "NULL", 4)) {
       IOT_Ioctl(IOTX_IOCTL_SET_ENDPOINT, (void *)ENDPOINT);
    }

    /* Choose whether get message response from cloud */
    int post_event_reply = 1;
    IOT_Ioctl(IOTX_IOCTL_RECV_EVENT_REPLY, (void *)&post_event_reply);

    /* Init device metadata, the device infomation is defined in makefile */
    memset(&device_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));    
    memcpy(device_meta_info.product_key, PRODUCT_KEY, strlen(PRODUCT_KEY));
    memcpy(device_meta_info.device_name, DEVICE_NAME, strlen(DEVICE_NAME));
    memcpy(device_meta_info.device_secret, DEVICE_SECRET, strlen(DEVICE_SECRET));

    /* Create master device resource */
    app_context.device_id = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &device_meta_info);
    if (app_context.device_id < 0) {
        APP_TRACE("IOT_Linkkit_Open Failed");
        return -1;
    }
    APP_TRACE("IOT_Linkkit_Open successfully");

    /* Start Connect AliCloud Server */
    res = IOT_Linkkit_Connect(app_context.device_id);
    if (res < 0) {
        APP_TRACE("IOT_Linkkit_Connect Failed");
        return -1;
    }
    APP_TRACE("IOT_Linkkit_Connect successfully");

    APP_TRACE("Linkkit enter loop");
    while (1) {
        IOT_Linkkit_Yield(USER_EXAMPLE_YIELD_TIMEOUT_MS);

        now = app_uptime_sec();
        if (prev_sec == now) {
            continue;
        }

        /* post all properties every 5 second */
        if (now % 5 == 0) {
            app_post_all_property();
        }

        /* after all, this is an sample, give a chance to return... */
        /* modify this value for this sample executaion time period */
        if (now > 60 * SAMPLE_EXECUTION_TIME) {
            APP_TRACE("sample run timeout, break form loop");
            break;
        }
        
        prev_sec = now;
    }
    
    /* close linkkit service */
    IOT_Linkkit_Close(app_context.device_id);

    return 0;
}

static void app_print_banner(void)
{
    HAL_Printf("\n");
    HAL_Printf("/*\n * Copyright (C) 2015-2018 Alibaba Group Holding Limited\n */\n");
    HAL_Printf("\n");
}

int main(int argc, char **argv)
{
    app_print_banner();
    IOT_SetLogLevel(IOT_LOG_ERROR);
    APP_TRACE("sample start!\n");

    app_linkkit_sample();
    IOT_SetLogLevel(IOT_LOG_NONE);

    APP_TRACE("sample end!\n");

    return 0;
}


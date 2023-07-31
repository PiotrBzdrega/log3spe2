/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
#include "freertos/queue.h"

#include "time.h"
#include "sys/time.h"

#define SPP_TAG "SPP_ACCEPTOR_DEMO"
#define TEL_TAG "TELEGRAM_PROCESS"
#define CRE_MSG "CREATE_MESSAGE"
#define ADD_NVS "ADD_NVS"
#define FIN_NVS "FIND_NVS"
#define REM_NVS "REMOVE_NVS"
#define LOGPASS "LOG_PASS"
#define EXTRUI "EXTRACT_ELEMENT"
#define SPP_SERVER_NAME "SPP_SERVER"
#define EXAMPLE_DEVICE_NAME "LOG3spe2"
#define SPP_SHOW_DATA 0
#define SPP_SHOW_SPEED 1
#define SPP_SHOW_MODE SPP_SHOW_DATA    /*Choose show mode: show data or speed*/
#define MAX_TELEGRAM 100
#define EXT ',' /* separator in telegram*/
static char credential_cache[100]; //TODO: use domain'ETH'login'ETH'password cache
static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const bool esp_spp_enable_l2cap_ertm = true;


static struct timeval time_new, time_old;
static long data_num = 0;

static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

/*Telegram enumeration*/
typedef enum UI_ENUM
{
    UI_UNKNOWN = -1,
    UI_DOMAIN = 0,
    UI_LOGIN = 1,
    UI_PASSWORD = 2,
    UI_DONE = 3,
    UI_NEW_CREDENTIAL = 4,
    UI_DELETE_ALL = 5,
    UI_MISSED = 6,
    
}UI_ENUM;

/* Queue for received telegrams */
QueueHandle_t ReceivedQueue;

static uint8_t* mode_to_str(esp_bt_pm_mode_t mode) 
{
   return (uint8_t *)(mode==ESP_BT_PM_MD_ACTIVE ? "active" : (mode==ESP_BT_PM_MD_HOLD ? "hold" : (mode==ESP_BT_PM_MD_SNIFF ? "sniff": (mode==ESP_BT_PM_MD_PARK ? "park" : "undefined") ) ));   
}

/*0-9 character convert to int*/
static UI_ENUM telegram_mode(uint8_t ch)
{   
    uint8_t mode=ch-(uint8_t)'0';
    return (mode>UI_UNKNOWN && mode<=UI_MISSED) ? mode : UI_UNKNOWN;
}

static char *bda2str(uint8_t * bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}

typedef struct 
{
    uint32_t          handle;         /*!< The connection handle */
    uint16_t          len;            /*!< The length of data */
    uint8_t           *data;          /*!< The data received */       
}rcv_tele;

static bool create_message(UI_ENUM element,const uint8_t* domain, const uint8_t* logpass,int32_t handle)
{

    if (element==UI_UNKNOWN || element>=10)
    {
        ESP_LOGE(CRE_MSG, "Message mode invalid: %d ",element); 
        return false;
    }
    
    /*domain has null pointer*/
    if(!domain)
    {
        ESP_LOGE(CRE_MSG, "Create message failed; domain value missing "); 
        return false;
    }
    else
    {

        /* telegram pointer with maximal bytes in buffer */
        uint8_t message[MAX_TELEGRAM];

        /*pointer for message to copy all elements*/
        uint8_t *tmp=(uint8_t*)&message;

        /* insert feedback mode into first character of massage*/
        *tmp++= element+'0';

        /* separator character*/
        *tmp++=EXT;

        /*go through each character of domain*/
        while ((*tmp++=*domain++)!='\0');

        /* move back pointer for null terminator position*/
        tmp--;

        /* login pointer valid */
        if(logpass)
        {   
            /*put comma separator insted '\0'*/
            *tmp++ =EXT;

            /*go through each character of login/password*/
            while ((*tmp++=*logpass++)!='\0');
        }
                esp_err_t res=esp_spp_write(handle, (tmp-(uint8_t*)&message), (uint8_t*)&message);
                ESP_LOGI(CRE_MSG, "invoked esp_spp_write status :%s",esp_err_to_name(res));
        return (res==0) ? true : false; 
    }
}

static uint8_t* logpass_concat(uint8_t* login, uint8_t* password)
{

    ESP_LOGI(LOGPASS, " login:%s\t password:%s passed to function ",login,password);

    /* pointer to track other objects*/
    uint8_t *tmp;

    /* length of string*/
    int len;

    /* keep address of login to count len*/
    tmp=login;

    /* stop when null comes up*/
    while (*tmp)
        tmp++;

    /* length of login + separetor */
    len=(tmp-login)+1;

    /* keep address of password to count len*/
    tmp=password;

     /* stop when null comes up*/
    while (*tmp)
        tmp++;

    /* combined length login + separator + password*/
    len+=(tmp-password);

    /* allocate sufficcient area for concatenated string*/
    uint8_t *logpass= (uint8_t*)malloc(sizeof(uint8_t)*len);

    ESP_LOGI(LOGPASS, " allocated %d bytes for logpass",len);

    /* keep address of logpass to combine strings*/    
    tmp=logpass;

    /* copy all login characters to new string*/
    while ((*tmp++=*login++)!='\0');
    

    /* move pointer backward to replace termination with separator character
       then move back */
    tmp--;
    *tmp++=EXT;

    ESP_LOGI(LOGPASS, " previous char:%c and null termination char:%c ",*(tmp-2),*(tmp-1));

    /* copy all password characters to new string*/
    while ((*tmp++=*password++)!='\0');
    

    ESP_LOGI(LOGPASS, " login%cpassword has been concatenated :%s",EXT,logpass); 
                                                                            

    return logpass;
}

// static bool delete_from_nvs()
// {

// }

static bool add_to_nvs(uint8_t* credential[3])
{
    esp_err_t err;
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(ADD_NVS, "Error (%s) opening NVS handle to write!\n",esp_err_to_name(err));
        return false;
    }        
    else
    {   
        /* concatenate login and password */
        uint8_t* new_value=logpass_concat(credential[1], credential[2]);
        if(!new_value)
        {
            return false;
        }
        ESP_LOGI(ADD_NVS, "before call nvs_set_str() key:%s, len:%d, value:%s, len:%d",(char*)credential[0],strlen((char*)credential[0]),(char*)new_value,strlen((char*)new_value));
        /* populate key with new login*/
        err = nvs_set_str(my_handle, (char*)credential[0], (char*)new_value);
        ESP_LOGI(ADD_NVS, "invoked nvs_set_str() with status :%s\t key:%s, value:%s",esp_err_to_name(err),credential[0],new_value);

        /* commit set values*/
        err = nvs_commit(my_handle);
        ESP_LOGI(ADD_NVS, "invoked commit() with status :%s",esp_err_to_name(err));

        /* release memory for concatenated value*/
        free(new_value);
        return true;
    }
}

static uint8_t* extract_credential(UI_ENUM element,uint8_t* logpass)
{
    /* copy original addres for credential */
    uint8_t *org=logpass;

    /* iterate till separator character*/
    while (*(logpass++)!=EXT)
    ;


    /* UI_LOGIN has been requested */
    if (element==UI_LOGIN)
    {
        /* move back to separator position*/
        logpass--;

        /* replace control character with null terminator to limit text*/
        *logpass='\0';
        ESP_LOGI(EXTRUI, "extracted UI_LOGIN :%s",org);
    }
    /* UI_PASSWORD has been requested */
    else
    {   /* save orginal addres of logpass*/
        uint8_t *tmp=org;
        while ((*tmp++=*logpass++)!='\0');
        
        ESP_LOGI(EXTRUI, "extracted UI_PASSWORD :%s",org);       
    }
    
    return org;
}

static uint8_t* find_in_nvs(UI_ENUM element,uint8_t *domain)
{
    esp_err_t err;
    nvs_handle_t my_handle;
    err = nvs_open("storage", NVS_READWRITE , &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(FIN_NVS, "Error (%s) opening NVS handle to read!\n",esp_err_to_name(err));
        return false;
    }        
    else
    {   
        /* variable to recognize length of value from nvs*/
        size_t required_size;

        /* get require size w/o any pointer */
        err=nvs_get_str(my_handle, (char*)domain, NULL, &required_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(FIN_NVS, "Error %s during call nvs_get_str() for key:%s!",esp_err_to_name(err),(char*)domain);
            return NULL;
        }
        ESP_LOGI(FIN_NVS, "Required %d bytes of memory for key:%s allocation ",required_size,domain);

        /* allocate required space for credential */
        uint8_t *logpass= (uint8_t*)malloc(required_size);

        /* invoke get function once again w/ pointer*/ 
        err=nvs_get_str(my_handle, (char*)domain, (char*)logpass, &required_size);
        if (err != ESP_OK)
        {
            ESP_LOGE(FIN_NVS, "Error %s during call invoked nvs_get_str()!",esp_err_to_name(err));
            return NULL;
        }
        ESP_LOGI(FIN_NVS, "Aquired %s value for key:%s allocation ",logpass,domain);

        /* return required credential part*/
        return extract_credential(element,logpass);
    }
    return NULL;
}

/*Process incomming messages from SPP client*/
static void process_telegram(void *arg)
{

    /*struct pointer for buffer from queue*/
    rcv_tele *tel;
    while(1)
    {       /*wait for next telegram*/
        if(xQueueReceive(ReceivedQueue, &tel, portMAX_DELAY) == pdTRUE)
        {
            /*messages in queue*/
            //UBaseType_t len= uxQueueMessagesWaiting( ReceivedQueue );
            printf("%s\t%d\n",tel->data,tel->len);

            /*first byte consist of telegram mode*/
             UI_ENUM mode=telegram_mode(tel->data[0]); //TODO:prio!!!correct functionality to not catch other letters

            /*valid mode in telegram*/
            if(mode)
            {
                /*telegram should contains at most 3 additional text places mode,domain,login,password*/
                uint8_t* content[3];

                /*entry number from telegram*/
                int j=0;

                /*first and last character of element in telegram*/
                int start_char,end_char=1;

                /* correct separator after mode bytefield*/
                if(tel->data[1]==',')
                {
                    /*loop through whole telegram w/o mode bit */
                    for (size_t i = 2 ; i <= tel->len; i++)
                    {   
                        /*comma separator ',' or last character is read*/
                        if(tel->data[i]==EXT || i==tel->len)
                        {
                            /*last end_char is ',' so next start_char should be end_char+1*/
                            start_char=end_char+1;
                            /*save found position*/
                            end_char=i;
                            
                            /*allocate memory for found element (+1 for null terminator)*/
                            content[j]=(uint8_t *)malloc(sizeof(uint8_t)*(end_char-start_char+1));
                            memcpy(content[j],&tel->data[start_char],(end_char-start_char+1));

                            /* replace seperator character with null terminator*/
                            content[j][end_char-start_char]='\0';
                            ESP_LOGI(TEL_TAG, "%d element found in telegram %s\t start_char:%d, end_char:%d ",j,content[j],start_char,end_char);
                            /*search for next element in telegram*/
                            j++;
                        }
                    }
                    /*which mode has telegram*/
                    switch (mode)
                    {
                    case UI_DOMAIN:
                        ESP_LOGI(TEL_TAG, "UI_DOMAIN telegram:%s",tel->data);
                        //TODO: domain telegram
                        break;
                    case UI_LOGIN...UI_PASSWORD:
                        /*TELEGRAM:UI_ENUM,domain*/
                        ESP_LOGI(TEL_TAG, "%s telegram:%s",(mode==UI_LOGIN) ? "UI_LOGIN" : "UI_PASSWORD",tel->data);

                        /*telegram should contains only one element*/
                        if (j==1)
                        {   
                            /*search credential in non-volatile storage memory*/
                            uint8_t* credential=find_in_nvs(mode,(uint8_t*)content[0]);

                            /* credential found; create message with found item*/
                            if(credential)
                                create_message(mode,content[0],credential,tel->handle);
                            else
                            {       
                                // ESP_LOGE(TEL_TAG, "%s for domain:%s missed in NVS",(mode==UI_LOGIN) ? "UI_LOGIN" : "UI_PASSWORD",content[0]); //redundant message
                                /* create message w/o credential*/
                                create_message(UI_MISSED,content[0],NULL,tel->handle);
                            }

                        }
                        else
                            ESP_LOGE(TEL_TAG, "Invalid amount of elements in telegram:%d",j);                        


                        break;
                    case UI_DONE:
                        ESP_LOGI(TEL_TAG, "UI_DONE telegram:%s",tel->data);
                        break;
                    case UI_NEW_CREDENTIAL:
                        /*TELEGRAM:UI_ENUM,domain,login,password*/
                        ESP_LOGI(TEL_TAG, "UI_NEW_CREDENTIAL telegram:%s",tel->data);
                        /*telegram should contains three elements*/
                        if (j==3)
                        {  /* add new credential*/
                            add_to_nvs(content);
                        }
                        else
                            ESP_LOGE(TEL_TAG, "Invalid amount of elements in telegram:%d",j);  

                        break;
                    case UI_DELETE_ALL:
                        ESP_LOGI(TEL_TAG, "UI_DELETE_ALL telegram:%s",tel->data);
                        break;
                    case UI_MISSED:
                        ESP_LOGI(TEL_TAG, "UI_MISSED telegram:%s",tel->data);
                        //TODO: missed telegram
                        break;                                                                                 
                    default:
                        ESP_LOGE(TEL_TAG, "Undifined mode telegram:%s",tel->data);
                        break;
                    }
        
                    /*release memory for extracted elements from telegram*/
                    while(--j)
                        free(content[j]);

                }
                else
                    ESP_LOGE(TEL_TAG, "first separator field has not been recognized telegram:%s",tel->data);

                
            }
            else
                ESP_LOGE(TEL_TAG, "UI_UNKNOWN structure telegram:%s",tel->data);

            


            /*release memory for original telegram after reading data*/      
            free(tel->data);
            free(tel);
        }
    }
}

static void print_speed(void)
{
    float time_old_s = time_old.tv_sec + time_old.tv_usec / 1000000.0;
    float time_new_s = time_new.tv_sec + time_new.tv_usec / 1000000.0;
    float time_interval = time_new_s - time_old_s;
    float speed = data_num * 8 / time_interval / 1000.0;
    ESP_LOGI(SPP_TAG, "speed(%fs ~ %fs): %f kbit/s" , time_old_s, time_new_s, speed);
    data_num = 0;
    time_old.tv_sec = time_new.tv_sec;
    time_old.tv_usec = time_new.tv_usec;
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
            esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_INIT_EVT status:%d", param->init.status);
        }
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT status:%d handle:%"PRIu32" close_by_remote:%d", param->close.status,
                 param->close.handle, param->close.async);
        break;
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT handle:%"PRIu32" sec_id:%d scn:%d", param->start.handle, param->start.sec_id,
                     param->start.scn);
            esp_bt_dev_set_device_name(EXAMPLE_DEVICE_NAME);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_START_EVT status:%d", param->start.status);
        }
        break;
        //TODO: 1.SPP server started -> look for last connected neighbor
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT:
#if (SPP_SHOW_MODE == SPP_SHOW_DATA)
        /*
         * We only show the data in which the data length is less than 128 here. If you want to print the data and
         * the data rate is high, it is strongly recommended to process them in other lower priority application task
         * rather than in this callback directly. Since the printing takes too much time, it may stuck the Bluetooth
         * stack and also have a effect on the throughput!
         */
        //TODO: 3. data received -> send to queue & reset timer to sleep -> verify correctness of telegram
        ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len:%d handle:%lu",
                 param->data_ind.len, param->data_ind.handle);
        if (param->data_ind.len < 128) {
            esp_log_buffer_hex("", param->data_ind.data, param->data_ind.len);

            /* memory allocation for telegram string */
            uint8_t* telegram=(uint8_t *)malloc(sizeof(uint8_t)*(param->data_ind.len+1));
            memcpy(telegram,param->data_ind.data,param->data_ind.len);
            telegram[param->data_ind.len]='\0';
            printf("%s\t %d\n",telegram,param->data_ind.len);
            
            /* memory allocation for struct to be stored in queue */
            rcv_tele *new_telegram=(rcv_tele*) malloc(sizeof(rcv_tele*));
            new_telegram->data=telegram;
            new_telegram->len=param->data_ind.len;
            new_telegram->handle=param->data_ind.handle;



        xQueueSend( /* The handle of the queue. */
               ReceivedQueue,
               /* The address of the variable that holds the address of new_telegram.
               sizeof( new_telegram* ) bytes are copied from here into the queue. As the
               variable holds the address of new_telegram it is the address of new_telegram
               that is copied into the queue. */
                &new_telegram,
               ( TickType_t ) 0 );
        }

        UBaseType_t len= uxQueueMessagesWaiting( ReceivedQueue );
        printf("%d\n",len);
        // param->data_ind.data[4]='\3';
        // esp_err_t res=esp_spp_write(param->data_ind.handle, param->data_ind.len, param->data_ind.data);
        /* Send the address of xMessage to the queue created to hold 10    pointers. */


#else
        gettimeofday(&time_new, NULL);
        data_num += param->data_ind.len;
        if (time_new.tv_sec - time_old.tv_sec >= 3) {
            print_speed();
        }
#endif
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
        //TODO: telegram sended
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT status:%d handle:%"PRIu32", rem_bda:[%s]", param->srv_open.status,
                 param->srv_open.handle, bda2str(param->srv_open.rem_bda, bda_str, sizeof(bda_str)));
        gettimeofday(&time_old, NULL);
        //TODO: 2.SPP connection opened -> say hello
        // esp_err_t res=esp_spp_write(param->srv_open.handle, 4, (uint8_t*)"wit");
        break;
    case ESP_SPP_SRV_STOP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_STOP_EVT");
        break;
    case ESP_SPP_UNINIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_UNINIT_EVT");
        break;
    default:
        break;
    }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:{
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(SPP_TAG, "authentication success: %s bda:[%s]", param->auth_cmpl.device_name,
                     bda2str(param->auth_cmpl.bda, bda_str, sizeof(bda_str)));
        } else {
            ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT:{
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit) {
            ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        } else {
            ESP_LOGI(SPP_TAG, "Input pin code: 1234");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '2';
            pin_code[2] = '3';
            pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }

//#if (CONFIG_BT_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %"PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%"PRIu32, param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
//#endif

    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%s bda:[%s]", mode_to_str(param->mode_chg.mode),
                 bda2str(param->mode_chg.bda, bda_str, sizeof(bda_str)));
                 //TODO: change mode handling
        break;
    case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT status:%d ",param->acl_conn_cmpl_stat.stat);
        /*to do*/
        break;
    case ESP_BT_GAP_CONFIG_EIR_DATA_EVT: //EIR provides information about discoverable devices during a Bluetooth Inquiry.
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CONFIG_EIR_DATA_EVT status:%d ",param->config_eir_data.stat);
        break;
    default: {
        ESP_LOGI(SPP_TAG, "event: %d", event);
        break;
        
    }
    }
    return;
}

void app_main(void)
{
    char bda_str[18] = {0};
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_init()) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bluedroid_enable()) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s gap register failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp register failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = esp_spp_mode,
        .enable_l2cap_ertm = esp_spp_enable_l2cap_ertm,
        .tx_buffer_size = 0, /* Only used for ESP_SPP_MODE_VFS mode */
    };
    if ((ret = esp_spp_enhanced_init(&bt_spp_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp init failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

// #if (CONFIG_BT_SSP_ENABLED == true)
    /* Set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
// #endif

    /*
     * Set default parameters for Legacy Pairing
     * Use variable pin, input pin code when pairing
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    ESP_LOGI(SPP_TAG, "Own address:[%s]", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));


    /* Create a queue capable of containing 10 char* */
    ReceivedQueue = xQueueCreate( 10, sizeof( rcv_tele* ) ); 

    /* Task for process received telegrams */
    TaskHandle_t ProcessMsgTaskHandle;

    /*Create task processing received telegram*/
    xTaskCreate(&process_telegram, "process_telegram", 2048,NULL,1,NULL );

    

}

/* bluetooth_manager.cpp — BLE连接管理 */
#include "bluetooth_manager.h"
#include "setting_manager.h"
#include "src/gui_guider.h"
#include <Arduino.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define BT_MAX_DEVICES  12
#define BT_NAME_LEN     32
#define BT_ADDR_LEN     18
#define ICAR_SERVICE_UUID   "000018f0-0000-1000-8000-00805f9b34fb"
#define ICAR_READ_CHAR_UUID "00002af0-0000-1000-8000-00805f9b34fb"
#define ICAR_WRITE_CHAR_UUID "00002af1-0000-1000-8000-00805f9b34fb"

/* app_screen_t 枚举值 */
#ifndef APP_SCREEN_GENERAL
#define APP_SCREEN_GENERAL   0
#define APP_SCREEN_RACE      1
#define APP_SCREEN_SETTING   2
#define APP_SCREEN_BLUETOOTH 3
#endif

typedef struct { char name[BT_NAME_LEN]; char addr[BT_ADDR_LEN]; esp_ble_addr_type_t addr_type; } bt_device_t;
static bt_device_t s_devices[BT_MAX_DEVICES];
static int  s_device_count = 0;
static volatile bool s_scan_done = false, s_scanning = false;
static lv_ui *s_ui = NULL;
static app_switch_cb_t s_switch_cb = NULL;
static bt_conn_cb_t s_conn_cb = NULL;
static BLEScan *pBLEScan = NULL;
static Preferences bt_prefs;
static BLEClient *s_client = NULL;
static bool s_is_connected = false, s_ble_enabled = false;
static char s_conn_addr[BT_ADDR_LEN] = "", s_conn_name[BT_NAME_LEN] = "";
static int s_connecting_idx = -1, s_target_idx = -1, s_state = 0;
static BLERemoteCharacteristic *s_write_char = NULL, *s_read_char = NULL;
static bool s_ble_inited = false;
static bool s_pending_reconnect = false;   /* true=开机自动回连, false=手动操作 */
static uint32_t s_reconnect_delay_ms = 0;  /* 开机延迟启动 BLE */

/* ---- 原始数据缓冲 ---- */
static char s_rx_buf[512] = "";
static bool s_rx_ready = false;

/* ================================================================ */
static void refresh_device_list(void);
static void start_scan(void);
static void do_connect(void);
static void do_auto_reconnect(void);
static void do_disconnect(bool full_cleanup);

static void save_ble_state(void){bt_prefs.begin("bt_prefs",false);bt_prefs.putBool("enabled",s_ble_enabled);bt_prefs.end();}
static void load_ble_state(void){bt_prefs.begin("bt_prefs",true);s_ble_enabled=bt_prefs.getBool("enabled",true);bt_prefs.end();}
static void save_last_device(const char*a,const char*n,uint8_t t){bt_prefs.begin("bt_prefs",false);bt_prefs.putString("last_addr",a);bt_prefs.putString("last_name",n);bt_prefs.putUChar("last_type",t);bt_prefs.end();}
static bool load_last_device(char*a,char*n,uint8_t*t){bt_prefs.begin("bt_prefs",true);String A=bt_prefs.getString("last_addr",""),N=bt_prefs.getString("last_name","");uint8_t T=bt_prefs.getUChar("last_type",BLE_ADDR_TYPE_PUBLIC);bt_prefs.end();if(A.length()==0)return false;strncpy(a,A.c_str(),BT_ADDR_LEN-1);a[BT_ADDR_LEN-1]=0;strncpy(n,N.c_str(),BT_NAME_LEN-1);n[BT_NAME_LEN-1]=0;*t=T;return true;}

class BTClientCB : public BLEClientCallbacks {
    void onConnect(BLEClient*){
        s_is_connected=true;s_connecting_idx=-1;
        Serial.println("[BT] onConnect");
        if(s_conn_cb) s_conn_cb(true);
    }
    void onDisconnect(BLEClient*){
        s_is_connected=false;s_conn_addr[0]=0;s_conn_name[0]=0;
        s_write_char=NULL;s_read_char=NULL;
        Serial.println("[BT] onDisconnect");
        if(s_conn_cb) s_conn_cb(false);
    }
};

static void on_notify(BLERemoteCharacteristic*, uint8_t*p, size_t l, bool){
    if(!l)return;
    size_t c=strlen(s_rx_buf);
    size_t a=sizeof(s_rx_buf)-1-c;
    size_t n=l<a?l:a;
    if(n){memcpy(s_rx_buf+c,p,n);s_rx_buf[c+n]=0;}
    s_rx_ready=true;
}

static bool discover_services(void){
    if(!s_client||!s_client->isConnected())return false;
    BLERemoteService*svc=s_client->getService(ICAR_SERVICE_UUID);
    if(!svc){Serial.println("[BT-ELM] Svc missing");return false;}
    s_write_char=svc->getCharacteristic(ICAR_WRITE_CHAR_UUID);
    s_read_char=svc->getCharacteristic(ICAR_READ_CHAR_UUID);
    if(!s_write_char||!s_read_char){Serial.println("[BT-ELM] Char missing");return false;}
    if(s_read_char->canNotify())s_read_char->registerForNotify(on_notify);
    s_rx_buf[0]=0;s_rx_ready=false;
    Serial.println("[BT] Services discovered OK");
    return true;
}

/* ================================================================ */
static void do_disconnect(bool full){
    s_write_char=NULL;s_read_char=NULL;
    s_rx_buf[0]=0;s_rx_ready=false;
    if(s_client){if(s_client->isConnected())s_client->disconnect();if(full){delete s_client;s_client=NULL;}}
    s_is_connected=false;s_conn_addr[0]=0;s_conn_name[0]=0;s_connecting_idx=-1;
}
static void do_connect(void){
    if(s_target_idx<0||s_target_idx>=s_device_count){s_state=0;return;}
    const char*a=s_devices[s_target_idx].addr,*n=s_devices[s_target_idx].name;
    esp_ble_addr_type_t t=s_devices[s_target_idx].addr_type;
    do_disconnect(true);s_client=BLEDevice::createClient();s_client->setClientCallbacks(new BTClientCB());
    if(s_client->connect(BLEAddress(a),t)){
        strncpy(s_conn_addr,a,BT_ADDR_LEN-1);s_conn_addr[BT_ADDR_LEN-1]=0;
        strncpy(s_conn_name,n,BT_NAME_LEN-1);s_conn_name[BT_NAME_LEN-1]=0;
        save_last_device(a,n,(uint8_t)t);discover_services();
    }else{if(s_client){delete s_client;s_client=NULL;}}
    s_state=0;refresh_device_list();
}
static void do_auto_reconnect(void){
    char a[BT_ADDR_LEN],n[BT_NAME_LEN];uint8_t t=BLE_ADDR_TYPE_PUBLIC;
    if(!load_last_device(a,n,&t)){s_state=0;return;}
    do_disconnect(true);
    s_client=BLEDevice::createClient();s_client->setClientCallbacks(new BTClientCB());
    if(s_client->connect(BLEAddress(a),(esp_ble_addr_type_t)t)){
        strncpy(s_conn_addr,a,BT_ADDR_LEN-1);s_conn_addr[BT_ADDR_LEN-1]=0;
        strncpy(s_conn_name,n,BT_NAME_LEN-1);s_conn_name[BT_NAME_LEN-1]=0;
        s_is_connected=true;discover_services();s_state=0;
        Serial.println("[BT] Auto-reconnect OK");
    }else{
        if(s_client){delete s_client;s_client=NULL;}
        Serial.println("[BT] Auto-reconnect failed, give up");
        s_state=0;
    }
    refresh_device_list();
}
class BTScanCB : public BLEAdvertisedDeviceCallbacks{
    void onResult(BLEAdvertisedDevice d)override{
        if(s_device_count>=BT_MAX_DEVICES)return;
        bt_device_t*dev=&s_devices[s_device_count];
        if(d.haveName()){strncpy(dev->name,d.getName().c_str(),BT_NAME_LEN-1);dev->name[BT_NAME_LEN-1]=0;}
        else{String A=d.getAddress().toString();snprintf(dev->name,BT_NAME_LEN,"[%s]",A.substring(0,8).c_str());}
        strncpy(dev->addr,d.getAddress().toString().c_str(),BT_ADDR_LEN-1);dev->addr[BT_ADDR_LEN-1]=0;
        dev->addr_type=d.getAddressType();s_device_count++;
    }
};
static void on_scan_done(BLEScanResults){s_scanning=false;s_scan_done=true;}
static void start_scan(void){
    if(!s_ui||!s_ble_enabled)return;
    if(s_scanning&&pBLEScan){pBLEScan->stop();s_scanning=false;delay(50);}
    s_device_count=0;s_scan_done=false;lv_obj_clean(s_ui->bluetooth_bt_list_devices);
    lv_obj_t*b=lv_list_add_btn(s_ui->bluetooth_bt_list_devices,LV_SYMBOL_BLUETOOTH,"Scanning...");lv_obj_clear_flag(b,LV_OBJ_FLAG_CLICKABLE);
    pBLEScan=BLEDevice::getScan();pBLEScan->setAdvertisedDeviceCallbacks(new BTScanCB(),true);
    pBLEScan->setActiveScan(true);pBLEScan->setInterval(100);pBLEScan->setWindow(99);
    pBLEScan->start(5,on_scan_done,false);s_scanning=true;s_state=0;
}
static void refresh_device_list(void){
    if(!s_ui||!s_ui->bluetooth_bt_list_devices)return;
    lv_obj_clean(s_ui->bluetooth_bt_list_devices);
    if(!s_ble_enabled){
        lv_obj_t*b=lv_list_add_btn(s_ui->bluetooth_bt_list_devices,LV_SYMBOL_BLUETOOTH,"Bluetooth OFF");
        lv_obj_clear_flag(b,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_text_color(b,lv_color_hex(0x888888),LV_PART_MAIN);
        return;
    }
    if(s_device_count==0){lv_obj_t*b=lv_list_add_btn(s_ui->bluetooth_bt_list_devices,LV_SYMBOL_BLUETOOTH,"No devices");lv_obj_clear_flag(b,LV_OBJ_FLAG_CLICKABLE);return;}
    for(int i=0;i<s_device_count;i++){
        char buf[64];
        if(s_connecting_idx==i)snprintf(buf,sizeof(buf),"... %s",s_devices[i].name);
        else if(s_is_connected&&strcmp(s_conn_addr,s_devices[i].addr)==0)snprintf(buf,sizeof(buf),"\xE2\x97\x8F %s",s_devices[i].name);
        else snprintf(buf,sizeof(buf),"  %s",s_devices[i].name);
        lv_obj_t*b=lv_list_add_btn(s_ui->bluetooth_bt_list_devices,LV_SYMBOL_BLUETOOTH,buf);
        lv_obj_add_event_cb(b,[](lv_event_t*e){
            int idx=(int)(intptr_t)lv_event_get_user_data(e);
            if(idx<0||idx>=s_device_count)return;
            if(s_is_connected&&strcmp(s_conn_addr,s_devices[idx].addr)==0){do_disconnect(true);refresh_device_list();return;}
            if(s_scanning&&pBLEScan){pBLEScan->stop();s_scanning=false;delay(50);}
            s_connecting_idx=idx;s_target_idx=idx;s_state=4;refresh_device_list();
        },LV_EVENT_CLICKED,(void*)(intptr_t)i);
        if(s_is_connected&&strcmp(s_conn_addr,s_devices[i].addr)==0)lv_obj_set_style_text_color(b,lv_color_hex(0x00e676),LV_PART_MAIN);
    }
}

static void on_back(lv_event_t*){
    if(s_switch_cb) s_switch_cb((app_screen_t)APP_SCREEN_SETTING, false);
}
static void on_sw(lv_event_t*e){
    lv_obj_t*sw=lv_event_get_target(e);
    bool on=lv_obj_has_state(sw,LV_STATE_CHECKED);
    if(on==s_ble_enabled)return;
    if(on){
        /* 手动打开：直接 init + 扫描，不走回连 */
        if(!s_ble_inited){BLEDevice::init("HybridHUD");s_ble_inited=true;}
        s_ble_enabled=true;
        save_ble_state();
        s_pending_reconnect=false;
        s_state=3;  /* 直接扫描 */
    }else{
        if(s_scanning&&pBLEScan){pBLEScan->stop();s_scanning=false;}
        do_disconnect(true);
        s_ble_enabled=false;
        s_state=0;
        save_ble_state();
        refresh_device_list();
    }
}
static void on_scan_btn(lv_event_t*){
    if(!s_ble_enabled){
        if(!s_ble_inited){BLEDevice::init("HybridHUD");s_ble_inited=true;}
        s_ble_enabled=true;
        save_ble_state();
        s_pending_reconnect=false;
        s_state=3;
        return;
    }
    s_state=3;
}

void bluetooth_manager_init(lv_ui* ui){
    s_ui=(lv_ui*)ui;load_ble_state();
    if(s_ui->bluetooth_btn_back)lv_obj_add_event_cb(s_ui->bluetooth_btn_back,on_back,LV_EVENT_CLICKED,NULL);
    if(s_ui->bluetooth_bt_sw_enable){lv_obj_add_event_cb(s_ui->bluetooth_bt_sw_enable,on_sw,LV_EVENT_VALUE_CHANGED,NULL);
        if(s_ble_enabled)lv_obj_add_state(s_ui->bluetooth_bt_sw_enable,LV_STATE_CHECKED);else lv_obj_clear_state(s_ui->bluetooth_bt_sw_enable,LV_STATE_CHECKED);
    }
    if(s_ui->bluetooth_bt_btn_scan)lv_obj_add_event_cb(s_ui->bluetooth_bt_btn_scan,on_scan_btn,LV_EVENT_CLICKED,NULL);
    /* 开机延迟 800ms 再启动 BLE，避免阻塞 UI */
    if(s_ble_enabled){s_ble_enabled=false;s_pending_reconnect=true;s_reconnect_delay_ms=millis()+800;}
}
void bluetooth_manager_set_switch_cb(app_switch_cb_t cb){s_switch_cb=cb;}
void bluetooth_manager_set_conn_cb(bt_conn_cb_t cb){s_conn_cb=cb;}
void bluetooth_manager_enter(void){
    if(!s_ui||!s_ui->bluetooth_bt_list_devices)return;lv_obj_clean(s_ui->bluetooth_bt_list_devices);
    if(s_ui->bluetooth_bt_sw_enable){if(s_ble_enabled)lv_obj_add_state(s_ui->bluetooth_bt_sw_enable,LV_STATE_CHECKED);else lv_obj_clear_state(s_ui->bluetooth_bt_sw_enable,LV_STATE_CHECKED);}
    char a[BT_ADDR_LEN],n[BT_NAME_LEN];uint8_t t=BLE_ADDR_TYPE_PUBLIC;
    if(s_ble_enabled&&load_last_device(a,n,&t)){
        char b[64];if(s_is_connected&&strcmp(s_conn_addr,a)==0)snprintf(b,sizeof(b),"\xE2\x97\x8F %s",n);else snprintf(b,sizeof(b),"Last: %s",n);
        lv_obj_t*btn=lv_list_add_btn(s_ui->bluetooth_bt_list_devices,LV_SYMBOL_BLUETOOTH,b);lv_obj_clear_flag(btn,LV_OBJ_FLAG_CLICKABLE);
        if(s_is_connected)lv_obj_set_style_text_color(btn,lv_color_hex(0x00e676),LV_PART_MAIN);
    }else{lv_obj_t*btn=lv_list_add_btn(s_ui->bluetooth_bt_list_devices,LV_SYMBOL_BLUETOOTH,s_ble_enabled?"Press Scan":"Enable BT first");lv_obj_clear_flag(btn,LV_OBJ_FLAG_CLICKABLE);}
}
void bluetooth_manager_exit(void){if(s_scanning&&pBLEScan){pBLEScan->stop();s_scanning=false;}}
void bluetooth_manager_update(void){
    /* 开机延迟回连 */
    if(s_reconnect_delay_ms>0){
        if((int)(millis()-s_reconnect_delay_ms)<0)return;
        s_reconnect_delay_ms=0;
        s_ble_enabled=true;
        s_pending_reconnect=true;
        s_state=1;
    }

    if(s_scan_done){s_scan_done=false;refresh_device_list();}

    switch(s_state){
        case 1:
            if(!s_ble_inited){BLEDevice::init("HybridHUD");s_ble_inited=true;}
            s_ble_enabled=true;
            /* 开机自动回连，手动打开直接扫描 */
            s_state = s_pending_reconnect ? 2 : 3;
            s_pending_reconnect = false;
            break;
        case 2:do_auto_reconnect();break;
        case 3:start_scan();break;
        case 4:do_connect();break;
        default:break;
    }
}
bool bluetooth_is_connected(void){return s_is_connected;}
const char* bluetooth_connected_name(void){return s_conn_name[0]?s_conn_name:nullptr;}
const char* bluetooth_connected_addr(void){return s_conn_addr[0]?s_conn_addr:nullptr;}

/* ================================================================
 *  原始数据接口
 * ================================================================ */
bool bluetooth_manager_write(const char* data) {
    if (!s_write_char || !s_is_connected) return false;
    s_rx_buf[0] = 0; s_rx_ready = false;
    s_write_char->writeValue(data);
    return true;
}
bool bluetooth_manager_rx_ready(void) { return s_rx_ready; }
const char* bluetooth_manager_rx_buf(void) { return s_rx_buf; }
void bluetooth_manager_rx_clear(void) { s_rx_ready = false; s_rx_buf[0] = 0; }

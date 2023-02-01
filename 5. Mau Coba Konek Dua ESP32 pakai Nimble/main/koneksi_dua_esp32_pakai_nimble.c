#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "sdkconfig.h"

#define NAMA_PERANGKAT "ESP32_pertama"
#define TARGET_PERANGKAT "ESP32_kedua"
#define GATT_SVR_SVC_ALERT_UUID 0x1811

uint8_t ble_addr_type;

void ble_app_for_connect_to_target(void);
void ble_app_for_advertise(void);

// menerima data dari esp/perangkat lain menggunakan bluetooth
// fungsi ini nantinya akan dimasukan kedalam struct servis gatt
static int write_from_another_device(uint16_t connection_handle, uint16_t attribute_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // nimble hanya bisa mengirimkan dan menerima tipe data string text
    printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);
    return 0;
}

// mengirim data ke esp/perangkat lain menggunakan bluetooth
// fungsi ini nantinya akan dimasukan kedalam struct servis gatt
static int read_data_for_another_device(uint16_t connection_handle, uint16_t attribute_handle, struct ble_gatt_access_ctxt *ctxt, char *data_will_send, void *arg)
{

    // nimble hanya bisa mengirimkan dan menerima tipe data string text
    os_mbuf_append(ctxt->om, data_will_send, strlen(data_will_send));
    return 0;
}

// Array of pointers to other service definitions
// UUID - Universal Unique Identifier
// fungsi GATT untuk pertukaran data antara Bluetooth menggunakan Attribute Protocol (ATT)
static const struct ble_gatt_svc_def for_gatt_services_and_uuid[] = {
    {// ada primary, secondary, dan no service
     .type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID16_DECLARE(0x180),
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0xFEF4),
          .flags = BLE_GATT_CHR_F_READ,
          .access_cb = read_data_for_another_device},
         {.uuid = BLE_UUID16_DECLARE(0xDEAD),
          .flags = BLE_GATT_CHR_F_WRITE,
          .access_cb = write_from_another_device},
         {0}}},
    {0}};

// fungsi untuk event handling BLE
// fungsi GAP untuk menemukan perangkat lain, menyambungkan dan memutus koneksi antar perangkat
static int event_for_gap(struct ble_gap_event *event, void *arg)
{
    struct ble_hs_adv_fields fields;

    switch (event->type)
    {
    // ini kalo misal bluetooth nya konek
    case BLE_GAP_EVENT_CONNECT:
        /* code */
        break;

    // ini untuk diskonek bluetooth
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "Status Terputus");
        break;

    // ini untuk discover BLE lain
    case BLE_GAP_EVENT_DISC:
        ESP_LOG_I("GAP", "Seeking Target");
        ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        ble_app_for_connect_to_target(&event->disc);        
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "GAP EVENT");
        break;
    default:
        break;
    }
    return 0;
}

// Menunjukkan apakah kita harus mencoba terhubung ke perangkat ini ? (pengirim advertise yang ditentukan).
int ble_app_for_try_connect_with_some_device(const struct ble_gap_disc_desc *disc){
    struct ble_hs_adv_fields fields;

    // jika tipe event PDU (protokol data unit) 
    if(disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND && disc->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND){
        return 0;
    }

    if(ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) != 0){
        return ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    }

    for (int i = 0; i < fields.num_uuids16; i++) {
        if (ble_uuid_u16(&fields.uuids16[i].u) == GATT_SVR_SVC_ALERT_UUID) {
            return 1;
        }
    }
    return 0;
}

// for connecting to another device using BLE
void ble_app_for_connect_to_target_if_intresting(const struct ble_gap_disc_desc *disc){

    uint8_t target_adress_type;

    // jangan melakukan apapun jika kita mengabaikan advertiser (si target)
    if (!ble_app_for_try_connect_with_some_device(disc)){
        return;
    }

    // scanning harus di stop sebelum konek ke server
    ble_gap_disc_cancel();

    // mencari alamat target device untuk konek (no privacy)
    ble_hs_id_infer_auto(0, target_adress_type);

    ble_gap_connect(target_adress_type, &disc->addr,10000, NULL,);
    
}

void ble_app_for_advertise(void){

}





void app_main(void)
{
}

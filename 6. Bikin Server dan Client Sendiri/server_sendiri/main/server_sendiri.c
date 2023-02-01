#include <stdio.h>
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

#define SERVER_NAME "ini adalah server"

// UUID nya saya ngarang aja
#define UUID_FOR_SERVICE 0x1234
#define UUID_FOR_SPP_SERVICE 0x5678
#define UUID_FOR_READ_CHARACTERISTIC 0x9012
#define UUID_FOR_WRITE_CHARACTERISTIC 0x3456

uint8_t ble_address_type;
uint16_t connection_handle;
static uint16_t ble_svc_gatt_read_val_handle;
// ============================AWAL - Function Prototype==============================================
// fungsi buat baca data dr client
static int read_data_from_device(uint16_t connection_handle, uint16_t attribute_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
// fungsi buat nulis data ke client
static int write_data_to_console(uint16_t connection_handle, uint16_t attribute_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
// fungsi event untuk GAP
static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg);
// ============================AKHIR - Function Prototype=============================================

// ============================AWAL - program inti dari BLE nya=======================================
// STEP-1 & 2 Bikin servis & characteristic
// array ini untuk membuat service dan characteristik
// penulisannya berupa array dari struct ble_gatt_svc_def
static const struct ble_gatt_svc_def service_for_ble[] = {
    // servis ke-1 (buat baca tulis)
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        // uuid buat service
        .uuid = BLE_UUID16_DECLARE(UUID_FOR_SERVICE),
        // di dalam service ada characteristics
        .characteristics = (struct ble_gatt_chr_def[]){
            // bikin characteristics ke-1 dengan properties read
            {   
                // ini descriptor (maksudnya UUID buat characteristic) characteristics ke-1 (buat baca)
                .uuid = BLE_UUID16_DECLARE(UUID_FOR_READ_CHARACTERISTIC),
                // ini jenis properties dari characteristics ke-1 dengan value untuk membaca (READ)
			    .val_handle = &ble_svc_gatt_read_val_handle,
			    .flags = BLE_GATT_CHR_F_READ,
                // ini yg akan dilakukan (kita buat berupa fungsi)-metadata ???
                .access_cb = read_data_from_device},
            // sebagai tanda bahwa ble yg kita buat udah gada charachteristics lagi (kita cuma buat 2 characteristics)
            {0}
        }
    },
    // servis ke-2 buat Serial Port Profile (SPP)
    {   .type = BLE_GATT_SVC_TYPE_PRIMARY,
        // uuid buat SPP
        .uuid = BLE_UUID16_DECLARE(UUID_FOR_SPP_SERVICE),
        .characteristics = (struct ble_gatt_chr_def[]){            
            // bikin characteristics ke-2 dengan properties write
            {
                // ini descriptor (maksudnya UUID buat characteristic) characteristics ke-2 (buat nulis data)
                .uuid = BLE_UUID16_DECLARE(UUID_FOR_WRITE_CHARACTERISTIC),
                // ini jenis properties dari characteristics ke-2 dengan value untuk menulis (WRITE)
			    .val_handle = &ble_svc_gatt_read_val_handle,
			    .flags = BLE_GATT_CHR_F_WRITE,
                // ini yg akan dilakukan (kita buat berupa fungsi)-metadata ???
                .access_cb = write_data_to_console},
            // sebagai tanda bahwa ble yg kita buat udah gada charachteristics lagi (kita cuma buat 2 characteristics)
            {0}
        }
    },
    // sebagai tanda bahwa ble yg kita buat udah gada service lagi (kita cuma buat 2 service)
    {0,},
};

// STEP - 3 & 4 pengturn field sebelum Advertise & Mulai Advertise -> masih blm paham semua bagian/fungsi sini :)
void start_advertise_ble(void){

    // Beberapa pengaturan (data) untuk Advertise BLE 
    struct ble_hs_adv_fields let_we_set_the_fields;
    
    memset(&let_we_set_the_fields, 0, sizeof let_we_set_the_fields);

    // Berikut apa-apa saja yg di atur sebelum memulai advertise :

    // 1-mengatur tipe scaning BLE kita
    // (biar BLE kita bisa di temukan oleh semua Bluetooth | biar BLE kita bisa di temukan oleh BLE-only)
    let_we_set_the_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    // 2-menentukan TX Power
    let_we_set_the_fields.tx_pwr_lvl_is_present = 1;
    let_we_set_the_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    // 3-biar nama BLE kita bisa dibaca sama yg scan
    const char *device_name;
    device_name = ble_svc_gap_device_name();
    let_we_set_the_fields.name = (uint8_t *)device_name;
    let_we_set_the_fields.name_len = strlen(device_name);
    let_we_set_the_fields.name_is_complete = 1;
    // 4-notifikasi alert
    let_we_set_the_fields.uuids16 = (ble_uuid16_t[]){
        // nilai UUID nya dari https://www.bluetooth.com/specifications/assigned-numbers/
        BLE_UUID16_INIT(0x1811)
    };
    let_we_set_the_fields.num_uuids16 = 1;
    let_we_set_the_fields.uuids16_is_complete = 1;

    ble_gap_adv_set_fields(&let_we_set_the_fields);

    // ======================================================

    // Mulai untuk advertising
    struct ble_gap_adv_params let_we_set_the_param_and_start;

    memset(&let_we_set_the_param_and_start, 0, sizeof let_we_set_the_param_and_start);

    let_we_set_the_param_and_start.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    let_we_set_the_param_and_start.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable

    // LETS START !!!
    ble_gap_adv_start(ble_address_type, NULL, BLE_HS_FOREVER, &let_we_set_the_param_and_start, ble_spp_server_gap_event, NULL);
}
// ============================AKHIR - program inti dari BLE nya=======================================

static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg){
    // descriptor untuk koneksi (perlu gak perlu)
    // struct ble_gap_conn_desc descriptor;
    struct ble_gap_disc_desc to_knowing_rssi;

    switch (event -> type){
    
    // advertise jika terhubung
    case BLE_GAP_EVENT_CONNECT:
        // buat notifikasi di print ke console
        // MODLOG_DFLT(INFO, "(Pake MODLOG_DFLT)-Status Koneksi %s; status = %d", event->connect.status == 0 ? "terhubung" : "gagal terhubung");
        // nyoba print console pake ini
        ESP_LOGI("(Pake ESP_LOGI)-GAP", "Status Koneksi : %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        // ini bagian perlu ga perlu
        // if(event->connect.status == 0){
        //     ble_gap_conn_find(event->connect.conn_handle, &descriptor);
        // }else 
        if(event->connect.status != 0){
            start_advertise_ble();
        }
        break;
    // advertise jika terputus
    case BLE_GAP_EVENT_DISCONNECT:
        // buat notifikasi di print ke console
        MODLOG_DFLT(INFO, "(Pake MODLOG_DFLT)-disconnect; reason=%d ", event->disconnect.reason);
        // nyoba perbedaan print console pake ini
        ESP_LOGI("(Pake ESP_LOGI)-Status BLE","Terputus !");
        printf("============== SELESAI dan MULAI ADVERTISE LAGI ============== \n");
        // stelah putus koneksinya, advertise lagi
        start_advertise_ble();
        break;
    // update status koneksi
    case BLE_GAP_EVENT_CONN_UPDATE:
        // buat notifikasi di print ke console
        MODLOG_DFLT(INFO, "(Pake MODLOG_DFLT)-connection updated; status=%d ",event->conn_update.status);
        // nyoba perbedaan print console pake ini
        ESP_LOGI("(Pake ESP_LOGI)-Update status koneksi", "status %s", event->conn_update.status == 0 ? "Koneksi berhasil di update" : "Koneksi gagal di Update");
        break;
    // kalo udah terhubung advertise nya selesai
    case BLE_GAP_EVENT_ADV_COMPLETE:
        // buat notifikasi di print ke console
        MODLOG_DFLT(INFO, "(Pake MODLOG_DFLT)-advertise complete; reason=%d", event->adv_complete.reason);
        // nyoba perbedaan print console pake ini
        if (event->adv_complete.reason == 0){
            ESP_LOGI("(Pake ESP_LOGI)-Advertise status ", "Advertise dihentikan karena ada perangkat terhubung");
        }else if (event->adv_complete.reason == BLE_HS_ETIMEOUT){
            ESP_LOGI("(Pake ESP_LOGI)-Advertise status ", "Durasi habis");
        }if (event->adv_complete.reason == BLE_HS_EPREEMPTED){
             ESP_LOGI("(Pake ESP_LOGI)-Advertise status ", "Dibatalkan oleh HOST");
        }else {
             // mulai advertise lagi kalo udah terputus/selesai advertise
            start_advertise_ble();
        }
        break;
    case BLE_GAP_EVENT_NOTIFY_RX:
        ble_gap_conn_rssi(event->connect.conn_handle, &to_knowing_rssi);

    // In Bluetooth, the Maximum Transmission Unit (MTU) is the maximum size of a single packet that can be transmitted over the connection. The MTU determines the maximum amount of data that can be sent in a single packet, and it plays a key role in the efficiency and performance of the connection.
    // masih ga paham apaan ini ???
    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.channel_id,
                    event->mtu.value);
        break;
    default:
        break;
    }
    return 0;
}

// fungsi buat baca data dari client
static int read_data_from_device(uint16_t connection_handle, uint16_t attribute_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
/*
    if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
        ESP_LOGI("Server BLE ","Data terbaca: ");
        // nyoba bisa apa ga pakai printf
        // printf("Data terbaca: %.*s\n", ctxt->op);
    }else{
        os_mbuf_append(ctxt->om, "ini data dari server", strlen("ini data dari server"));
    }
*/
    os_mbuf_append(ctxt->om, "ini data dari server", strlen("ini data dari server"));
    return 0;
}

// fungsi untuk menulis data ke client
static int write_data_to_console(uint16_t connection_handle, uint16_t attribute_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
/*
    if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
        
    }
    else{
        printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);   
    }
*/
    if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
        printf("Data from the client: %.*s\n", ctxt->om->om_len, ctxt->om->om_data);  
    }
    return 0;
}

// 
void ble_app_sycronisation(void){
    // menentukan adress ble secara otomatis saat memulai advertise
    ble_hs_id_infer_auto(0, &ble_address_type);
    // masukan fungsi advertise disini biar bisa advertise
    start_advertise_ble();
}

void host_task(void *param){
    printf("============== YUK MULAI ============== \n");
    // fungsi untuk memulai nimble kembali saat nimble_port_stop() si eksekusi
    // jadinya ble kita jalan terus menerus
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void)
{
    // berikut adalah step-step untuk memulai BLE :
    
    // 1 - Initialize NVS — it is used to store PHY calibration data
    nvs_flash_init();
    // 2 - Initialize ESP controller
    esp_nimble_hci_and_controller_init();
    // 3 - Initialize the host stack
    nimble_port_init();
    // 4 - Initialize NimBLE configuration - Set the default device name
    ble_svc_gap_device_name_set(SERVER_NAME);
    ble_svc_gap_init();
    ble_svc_gatt_init();
    // masukan array yg berisi service dan characteristics
    ble_gatts_count_cfg(service_for_ble);
    ble_gatts_add_svcs(service_for_ble);
    // menentukan konfigurasi host
    ble_hs_cfg.sync_cb = ble_app_sycronisation;
    nimble_port_freertos_init(host_task);
}
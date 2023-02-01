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
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#include "sdkconfig.h"

// nama server yg akan dihubungkan
#define CLIENT_NAME "client kita"

// UUID untuk ble client
// UUID nya saya ngarang aja
#define UUID_CLIENT 0x7384

// UUID nya saya ngarang aja (ini sama kaya yg di server)
// UUID nya saya ngarang aja
#define UUID_FOR_SERVICE 0x1234
#define UUID_FOR_SPP_SERVICE 0x5678
#define UUID_FOR_READ_CHARACTERISTIC 0x9012
#define UUID_FOR_WRITE_CHARACTERISTIC 0x3456

// ============== AWAL - variable ====================
uint8_t ble_address_type;
// ============== AKHIR - variable ====================

// ============== AWAL - Prototipe function ====================
static int ble_gap_spp_client_event(struct ble_gap_event *event, void *arg);
// ============== AKHIR - Prototipe funcyion ====================

// ============== AWAL - fungsi mencoba membuat koneksi ====================
// akan me return TRUE jika target advertise koneksi dan mendukung notifikasi alert
static int ble_decide_to_connect_target(const struct ble_gap_disc_desc *discover){
    struct ble_hs_adv_fields fields;

    // target harusadvertising konektabilitas
    if (discover->event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND && discover->event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND)
    {
        return 0;
    }
    
    ble_hs_adv_parse_fields(&fields, discover->data, discover->length_data);

    // kalo mau terhubung, si target harus suport service "alert notification"
    for (int i = 0; i < fields.num_uuids16; i++)
    {
        if (ble_uuid_u16(&fields.uuids16[i].u) == 0x1811)
        {
            return 1;
        }
        
    }
    return 0;
}

static void ble_try_toconnect_target(const struct ble_gap_disc_desc *discover){
    // uint8_t ble_address_type;

    // tidak melakukan apa2 (return 0) kalau misal target ga sesuai
    // printf("nilai return should connect : %d \n", ble_decide_to_connect_target(discover));
    if (!ble_decide_to_connect_target(discover)){
        return;
    }

    // sebelum melakukan koneksi, scaning/discovery harus di stop dulu
    ble_gap_disc_cancel();
    // menentukan alamat otomatis untuk digunakan saat proses koneksi
    ble_hs_id_infer_auto(0, &ble_address_type);
    // mencoba menghubungkan target, timeout nya kita kasih 30 s
    printf("coba konek selama 30 s \n");
    ble_gap_connect(ble_address_type, &discover->addr, 30000, NULL, ble_gap_spp_client_event, NULL);
}
// ============== AKHIR - fungsi mencoba membuat koneksi ====================

// ============== AWAL - fungsi buat scaning/GAP ====================
void ble_scaning(void){

    printf("Mulai scaning..... \n");

    struct ble_gap_disc_params paramater_settings_for_scaning;

    // filter duplikat advertisements, agar tidak terjadi pengulangan
    paramater_settings_for_scaning.filter_duplicates = 1;
    // kita gunakan aktive scaning dan nantinya kita print ke console advertisements yg ada
    paramater_settings_for_scaning.passive = 0;
    // scaning interval
    paramater_settings_for_scaning.itvl = 0;
    // scan window
    paramater_settings_for_scaning.window = 0;
    // scan filter policy (mungkin kalau ada pengaman/password ???)
    paramater_settings_for_scaning.filter_policy = 0;
    // gaatau ini untuk apa
    paramater_settings_for_scaning.limited = 0;

    ble_gap_disc(ble_address_type, BLE_HS_FOREVER, &paramater_settings_for_scaning, ble_gap_spp_client_event, NULL);
}
// ============== AKHIR - fungsi buat scaning/GAP ====================

// ============== AWAL - fungsi GAP event buat client (SPP) ====================
static int ble_gap_spp_client_event(struct ble_gap_event *event, void *arg){

    struct ble_hs_adv_fields fields;

    switch (event->type){
    case BLE_GAP_EVENT_DISC:
        // ESP_LOGI("GAP", "GAP EVENT DISCOVERY");
        ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        
        if (fields.name_len > 0){
            printf("Nama Bluetooth yg Tersedia: %.*s | Nilai RSSI : %d dBm\n", fields.name_len, fields.name, event->disc.rssi);
        }

        // jika sudah ketemu perangkatnya coba minta konek
        ble_try_toconnect_target(&event->disc);
        break;
    // event kalau connect
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("(Pake ESP_LOGI)-GAP", "Status Koneksi : %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0){
            ble_scaning();
        }
        
        break;
    // event kalau disconnect
    case BLE_GAP_EVENT_DISCONNECT:
        // buat notifikasi di print ke console
        MODLOG_DFLT(INFO, "(Pake MODLOG_DFLT)-disconnect; reason=%d ", event->disconnect.reason);
        // nyoba perbedaan print console pake ini
        ESP_LOGI("(Pake ESP_LOGI)-Status BLE","Terputus !");
        printf("============== SELESAI dan MULAI SCAN TARGET LAGI ============== \n");
        ble_scaning();
        break;
    
    case BLE_GAP_EVENT_DISC_COMPLETE:
        break;

    case BLE_GAP_EVENT_NOTIFY_RX:
        // print data ke console
        printf("Data dari server : %.*s\n", event->notify_rx.om->om_len, event->notify_rx.om->om_data);
        break;
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
// ============== AKHIR - fungsi GAP event buat client (SPP) ====================

// ============== AWAL - fungsi app BLE Client (SPP) ====================
void ble_spp_client_app_on_sync(void){

    // ga tau buat apa
    ble_hs_util_ensure_addr(0);
    // menentukan adress yg terbaik secara otomatis
    ble_hs_id_infer_auto(0, &ble_address_type);
    ble_scaning();
}
// ============== AKHIR - fungsi app BLE Client (SPP) ====================

void host_task(void *param){
    printf("============== YUK MULAI CLIENT ============== \n");
    // fungsi untuk memulai nimble kembali saat nimble_port_stop() si eksekusi
    // jadinya ble kita jalan terus menerus
    nimble_port_run();
}


void app_main(void)
{    
    nvs_flash_init();
    esp_nimble_hci_and_controller_init();
    nimble_port_init();
    ble_svc_gap_device_name_set(CLIENT_NAME);
    ble_svc_gap_init();
    ble_hs_cfg.sync_cb = ble_spp_client_app_on_sync;
    nimble_port_freertos_init(host_task);
}

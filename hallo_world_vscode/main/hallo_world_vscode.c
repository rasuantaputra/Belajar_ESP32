#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// static void testingPrint(void)
// {
//     printf("ini cuma ngetes");    
// }

// static void test(void){
//     printf("coba aja");
// }

void app_main(void)
{   

    while (1)
    {
        printf("ini cuma ngetes");  
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
    

}


// Blink LED RTOS Program

// for use only 1 core
#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0; 
#else
static const BaseType_t app_cpu = 1;
#endif

// pins blink led
static const int led1 = 5;
static const int led2 = 6;

// task : blink an led
// for free RTOS the function should return nothing and accept one void pointer as a parameter
void toggleLed_1(void *parameter){
  while(1){
    digitalWrite(led1, HIGH);
    // The interupt period is known as a tick
    // FreeRTOS sets the tick period to 1 ms (portTICK_PERIOD_MS = 1 ms)
    vTaskDelay(500/portTICK_PERIOD_MS);
    digitalWrite(led1, LOW);
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
}

void toggleLed_2(void *parameter){
  while(1){
    digitalWrite(led2, HIGH);
    vTaskDelay(500/portTICK_PERIOD_MS);
    digitalWrite(led2, LOW);
    vTaskDelay(500/portTICK_PERIOD_MS);
  }
}

void setup() {
  // configure pin
  // note: in esp-idf is difference
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);

  // use "xTaskCreatePinnedToCore" to tell the scheduller that we want to run our task in only one of the cores
  // "xTaskCreatePinnedToCore" not exist in Vanilla FreeRTOS, you's want to use "xTaskCreate" instead
  // "xTaskCreate" does still work in ESP-IDF
  xTaskCreatePinnedToCore(
    // function to be called
    toggleLed_1,

    // name of task
    "Menyalkan LED 1",

    // stack size (byte in esp32, words in FreeRTOS)
    // the smallest stack size we can set here is 768 bytes
    1024,

    // parameter to pass to function
    NULL,

    // task priority (0 to configMAX_PRIORITIES - 1) 
    // (we can set priority 0-24, 0 is lowest priority)
    1,

    // task handle
    // belum paham bagian handle ini ??? 
    NULL,

    // run on 1 core for demo purposes (ESP32 only)
    app_cpu);

    xTaskCreatePinnedToCore(
    toggleLed_2,
    "Menyalkan LED 2",
    1024,
    NULL,
    1,
    NULL,
    app_cpu);

    // if this Vanilla FreeRTOS, you'd want to call vTaskStartScheduler() in
    // main after setting up your tasks.
}

void loop() {
  // put your main code here, to run repeatedly:

}

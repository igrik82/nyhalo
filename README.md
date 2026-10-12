# Entrance Node

ESP32 node для обнаружения движения на лестничной площадке, также мониторит задымление(проводка и т.д.), передает данные по MQTT, предусмотрена OTA будущий ESP-NOW gateway.

Проект использует стандартную событийную модель ESP‑IDF: прикладные события публикуются через esp_event, потоки сырых данных передаются через FreeRTOS-очереди, а поведение устройств описывается конечными автоматами.

1. Драйверы:
   - led
   - buzzer
   - PIR
   - ENS160
   - AHT21
   - WS2812

2. Очереди данных:
   - telemetry queue
   - motion raw queue
   - command queue
   - espnow packet queue

3. Сервисы:
   - motion service
   - air quality service
   - alert service
   - wifi manager
   - mqtt service
   - ota manager
   - espnow gateway

4. FSM:
   - motion FSM
   - alert FSM
   - wifi FSM
   - mqtt FSM
   - ota FSM
   - status LED FSM

железо даёт данные
очереди переносят данные
сервисы принимают решения
события сообщают о фактах
подписчики реагируют
FSM управляют состояниями

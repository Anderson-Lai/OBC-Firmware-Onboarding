#include "thermal_mgr.h"
#include "errors.h"
#include "lm75bd.h"
#include "console.h"

#include <FreeRTOS.h>
#include <os_task.h>
#include <os_queue.h>

#include <string.h>

#define THERMAL_MGR_STACK_SIZE 256U

static TaskHandle_t thermalMgrTaskHandle;
static StaticTask_t thermalMgrTaskBuffer;
static StackType_t thermalMgrTaskStack[THERMAL_MGR_STACK_SIZE];

#define THERMAL_MGR_QUEUE_LENGTH 10U
#define THERMAL_MGR_QUEUE_ITEM_SIZE sizeof(thermal_mgr_event_t)

static QueueHandle_t thermalMgrQueueHandle;
static StaticQueue_t thermalMgrQueueBuffer;
static uint8_t thermalMgrQueueStorageArea[THERMAL_MGR_QUEUE_LENGTH * THERMAL_MGR_QUEUE_ITEM_SIZE];

static void thermalMgr(void *pvParameters);

void initThermalSystemManager(lm75bd_config_t *config) {
  memset(&thermalMgrTaskBuffer, 0, sizeof(thermalMgrTaskBuffer));
  memset(thermalMgrTaskStack, 0, sizeof(thermalMgrTaskStack));
  
  thermalMgrTaskHandle = xTaskCreateStatic(
    thermalMgr, "thermalMgr", THERMAL_MGR_STACK_SIZE,
    config, 1, thermalMgrTaskStack, &thermalMgrTaskBuffer);

  memset(&thermalMgrQueueBuffer, 0, sizeof(thermalMgrQueueBuffer));
  memset(thermalMgrQueueStorageArea, 0, sizeof(thermalMgrQueueStorageArea));

  thermalMgrQueueHandle = xQueueCreateStatic(
    THERMAL_MGR_QUEUE_LENGTH, THERMAL_MGR_QUEUE_ITEM_SIZE,
    thermalMgrQueueStorageArea, &thermalMgrQueueBuffer);

}

error_code_t thermalMgrSendEvent(thermal_mgr_event_t *event) {
  /* Send an event to the thermal manager queue */
  if (!event)
    return ERR_CODE_INVALID_ARG;

  thermal_mgr_event_type_t item = THERMAL_MGR_EVENT_MEASURE_TEMP_CMD;
  xQueueSend(thermalMgrQueueHandle, &item, 10);

  return ERR_CODE_SUCCESS;
}

void osHandlerLM75BD(void) {
  /* Implement this function */
  float temperature = 0.0f;
  readTempLM75BD(LM75BD_OBC_I2C_ADDR, &temperature);

  const int overTemperature = 80;
  const int hysteresis = 75;

  if (temperature >= overTemperature) {
    thermal_mgr_event_type_t item = THERMAL_MGR_EVENT_OVER_TEMPERATURE;

    if (xQueueSendFromISR(thermalMgrQueueHandle, &item, NULL) == errQUEUE_FULL) {
      return;
    }
  } else if (temperature <= hysteresis) {
    thermal_mgr_event_type_t item = THERMAL_MGR_EVENT_SAFE_OPERATING;

    if (xQueueSendFromISR(thermalMgrQueueHandle, &item, NULL) == errQUEUE_FULL) {
      return;
    }
  }
}

static void thermalMgr(void *pvParameters) {
  /* Implement this task */
  while (1) {
    thermal_mgr_event_t buffer;

    if (xQueueReceive(thermalMgrQueueHandle, &buffer, 10) == pdFALSE) {
      continue;
    }

    if (buffer.type == THERMAL_MGR_EVENT_MEASURE_TEMP_CMD) {
      float temperature = 0.0f;

      readTempLM75BD(LM75BD_OBC_I2C_ADDR, &temperature);
      addTemperatureTelemetry(temperature);
    } else if (buffer.type == THERMAL_MGR_EVENT_OVER_TEMPERATURE) {
      overTemperatureDetected();
    } else if (buffer.type == THERMAL_MGR_EVENT_SAFE_OPERATING) {
      safeOperatingConditions();
    }
  }
}

void addTemperatureTelemetry(float tempC) {
  printConsole("Temperature telemetry: %f deg C\n", tempC);
}

void overTemperatureDetected(void) {
  printConsole("Over temperature detected!\n");
}

void safeOperatingConditions(void) { 
  printConsole("Returned to safe operating conditions!\n");
}

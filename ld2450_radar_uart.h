#pragma once

#include <furi.h>
#include <furi_hal.h>
#include "ld2450_radar_parser.h"

typedef struct LD2450RadarUart LD2450RadarUart;

typedef void (*LD2450RadarUartCallback)(LD2450Data* data, void* context);

LD2450RadarUart* ld2450_radar_uart_alloc();
void ld2450_radar_uart_free(LD2450RadarUart* uart);

bool ld2450_radar_uart_start(LD2450RadarUart* uart);
void ld2450_radar_uart_stop(LD2450RadarUart* uart);

uint32_t ld2450_radar_uart_get_rx_bytes(LD2450RadarUart* uart);

void ld2450_radar_uart_set_handle_rx_data_cb(
    LD2450RadarUart* uart,
    LD2450RadarUartCallback callback,
    void* context);

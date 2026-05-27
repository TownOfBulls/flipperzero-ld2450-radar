#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LD2450_TARGET_COUNT 3
#define LD2450_FRAME_SIZE 30

typedef struct {
    int16_t x_mm;
    int16_t y_mm;
    int16_t speed_cm_s;
    uint16_t distance_resolution_mm;
    bool valid;
} LD2450Target;

typedef struct {
    LD2450Target targets[LD2450_TARGET_COUNT];
} LD2450Data;

typedef struct {
    uint8_t buffer[LD2450_FRAME_SIZE];
    uint8_t index;
} LD2450Parser;

void ld2450_radar_parser_init(LD2450Parser* parser);

bool ld2450_radar_parser_push_byte(
    LD2450Parser* parser,
    uint8_t byte,
    LD2450Data* out_data);
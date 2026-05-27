#include "ld2450_radar_parser.h"
#include <string.h>

static int16_t ld2450_read_coord_or_speed(uint8_t lo, uint8_t hi) {
    uint16_t raw = (uint16_t)lo | ((uint16_t)hi << 8);

    if(raw & 0x8000) {
        return (int16_t)(raw & 0x7FFF);
    } else {
        return -(int16_t)(raw & 0x7FFF);
    }
}

static uint16_t ld2450_read_u16_le(uint8_t lo, uint8_t hi) {
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

void ld2450_radar_parser_init(LD2450Parser* parser) {
    memset(parser, 0, sizeof(LD2450Parser));
}

static bool ld2450_frame_is_valid(uint8_t* b) {
    return b[0] == 0xAA &&
           b[1] == 0xFF &&
           b[2] == 0x03 &&
           b[3] == 0x00 &&
           b[28] == 0x55 &&
           b[29] == 0xCC;
}

bool ld2450_radar_parser_push_byte(
    LD2450Parser* parser,
    uint8_t byte,
    LD2450Data* out_data) {
    if(parser->index == 0 && byte != 0xAA) {
        return false;
    }

    parser->buffer[parser->index++] = byte;

    if(parser->index < LD2450_FRAME_SIZE) {
        return false;
    }

    parser->index = 0;

    if(!ld2450_frame_is_valid(parser->buffer)) {
        return false;
    }

    memset(out_data, 0, sizeof(LD2450Data));

    for(uint8_t i = 0; i < LD2450_TARGET_COUNT; i++) {
        uint8_t offset = 4 + (i * 8);

        out_data->targets[i].x_mm =
            ld2450_read_coord_or_speed(parser->buffer[offset], parser->buffer[offset + 1]);

        out_data->targets[i].y_mm =
            ld2450_read_coord_or_speed(parser->buffer[offset + 2], parser->buffer[offset + 3]);

        out_data->targets[i].speed_cm_s =
            ld2450_read_coord_or_speed(parser->buffer[offset + 4], parser->buffer[offset + 5]);

        out_data->targets[i].distance_resolution_mm =
            ld2450_read_u16_le(parser->buffer[offset + 6], parser->buffer[offset + 7]);

        out_data->targets[i].valid =
            out_data->targets[i].x_mm != 0 ||
            out_data->targets[i].y_mm != 0 ||
            out_data->targets[i].speed_cm_s != 0 ||
            out_data->targets[i].distance_resolution_mm != 0;
    }

    return true;
}
/**
 * @file mqtt_parser.h
 */
#ifndef E2_MQTT_PARSER_H
#define E2_MQTT_PARSER_H

#include "mqtt_base.h"

typedef struct mqtt_parser mqtt_parser_t;

mqtt_parser_t *mqtt_parser_create(void);
void           mqtt_parser_destroy(mqtt_parser_t *p);
int            mqtt_parser_feed(mqtt_parser_t *p, uint8_t byte);
mqtt_packet_t *mqtt_parser_take(mqtt_parser_t *p);

#endif /* E2_MQTT_PARSER_H */

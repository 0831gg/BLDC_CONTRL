/**
 * @file    bsp_hall.h
 * @brief   Hall input sampling and timing support
 */

#ifndef __BSP_HALL_H
#define __BSP_HALL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define BSP_HALL_DIR_UNKNOWN   (0)
#define BSP_HALL_DIR_CW        (1)
#define BSP_HALL_DIR_CCW       (-1)

uint8_t bsp_hall_init(void);
uint8_t bsp_hall_get_state(void);
uint8_t bsp_hall_get_last_state(void);
uint32_t bsp_hall_get_speed(void);
uint32_t bsp_hall_get_commutation_time(void);
uint32_t bsp_hall_get_period_ticks(void);
uint32_t bsp_hall_get_period_us(void);
uint32_t bsp_hall_get_edge_count(void);
uint32_t bsp_hall_get_valid_transition_count(void);
uint32_t bsp_hall_get_invalid_transition_count(void);
int8_t bsp_hall_get_direction(void);
uint8_t bsp_hall_is_irq_enabled(void);
void bsp_hall_clear_stats(void);
void bsp_hall_irq_enable(void);
void bsp_hall_irq_disable(void);
void bsp_hall_poll_and_commutate(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_HALL_H */

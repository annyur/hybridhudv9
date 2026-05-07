/* ecu.h — ECU UDS DID 接口 */
#ifndef ECU_H
#define ECU_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ecu_init(void);
void ecu_enter_pid(void);

/* 查询下一个该发的 DID（消耗队列）
 * 返回 true: 有 DID 待发，did 和 addr 被填充
 * 返回 false: 无 DID 待发（周期未到或 pending 未清除） */
bool ecu_get_next_did(uint16_t *did, const char **addr);

/* 偷看下一个 DID（不消耗队列）
 * 用于 obd.c 判断下一个 DID 是否与当前地址相同，决定是否跳过 ATSH7E0 切换 */
bool ecu_peek_next_did(uint16_t *did, const char **addr);

/* 标记 DID 已发出，设置 pending 锁 */
void ecu_set_pending(uint16_t did);

/* 解除 pending 锁（响应到达或超时时调用） */
void ecu_clear_pending(void);

/* 解析 UDS 正响应（由 obd.c 的 handle_rx 调用） */
void ecu_parse_uds(const char *resp);

#ifdef __cplusplus
}
#endif

#endif
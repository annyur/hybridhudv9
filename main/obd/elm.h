/* elm.h — ELM327 AT 命令 */
#ifndef ELM_H
#define ELM_H

#ifdef __cplusplus
extern "C" {
#endif

void elm_init(void);
void elm_poll(uint32_t now_ms);
bool elm_tx(const char *cmd);
bool elm_rx_ready(void);
const char *elm_rx_buf(void);
void elm_rx_clear(void);

#ifdef __cplusplus
}
#endif

#endif
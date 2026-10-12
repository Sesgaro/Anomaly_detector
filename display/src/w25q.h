#ifndef W25Q_H
#define W25Q_H

#include <stdint.h>
#include <stdbool.h>

/* Flash SPI W25Qxx de la placa (SPI3). Devuelve false si no responde. */
bool w25q_init(void);
uint32_t w25q_size(void);
/* ID JEDEC leído (fabricante, tipo, capacidad), aunque no sea válido */
uint32_t w25q_jedec(void);

void w25q_read(uint32_t addr, void *buf, uint32_t len);
void w25q_erase_sector(uint32_t addr);               /* sector de 4 KB */
void w25q_erase_block64(uint32_t addr);              /* bloque de 64 KB */
void w25q_write(uint32_t addr, const void *buf, uint32_t len);

#endif /* W25Q_H */

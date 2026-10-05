/* Driver LPS22HH indépendant du HAL : le bus I2C est injecté par pointeurs de fonctions. */
#ifndef LPS22HH_H
#define LPS22HH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LPS22HH_WHO_AM_I       0x0FU
#define LPS22HH_CTRL_REG1      0x10U
#define LPS22HH_PRESS_OUT_XL   0x28U   /* 5 octets : P(3) + T(2) */
#define LPS22HH_ID             0xB3U
#define LPS22HH_CTRL1_10HZ_BDU 0x22U   /* ODR = 10 Hz, BDU = 1 */

/* Convention : 0 = succès, non-nul = erreur (identique à HAL_OK == 0). */
typedef struct {
  void *ctx;
  int      (*mem_read)(void *ctx, uint8_t reg, uint8_t *buf, uint16_t n);
  int      (*mem_write)(void *ctx, uint8_t reg, const uint8_t *buf, uint16_t n);
  void     (*recover)(void *ctx);      /* libération du bus (9 impulsions SCL + STOP) */
  uint32_t (*last_error)(void *ctx);   /* hi2c2.ErrorCode */
} lps22hh_bus_t;

typedef struct {
  uint8_t ready;
} lps22hh_t;

/* Une itération de la boucle principale (sans HAL_Delay ni UART).
 * Écrit le message à transmettre dans msg et renvoie sa longueur (< size). */
int lps22hh_step(lps22hh_t *dev, const lps22hh_bus_t *bus, char *msg, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* LPS22HH_H */

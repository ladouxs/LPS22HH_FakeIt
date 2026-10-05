/* Adaptateur cible : branche le driver LPS22HH (lib/lps22hh) sur le HAL STM32U5.
 * Non compilé dans les tests hôte. */
#include "lps22hh_port_stm32.h"
#include "main.h"
#include "i2c.h"

#define LPS22HH_ADDR       (0x5DU << 1)  /* SA0 = 1 sur cette carte */
#define LPS22HH_TIMEOUT_MS 100U

/* Libère un esclave I2C bloqué (9 impulsions SCL + STOP) puis réinitialise I2C2 */
static void i2c2_recover(void)
{
  GPIO_InitTypeDef g = {0};

  HAL_I2C_DeInit(&hi2c2);

  __HAL_RCC_GPIOH_CLK_ENABLE();
  g.Pin   = GPIO_PIN_4 | GPIO_PIN_5;        /* PH4 = SCL, PH5 = SDA */
  g.Mode  = GPIO_MODE_OUTPUT_OD;
  g.Pull  = GPIO_PULLUP;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &g);

  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_SET);
  HAL_Delay(1);
  for (int i = 0; i < 9; i++)
  {
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4, GPIO_PIN_RESET); HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4, GPIO_PIN_SET);   HAL_Delay(1);
  }
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_RESET); HAL_Delay(1);  /* STOP */
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4, GPIO_PIN_SET);   HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_SET);   HAL_Delay(1);

  MX_I2C2_Init();                           /* remet les broches en AF4 */
}

static int hal_read(void *ctx, uint8_t reg, uint8_t *b, uint16_t n)
{
  (void)ctx;
  return (int)HAL_I2C_Mem_Read(&hi2c2, LPS22HH_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                               b, n, LPS22HH_TIMEOUT_MS);
}

static int hal_write(void *ctx, uint8_t reg, const uint8_t *b, uint16_t n)
{
  (void)ctx;
  return (int)HAL_I2C_Mem_Write(&hi2c2, LPS22HH_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                                (uint8_t *)b, n, LPS22HH_TIMEOUT_MS);
}

static void hal_recover(void *ctx)
{
  (void)ctx;
  i2c2_recover();
}

static uint32_t hal_last_error(void *ctx)
{
  (void)ctx;
  return hi2c2.ErrorCode;
}

const lps22hh_bus_t lps22hh_stm32_bus = {
  .ctx        = NULL,
  .mem_read   = hal_read,
  .mem_write  = hal_write,
  .recover    = hal_recover,
  .last_error = hal_last_error,
};

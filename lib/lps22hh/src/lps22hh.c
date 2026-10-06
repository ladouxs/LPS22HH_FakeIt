#include "lps22hh.h"

#include <stdio.h>

static int clamp_len(int len, size_t size)
{
  if (len < 0) return 0;
  if ((size_t)len >= size) return (int)size - 1;
  return len;
}

int lps22hh_step(lps22hh_t *dev, const lps22hh_bus_t *bus, char *msg, size_t size)
{
  int len;
  int st;

  if (!dev->ready)
  {
    uint8_t id = 0;
    st = bus->mem_read(bus->ctx, LPS22HH_WHO_AM_I, &id, 1);
    if (st == 0 && id == LPS22HH_ID)
    {
      uint8_t cfg = LPS22HH_CTRL1_10HZ_BDU;
      st = bus->mem_write(bus->ctx, LPS22HH_CTRL_REG1, &cfg, 1);
      dev->ready = (st == 0);
    }
    len = snprintf(msg, size, "init: st=%d err=0x%02lX id=0x%02X\r\n",
                   st, (unsigned long)bus->last_error(bus->ctx), id);
    if (!dev->ready) bus->recover(bus->ctx);
  }
  else
  {
    uint8_t buf[5];
    st = bus->mem_read(bus->ctx, LPS22HH_PRESS_OUT_XL, buf, 5);
    if (st == 0)
    {
      int32_t raw_p = ((int32_t)buf[2] << 16) | ((int32_t)buf[1] << 8) | buf[0];
      int16_t raw_t = (int16_t)(((uint16_t)buf[4] << 8) | buf[3]);
      int32_t p_c = (raw_p * 100) / 4096;   /* centi-hPa (4096 LSB/hPa) */
      int32_t t_c = raw_t;                  /* centi-°C  (100 LSB/°C)   */
      const char *sign = (t_c < 0) ? "-" : "";
      if (t_c < 0) t_c = -t_c;

      len = snprintf(msg, size, "P=%ld.%02ld hPa  T=%s%ld.%02ld C\r\n",
                     (long)(p_c / 100), (long)(p_c % 100),
                     sign, (long)(t_c / 100), (long)(t_c % 100));
    }
    else
    {
      len = snprintf(msg, size, "reading: st=%d err=0x%02lX\r\n",
                     st, (unsigned long)bus->last_error(bus->ctx));
      dev->ready = 0;
      bus->recover(bus->ctx);
    }
  }
  return clamp_len(len, size);
}

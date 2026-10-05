#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <fakeit.hpp>

#include <cstring>
#include <string>

extern "C" {
#include "lps22hh.h"
}

using namespace fakeit;

// --- Interface mockable by FakeIt (FakeIt only mocks virtual methods) ---
struct I2cBus {
  virtual ~I2cBus() = default;
  virtual int      mem_read(uint8_t reg, uint8_t *buf, uint16_t n) = 0;
  virtual int      mem_write(uint8_t reg, const uint8_t *buf, uint16_t n) = 0;
  virtual void     recover() = 0;
  virtual uint32_t last_error() = 0;
};

// C adapters -> mock
static int      t_read(void *c, uint8_t r, uint8_t *b, uint16_t n)       { return static_cast<I2cBus *>(c)->mem_read(r, b, n); }
static int      t_write(void *c, uint8_t r, const uint8_t *b, uint16_t n) { return static_cast<I2cBus *>(c)->mem_write(r, b, n); }
static void     t_recover(void *c)                                        { static_cast<I2cBus *>(c)->recover(); }
static uint32_t t_err(void *c)                                            { return static_cast<I2cBus *>(c)->last_error(); }

// --- Simulated sensor: register table + fault injection ---
constexpr uint32_t ERR_AF      = 0x04;  // HAL_I2C_ERROR_AF (NACK)
constexpr uint32_t ERR_TIMEOUT = 0x20;  // HAL_I2C_ERROR_TIMEOUT

struct FakeLps {
  uint8_t  regs[256] = {};
  uint32_t err = 0;
  bool     stuck = false;      // slave blocked: NACK until recover() is called
  int      fail_writes = 0;    // number of writes to fail
  int      timeout_reads = 0;  // number of reads to force to time out (st=3)

  FakeLps() { regs[LPS22HH_WHO_AM_I] = LPS22HH_ID; }

  void set_measure(uint32_t raw_p, int16_t raw_t) {
    regs[LPS22HH_PRESS_OUT_XL + 0] = raw_p & 0xFF;
    regs[LPS22HH_PRESS_OUT_XL + 1] = (raw_p >> 8) & 0xFF;
    regs[LPS22HH_PRESS_OUT_XL + 2] = (raw_p >> 16) & 0xFF;
    regs[LPS22HH_PRESS_OUT_XL + 3] = static_cast<uint16_t>(raw_t) & 0xFF;
    regs[LPS22HH_PRESS_OUT_XL + 4] = (static_cast<uint16_t>(raw_t) >> 8) & 0xFF;
  }
  int read(uint8_t reg, uint8_t *b, uint16_t n) {
    err = 0;
    if (timeout_reads > 0) { timeout_reads--; err = ERR_TIMEOUT; return 3; }
    if (stuck) { err = ERR_AF; return 1; }
    for (uint16_t i = 0; i < n; i++) b[i] = regs[(reg + i) & 0xFF];
    return 0;
  }
  int write(uint8_t reg, const uint8_t *b, uint16_t n) {
    err = 0;
    if (stuck || fail_writes > 0) { if (fail_writes > 0) fail_writes--; err = ERR_AF; return 1; }
    for (uint16_t i = 0; i < n; i++) regs[(reg + i) & 0xFF] = b[i];
    return 0;
  }
};

// --- Fixture: mocked bus, connected by default to the simulated sensor ---
struct Env {
  Mock<I2cBus> mock;
  FakeLps      lps;
  lps22hh_bus_t bus{};
  lps22hh_t     dev{};
  char          msg[96] = {};

  Env() {
    When(Method(mock, mem_read)).AlwaysDo([this](uint8_t r, uint8_t *b, uint16_t n) { return lps.read(r, b, n); });
    When(Method(mock, mem_write)).AlwaysDo([this](uint8_t r, const uint8_t *b, uint16_t n) { return lps.write(r, b, n); });
    When(Method(mock, last_error)).AlwaysDo([this]() { return lps.err; });
    When(Method(mock, recover)).AlwaysDo([this]() { lps.stuck = false; });
    bus = { &mock.get(), t_read, t_write, t_recover, t_err };
  }
  std::string step() {
    int len = lps22hh_step(&dev, &bus, msg, sizeof msg);
    return std::string(msg, static_cast<size_t>(len));
  }
};

TEST_CASE("init nominale") {
  Env e;
  CHECK(e.step() == "init: st=0 err=0x00 id=0xB3\r\n");
  CHECK(e.dev.ready == 1);
  CHECK(e.lps.regs[LPS22HH_CTRL_REG1] == 0x22);      // ODR 10 Hz + BDU
  Verify(Method(e.mock, mem_write).Using(LPS22HH_CTRL_REG1, _, 1)).Exactly(1);
  Verify(Method(e.mock, recover)).Exactly(0);
}

TEST_CASE("init : mauvais WHO_AM_I -> pas d'écriture, bus récupéré") {
  Env e;
  e.lps.regs[LPS22HH_WHO_AM_I] = 0x42;
  CHECK(e.step() == "init: st=0 err=0x00 id=0x42\r\n");
  CHECK(e.dev.ready == 0);
  Verify(Method(e.mock, mem_write)).Exactly(0);
  Verify(Method(e.mock, recover)).Exactly(1);
}

TEST_CASE("init : NACK sur WHO_AM_I") {
  Env e;
  e.lps.stuck = true;
  CHECK(e.step() == "init: st=1 err=0x04 id=0x00\r\n");
  CHECK(e.dev.ready == 0);
  Verify(Method(e.mock, recover)).Exactly(1);
}

TEST_CASE("init : échec de l'écriture CTRL_REG1") {
  Env e;
  e.lps.fail_writes = 1;
  CHECK(e.step() == "init: st=1 err=0x04 id=0xB3\r\n");
  CHECK(e.dev.ready == 0);
  Verify(Method(e.mock, recover)).Exactly(1);
}

TEST_CASE("lecture nominale : 1013.25 hPa, 23.45 C") {
  Env e;
  e.step();                                  // init
  e.lps.set_measure(4150272, 2345);          // 1013.25 * 4096 ; 23.45 * 100
  CHECK(e.step() == "P=1013.25 hPa  T=23.45 C\r\n");
  // burst read of 5 bytes starting from PRESS_OUT_XL
  Verify(Method(e.mock, mem_read).Using(LPS22HH_PRESS_OUT_XL, _, 5)).Exactly(1);
  Verify(Method(e.mock, recover)).Exactly(0);
}

TEST_CASE("température négative") {
  Env e;
  e.step();
  e.lps.set_measure(4150272, -507);
  CHECK(e.step() == "P=1013.25 hPa  T=-5.07 C\r\n");
}

TEST_CASE("température négative inférieure à 1 degré garde son signe") {
  Env e;
  e.step();
  e.lps.set_measure(4150272, -5);
  CHECK(e.step() == "P=1013.25 hPa  T=-0.05 C\r\n");
}

TEST_CASE("pression maximale 24 bits sans débordement") {
  Env e;
  e.step();
  e.lps.set_measure(0xFFFFFF, 0);
  CHECK(e.step() == "P=4095.99 hPa  T=0.00 C\r\n");
}

TEST_CASE("erreur de lecture : retour en init et récupération du bus") {
  Env e;
  e.step();
  e.lps.set_measure(4150272, 2345);
  REQUIRE(e.dev.ready == 1);

  e.lps.timeout_reads = 1;                

  // timeout on the next read only
  CHECK(e.step() == "lecture: st=3 err=0x20\r\n");
  CHECK(e.dev.ready == 0);
  Verify(Method(e.mock, recover)).Exactly(1);

  // the reset must succeed
  CHECK(e.step() == "init: st=0 err=0x00 id=0xB3\r\n");
  CHECK(e.dev.ready == 1);
}

TEST_CASE("scénario : esclave bloqué libéré par recover()") {
  Env e;
  e.lps.stuck = true;
  CHECK(e.step() == "init: st=1 err=0x04 id=0x00\r\n");   // blocked, recover() releases it
  CHECK(e.step() == "init: st=0 err=0x00 id=0xB3\r\n");   // initialization successful
  e.lps.set_measure(4150272, 2345);
  CHECK(e.step() == "P=1013.25 hPa  T=23.45 C\r\n");
  Verify(Method(e.mock, recover)).Exactly(1);
}

TEST_CASE("le message ne déborde jamais du buffer fourni") {
  Env e;
  char small[16];
  int len = lps22hh_step(&e.dev, &e.bus, small, sizeof small);
  CHECK(len == 15);
  CHECK(std::strlen(small) == 15);
}

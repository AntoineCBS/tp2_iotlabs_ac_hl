#include "sl_sensor_rht.h"
#include "stdint.h"
#include "app_assert.h"

sl_status_t tempfunc(int16_t *BLE_temp){

  uint32_t rh;
  int32_t t;

  sl_status_t sc = sl_sensor_rht_get(&rh, &t);
  app_assert_status(sc);

  *BLE_temp = t/10;

  return sc;
}

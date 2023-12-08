/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"
#include "sl_sensor_rht.h"
#include "temperature.h"
#include "gatt_db.h"
#define TEMPERATURE_TIMER_SIGNAL                (1<<0)


// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;

struct timer_struct {
  int connection;
  int characteristic;
};
/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.
  /////////////////////////////////////////////////////////////////////////////

  app_log_info("%s\n", __FUNCTION__);

}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}

/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/

void my_timer_function(sl_sleeptimer_timer_handle_t *handle, void *data){

  handle = handle;
  uint8_t * p = (uint8_t *) data;
  *p  = *(p) + 1;
  app_log_info("Timer step %d \n",  *p);
  sl_status_t sc = sl_bt_external_signal(TEMPERATURE_TIMER_SIGNAL);
  app_assert_status(sc);
}


void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;
  static struct timer_struct temperature_data;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("%s: connection_opened!\n", __FUNCTION__);
      sc = sl_sensor_rht_init();
      app_assert_status(sc);

      int16_t BLE_temp;
      sc = tempfunc(&BLE_temp);
      app_log_info("sensing at the first connection - result : %d\n", BLE_temp);
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("%s: connection_closed!\n", __FUNCTION__);
      sl_sensor_rht_deinit();

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_advertiser_connectable_scannable);
      app_assert_status(sc);
      break;



    case sl_bt_evt_gatt_server_user_read_request_id:   // If a read is asked to the chip
      if (evt->data.evt_gatt_server_user_read_request.characteristic == gattdb_temperature) // If the read characteristic is temperature
      {
          app_log_info("User requested the temperature !\n");

          int16_t BLE_temp;
          sc = tempfunc(&BLE_temp);
          app_assert_status(sc);
          app_log_info("Result : %d\n", BLE_temp);

          struct timer_struct local_temperature_data;
          local_temperature_data.connection = evt->data.evt_gatt_server_user_read_request.connection;
          local_temperature_data.characteristic = evt->data.evt_gatt_server_user_read_request.characteristic;
          uint8_t att_errorcode = 0;
          size_t value_len = sizeof(BLE_temp);
          uint16_t sent_len;

          sc = sl_bt_gatt_server_send_user_read_response(local_temperature_data.connection, local_temperature_data.characteristic, att_errorcode, value_len, (uint8_t *) &BLE_temp, &sent_len);
          app_assert_status(sc);
          app_log_info("Temperature sent\n");
      }
      break;


    case sl_bt_evt_gatt_server_characteristic_status_id :
      app_log_info("Button 'Notify' clicked \n");

      if (evt->data.evt_gatt_server_characteristic_status.characteristic == gattdb_temperature
            && evt->data.evt_gatt_server_characteristic_status.status_flags == sl_bt_gatt_server_client_config) // If the notify request is associated to a temperature
      {

          temperature_data.characteristic = evt->data.evt_gatt_server_characteristic_status.characteristic;
          temperature_data.connection = evt->data.evt_gatt_server_characteristic_status.connection;

          int config_flags =  evt->data.evt_gatt_server_characteristic_status.client_config_flags;
          app_log_info("Notify mode clicked for temperature and flag changed \n");
          app_log_info("Config notification flag %d\n", config_flags);
          static sl_sleeptimer_timer_handle_t handle;

          if (config_flags == 1 ) {
            static uint32_t timeout_ms = 1000;
            static uint8_t callback_data_iteration = 0; // We use the callback data as a counter of iterations
            static uint8_t priority = 0;
            static uint16_t option_flags = 0;

            sc = sl_sleeptimer_start_periodic_timer_ms(
                (sl_sleeptimer_timer_handle_t *) &handle, timeout_ms, my_timer_function, (uint8_t *) &callback_data_iteration, priority, option_flags);
            app_log_info("TIMER START DONE!\n");
            app_assert_status(sc);
          }
          else if (config_flags == 0){
            sc = sl_sleeptimer_stop_timer((sl_sleeptimer_timer_handle_t *) &handle);
            app_log_info("TIMER STOP DONE!\n");
            app_assert_status(sc);
          }
       }
      break;

    case sl_bt_evt_system_external_signal_id :
      if (evt->data.evt_system_external_signal.extsignals == 1){

          int16_t BLE_temp;
          sc = tempfunc(&BLE_temp);
          app_assert_status(sc);
          app_log_info("[Notify] Temp sensed : %d\n", BLE_temp);

          size_t value_len = sizeof(BLE_temp);

          sc = sl_bt_gatt_server_send_notification(temperature_data.connection, temperature_data.characteristic, value_len, (uint8_t *) &BLE_temp);
          app_assert_status(sc);
      }
      break;


    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////

    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}

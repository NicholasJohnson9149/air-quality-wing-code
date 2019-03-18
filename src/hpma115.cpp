#include "hpma115.h"

HPMA115::HPMA115(void){}

uint32_t HPMA115::setup(hpma115_init_t *p_init) {

  // Set up serial
  Serial1.begin(HPMA115_BAUD);

  // Set up callback
  this->callback = p_init->callback;

  // Set enable pin
  this->enable_pin = p_init->enable_pin;

  // Stop device
  this->disable();

  return HPMA115_SUCCESS;
}

uint32_t HPMA115::enable(){

  // TODO: set gpio high

  if( this->state == DISABLED ) {

    Serial1.flush();

    this->state = READY;
  }

  return HPMA115_SUCCESS;
}
uint32_t HPMA115::disable() {

  // TODO: set gpio low

  this->state = DISABLED;

  return HPMA115_SUCCESS;
}

void HPMA115::int_handler() {

  // If we are ready, change state
  if ( this->state == READY && Serial1.available() >= 32 ) {
    this->state = DATA_AVAILABLE;
  }

}

void HPMA115::process() {

    // First read
    while( this->state == DATA_AVAILABLE && Serial1.available() >= 2 ) {

      Serial1.readBytes(this->rx_buf,2);

      // Make sure two header bytes are correct
      if( this->rx_buf[0] == 0x42 && this->rx_buf[1] == 0x4d ) {

        // Wait for 30 bytes
        while( Serial1.available() < 30);

        // Then read
        Serial1.readBytes(this->rx_buf+2,30);

        // Change state
        this->state = DATA_READ;
      }

    }

    // Then process
    if (this->state == DATA_READ) {

      uint16_t calc_checksum = 0;

      // Calculate the checksum
      for( int i = 0; i < 31; i++ ) {
        calc_checksum += this->rx_buf[i];
      }

      // Un-serialize checksum from data
      uint16_t data_checksum = (this->rx_buf[30] << 8) + this->rx_buf[31];

      // Make sure the calculated and the provided are the same
      if ( calc_checksum != data_checksum ) {
        this->state = READY;
        return;
      }

      // Combine the serialized data
      this->data.pm25 = (this->rx_buf[6] << 8) + this->rx_buf[7];
      this->data.pm10 = (this->rx_buf[8] << 8) + this->rx_buf[9];

      // Callback
      this->callback(&this->data);

      // Set control pin low.
      this->disable();
    }
}
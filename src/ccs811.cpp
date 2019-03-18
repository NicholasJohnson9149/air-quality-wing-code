#include "ccs811.h"

CCS811::CCS811() {}

void CCS811::int_handler() {
  this->data_ready = true;
}

uint32_t CCS811::setup( ccs811_init_t * p_init ) {

  // Return an error if init is NULL
  if( p_init == NULL ) {
    return CCS811_NULL_ERROR;
  }

  // Configures the important stuff
  this->int_pin       = p_init->int_pin;
  this->address       = p_init->address;
  this->rst_pin       = p_init->rst_pin;
  this->pin_interrupt = p_init->pin_interrupt;
  this->wake_pin      = p_init->wake_pin;

  // Configure the pin interrupt
  pinMode(this->int_pin, INPUT);
  attachInterrupt(this->int_pin, this->pin_interrupt, FALLING);

  // Wake it up
  pinMode(this->wake_pin, OUTPUT);
  digitalWrite(this->wake_pin, LOW); // Has a pullup

  // Toggle reset pin
  pinMode(this->rst_pin, OUTPUT);
  digitalWrite(this->rst_pin, LOW);
  delay(1);
  pinMode(this->rst_pin, INPUT);
  delay(30);

  // Run the app
  Wire.beginTransmission(this->address);
  Wire.write(CCS811_START_APP); // sends register address
  Wire.endTransmission();  // stop transaction

  delay(30);

  Wire.beginTransmission(this->address);
  Wire.write(CCS811_STATUS_REG);
  Wire.endTransmission();  // stop transaction
  Wire.requestFrom(this->address,1);

  uint8_t status = Wire.read();

  // Checks if the app is running
  if( status & CCS811_FW_MODE_RUN ) {
    return CCS811_SUCCESS;
  } else {
    return CCS811_RUN_ERROR;
  }

}

uint32_t CCS811::set_env(float temp, float hum) {

  uint32_t err_code;

  // Shift left b/c this register is shifted left by 1
  uint16_t hum_conv = (uint16_t)hum << 1;

  // Get the fraction part (typecasting to remove int)
  float frac_part = temp - (uint16_t)temp;

  // Serial.printf("temp %.2f", temp);
  // Serial.printf("hum %.2f", hum);
  // Serial.printf("frac %.2f", frac_part);

  // Generate the calculated values
  uint16_t temp_high = (((uint16_t)temp + 25) << 9);
	uint16_t temp_low = ((uint16_t)(frac_part / (1.0/512)) & 0x1FF);
  uint16_t temp_conv = (temp_high | temp_low);

  // Data to send
  char data[] = { hum_conv, 0x00, temp_high, temp_low };

  // Write this
  Wire.beginTransmission(this->address);
  Wire.write(CCS811_ENV_REG);
  Wire.write(data);
  Wire.endTransmission();  // stop transaction

}

uint32_t CCS811::enable(void) {

  uint32_t err_code;
  char data[4];

  // Set mode to 10 sec mode & enable int
  Wire.beginTransmission(this->address);
  Wire.write(CCS811_MEAS_MODE_REG); // sends register address
  Wire.write(CCS811_CONSTANT_MODE | CCS811_INT_EN);     // enables consant mode with interrupt
  err_code = Wire.endTransmission();           // stop transaction
  if( err_code != 0 ){
    return err_code;
  }

  // Clear any interrupts
  Wire.beginTransmission(this->address);
  Wire.write(CCS811_RESULT_REG); // sends register address
  err_code = Wire.endTransmission(false);  // stop transaction
  if( err_code != 0 ){
    return err_code;
  }

  // Flush bytes
  Wire.requestFrom(this->address, 4); // Read the bytes
  while(Wire.available()) {
    Wire.read();
  }

  return CCS811_SUCCESS;

}

uint32_t CCS811::read(ccs811_data_t * p_data) {

  // If the data is ready, read
  if( this->data_ready ) {

      // Disable flag
      this->data_ready = false;

      Wire.beginTransmission(this->address);
      Wire.write(CCS811_RESULT_REG); // sends register address
      Wire.endTransmission();  // stop transaction
      Wire.requestFrom(this->address, 4); // request the bytes

      // Convert data to something useful
      p_data->c02 = Wire.read();
      p_data->c02 = (p_data->c02<<8) + Wire.read();

      p_data->tvoc = Wire.read();
      p_data->tvoc = (p_data->tvoc<<8) + Wire.read();

      // Serial.printf("c02: %dppm tvoc: %dppb\n",p_data->c02,p_data->tvoc);

      // If this value is < 400 not ready yet
      if ( p_data->c02 <= CCS811_MIN_C02_LEVEL ) {
        return CCS811_NO_DAT_AVAIL;
      }

      return CCS811_SUCCESS;
  } else {
      return CCS811_NO_DAT_AVAIL;
  }

}
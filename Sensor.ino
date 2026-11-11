// ESP32 + INMP441 I2S microphone -> decibel level in the Serial Monitor
//
// Wiring:
//   INMP441 VDD -> 3V3     INMP441 GND -> GND     INMP441 L/R -> GND
//   INMP441 SCK -> GPIO 26 INMP441 WS  -> GPIO 25 INMP441 SD  -> GPIO 33

#include <driver/i2s.h>
#include <math.h>

#define I2S_PORT I2S_NUM_0

const int PIN_SCK = 23;   // bit clock
const int PIN_WS  = 4;   // word select (left/right clock)
const int PIN_SD  = 2;   // serial data from the mic

const int SAMPLE_RATE = 16000;
const int BLOCK_SIZE  = 1024;   // samples per reading (about 64 ms)

// The INMP441 outputs -26 dBFS at 94 dB SPL, so dB SPL = dBFS + 94 + 26.
// Adjust this number if you compare against a sound meter and it disagrees.
const float CAL_OFFSET_DB = 120.0;

int32_t samples[BLOCK_SIZE];

void setupI2S() {
  i2s_config_t config;
  memset(&config, 0, sizeof(config));
  config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;   // L/R pin tied to GND
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = 256;
  config.use_apll = false;

  i2s_pin_config_t pins;
  memset(&pins, 0, sizeof(pins));
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_SCK;
  pins.ws_io_num = PIN_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_SD;

  i2s_driver_install(I2S_PORT, &config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pins);
  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  setupI2S();
  Serial.println("Microphone ready. Make some noise!");
}

void loop() {
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
  int count = bytesRead / sizeof(int32_t);
  if (count == 0) return;

  // The mic sends 24-bit audio in the top bits of each 32-bit word
  double sum = 0;
  for (int i = 0; i < count; i++) {
    samples[i] >>= 8;
    sum += samples[i];
  }
  double mean = sum / count;   // remove the DC offset

  double sumSq = 0;
  for (int i = 0; i < count; i++) {
    double s = samples[i] - mean;
    sumSq += s * s;
  }
  double rms = sqrt(sumSq / count);

  // 24-bit full scale is 2^23 = 8388608
  double dbfs = 20.0 * log10(fmax(rms, 1.0) / 8388608.0);
  double db = dbfs + CAL_OFFSET_DB;

  // Simple text bar: one # per 2 dB above 30 dB
  int bars = constrain((int)((db - 30) / 2), 0, 45);
  char bar[50];
  memset(bar, '#', bars);
  bar[bars] = '\0';

  Serial.printf("%5.1f dB  %s\n", db, bar);
}
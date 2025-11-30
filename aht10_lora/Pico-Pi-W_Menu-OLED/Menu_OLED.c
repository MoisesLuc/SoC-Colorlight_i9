#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ssd1306.h"
#include "hardware/spi.h"
#include <string.h>
#include <stdint.h>
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"

//pinos e módulos controlador i2c selecionado
#define I2C_PORT i2c1
#define PINO_SCL 14
#define PINO_SDA 15

ssd1306_t disp;

// ================= LoRa/SX1276 (RFM95) SPI Pins =================
#define PIN_MISO 16
#define PIN_CS   17
#define PIN_SCK  18
#define PIN_MOSI 19
#define PIN_DIO0 28
#define PIN_RST  20

// ================= LoRa Registers and Modes =====================
#define REG_FIFO                 0x00
#define REG_OP_MODE              0x01
#define REG_FRF_MSB              0x06
#define REG_FRF_MID              0x07
#define REG_FRF_LSB              0x08
#define REG_PA_CONFIG            0x09
#define REG_OCP                  0x0B
#define REG_LNA                  0x0C
#define REG_FIFO_ADDR_PTR        0x0D
#define REG_FIFO_RX_BASE_ADDR    0x0F
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS            0x12
#define REG_RX_NB_BYTES          0x13
#define REG_MODEM_CONFIG_1       0x1D
#define REG_MODEM_CONFIG_2       0x1E
#define REG_PREAMBLE_MSB         0x20
#define REG_PREAMBLE_LSB         0x21
#define REG_MODEM_CONFIG_3       0x26
#define REG_SYNC_WORD            0x39
#define REG_DIO_MAPPING_1        0x40
#define REG_VERSION              0x42
#define REG_PA_DAC               0x4D

#define LORA_FLAG                0x80
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_RX_CONTINUOUS       0x05

#define IRQ_RX_DONE              0x40
#define IRQ_CRC_ERR              0x20

// Pacote de telemetria (igual ao do SoC)
#pragma pack(push,1)
typedef struct {
    uint8_t  ver;
    uint16_t seq;
    int16_t  temp_c_centi;
    uint16_t hum_x100;
    uint8_t  flags;
    uint8_t  crc8;
} lora_env_payload_t;
#pragma pack(pop)

static inline uint8_t crc8_sum(const uint8_t* p, size_t n) {
    uint32_t s = 0; for (size_t i=0;i<n;i++) s+=p[i]; return (uint8_t)(s & 0xFF);
}

// ================= LoRa SPI helpers =============================
static inline void cs_select()   { gpio_put(PIN_CS, 0);  __asm volatile("nop \n nop"); }
static inline void cs_deselect() { gpio_put(PIN_CS, 1);  __asm volatile("nop \n nop"); }

static uint8_t spi_txrx(uint8_t b) {
    uint8_t rx; spi_write_read_blocking(spi0, &b, &rx, 1); return rx;
}

static uint8_t rfm_read(uint8_t reg) {
    cs_select(); spi_txrx(reg & 0x7F); uint8_t v = spi_txrx(0x00); cs_deselect(); return v;
}
static void rfm_write(uint8_t reg, uint8_t val) {
    cs_select(); spi_txrx(reg | 0x80); spi_txrx(val); cs_deselect();
}
static void rfm_burst_read(uint8_t reg, uint8_t* buf, uint8_t len) {
    cs_select(); spi_txrx(reg & 0x7F); for (uint8_t i=0;i<len;i++) buf[i]=spi_txrx(0x00); cs_deselect();
}
static inline void lora_set_mode(uint8_t mode) { rfm_write(REG_OP_MODE, LORA_FLAG | mode); }

static bool lora_initialized = false;
static bool lora_init(void) {
    if (lora_initialized) return true;
    // Configure SPI0 and pins
    spi_init(spi0, 1*1000*1000);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS, GPIO_OUT);  gpio_put(PIN_CS, 1);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT); gpio_put(PIN_RST, 1);
    gpio_init(PIN_DIO0); gpio_set_dir(PIN_DIO0, GPIO_IN);

    // Reset
    gpio_put(PIN_RST, 0); sleep_ms(5);
    gpio_put(PIN_RST, 1); sleep_ms(10);

    uint8_t v = rfm_read(REG_VERSION);
    if (v != 0x12) {
        printf("RFM95 version mismatch: 0x%02x\n", v);
        return false;
    }

    lora_set_mode(MODE_SLEEP);

    // 915 MHz
    uint64_t frf = ((uint64_t)915000000 << 19) / 32000000;
    rfm_write(REG_FRF_MSB, (uint8_t)(frf >> 16));
    rfm_write(REG_FRF_MID, (uint8_t)(frf >> 8));
    rfm_write(REG_FRF_LSB, (uint8_t)(frf >> 0));

    // Match TX params from SoC
    rfm_write(REG_PA_CONFIG, 0xFF);
    rfm_write(REG_PA_DAC,    0x87);
    rfm_write(REG_MODEM_CONFIG_1, 0x78); // BW=62.5k, CR=4/8, explicit
    rfm_write(REG_MODEM_CONFIG_2, 0xC4); // SF12, CRC on
    rfm_write(REG_MODEM_CONFIG_3, 0x0C); // LowDataRateOptimize+AGC
    rfm_write(REG_PREAMBLE_MSB, 0x00);
    rfm_write(REG_PREAMBLE_LSB, 0x0C);
    rfm_write(REG_SYNC_WORD,    0x12);
    rfm_write(REG_OCP,          0x37);
    rfm_write(REG_LNA,          0x23);

    rfm_write(REG_FIFO_RX_BASE_ADDR, 0x00);
    rfm_write(REG_FIFO_ADDR_PTR,     0x00);
    rfm_write(REG_DIO_MAPPING_1, 0x00); // DIO0 = RxDone
    rfm_write(REG_IRQ_FLAGS, 0xFF);
    lora_set_mode(MODE_RX_CONTINUOUS);
    lora_initialized = true;
    return true;
}

static void lora_handle_rx_once(void) {
    uint8_t irq = rfm_read(REG_IRQ_FLAGS);
    if (!(irq & IRQ_RX_DONE)) return;
    if (irq & IRQ_CRC_ERR) {
        rfm_write(REG_IRQ_FLAGS, 0xFF);
        return;
    }
    uint8_t cur = rfm_read(REG_FIFO_RX_CURRENT_ADDR);
    uint8_t nb  = rfm_read(REG_RX_NB_BYTES);
    rfm_write(REG_FIFO_ADDR_PTR, cur);
    uint8_t buf[64]; if (nb > sizeof(buf)) nb = sizeof(buf);
    rfm_burst_read(REG_FIFO, buf, nb);
    rfm_write(REG_IRQ_FLAGS, 0xFF);

    if (nb < sizeof(lora_env_payload_t)) return;
    lora_env_payload_t pl; memcpy(&pl, buf, sizeof(pl));
    uint8_t expect = crc8_sum((uint8_t*)&pl, sizeof(pl)-1);
    if (expect != pl.crc8 || pl.ver != 1) return;

    int t_int = pl.temp_c_centi / 100;
    int t_dec = pl.temp_c_centi % 100; if (t_dec < 0) t_dec = -t_dec;
    int h_int = pl.hum_x100 / 100;
    int h_dec = pl.hum_x100 % 100;

    // Update OLED content
    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "LoRa RX");
    char l1[24], l2[24], l3[24];
    snprintf(l1, sizeof(l1), "seq=%u", (unsigned)pl.seq);
    snprintf(l2, sizeof(l2), "T=%d.%02dC", t_int, t_dec);
    snprintf(l3, sizeof(l3), "H=%d.%02d%%", h_int, h_dec);
    ssd1306_draw_string(&disp, 0, 16, 2, l2);
    ssd1306_draw_string(&disp, 0, 38, 2, l3);
    ssd1306_draw_string(&disp, 90, 0, 1, l1);
    ssd1306_show(&disp);
}

//função para inicialização de todos os recursos do sistema
void inicializa() {
    stdio_init_all();
    adc_init();
    i2c_init(I2C_PORT, 400*1000);// I2C Inicialização. Usando 400Khz.
    gpio_set_function(PINO_SCL, GPIO_FUNC_I2C);
    gpio_set_function(PINO_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(PINO_SCL);
    gpio_pull_up(PINO_SDA);
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, I2C_PORT);
}

//função escrita no display.
void print_texto(char *msg, uint pos_x, uint pos_y, uint scale){
    ssd1306_draw_string(&disp, pos_x, pos_y, scale, msg); //desenha texto
    ssd1306_show(&disp); //apresenta no Oled
}

//o desenho do retangulo fará o papel de seletor
void print_retangulo(int x1, int y1, int x2, int y2){
    ssd1306_draw_empty_square(&disp,x1,y1,x2,y2);
    ssd1306_show(&disp);
}



int main() {
    inicializa();
   
    if (!lora_init()) {
            ssd1306_clear(&disp);
            ssd1306_draw_string(&disp, 0, 0, 1, "LoRa init FAIL");
            ssd1306_show(&disp);
            sleep_ms(1500);
        }

    ssd1306_clear(&disp);
    ssd1306_draw_string(&disp, 0, 0, 1, "LoRa RX Listening...");
    ssd1306_draw_string(&disp, 0, 16, 1, "Aguarde pacote...");
    ssd1306_show(&disp);

    while(true) {
        lora_handle_rx_once();
        sleep_ms(10);
    }

return 0;

}

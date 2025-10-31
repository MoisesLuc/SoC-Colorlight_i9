#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdint.h>

#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>
#include <stdbool.h>
#include "../libs/lora_RFM95.h"

static void ahtx0_soft_reset(void);
static int ahtx0_read_status(uint8_t* status);

// -------------------- I2C bit-bang helpers (LiteX CSR) --------------------
#define I2C_ADDR_AHT10 0x38
#define I2C_DELAY 300  // Ajuste conforme necessário (bus mais lento = valores maiores)
// Variante detectada: 0=desconhecida, 10=AHT10, 20=AHT20
static int g_aht_variant = 0;

// Escreve os sinais no registrador W:
// scl=1 -> solta SCL (pull-up). scl=0 -> força LOW.
// oe=1 ativa driver de SDA, oe=0 solta SDA.
// sda=1 com oe=1 solta SDA (open-drain), sda=0 com oe=1 força LOW.
static inline void i2c_write_lines(int scl, int oe, int sda) {
    uint32_t w = 0;
    if (scl) w |= (1u << CSR_I2C_W_SCL_OFFSET);
    if (oe)  w |= (1u << CSR_I2C_W_OE_OFFSET);
    if (sda) w |= (1u << CSR_I2C_W_SDA_OFFSET);
    i2c_w_write(w);
}

static inline int i2c_read_sda(void) {
    return (i2c_r_read() >> CSR_I2C_R_SDA_OFFSET) & 1;
}

static inline void i2c_delay(void) {
    for (volatile int i = 0; i < I2C_DELAY; i++) { __asm__ __volatile__("" ::: "memory"); }
}

// Coloca o barramento em idle (SCL=1, SDA solta)
static void i2c_idle(void) {
    i2c_write_lines(1, 0, 1); // SCL=1, SDA solta
    i2c_delay();
}

// START: durante SCL=1, puxa SDA para LOW
static void i2c_start(void) {
    i2c_idle();
    // SDA low while SCL high
    i2c_write_lines(1, 1, 0); // drive SDA=0, SCL solto
    i2c_delay();
    // Baixa SCL para iniciar ciclo de bits
    i2c_write_lines(0, 1, 0);
    i2c_delay();
}

// STOP: com SCL=1, libera SDA para HIGH
static void i2c_stop(void) {
    // Garante SCL baixo
    i2c_write_lines(0, 1, 0);
    i2c_delay();
    // Solta SDA (alto) com SCL ainda baixo
    i2c_write_lines(0, 0, 1);
    i2c_delay();
    // Sobe SCL
    i2c_write_lines(1, 0, 1);
    i2c_delay();
    // STOP: libera SDA enquanto SCL alto
    i2c_write_lines(1, 0, 1);
    i2c_delay();
}

// Repeated-START (igual ao START, sem STOP antes)
static void i2c_restart(void) {
    // SCL alto, SDA alto -> puxa SDA baixo mantendo SCL alto
    i2c_write_lines(1, 0, 1);
    i2c_delay();
    i2c_write_lines(1, 1, 0);
    i2c_delay();
    i2c_write_lines(0, 1, 0);
    i2c_delay();
}

// Envia 1 bit (MSB-first na camada superior)
static void i2c_write_bit(int bit) {
    // Coloca SDA conforme bit (0 = drive LOW, 1 = solta)
    if (bit) {
        i2c_write_lines(0, 1, 1); // SDA "alto" (open-drain)
    } else {
        i2c_write_lines(0, 1, 0); // SDA LOW
    }
    i2c_delay();
    // Clock high
    i2c_write_lines(1, 1, bit ? 1 : 0);
    i2c_delay();
    // Clock low
    i2c_write_lines(0, 1, bit ? 1 : 0);
    i2c_delay();
}

// Lê 1 bit
static int i2c_read_bit(void) {
    int bit;
    // Libera SDA (entrada)
    i2c_write_lines(0, 0, 1);
    i2c_delay();
    // Clock high e amostra SDA
    i2c_write_lines(1, 0, 1);
    i2c_delay();
    bit = i2c_read_sda();
    // Clock low
    i2c_write_lines(0, 0, 1);
    i2c_delay();
    return bit;
}

// Escreve 1 byte. Retorna 0 se ACK, -1 se NACK.
static int i2c_write_byte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        i2c_write_bit((byte >> i) & 1);
    }
    // Lê ACK (bit 9): 0=ACK, 1=NACK
    int ack = i2c_read_bit();
    return ack ? -1 : 0;
}

// Lê 1 byte. ack=1 envia ACK ao slave; ack=0 envia NACK.
static uint8_t i2c_read_byte(int ack) {
    uint8_t val = 0;
    for (int i = 7; i >= 0; i--) {
        int b = i2c_read_bit();
        val |= (b & 1) << i;
    }
    // Bit ACK do master: 0=ACK, 1=NACK
    i2c_write_bit(ack ? 0 : 1);
    return val;
}

// -------------------- AHT10/AHT20 (Temp/Umid) --------------------
// Tenta AHT10 (0xE1); se NACK, tenta AHT20 (0xBE).
static int aht10_init(void) {
    // aguarda power-up (>20ms)
    for (int i = 0; i < 20000; i++) i2c_delay();

    ahtx0_soft_reset();

    // Tenta sequência AHT10 (0xE1 0x08 0x00)
    i2c_start();
    if (i2c_write_byte((I2C_ADDR_AHT10 << 1) | 0x00) != 0) { i2c_stop(); return -1; }
    if (i2c_write_byte(0xE1) != 0) {
        // Provavelmente AHT20: tenta 0xBE 0x08 0x00
        i2c_stop();

        i2c_start();
        if (i2c_write_byte((I2C_ADDR_AHT10 << 1) | 0x00) != 0) { i2c_stop(); return -1; }
        if (i2c_write_byte(0xBE) != 0) { i2c_stop(); return -2; }     // init error (AHT20)
        if (i2c_write_byte(0x08) != 0) { i2c_stop(); return -3; }
        if (i2c_write_byte(0x00) != 0) { i2c_stop(); return -4; }
        i2c_stop();
        g_aht_variant = 20;
    } else {
        if (i2c_write_byte(0x08) != 0) { i2c_stop(); return -3; }
        if (i2c_write_byte(0x00) != 0) { i2c_stop(); return -4; }
        i2c_stop();
        g_aht_variant = 10;
    }

    // Espera pós-init (~10ms)
    for (int i = 0; i < 8000; i++) i2c_delay();

    // Checa status (opcional)
    uint8_t st = 0;
    if (ahtx0_read_status(&st) == 0) {
        // bit7 busy; bit3 calib enable (datasheet AHT20)
        // Apenas imprime; não falha init se calib não setado.
        printf("AHT status: 0x%02x (var=%d)\n", st, g_aht_variant);
    }
    return 0;
}

static int aht10_trigger_measurement(void) {
    // Trigger: 0xAC, 0x33, 0x00 (AHT10/AHT20)
    i2c_start();
    if (i2c_write_byte((I2C_ADDR_AHT10 << 1) | 0x00) != 0) { i2c_stop(); return -1; }
    if (i2c_write_byte(0xAC) != 0) { i2c_stop(); return -2; }
    if (i2c_write_byte(0x33) != 0) { i2c_stop(); return -3; }
    if (i2c_write_byte(0x00) != 0) { i2c_stop(); return -4; }
    i2c_stop();
    return 0;
}

// Lê medição. Retorna 0 ok; temp_c_centi em centésimos de °C; hum_x100 em centésimos de %.
static int aht10_read_measurement(int32_t* temp_c_centi, uint32_t* hum_x100) {
    uint8_t d[6];

    // AHT10 precisa ~75-80ms para conversão. Espera simples:
    for (int i = 0; i < 40000; i++) i2c_delay(); // ~80ms (ajuste conforme CPU)

    i2c_start();
    if (i2c_write_byte((I2C_ADDR_AHT10 << 1) | 0x01) != 0) { i2c_stop(); return -1; }

    // Lê 6 bytes (ACK nos 5 primeiros, NACK no último)
    for (int i = 0; i < 5; i++) d[i] = i2c_read_byte(1);
    d[5] = i2c_read_byte(0);
    i2c_stop();

    // status em d[0] (bit7 = busy). Se ainda ocupado, falha temporária.
    if (d[0] & 0x80) return -2;

    // Monta valores brutos (20 bits)
    uint32_t raw_rh = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | ((uint32_t)(d[3] >> 4) & 0x0F);
    uint32_t raw_t  = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | (uint32_t)d[5];

    // Converte para centésimos sem float:
    // RH[%] = raw_rh * 100 / 2^20
    // T[C]  = raw_t  * 200 / 2^20 - 50
    // Em centésimos: RH_x100 = raw_rh * 10000 / 2^20
    //                T_x100  = raw_t  * 20000 / 2^20 - 5000
    uint64_t rh_num = (uint64_t)raw_rh * 10000ull;
    uint64_t t_num  = (uint64_t)raw_t  * 20000ull;

    *hum_x100      = (uint32_t)(rh_num / 1048576ull);
    *temp_c_centi  = (int32_t)(t_num / 1048576ull) - 5000;

    // Satura umidade entre 0..10000 (0..100.00%)
    if (*hum_x100 > 10000u) *hum_x100 = 10000u;

    return 0;
}

// Soft reset comum (AHT10/AHT20): 0xBA
static void ahtx0_soft_reset(void) {
    i2c_start();
    (void)i2c_write_byte((I2C_ADDR_AHT10 << 1) | 0x00);
    (void)i2c_write_byte(0xBA);
    i2c_stop();
    // aguarda ~20ms
    for (int i = 0; i < 12000; i++) i2c_delay();
}

// Lê 1 byte de status via comando 0x71 (Repeated-START)
static int ahtx0_read_status(uint8_t* status) {
    i2c_start();
    if (i2c_write_byte((I2C_ADDR_AHT10 << 1) | 0x00) != 0) { i2c_stop(); return -1; }
    if (i2c_write_byte(0x71) != 0) { i2c_stop(); return -2; }
    i2c_restart();
    if (i2c_write_byte((I2C_ADDR_AHT10 << 1) | 0x01) != 0) { i2c_stop(); return -3; }
    *status = i2c_read_byte(0); // NACK no último
    i2c_stop();
    return 0;
}

// Scan simples do barramento I2C (0x08..0x77)
static void i2c_scan(void) {
    puts("I2C scan:");
    for (int addr = 0x08; addr <= 0x77; addr++) {
        i2c_start();
        int ok = (i2c_write_byte((addr << 1) | 0x00) == 0);
        i2c_stop();
        if (ok) {
            printf(" - Found 0x%02x\n", addr);
        }
    }
}

// -------------------- CLI existente + novos comandos --------------------

static char *readstr(void)
{
    char c[2];
    static char s[64];
    static int ptr = 0;

    if(readchar_nonblock()) {
        c[0] = readchar();
        c[1] = 0;
        switch(c[0]) {
            case 0x7f:
            case 0x08:
                if(ptr > 0) {
                    ptr--;
                    putsnonl("\x08 \x08");
                }
                break;
            case 0x07:
                break;
            case '\r':
            case '\n':
                s[ptr] = 0x00;
                putsnonl("\n");
                ptr = 0;
                return s;
            default:
                if(ptr >= (sizeof(s) - 1))
                    break;
                putsnonl(c);
                s[ptr] = c[0];
                ptr++;
                break;
        }
    }

    return NULL;
}

static char *get_token(char **str)
{
    char *c, *d;

    c = (char *)strchr(*str, ' ');
    if(c == NULL) {
        d = *str;
        *str = *str+strlen(*str);
        return d;
    }
    *c = 0;
    d = *str;
    *str = c+1;
    return d;
}

static void prompt(void)
{
    printf("RUNTIME>");
}

static void help(void)
{
    puts("Available commands:");
    puts("help                            - this command");
    puts("reboot                          - reboot CPU");
    puts("led                             - led test");
    puts("i2cscan                         - scan I2C bus");
    puts("ahtinit                         - init/calibrate AHT10/AHT20");
    puts("ahtstatus                       - read AHT status");
    puts("aht                             - measure + read AHT10/AHT20");
    puts("lorainit                        - init LoRa RFM95");
    puts("loratx                          - read sensor and TX via LoRa");
}

static void reboot(void)
{
    ctrl_reset_write(1);
}

static void toggle_led(void)
{
    int i;
    printf("invertendo led...\n");
    i = leds_out_read();
    leds_out_write(!i);
}

static void cmd_ahtinit(void) {
    int r = aht10_init();
    if (r == 0) puts("AHT10 init OK");
    else        printf("AHT10 init error %d\n", r);
}

static void cmd_aht_read(void) {
    int r = aht10_trigger_measurement();
    if (r != 0) {
        printf("AHT trigger error %d\n", r);
        return;
    }
    int32_t t_c_centi;
    uint32_t h_x100;
    r = aht10_read_measurement(&t_c_centi, &h_x100);
    if (r != 0) {
        printf("AHT read error %d\n", r);
        return;
    }
    int t_int = t_c_centi / 100;
    int t_dec = t_c_centi % 100; if (t_dec < 0) t_dec = -t_dec;
    int h_int = h_x100 / 100;
    int h_dec = h_x100 % 100;
    printf("AHT(var=%d): Temp=%d.%02d C  Hum=%d.%02d %%\n", g_aht_variant, t_int, t_dec, h_int, h_dec);
}

static void cmd_ahtstatus(void) {
    uint8_t st=0;
    int r = ahtx0_read_status(&st);
    if (r != 0) {
        printf("AHT status err %d\n", r);
    } else {
        printf("AHT status=0x%02x (busy=%d, cal=%d)\n", st, (st>>7)&1, (st>>3)&1);
    }
}

static void cmd_i2cscan(void) {
    i2c_scan();
}

static void cmd_lorainit(void) {
    if (lora_init()) puts("LoRa init OK");
    else             puts("LoRa init FAIL");
}

static void cmd_loratx(void) {
    // Gera uma leitura e envia via LoRa
    int r = aht10_trigger_measurement();
    if (r != 0) { printf("AHT trigger error %d\n", r); return; }

    int32_t t_c_centi;
    uint32_t h_x100;
    r = aht10_read_measurement(&t_c_centi, &h_x100);
    if (r != 0) { printf("AHT read error %d\n", r); return; }

    uint8_t payload[sizeof(lora_env_payload_t)];
    size_t len = build_env_payload(payload, t_c_centi, h_x100);

    bool ok = lora_send_bytes(payload, len);
    if (!ok) {
        puts("LoRa TX FAIL");
        return;
    }
    int t_int = t_c_centi / 100;
    int t_dec = t_c_centi % 100; if (t_dec < 0) t_dec = -t_dec;
    int h_int = h_x100 / 100;
    int h_dec = h_x100 % 100;
    printf("LoRa TX OK: seq=%u T=%d.%02dC H=%d.%02d%%\n", (unsigned)(g_lora_seq-1), t_int, t_dec, h_int, h_dec);
}

static void console_service(void)
{
    char *str;
    char *token;

    str = readstr();
    if(str == NULL) return;
    token = get_token(&str);
    if(strcmp(token, "help") == 0)
        help();
    else if(strcmp(token, "reboot") == 0)
        reboot();
    else if(strcmp(token, "led") == 0)
        toggle_led();
    else if(strcmp(token, "i2cscan") == 0)
        cmd_i2cscan();
    else if(strcmp(token, "ahtinit") == 0)
        cmd_ahtinit();
    else if(strcmp(token, "aht") == 0)
        cmd_aht_read();
    else if(strcmp(token, "ahtstatus") == 0)
        cmd_ahtstatus();
    else if(strcmp(token, "lorainit") == 0)
        cmd_lorainit();
    else if(strcmp(token, "loratx") == 0)
        cmd_loratx();
    prompt();
}

int main(void)
{
#ifdef CONFIG_CPU_HAS_INTERRUPT
    irq_setmask(0);
    irq_setie(1);
#endif
    uart_init();

    puts("\nLab004 - CPU testing software built "__DATE__" "__TIME__"\n");
    printf("Hellorld!\n");
    help();
    prompt();

    // Garante barramento I2C em estado idle
    i2c_idle();

    while(1) {
        console_service();
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>

// Definição de funções
static int i2c_write_byte(unsigned char byte);
static unsigned char i2c_read_byte(int send_ack);

#ifndef CONFIG_CLOCK_FREQUENCY
#define CONFIG_CLOCK_FREQUENCY 60000000
#endif

// Bits do CSR (bit 0 = SCL, bit 1 = SDA)
#define I2C_SCL 0x01
#define I2C_SDA 0x02

// Shadow das saídas open-drain
static uint8_t i2c_out = I2C_SCL | I2C_SDA;

static inline void i2c_apply(void) {
	i2c_w_write(i2c_out & (I2C_SCL | I2C_SDA));
}

// static inline void i2c_set_lines(unsigned int mask) {
// 	// Cada escrita substitui SCL/SDA
// 	i2c_w_write(mask & (I2C_SCL | I2C_SDA));
// }

static inline unsigned int i2c_get_lines(void) {
	// Cada leitura retorna o estado atual de SCL/SDA
	return i2c_r_read() & (I2C_SCL | I2C_SDA);
}

static inline void i2c_delay_halfcycle(void) {
	// ~5us por metade do ciclo
	//volatile int n = CONFIG_CLOCK_FREQUENCY / 200000;
	volatile int n = CONFIG_CLOCK_FREQUENCY / 20000; // Para depuração
	while (n--) __asm__ volatile("");
}

static inline void i2c_scl_high(void) { i2c_out |= I2C_SCL; i2c_apply(); }
static inline void i2c_scl_low(void)  { i2c_out &= ~I2C_SCL; i2c_apply(); }
static inline void i2c_sda_high(void) { i2c_out |= I2C_SDA; i2c_apply(); }
static inline void i2c_sda_low(void)  { i2c_out &= ~I2C_SDA; i2c_apply(); }

static int i2c_wait_high(uint8_t mask) {
    // Aguarda a(s) linha(s) subir(em), detecta falta de pull-up
    for (int i = 0; i < 1000; ++i) { // ~50 ms total
        if ((i2c_get_lines() & mask) == mask) return 0;
        i2c_delay_halfcycle();
    }
    return -1;
}

static void i2c_bus_idle(void) {
	i2c_sda_high();
	i2c_scl_high();
	i2c_wait_high(I2C_SCL | I2C_SDA);
	i2c_delay_halfcycle();
}

static void i2c_start(void) {
	// SDA alto, depois SCL alto, depois aguarda a transição SDA alto->baixo com SCL alto
	i2c_sda_high(); i2c_scl_high(); i2c_delay_halfcycle();
	i2c_sda_low(); i2c_delay_halfcycle();
	i2c_scl_low(); i2c_delay_halfcycle();
}

static void i2c_stop(void) {
	// SDA baixo, depois SCL alto, depois aguarda a transição SDA baixo->alto com SCL alto
	i2c_sda_low(); i2c_delay_halfcycle();
	i2c_scl_high(); i2c_delay_halfcycle();
	i2c_sda_high(); i2c_delay_halfcycle();
}

static int i2c_write_bit(int bit) { 
	if (bit) i2c_sda_high(); else i2c_sda_low();
    i2c_delay_halfcycle();
    i2c_scl_high();
    if (i2c_wait_high(I2C_SCL) != 0) return -10; // SCL não sobe
    i2c_delay_halfcycle();
    i2c_scl_low();
    i2c_delay_halfcycle();
    return 0;
}

static int i2c_probe(uint8_t addr7) {
	i2c_bus_idle();
	i2c_start();
	int r = i2c_write_byte((addr7 << 1) | 0x00); // Write
	i2c_stop();
	return r;
}

static int i2c_read_bit(void) {
	// Libera SDA e amostra com SCL alto
    i2c_sda_high();
    i2c_delay_halfcycle();
    i2c_scl_high();
    if (i2c_wait_high(I2C_SCL) != 0) return 0; // trata como 0 se SCL não sobe
    i2c_delay_halfcycle();
    int bit = (i2c_get_lines() & I2C_SDA) ? 1 : 0;
    i2c_scl_low();
    i2c_delay_halfcycle();
    return bit;
}

// Quick check: barramento deve ficar em 1/1 quando liberado
static int i2c_bus_test(void) {
    i2c_sda_high();
    i2c_scl_high();
    i2c_delay_halfcycle();
    unsigned v = i2c_get_lines();
    return (v & (I2C_SCL | I2C_SDA)) == (I2C_SCL | I2C_SDA) ? 0 : -1;
}

// Comando de diagnóstico
static void cmd_i2c_check(void) {
    int r = i2c_bus_test();
    unsigned v = i2c_get_lines();
    printf("I2C lines: SCL=%d SDA=%d -> %s\n",
        (v & I2C_SCL) != 0, (v & I2C_SDA) != 0,
        (r == 0) ? "OK (pull-ups presentes)" : "ERRO (linhas não sobem)");
}

static int i2c_write_byte(unsigned char byte) { 
	for (int i = 7; i >= 0; --i) 
		i2c_write_bit((byte >> i) & 1); 
	// ACK: slave puxa SDA para baixo (0) durante o 9º clock 
	int ack = (i2c_read_bit() == 0); 
	return ack ? 0 : -1; 
}

static unsigned char i2c_read_byte(int send_ack) {
	unsigned char val = 0; 
	for (int i = 7; i >= 0; --i) { 
		int b = i2c_read_bit(); 
		val |= (b & 1) << i; 
	} 
	// Envia ACK (0) ou NACK (1) 
	i2c_write_bit(send_ack ? 0 : 1); 
	return val; 
}

static void delay_ms(unsigned int ms) { 
	// AHT10 precisa de ~80 ms por medição; laço simples basta. 
	while (ms--) { 
		volatile int n = CONFIG_CLOCK_FREQUENCY / 4000; // ~0,25 ms por iteração 
		while (n--) asm volatile(""); 
	} 
}




// Configuração do driver AHT10
#define AHT10_ADDR         0x38
#define AHT10_CMD_INIT     0xE1
#define AHT10_CMD_TRIG  0xAC
#define AHT10_CMD_RESET    0xBA

static int aht10_reset(void) {
    i2c_bus_idle();
    i2c_start();
    if (i2c_write_byte((AHT10_ADDR << 1) | 0x00)) { i2c_stop(); return -1; }
    if (i2c_write_byte(AHT10_CMD_RESET))          { i2c_stop(); return -2; }
    i2c_stop();
    delay_ms(20);
    return 0;
}

static int aht10_init(void) {
    i2c_bus_idle();
    i2c_start();
    if (i2c_write_byte((AHT10_ADDR << 1) | 0x00)) { i2c_stop(); return -1; }
    if (i2c_write_byte(AHT10_CMD_INIT))           { i2c_stop(); return -2; }
    if (i2c_write_byte(0x08))                     { i2c_stop(); return -3; } // Cal enable, normal mode
    if (i2c_write_byte(0x00))                     { i2c_stop(); return -4; }
    i2c_stop();
    delay_ms(20);
    return 0;
}

static int aht10_probe(void) {
	int r;
	// Testa ACK de write
	r = i2c_probe(AHT10_ADDR);
	if (r) return -1; // Sem ACK no write

	// Testa ACK de read
	i2c_bus_idle();
	i2c_start();
	r = i2c_write_byte((AHT10_ADDR << 1) | 0x01);
	i2c_stop();
	if (r) return -2; // Sem ACK no read

	return 0; // Tudo OK
}

static int aht10_measure(int* rh, int* temp_c) {
    // Aciona medição
    i2c_bus_idle();
    i2c_start();
    if (i2c_write_byte((AHT10_ADDR << 1) | 0x00)) { i2c_stop(); return -1; }
    if (i2c_write_byte(AHT10_CMD_TRIG))           { i2c_stop(); return -2; }
    if (i2c_write_byte(0x33))                     { i2c_stop(); return -3; }
    if (i2c_write_byte(0x00))                     { i2c_stop(); return -4; }
    i2c_stop();

    // Poll do busy bit (até ~200 ms)
    for (int tries = 0; tries < 20; ++tries) {
        delay_ms(10);
        i2c_start();
        if (i2c_write_byte((AHT10_ADDR << 1) | 0x01) == 0) {
            unsigned char d0 = i2c_read_byte(0); // só status
            i2c_stop();
            if ((d0 & 0x80) == 0) {
                // Pronto: ler os 6 bytes completos
                i2c_start();
                if (i2c_write_byte((AHT10_ADDR << 1) | 0x01)) { i2c_stop(); return -5; }
                unsigned char s  = i2c_read_byte(1);
                unsigned char d1 = i2c_read_byte(1);
                unsigned char d2 = i2c_read_byte(1);
                unsigned char d3 = i2c_read_byte(1);
                unsigned char d4 = i2c_read_byte(1);
                unsigned char d5 = i2c_read_byte(0);
                i2c_stop();

                // Converte
                unsigned long raw_h = ((unsigned long)d1 << 12) | ((unsigned long)d2 << 4) | ((unsigned long)d3 >> 4);
                unsigned long raw_t = (((unsigned long)d3 & 0x0F) << 16) | ((unsigned long)d4 << 8) | (unsigned long)d5;

                uint64_t rh_num = (uint64_t)raw_h * 10000ULL;
                uint64_t t_num  = (uint64_t)raw_t * 20000ULL;
                *rh     = (int)((rh_num + 524288ULL) / 1048576ULL);
                *temp_c = (int)(((t_num + 524288ULL) / 1048576ULL) - 5000LL);
                return 0;
            }
        } else {
            // sem ACK na leitura: aguarda e tenta de novo
            i2c_stop();
        }
    }
    return -6; // timeout aguardando fim do busy

}


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
	puts("i2c_check                       - check I2C lines levels (pull-ups)");
	puts("i2c_scan                        - scan I2C bus for devices");
	puts("aht10_probe					  - probe AHT10 sensor");
	puts("aht10_init					  - initialize AHT10 sensor");
	puts("aht10                           - single measurement (RH, Temp)");
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

static void cmd_i2c_scan(void) {
	int found = 0;
	puts("I2C scan:");
	for (uint8_t addr = 0x03; addr < 0x77; addr++) {
		if (i2c_probe(addr) == 0) {
			printf("  Found device at 0x%02X\n", addr);
			found++;
		}
	}
	if (!found) puts("  No devices found\n");
}

static void cmd_aht10_init(void) {
	int r = aht10_reset();
	if (r == 0) {
		if ((r = aht10_probe()) != 0) {
			printf("AHT10 probe failed with error code %d\n", r);
			return;
		}
		r = aht10_init();
	}

	if (r == 0) {
		puts("AHT10 initialized successfully\n");
	} else {
		printf("AHT10 initialization failed with error code %d\n", r);
	}
}

static void cmd_aht10_probe(void) {
	int r = aht10_probe();
	if (r == 0) {
		puts("AHT10 device found\n");
	} else {
		printf("AHT10 probe failed with error code %d\n", r);
	}
}

static void cmd_aht10_measure(void) {
	int rh, t;
	int r = aht10_measure(&rh, &t);
	if (r == 0) {
		printf("AHT10: RH = %d.%02d %%, Temp = %d.%02d C\n", rh/100, rh%100, t/100, t%100);
	} else {
		printf("AHT10 failed with error code %d\n", r);
	}
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
	else if(strcmp(token, "i2c_check") == 0)
        cmd_i2c_check();
    else if(strcmp(token, "i2c_scan") == 0)
        cmd_i2c_scan();
	else if(strcmp(token, "aht10_probe") == 0)
		cmd_aht10_probe();
	else if(strcmp(token, "aht10_init") == 0)
		cmd_aht10_init();
	else if(strcmp(token, "aht10") == 0)
		cmd_aht10_measure();
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

	while(1) {
		console_service();
	}

	return 0;
}

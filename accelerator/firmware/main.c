#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>

static void produto_escalar(void);

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
    puts("prod				  - produto escalar");
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

static int64_t prod_escalar_sw(int32_t *a, int32_t *b)
{
    int64_t result = 0;
    for (int i = 0; i < 8; i++) {
        result += a[i] * b[i];
    }
    return result;
}

static void produto_escalar(void)
{
    // Arrays para armazenar os valores lidos da UART
    int32_t a[8], b[8];
    int i;

    int64_t result;
    int64_t result_hi;
    uint32_t result_lo;

    printf("Digite os 8 elementos do vetor A (pressione Enter apos cada numero):\n");
    for (i = 0; i < 8; i++) {
        printf("A[%d]> ", i);
        scanf("%ld", &a[i]);
	printf("%ld\n", a[i]);
    }

    printf("\nDigite os 8 elementos do vetor B:\n");
    for (i = 0; i < 8; i++) {
        printf("B[%d]> ", i);

        scanf("%ld", &b[i]);
	printf("%ld\n", b[i]);
    }

    printf("\nValores recebidos. Enviando para o hardware e calculando em software...\n");
    
    int64_t result_sw = prod_escalar_sw(a, b);
    
    // Escrever os dados nos CSRs de entrada
    accelerator_a0_write(a[0]);
    accelerator_a1_write(a[1]);
    accelerator_a2_write(a[2]);
    accelerator_a3_write(a[3]);
    accelerator_a4_write(a[4]);
    accelerator_a5_write(a[5]);
    accelerator_a6_write(a[6]);
    accelerator_a7_write(a[7]);

    accelerator_b0_write(b[0]);
    accelerator_b1_write(b[1]);
    accelerator_b2_write(b[2]);
    accelerator_b3_write(b[3]);
    accelerator_b4_write(b[4]);
    accelerator_b5_write(b[5]);
    accelerator_b6_write(b[6]);
    accelerator_b7_write(b[7]);

    // Habilitar start para começar cálculo
    accelerator_start_write(1);

    // Aguarda sinal de done
    printf("Aguardando o hardware finalizar...\n");
    while (!accelerator_done_read()) {
      printf("%ld", accelerator_done_read());
    }

    accelerator_start_write(0);
    
    // Ler resultado final
    result_lo = accelerator_result_lo_read();
    result_hi = (int32_t)accelerator_result_hi_read();
    result = (result_hi << 32) | result_lo;

    // Mostrar o resultado ---
    printf("Hardware finalizou!\n");
    printf("Produto Escalar em Hardware= %ld\n", (long)result);
    printf("Produto Escalar em Software= %ld\n", (long)result_sw);
}


static void console_service(void) {
    char *str;
    char *token;

    str = readstr();
    if
(str == NULL) return;
    token = get_token(&str);
    if(strcmp(token, "help") == 0)
        help();
    else if(strcmp(token, "reboot") == 0)
        reboot();
    else if(strcmp(token, "led") == 0)
        toggle_led();
    else if(strcmp(token, "prod") == 0)
        produto_escalar();
    prompt();
}

int main(void) {
#ifdef CONFIG_CPU_HAS_INTERRUPT
    irq_setmask(0);
    irq_setie(1);
#endif
    uart_init();

    printf("Hellorld!\n");
    help();
    prompt();

    while(1) {
        console_service();
    }

    return 0;
}

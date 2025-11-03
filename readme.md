# Introdução

Aplicação de um SoC utilizando a placa FPGA Colorlight i9 (adaptada da i5) através da ferramenta [Litex](https://github.com/enjoy-digital/litex).

# Módulos

O repositório contém dois módulos principais, cada um em sua pasta: `accelerator` e `aht10_lora`.

## aht10_lora

Resumo: este módulo lê temperatura e umidade de um sensor AHT10/AHT20 e transmite os dados via um rádio LoRa (módulo RFM95/SX1276). Há um pequeno console que permite inicializar o sensor e o rádio, fazer medições e enviar pacotes.

O que está incluído:
- Firmware: `aht10_lora/firmware/main.c` — código que faz a leitura do sensor por I2C (bit-bang) e usa a biblioteca LoRa para transmissão.
- Biblioteca LoRa: `aht10_lora/libs/lora_RFM95.c` / `.h` — implementação de acesso SPI ao RFM95.

Pinos utilizados (mapeamento físico no design Litex para a placa Colorlight i9/i5):
- Sensor AHT10/AHT20 (J4): SCL = J20, SDA = K20 (GPIOs configuradas com pull-up)
- LoRa RFM95 (CN5): SCK = N2, MOSI = M1, MISO = T2, CS = T3
- Reset do módulo LoRa: N3

Frequência de operação do rádio:
- O código configura o rádio para 915 MHz. Veja `aht10_lora/libs/lora_RFM95.c` onde a frequência é impressa e programada.

Comandos úteis no console do firmware:
- `ahtinit` / `aht` — inicializar e ler o sensor AHT.
- `lorainit` / `loratx` — inicializar e transmitir via LoRa.

## accelerator

Resumo: o módulo `accelerator` é um bloco de hardware que acelera o cálculo do produto escalar entre dois vetores de 8 elementos. Ele foi integrado ao SoC e é acessível a partir do console via UART.

O que está incluído:
- Hardware: `accelerator/rtl/accelerator.sv` — implementação SystemVerilog do bloco acelerador.
- Firmware: `accelerator/firmware/main.c` — código que fornece um pequeno console com o comando `prod` para enviar dois vetores ao hardware, iniciar o cálculo e ler o resultado.

Como usar:
- Carregue o SoC na placa e execute o console via serial.
- No prompt, use o comando `prod` e entre com os 8 valores do vetor A e os 8 valores do vetor B — o firmware envia os valores para o bloco de hardware, espera a conclusão e mostra o resultado calculado pelo hardware e pelo software para comparação.

Este módulo serve como exemplo de integração de um bloco customizado no SoC e ilustra a comunicação entre firmware e lógica em FPGA através de CSRs.

# Utilização

Primeiramente realize a instalação da toolchain do [OSS-CAD-SUITE](https://github.com/YosysHQ/oss-cad-suite-build).
* Após a instalação, ative o ambiente virtual através do comando `source oss-cad-suite/environment` ou outro equivalente.

Dentro da pasta SoC-Colorlight_i9/accelerator, execute os comandos para compilação e execução:
* Para construção do build:
```
python3 litex/colorlight_i5.py --board i9 --revision 7.2 --build --cpu-type=picorv32 --ecppack-compress
```

* Para configuração do Firmware:
```
cd firmware
make
cd ../
```

* Para carregamento do SoC diretamente na placa:
```
which openFPGALoader
<caminho-openFPGALoader> -b colorlight-i5 build/colorlight_i5/gateware/colorlight_i5.bit
```

* E por fim para execução da BIOS e testes:
```
litex_term --kernel firmware/main.bin /dev/ttyACM0
```

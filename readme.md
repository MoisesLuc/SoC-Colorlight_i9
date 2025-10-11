# Introdução

Aplicação de um SoC utilizando a placa FPGA Colorlight i9 (adaptada da i5) através da ferramenta [Litex](https://github.com/enjoy-digital/litex). O sistema conta com um bloco de harware escrito em SystemVerilog e um firmware escrito em C para comunicação serial via UART.

# Utilização

Primeiramente realize a instalação da toolchain do [OSS-CAD-SUITE](https://github.com/YosysHQ/oss-cad-suite-build).
* Após a instalação, ative o ambiente virtual através do comando `source oss-cad-suite/environment` ou outro equivalente.

Dentro da pasta SoC-Colorlight_i9/accelerator, execute os comandos para compilação e execução:
* Para construção do build:
```
python3 litex/colorlight_i5.py --board i9 --revision 7.2 --build --cpu-type=picorv32 --ecppack-compress
```

* Para carregamento do SoC diretamente na placa:
```
which openFPGALoader
<caminho-openFPGALoader> -b colorlight-i5 build/colorlight_i5/gateware/colorlight_i5.bit
```

* Para carregamento da instância do bloco de hardware:
```
cd firmware
make
cd ../
```

* E por fim para execução da BIOS e testes:
```
litex_term --kernel firmware/main.bin /dev/ttyACM0
```

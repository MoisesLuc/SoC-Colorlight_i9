    # TensorFlow Lite Micro on LiteX FPGA

Este projeto integra o TensorFlow Lite Micro (TFLM) com o framework LiteX para executar inferência de machine learning em uma FPGA usando um processador soft-core RISC-V.

## Estrutura do Projeto

```
firmware/
├── main.cc                    # Firmware principal (C++ para TFLM)
├── main.c                     # Firmware original (mantido como referência)
├── Makefile                   # Build do firmware
├── build.sh                   # Script de build automatizado
├── linker.ld                  # Linker script
├── models/                    # Modelos TensorFlow Lite
│   ├── hello_world_int8_model_data.cc
│   └── hello_world_int8_model_data.h
├── platform/                  # Adaptadores para LiteX
│   ├── litex_debug_log.cc    # Logging via UART
│   ├── litex_micro_time.cc   # Timer
│   └── litex_system_setup.cc # Inicialização
└── tflm/                      # TensorFlow Lite Micro
    ├── Makefile
    ├── tensorflow/
    ├── examples/
    └── third_party/
```

## Características

- **Modelo**: Hello World (predição de seno usando rede neural)
- **Formato**: Quantizado INT8 para eficiência em hardware embarcado
- **Tamanho do modelo**: ~2.7KB
- **RAM requerida**: ~4KB para tensor arena
- **Arquitetura**: RISC-V 32-bit (RV32IM)

## Como Compilar

### 1. Primeiro, compile o SoC LiteX

```bash
cd litex
python3 litex/colorlight_i5.py --board i9 --revision 7.2 --build --cpu-type=picorv32 --ecppack-compress
```

### 2. Compile o firmware TFLM

```bash
cd firmware
make
```

## Como Usar

### 1. Upload para FPGA

Primeiro, faça o upload do bitstream:

```bash
which openFPGALoader
<caminho-openFPGALoader> -b colorlight-i5 build/colorlight_i5/gateware/colorlight_i5.bit
```

Depois, carregue o firmware:

```bash
litex_term /dev/ttyUSB0 --kernel=firmware/build/main.bin
```

### 2. Comandos Disponíveis

No console do firmware:

```
TFLM> reboot            # Reinicializar a BIOS com o modelo
TFLM> init              # Inicializa o TensorFlow Lite Micro
TFLM> run               # Execução do modo de inferência contínua

```
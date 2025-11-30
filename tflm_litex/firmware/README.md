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
python3 colorlight_i5.py --build
```

### 2. Compile o firmware TFLM

```bash
cd firmware
./build.sh
```

O script irá:
1. Verificar se o build do LiteX existe
2. Compilar a biblioteca TFLM (se necessário)
3. Compilar o firmware
4. Gerar `build/main.bin`

### Compilação Manual

Se preferir compilar manualmente:

```bash
# 1. Compilar biblioteca TFLM
cd firmware/tflm
make -j$(nproc)

# 2. Compilar firmware
cd ..
make clean
make -j$(nproc)
```

## Como Usar

### 1. Upload para FPGA

Primeiro, faça o upload do bitstream:

```bash
cd build/colorlight_i5/gateware
openFPGALoader -b colorlight-i5 colorlight_i5.bit
```

Depois, carregue o firmware:

```bash
cd ../../..
litex_term /dev/ttyUSB0 --kernel=firmware/build/main.bin
```

### 2. Comandos Disponíveis

No console do firmware:

```
TFLM> help              # Mostra comandos disponíveis
TFLM> init              # Inicializa o TensorFlow Lite Micro
TFLM> info              # Mostra informações do modelo
TFLM> test              # Executa suite de testes
TFLM> infer 1.57        # Executa inferência com valor específico
TFLM> led               # Liga/desliga LED
TFLM> reboot            # Reinicia o sistema
```

### 3. Exemplo de Uso

```
TFLM> init
Initializing TensorFlow Lite Micro...
Model loaded (version 3)
Operations registered
Tensors allocated
Input: 1 bytes, type 9
Output: 1 bytes, type 9
Arena used: 2976 / 4096 bytes
TensorFlow Lite Micro initialized successfully!

TFLM> infer 1.57
Input: 1.57 (q=-63)
Output: 1.0007 (q=127)
Expected: 1.0000 (sin(1.57))
Error: 0.0007

TFLM> test
=== Running TensorFlow Lite Micro Test Suite ===

--- Test 1/7 ---
Input: 0.00 (q=-127)
Output: -0.0039 (q=-1)
Expected: 0.0000 (sin(0.00))
Error: 0.0039

--- Test 2/7 ---
Input: 0.77 (q=-96)
...
```

## Detalhes Técnicos

### Modelo Neural

O modelo "Hello World" é uma rede neural simples que aprende a função seno:
- **Entrada**: Valor float representando ângulo em radianos
- **Saída**: sin(entrada)
- **Arquitetura**: 2 camadas fully-connected
- **Quantização**: INT8 para otimização

### Memória

- **Código do firmware**: ~100KB
- **Biblioteca TFLM**: ~200KB
- **Modelo**: 2.7KB
- **Tensor Arena**: 4KB (alocado em runtime)
- **Total estimado**: ~350KB

### Performance

- **Latência de inferência**: < 1ms (dependendo do clock da CPU)
- **Precisão**: Erro típico < 0.05 após quantização

## Arquivos de Plataforma

Os arquivos em `platform/` implementam a interface entre TFLM e LiteX:

- **litex_debug_log.cc**: Redireciona logs do TFLM para UART do LiteX
- **litex_micro_time.cc**: Implementa timer usando periférico do LiteX
- **litex_system_setup.cc**: Inicialização específica da plataforma

## Troubleshooting

### Erro: "Model schema version mismatch"
- Recompile a biblioteca TFLM: `cd tflm && make clean && make`

### Erro: "AllocateTensors() failed"
- Aumente `kTensorArenaSize` em `main.cc`
- Verifique memória RAM disponível no SoC

### Erro ao compilar: "undefined reference to..."
- Certifique-se que a biblioteca TFLM foi compilada: `ls tflm/build/libtflm.a`
- Recompile: `cd tflm && make clean && make`

### Serial não conecta
- Verifique porta: `ls /dev/ttyUSB*`
- Verifique permissões: `sudo usermod -a -G dialout $USER` (relogin necessário)

## Customização

### Usar Outro Modelo

1. Converta seu modelo para TFLite:
```python
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()
```

2. Converta para C array:
```bash
xxd -i model.tflite > model_data.cc
```

3. Substitua em `models/` e atualize `main.cc`

### Ajustar Tamanho da Arena

Edite em `main.cc`:
```cpp
constexpr int kTensorArenaSize = 4096;  // Ajuste conforme necessário
```

## Referências

- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
- [LiteX](https://github.com/enjoy-digital/litex)
- [Colorlight i5](https://github.com/wuxx/Colorlight-FPGA-Projects)

## Licença

- Código do projeto: MIT License
- TensorFlow Lite Micro: Apache License 2.0
- LiteX: BSD License

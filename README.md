# Nivelador vMix (VST3)

Nivelador automático de loudness + limitador true-peak para usar no vMix.
Instruções de uso e instalação: [LEIA-ME.md](LEIA-ME.md).

## Baixar o plugin pronto

Na aba **Actions** do GitHub, abra o último build verde e baixe
`Nivelador-vMix-Windows-x64`. Ou, quando houver, pegue o zip na aba **Releases**.

## Compilar

Requer CMake 3.22+ e Visual Studio 2022 ou mais novo (o JUCE é baixado automaticamente).

```
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

O plugin fica em `build/NiveladorVMix_artefacts/Release/VST3/`.

## Estrutura

- `Source/LevelerDSP.h`: todo o processamento (sem dependência do JUCE).
- `Source/PluginProcessor.*`: parâmetros e ligação com o host.
- `Source/PluginEditor.*`: tela do plugin.
- `tests/test_dsp.cpp`: testes do nivelador, do medidor LUFS e do limitador.

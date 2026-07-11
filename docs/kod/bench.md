# Performans (`src/bench/`)

## Sorumluluk

`saqut bench` komutu tarafından kullanılan profil ölçüm altyapısı. VM
çalışırken opcode bazında zaman damgası toplar, çalışma sonunda analiz
eder. Normal derleme/çalışma pipeline'ına sıfır etkisi vardır — yalnızca
profil etkinleştirildiğinde devreye girer.

## Dosya envanteri

| Dosya | Rol |
|-------|-----|
| `profile.hpp` | `BenchProfile`, `BenchVMTrace`, `analyzeVMTrace()`, `printBenchProfile()` — profil veri yapıları ve analiz. |

## Ana tipler

```
BenchVMTrace
  ├─ traceOpcodes: vector<Opcode> — her dispatch'teki opcode
  └─ traceTicks  : vector<uint64_t> — her dispatch'teki zaman damgası

BenchProfile
  ├─ totals  : opcode frequency + toplam süre
  └─ per-opcode istatistikleri

analyzeVMTrace(BenchVMTrace&) → BenchProfile
printBenchProfile(profile, ...) — insan-okur rapor
```

## Tasarım kararları

- **VM çalışırken hesap yok**: İki paralel büyüyen vektör (opcode + ticks).
  Tüm istatistikler VM bittikten sonra analyzeVMTrace() ile türetilir.
- **Donanım zamanlama**: x86/x86_64'te `__rdtsc()` (2-3 saat döngüsü,
  nanosaniyeden hızlı). Diğer platformlarda `std::chrono::steady_clock`.
  analyzeVMTrace içinde tek seferlik TSC kalibrasyonu yapılır.
- **Sıfır etki**: Profil kapalıyken (`vmTrace_ = nullptr`) VM'de ek yük yoktur.

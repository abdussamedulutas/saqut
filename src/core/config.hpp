#ifndef SAQUT_CORE_CONFIG
#define SAQUT_CORE_CONFIG

// Derleyici yapılandırması — hangi optimizasyon pass'lerinin çalışacağı.
struct CompilerConfig {
    bool optConstantFolding = true;
    bool optDeadCodeElim    = true;
    int  maxFixpointRounds  = 10;
};

#endif // SAQUT_CORE_CONFIG

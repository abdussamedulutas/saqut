#include "vm/shadow_stack.hpp"

// THREAD NOTU (JIT çalışma bağlamı refactor'ünün tamamlayıcısı): depo
// artık thread_local — her iş parçacığı kendi shadow stack'ini görür.
// Tek iş parçacıklı çalışışta davranış birebir aynıdır (tek örnek vardır);
// çok thread'de GC kökleri iş parçacığına ait kalır (MIRPLAN §9 modeli,
// mir_backend.cpp içindeki JitRuntime/rt() ile aynı karar).
ShadowStack& jitShadowStack() {
    thread_local ShadowStack s;
    return s;
}

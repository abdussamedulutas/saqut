extern "C" {
    #include "lightning.h"
}
#include <iostream>
#include <fstream>
#include <sys/mman.h>
#include <unistd.h>
#include <chrono>

extern "C" int saqut_print(int value) {
    return value + 1;
}

int main() {
    init_jit("saqut");
    jit_state_t *_jit = jit_new_state();

    jit_prolog();

    jit_movi(JIT_V0, 0); // sum = 0 (V0 kayıtçısı sum olsun)
    jit_movi(JIT_V1, 1); // a = 1 (V1 kayıtçısı a olsun)

    jit_node_t *loop_start = jit_label();

    // sum += a (Kayıtçıdan kayıtçıya toplama - Işık hızında)
    jit_addr(JIT_V0, JIT_V0, JIT_V1);

    // a++
    jit_addi(JIT_V1, JIT_V1, 1);
    // a++
    jit_addi(JIT_V1, JIT_V1, 1);
    // a++
    jit_addi(JIT_V1, JIT_V1, 1);
    // a++
    jit_addi(JIT_V1, JIT_V1, 1);

    // a < 15000 kontrolü
    jit_movi(JIT_R1, 15000);
    jit_node_t *if_node = jit_bltr(JIT_V1, JIT_R1); // a < 15000 ise loop_start'a zıpla
    jit_patch_at(if_node, loop_start);

    jit_movr(JIT_R0, JIT_V0); // sonucu döndür
    jit_retr(JIT_R0);
    jit_epilog();

    // --- ÇALIŞTIRMA VE KAYDETME ---
    jit_realize();
    
    jit_word_t size;
    void* final_code = jit_emit(); 
    jit_get_code(&size); 

    void (*func)() = (void (*)())final_code;
    
    std::cout << "--- saQut Programı Başlıyor ---" << std::endl;
    if (final_code) {
        auto start = std::chrono::high_resolution_clock::now();
        func(); 
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed = end - start;
        std::cout << "Süre: " << elapsed.count() << " mikrosaniye." << std::endl;
    }


    if (final_code) {
        volatile int prevent_optimization = 0;
        auto start = std::chrono::high_resolution_clock::now();
        int sum = 0;
        for(int a = 0; a < 15000; a++) {
            sum += saqut_print(a); 
        }
        prevent_optimization = sum;

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed = end - start;
        std::cout << "Süre: " << elapsed.count() << " mikrosaniye." << prevent_optimization << std::endl;
    }

    std::cout << "--- saQut Programı Bitti ---" << std::endl;

    std::cout << "Kod üretildi. Boyut: " << static_cast<signed long>(size) << " bayt." << std::endl;

    std::ofstream outfile("calc.bin", std::ios::binary);
    outfile.write(reinterpret_cast<const char*>(final_code), size);
    outfile.close();

    jit_destroy_state();
    finish_jit();
    return 0;
}
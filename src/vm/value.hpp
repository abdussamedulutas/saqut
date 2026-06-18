// ============================================================================
// saQut VM — Value (Çalışma Zamanı Değer)
//
// Bir saQut değerinin bellekteki temsilidir.
//
// ŞU AN SADECE INT:
//   fibonacci.sqt tamamen int kullanır, bu dikey dilim için int yeterli.
//   İleride float, bool, string eklenmesi için "kind" alanı iskelet olarak bırakıldı.
//
// BOOLEAN OLARAK KULLANIM:
//   JIF_FALSE talimatı değerin 0 olup olmadığına bakar.
//   0 = yanlış, sıfır-dışı = doğru. C geleneği.
// ============================================================================

#ifndef SAQUT_VM_VALUE
#define SAQUT_VM_VALUE

// Gelecekte float/bool/string eklendiğinde burası genişleyecek.
// Şimdilik sadece int.
enum class ValueKind {
    Int,
    // Float,   // TODO(vm-genişletme)
    // Bool,    // TODO(vm-genişletme)
    // String,  // TODO(vm-genişletme)
};

struct Value {
    ValueKind kind     = ValueKind::Int;
    int       intValue = 0;

    // Kolay oluşturma
    static Value fromInt(int n) {
        Value v;
        v.kind     = ValueKind::Int;
        v.intValue = n;
        return v;
    }

    // JIF_FALSE için: 0 = yanlış, diğer = doğru
    bool isTruthy() const { return intValue != 0; }
};

#endif // SAQUT_VM_VALUE

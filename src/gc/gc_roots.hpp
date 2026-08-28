// ============================================================================
// saQut GC — Kök Sağlayıcı Sözleşmesi
// ============================================================================
//
// DİZİN:   src/gc/gc_roots.hpp
// KATMAN:  GC — backend'lerden BAĞIMSIZ ortak çalışma zamanı katmanı
//
// AMAÇ:
//   "Bir toplama turunda hangi nesneler kesinlikle canlıdır?" sorusunun
//   cevabını Heap'e veren arayüz.
//
// NEDEN BÖYLE:
//   Kök kümesi backend'in İÇ yapısıdır: VM'inkiler çağrı yığınındaki
//   frame slot'ları, JIT'inkiler shadow stack'teki register yansımaları.
//   Heap bu yapıların hiçbirini bilmemelidir — bilirse her yeni backend
//   Heap'i değiştirmeyi gerektirir ve toplama kodu backend sayısına göre
//   dallanır.
//
//   Bunun yerine ilişki tersine çevrilir: kökü OLAN, kendini Heap'e kaydeder
//   ve sorulduğunda söyler. Heap yalnızca "kayıtlı herkese sor" bilir.
//
//   Somut kazanç: JIT'in GC'ye görünürlüğü (#228 shadow stack) Heap'te tek
//   satır değişiklik gerektirmez; ileride her iş parçacığının kendi heap'i
//   olduğunda (bkz. #222 izolasyon modeli) her thread yalnızca kendi kök
//   sağlayıcılarını kaydeder.
//
// SÖZLEŞME — sağlayıcı şunlara uymak zorundadır:
//   1. collectRoots yalnızca toplama sırasında, Heap tarafından çağrılır.
//   2. Çağrı sırasında sağlayıcı TAHSİS YAPMAZ ve heap'i değiştirmez.
//   3. Erişilebilir olması gereken HER referans bildirilmelidir. Eksik
//      bildirim canlı nesnenin süpürülmesidir (use-after-free); fazla
//      bildirim yalnızca gecikmiş toplamadır. Emin olunmayan yerde
//      bildirmek doğru taraftır.
// ============================================================================

#ifndef SAQUT_GC_ROOTS
#define SAQUT_GC_ROOTS

struct Object;
struct Value;

// Toplama sırasında Heap'in sağlayıcıya verdiği bildirim kanalı.
//
// Sağlayıcı elindeki referansları bu arayüzle bildirir; işaretlemenin nasıl
// yapıldığını (ve nesnenin çocuklarına nasıl inildiğini) bilmesi gerekmez.
struct RootSink {
    // Bir Value'yu bildir. Ref ve String türleri işaretlenir, diğerleri
    // (int/float/decimal/null) sessizce yok sayılır — çağıran tarafın tür
    // ayıklaması yapması gerekmez.
    virtual void acceptValue(const Value& value) = 0;

    // Doğrudan bir nesneyi bildir (JIT shadow stack gibi Value taşımayan
    // kaynaklar için). nullptr güvenlidir.
    virtual void acceptObject(Object* object) = 0;

protected:
    ~RootSink() = default;  // sahiplenilmez; Heap yığında tutar
};

// Kökü olan her taraf bunu gerçekler ve Heap'e kaydolur.
struct RootSource {
    // Elindeki tüm canlı referansları sink'e bildir.
    virtual void collectRoots(RootSink& sink) = 0;

    virtual ~RootSource() = default;
};

#endif // SAQUT_GC_ROOTS

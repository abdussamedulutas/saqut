# ADR-031 — Modül Döngüsü Tespiti Politikası

**Durum:** Beklemede (TODO)  
**Tarih:** 2026-06-25

## Mevcut durum

`src/module/module_loader.cpp`: `seen_` seti her yüklenen modül yolunu izler. Tekrar
karşılaşılan modül `loadUnit` çağrısı atlanır — sonsuz döngü önlenir. Ancak döngüsel
bağımlılık (`A → B → A`) açık bir hata üretmez; sessizce kısa devre yapılır.

## Kabul edilen karar (gelecek)

Döngüsel bağımlılık tespit edildiğinde derleme hatası üretilmeli:

```
E_MODULE_CYCLE: döngüsel modül bağımlılığı tespit edildi: A → B → A
```

`seen_` seti yerine veya yanında `inProgress_` seti eklenerek bir modülün kendi yükleme
zincirinde tekrar görünmesi döngü olarak işaretlenebilir.

## Bekleyen neden

Modül sistemi golden testleri henüz yok. Döngü politikası tanımlanmadan önce temel
çok-modüllü test altyapısı kurulmalı. Bu issue açık (#TODO — issue yoksa açılacak).

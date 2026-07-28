// ============================================================================
// saQut CLI — merkezi exit-code sözleşmesi (#156, ürün sahibi kararı)
//
// 0  kSuccess:       komut başarıyla sonuçlandı.
// 64 kUsageError:    CLI kullanım/argüman hatası; girdi derlenmeye
//                    başlamadan reddedildi (ör. eksik dosya argümanı).
// 65 kDataError:     kaynak/veri tanısı — parse/semantic diagnostic.
// 70 kSoftwareError: runtime/compiler çalışma hatası.
//
// `run`ın kendi programının main() dönüş statusunu (0/1/255 vb.) aktarması
// bu sözleşmenin kapsamı DIŞINDADIR — o, çalıştırılan saQut programının
// kendi process status'udur, saqut CLI'ının kendi hata sınıflandırması
// değildir.
// ============================================================================

#ifndef SAQUT_CLI_EXIT_CODES
#define SAQUT_CLI_EXIT_CODES

namespace saqut::exit_code {
constexpr int kSuccess = 0;
constexpr int kUsageError = 64;
constexpr int kDataError = 65;
constexpr int kSoftwareError = 70;
}  // namespace saqut::exit_code

#endif  // SAQUT_CLI_EXIT_CODES

// ============================================================================
// saQut DAP — FrameReader (fd-tabanlı Content-Length çerçeve okuyucu)
// ============================================================================
//
// DİZİN:   src/dap/frame_reader.hpp
// KATMAN:  DAP — stdin'den DAP mesajlarını okur
//
// AMAÇ (Faz 8, #105):
//   std::cin/stdio kullanılSAYDI, tampon fd'deki baytları yutar ve
//   runWithBudget'ın tur-arası poll() kontrolü pipe'ta hiçbir zaman veri
//   göremezdi (pause kaybolur, sonsuz döngüde sunucu kilitlenir). Bu sınıf
//   fd 0'dan KENDİ tamponuna okur: bekleyen veri ya tampondadır (kontrol
//   edilebilir) ya da fd'dedir (poll'lanabilir).
//
// TODO(#95): poll()/read() POSIX'e özgü — Windows CI için eşlenik gerekecek.
//
// ============================================================================

#ifndef SAQUT_DAP_FRAME_READER
#define SAQUT_DAP_FRAME_READER

#include <string>
#include <poll.h>
#include <unistd.h>
#include "vendor/nlohmann/json.hpp"

class FrameReader {
public:
    explicit FrameReader(int fd = 0) : fd_(fd) {}

    // Akış kapandı mı? (EOF görüldü ve tamponda tam çerçeve kalmadı)
    bool eof() const { return eof_ && buf_.empty(); }

    // Bekleyen mesaj var mı: tamponda veri YA DA fd'de okunmayı bekleyen bayt.
    // Bloklamaz — runWithBudget'ın tur-arası kontrolü için.
    bool hasPending() {
        if (!buf_.empty()) return true;
        struct pollfd p{};
        p.fd     = fd_;
        p.events = POLLIN;
        return poll(&p, 1, 0) > 0 && (p.revents & (POLLIN | POLLHUP));
    }

    // Bir DAP mesajı oku (gerekirse bloklar). EOF'ta ya da bozuk çerçevede
    // discarded json döner — çağıran eof() ile ayırt eder.
    nlohmann::json readMessage() {
        while (true) {
            // Tamponda tam çerçeve var mı?
            size_t hdrEnd = buf_.find("\r\n\r\n");
            if (hdrEnd != std::string::npos) {
                int contentLength = parseContentLength(buf_.substr(0, hdrEnd));
                if (contentLength <= 0) {
                    // Bozuk başlık — başlığı at, akışı sürdür (dayanıklılık)
                    buf_.erase(0, hdrEnd + 4);
                    continue;
                }
                size_t total = hdrEnd + 4 + (size_t)contentLength;
                if (buf_.size() >= total) {
                    std::string body = buf_.substr(hdrEnd + 4, contentLength);
                    buf_.erase(0, total);
                    return nlohmann::json::parse(body, nullptr, false);
                }
            }
            // Eksik çerçeve — bloklayarak veri bekle
            if (!fillBlocking())
                return nlohmann::json(nullptr); // EOF
        }
    }

private:
    // Bozuk "Content-Length: abc" başlığında 0 döner (stoi guard).
    static int parseContentLength(const std::string& header) {
        size_t pos = header.find("Content-Length:");
        if (pos == std::string::npos) return 0;
        try {
            return std::stoi(header.substr(pos + 15));
        } catch (...) {
            return 0;
        }
    }

    // fd'den en az 1 bayt oku (bloklar). EOF/hata → false.
    bool fillBlocking() {
        if (eof_) return false;
        char tmp[4096];
        ssize_t n = ::read(fd_, tmp, sizeof(tmp));
        if (n <= 0) { eof_ = true; return false; }
        buf_.append(tmp, (size_t)n);
        return true;
    }

    int         fd_;
    std::string buf_;
    bool        eof_ = false;
};

#endif // SAQUT_DAP_FRAME_READER

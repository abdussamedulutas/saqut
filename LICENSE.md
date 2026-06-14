## Lisans / License / Lizenz

[Türkçe Sürüm](#türkçe-lisans-sürümü) | [English Version](#english-license-version) | [Deutsche Version](#deutsche-lizenzversion)

---

### Türkçe Lisans Sürümü

# saQut Kamu Lisansı (Sürüm 1.0)

Telif Hakkı (c) 2026, Abdussamed ULUTAŞ (ve saQut Katkıda Bulunanları).
Tüm hakları saklıdır.

Bu lisans; saQut Derleyicisi, kaynak kodları, derleme betikleri (build scripts) ve beraberindeki dokümantasyonu (toplu olarak "Yazılım" olarak anılacaktır) kapsar.

---

#### 1. Temel Felsefe (Alet Çantası Kuralı)
saQut, programlanabilir ve incelenebilir bir derleyici alet çantası (toolbox) olarak tasarlanmıştır. Ana varlık sebebi; derleme sürecinin (Token'lar, AST, Sembol Tabloları ve IR) dışarıdan tamamen şeffaf ve görülebilir kalmasıdır. Bu Yazılımın her türlü yeniden dağıtımı veya üzerinde yapılacak değişiklikler bu şeffaflık ilkesine saygı duymak zorundadır.

#### 2. İzinler ve Koşullar
Bu Yazılımın bir kopyasını edinen herhangi bir kişiye; aşağıdaki koşullara uymak kaydıyla, Yazılımı kullanma, kopyalama, değiştirme, birleştirme, yayınlama veya dağıtma hakkı ücretsiz olarak tanınmıştır:

* **Copyleft (Açık Kaynak Kalma) Şartı:** Eğer Yazılım üzerinde bir değişiklik yaparsanız (derleyici, lexer, parser, VM veya çekirdek mimari) ve bunu dağıtırsanız, değiştirdiğiniz kaynak kodları da BU AYNI LİSANS altında açık kaynaklı olarak herkese sunmak zorundasınız. saQut'u alıp kapalı kaynaklı, tescilli bir ticari ürüne dönüştüremezsiniz.
* **Atıf Şartı:** Orijinal telif hakkı bildirimi ve işbu izin bildirimi, Yazılımın tüm kopyalarına veya önemli parçalarına dahil edilmelidir.
* **İnceleme ve Şeffaflık Daveti:** saQut incelenmek için var olduğundan; bu derleyiciyi eğitim projelerinde, sınıflarda veya topluluk sunumlarında kullanırsanız, öğrencilerinize veya dinleyicilerinize en az bir kez `saqut tokens` veya `saqut ast` çıktılarını göstermeniz önemle tavsiye edilir (hukuki bir zorunluluk değildir). İç mekanizmayı görünür tutun!

#### 3. Runtime (Çalışma Zamanı) ve Çıktı İstisnası (KRİTİK)
Bu dili kullanan geliştiricilerin mutlak özgürlüğünü garanti altına almak adına:
* **Kodunuz Sizindir:** saQut diliyle yazılmış tüm kaynak kodları (örn. `.sqt` dosyaları) ve derleyici tarafından üretilen tüm veriler (JSON AST dökümleri, sembol tabloları veya derlenmiş bytecode çıktısı) bu lisansın kısıtlamalarına **TABİ DEĞİLDİR**.
* Geliştiriciler, kendi saQut programlarının kaynak kodlarını ve derleyici çıktılarını istedikleri gibi lisanslamakta, satmakta veya kapalı kaynak yapmakta tamamen özgürdür. saQut tarafından sağlanan çalışma zamanı ortamı (runtime/VM), bu ortamı barındıran ana uygulamaların lisanslarını kısıtlamaksızın diğer uygulamaların içine gömülebilir (embed edilebilir).

#### 4. Garanti Reddi
YAZILIM "OLDUĞU GİBİ" SUNULMAKTADIR; TİCARETE UYGUNLUK, BELİRLİ BİR AMACA UYGUNLUK VE İHLAL ETMEME GARANTİLERİ DAHİL ANCAK BUNLARLA SINIRLI OLMAMAK ÜZERE, AÇIK VEYA ZIMNİ HERHANGİ BİR GARANTİ VERİLMEMEKTEDİR. YAZARLAR VEYA TELİF HAKKI SAHİPLERİ, YAZILIMLA VEYA YAZILIMIN KULLANIMIYLA YA DA DİĞER İŞLEMLERLE BAĞLANTILI OLARAK ORTAYA ÇIKAN SÖZLEŞME, HAKSIZ FİİL VEYA DİĞER DURUMLARDAKİ HİÇBİR TALEP, TAZMİNAT VEYA DİĞER YÜKÜMLÜLÜKLERDEN SORUMLU TUTULAMAZ.

---

### English License Version

# The saQut Public License (Version 1.0)

Copyright (c) 2026, Abdussamed ULUTAŞ (and saQut Contributors).
All rights reserved.

This license applies to the saQut Compiler, its source code, build scripts, and accompanying documentation (collectively referred to as "the Software").

---

#### 1. The Core Philosophy (The Toolbox Rule)
saQut is designed as a programmable, inspectable compiler toolbox. Its core purpose is to keep the compilation process transparent (Tokens, AST, Symbol Tables, and IR). Any redistribution or modification of this Software must respect this transparency.

#### 2. Permissions & Conditions
Permission is hereby granted, free of charge, to any person obtaining a copy of this Software, to use, copy, modify, merge, publish, or distribute the Software, subject to the following conditions:

* **The Copyleft Requirement:** If you modify the Software (the compiler, lexer, parser, VM, or core architecture) and distribute it, you MUST make your modified source code publicly available under this same license. You cannot turn saQut into a closed-source proprietary product.
* **The Attribution Requirement:** The original copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
* **The Inspection Invitation:** Since saQut is built to be inspected, if you use this compiler in an educational project, classroom, or public presentation, you are highly encouraged (though not legally forced) to show your students or audience the output of `saqut tokens` or `saqut ast` at least once. Keep the internals visible!

#### 3. The Runtime & Output Exception (CRITICAL)
To ensure the absolute freedom of the developers using this language:
* **Your Code is Yours:** Any source code written *in* the saQut language (e.g., `.sqt` files), and any data generated *by* the compiler (such as JSON AST dumps, symbol tables, or compiled bytecode) are **NOT** subject to this license.
* Developers are completely free to license, sell, or close the source code of their own saQut programs and compiler outputs as they see fit. The runtime environment/VM provided by saQut may be embedded into other applications without restricting those applications' licenses.

#### 4. Disclaimer of Warranty
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

### Deutsche Lizenzversion

# Die saQut Öffentliche Lizenz (Version 1.0)

Copyright (c) 2026, Abdussamed ULUTAŞ (und saQut Mitwirkende).
Alle Rechte vorbehalten.

Diese Lizenz gilt für den saQut-Compiler, seinen Quellcode, Build-Skripte und die dazugehörige Dokumentation (zusammenfassend als „die Software“ bezeichnet).

---

#### 1. Die Kernphilosophie (Die Toolbox-Regel)
saQut ist als programmierbare, inspizierbare Compiler-Toolbox konzipiert. Der Hauptzweck besteht darin, den Kompilierungsprozess (Tokens, AST, Symboltabellen und IR) transparent zu halten. Jede Weitergabe oder Änderung dieser Software muss diese Transparenz respektieren.

#### 2. Berechtigungen & Bedingungen
Hiermit wird jeder Person, die eine Kopie dieser Software erhält, kostenlos die Berechtigung erteilt, die Software zu nutzen, zu kopieren, zu modifizieren, zusammenzufügen, zu veröffentlichen oder zu verbreiten, sofern folgende Bedingungen erfüllt sind:

* **Die Copyleft-Bedingung:** Wenn Sie die Software verändern (Compiler, Lexer, Parser, VM oder Kernarchitektur) und verbreiten, MÜSSEN Sie Ihren geänderten Quellcode unter derselben Lizenz öffentlich zugänglich machen. Sie dürfen saQut nicht in ein proprietäres Closed-Source-Produkt umwandeln.
* **Die Namensnennung-Bedingung:** Der obige Urheberrechtshinweis und dieser Berechtigungshinweis müssen in allen Kopien oder wesentlichen Teilen der Software enthalten sein.
* **Die Einladung zur Inspektion:** Da saQut dafür gebaut wurde, inspiziert zu werden, werden Sie ausdrücklich dazu ermutigt (wenn auch nicht rechtlich gezwungen), Ihren Schülern, Studenten oder dem Publikum mindestens einmal die Ausgabe von `saqut tokens` oder `saqut ast` zu zeigen, wenn Sie diesen Compiler in einem Bildungsprojekt, im Unterricht oder bei einer öffentlichen Präsentation verwenden. Halten Sie die Interna sichtbar!

#### 3. Die Laufzeit- und Ausgabe-Ausnahme (KRITISCH)
Um die absolute Freiheit der Entwickler zu gewährleisten, die diese Sprache verwenden:
* **Ihr Code gehört Ihnen:** Quellcode, der *in* der saQut-Sprache geschrieben wurde (z. B. `.sqt`-Dateien), und alle vom Compiler generierten Daten (wie JSON-AST-Dumps, Symboltabellen oder kompilierter Bytecode) unterliegen **NICHT** dieser Lizenz.
* Entwickler können den Quellcode ihrer eigenen saQut-Programme und Compiler-Ausgaben nach eigenem Ermessen lizenzieren, verkaufen oder schließen. Die von saQut bereitgestellte Laufzeitumgebung/VM kann in andere Anwendungen eingebettet werden, ohne die Lizenzen dieser Anwendungen einzuschränken.

#### 4. Haftungsausschluss
DIE SOFTWARE WIRD „WIE BESEHEN“ (AS IS) OHNE JEGLICHE AUSDRÜCKLICHE ODER ZIMNLICHE GEWÄHRLEISTUNG ZUR VERFÜGUNG GESTELLT, EINSCHLIESSLICH, ABER NICHT BESCHRÄNKT AUF DIE GEWÄHRLEISTUNG DER MARKTGÄNGIGKEIT, DER EIGNUNG FÜR EINEN BESTIMMTEN ZWECK UND DER NICHTVERLETZUNG VON RECHTEN DRITTER. IN KEINEM FALL SIND DIE AUTOREN ODER URHEBERRECHTSINHABER FÜR ANSPRÜCHE, SCHÄDEN ODER ANDERE HAFTUNGEN HAFTPAR, SEI ES IN FOLGE EINES VERTRAGES, EINER UNERLAUBTEN HANDLUNG ODER AUF ANDERE WEISE, DIE SICH AUS ODER IM ZUSAMMENHANG MIT DER SOFTWARE ODER DER NUTZUNG ODER DEM UMGANG MIT DER SOFTWARE ERGEBEN.
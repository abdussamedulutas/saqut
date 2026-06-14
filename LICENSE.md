## Lisans / License / Lizenz

[Türkçe Sürüm](#türkçe-lisans-sürümü) | [English Version](#english-license-version) | [Deutsche Version](#deutsche-lizenzversion)

> Bu lisans üç dilde sunulmuştur. Yorum farklılıklarında **Türkçe sürüm esas alınır.**
> This license is provided in three languages. In case of any discrepancy, the **Turkish version prevails.**

---

### Türkçe Lisans Sürümü

# saQut Lisansı (Sürüm 1.0)
**Kaynağı Açık — Ticari Kullanımı Kısıtlı Lisans**

Telif Hakkı (c) 2026, Abdussamed ULUTAŞ (ve saQut Katkıda Bulunanları). Tüm hakları saklıdır.

Bu lisans; saQut derleyicisini, kaynak kodlarını, derleme betiklerini (build scripts), tüm backend bileşenlerini (yorumlayıcı, bytecode VM, derleyici ve transpiler) ve beraberindeki dokümantasyon ile mimari tasarımı (ADR belgeleri dâhil) — toplu olarak "Yazılım" olarak anılacaktır — kapsar.

saQut açık kaynaklı (kaynağı görülebilir) bir projedir; ancak **özgür yazılım (free/libre software) değildir.** Yazılımın incelenmesi ve değiştirilmesi serbestken, ticari kullanımı işbu lisansın koşullarına ve telif hakkı sahibinin iznine tabidir.

#### 1. Temel Felsefe (Alet Çantası Kuralı)
saQut; programlanabilir ve incelenebilir bir derleyici alet çantası (toolbox) olarak tasarlanmıştır. Varlık sebebi, derleme sürecinin her aşamasının — token'lar, Soyut Sözdizimi Ağacı (AST), sembol tabloları ve Ara Temsil (IR) — dışarıdan şeffaf ve görülebilir kalmasıdır. Yazılımın her türlü yeniden dağıtımı ve üzerinde yapılacak değişiklikler bu şeffaflık ilkesini korumakla yükümlüdür.

#### 2. Tanımlar
- **Yazılım:** Yukarıda tanımlanan saQut derleyicisi ve tüm bileşenleri (kaynak kod, yorumlayıcı, bytecode VM, derleyici ve transpiler backend'leri, dokümantasyon ve mimari tasarım).
- **Çıktı:** Yazılım kullanılarak üretilen her şey; saQut dilinde yazılmış kaynak kodlar (`.sqt` dosyaları), token/AST/sembol tablosu dökümleri, üretilen bytecode, transpile edilmiş kaynak kod, bağımsız çalıştırılabilir dosyalar ve Yazılımın işlediği verilerden elde edilen sonuçlar.
- **Ticari Kullanım:** Yazılımın kendisinden doğrudan veya dolaylı olarak gelir elde edilmesi; Yazılımın bir ürün, hizmet, motor, bileşen veya otomasyon aracı olarak Üçüncü Taraflara sunulması, kurulması ya da onlar adına işletilmesi.
- **Üçüncü Taraf:** Yazılımı kullanan kişi veya kurum dışında kalan müşteri, son kullanıcı ya da diğer taraflar.

#### 3. İnceleme, Değiştirme ve Ticari Olmayan Kullanım İzni
İşbu lisans kapsamında herkese, ücretsiz olarak aşağıdaki haklar tanınır:
- Yazılımı kişisel, eğitsel ve ticari olmayan amaçlarla kullanmak, incelemek ve çalıştırmak.
- Yazılımın kaynak kodunu incelemek ve değiştirmek.
- Değiştirilmiş veya değiştirilmemiş kopyaları ticari olmayan koşullarda dağıtmak. Ancak dağıtılan her kopya; (a) işbu lisansa tabi kalmalı, (b) kaynağı açık (source-available) kalmalı ve (c) aşağıdaki Atıf Şartı'na uymalıdır.

#### 4. Çıktı ve Sonuç İstisnası (KRİTİK)
Yazılımı kullanan geliştiricilerin özgürlüğünü güvence altına almak amacıyla:
- **Çıktı sizindir.** Bölüm 2'de tanımlanan tüm Çıktı, işbu lisansın kısıtlamalarına **tabi değildir.** Geliştiriciler kendi Çıktılarını diledikleri gibi kullanma, kapatma (kapalı kaynak yapma), lisanslama ve **ticari olarak satma** hakkına tam olarak sahiptir.
- **Gelir, Çıktıdan elde edilir.** Yazılımı kendi makinenizde (kişisel veya kurumsal) özel bir araç olarak kullanıp ürettiğiniz programları, bağımsız çalıştırılabilir dosyaları veya işlenmiş verileri (örneğin bir Excel raporu veya bir veritabanı sonucu) satmanız serbesttir. Bu durumda Üçüncü Taraf, Yazılımı değil yalnızca sonucu alır.

#### 5. Ticari Kısıtlamalar
Aşağıdaki kullanımlar, Yazılımın bizzat kendisinden gelir elde edilmesi anlamına geldiğinden, telif hakkı sahibinin önceden yazılı izni olmaksızın yapılamaz:
- Yazılımı bir Üçüncü Taraf'ın sunucusuna veya cihazına kurup çalıştırılması karşılığında ücret almak (örneğin bir müşteri için `saqut run yedek.sql` komutunu çalıştırıp ücret talep etmek).
- Yazılımı çevrimiçi bir hizmet olarak sunmak (Web tabanlı IDE, çevrimiçi derleme veya çalıştırma servisi).
- Yazılımı, Üçüncü Tarafların canlı istekleriyle işleyen ticari bir hizmetin backend'i veya motoru hâline getirmek.
- Yazılımı ticari bir otomasyon/iş hattının (pipeline) aracı olarak işletmek ya da bir yapay zekâ sistemine araç olarak sağlamak.
- Yazılımı ticari bir ürünün alt bileşeni olarak gömmek (embed).
- Yazılımın iç bileşenlerini veya mimari tasarımını (örneğin AST görüntüleme veya optimizasyon algoritmaları) ticari bir özellik ya da ürün olarak yeniden kullanmak.

**Sunucu ve sürüm notu:** Yazılım çalışmak için saQut çalışma zamanına (yorumlayıcı / bytecode VM) ihtiyaç duyduğu sürece, sunucu tarafındaki her ticari kullanım fiilen Yazılımı sunucuya yerleştirmek anlamına gelir ve bu nedenle yukarıdaki kısıtlamalara tabidir. Yazılımın, saQut çalışma zamanına ihtiyaç duymayan bağımsız çalıştırılabilir dosyalar üreten backend'i kullanıma sunulduğunda; bu tür bağımsız Çıktıların sunucuda ticari olarak çalıştırılması Bölüm 4 kapsamında serbesttir.

#### 6. İzin ve Ticari Lisans
Bölüm 5'te sayılan kısıtlamaların tamamı yalnızca telif hakkı sahibine aittir ve istisna tanıma yetkisi münhasıran ona aittir. Bu kullanımlardan herhangi birini gerçekleştirmek isteyen kişi veya kurum, telif hakkı sahibinden (Abdussamed ULUTAŞ) önceden yazılı izin talep edebilir; izin verildiği takdirde ilgili kullanım serbest hâle gelir.
İletişim (ticari lisans talepleri): saqutsoftware+gitea@gmail.com

#### 7. Atıf Şartı
Orijinal telif hakkı bildirimi ile işbu izin bildirimi, Yazılımın tüm kopyalarına veya önemli parçalarına dâhil edilmelidir.

#### 8. Garanti Reddi
YAZILIM "OLDUĞU GİBİ" SUNULMAKTADIR; TİCARETE UYGUNLUK, BELİRLİ BİR AMACA UYGUNLUK VE İHLAL ETMEME GARANTİLERİ DAHİL ANCAK BUNLARLA SINIRLI OLMAMAK ÜZERE, AÇIK VEYA ZIMNİ HİÇBİR GARANTİ VERİLMEMEKTEDİR. YAZARLAR VEYA TELİF HAKKI SAHİPLERİ, YAZILIMLA VEYA YAZILIMIN KULLANIMIYLA YA DA DİĞER İŞLEMLERLE BAĞLANTILI OLARAK ORTAYA ÇIKAN SÖZLEŞME, HAKSIZ FİİL VEYA DİĞER DURUMLARDAKİ HİÇBİR TALEP, TAZMİNAT VEYA DİĞER YÜKÜMLÜLÜKLERDEN SORUMLU TUTULAMAZ.

---

### English License Version

# The saQut License (Version 1.0)
**Source-Available — Commercial-Use Restricted License**

Copyright (c) 2026, Abdussamed ULUTAŞ (and saQut Contributors). All rights reserved.

This license applies to the saQut compiler, its source code, build scripts, all backend components (the interpreter, the bytecode VM, the compiler, and the transpiler), and the accompanying documentation and architectural design (including the ADR documents) — collectively referred to as "the Software".

saQut is an open-source (source-available) project; however, it is **not free/libre software.** While inspecting and modifying the Software is permitted, its commercial use is subject to the terms of this license and to the permission of the copyright holder.

#### 1. Core Philosophy (The Toolbox Rule)
saQut is designed as a programmable, inspectable compiler toolbox. Its reason for existence is to keep every stage of the compilation process — tokens, the Abstract Syntax Tree (AST), symbol tables, and the Intermediate Representation (IR) — transparent and observable from the outside. Any redistribution or modification of the Software must preserve this principle of transparency.

#### 2. Definitions
- **The Software:** The saQut compiler and all of its components as defined above (source code, interpreter, bytecode VM, compiler and transpiler backends, documentation, and architectural design).
- **Output:** Anything produced using the Software; source code written in the saQut language (`.sqt` files), token/AST/symbol-table dumps, generated bytecode, transpiled source code, standalone executables, and any results derived from data processed by the Software.
- **Commercial Use:** Deriving revenue, directly or indirectly, from the Software itself; offering, deploying, or operating the Software as a product, service, engine, component, or automation tool for or on behalf of Third Parties.
- **Third Party:** Any customer, end user, or other party other than the person or organization using the Software.

#### 3. Permission to Inspect, Modify, and Use Non-Commercially
This license grants everyone, free of charge, the following rights:
- To use, inspect, and run the Software for personal, educational, and non-commercial purposes.
- To inspect and modify the source code of the Software.
- To distribute modified or unmodified copies under non-commercial terms, provided that each distributed copy (a) remains under this license, (b) remains source-available, and (c) complies with the Attribution Requirement below.

#### 4. The Output & Results Exception (CRITICAL)
To safeguard the freedom of developers using the Software:
- **Your Output is yours.** All Output as defined in Section 2 is **not** subject to the restrictions of this license. Developers retain the full right to use, close (make proprietary), license, and **sell commercially** their own Output.
- **Revenue is earned from the Output.** You are free to use the Software as a private tool on your own machine (personal or corporate) and to sell the programs, standalone executables, or processed data you produce (for example, an Excel report or a database result). In such cases the Third Party receives only the result, not the Software.

#### 5. Commercial Restrictions
Because the following uses amount to deriving revenue from the Software itself, they may not be carried out without the prior written permission of the copyright holder:
- Installing the Software on a Third Party's server or device and charging for its execution (for example, running `saqut run backup.sql` for a customer for a fee).
- Offering the Software as an online service (a web-based IDE, or an online compilation or execution service).
- Making the Software the backend or engine of a commercial service that is driven by the live requests of Third Parties.
- Operating the Software as a tool within a commercial automation pipeline, or providing it as a tool to an artificial-intelligence system.
- Embedding the Software as a sub-component of a commercial product.
- Repurposing the internal components or architectural design of the Software (for example, its AST visualization or optimization algorithms) as a commercial feature or product.

**Server and version note:** As long as the Software requires the saQut runtime (the interpreter / bytecode VM) in order to run, any commercial server-side use effectively places the Software on the server and is therefore subject to the restrictions above. Once a backend that produces standalone executables not requiring the saQut runtime becomes available, running such standalone Output commercially on a server is permitted under Section 4.

#### 6. Permission and Commercial Licensing
All restrictions listed in Section 5 belong solely to the copyright holder, who has the exclusive authority to grant exceptions. Any person or organization wishing to carry out any of these uses may request prior written permission from the copyright holder (Abdussamed ULUTAŞ); once granted, the relevant use becomes permitted.
Contact (commercial-license requests): saqutsoftware+gitea@gmail.com

#### 7. Attribution Requirement
The original copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

#### 8. Disclaimer of Warranty
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

---

### Deutsche Lizenzversion

# Die saQut-Lizenz (Version 1.0)
**Quelloffene Lizenz mit eingeschränkter kommerzieller Nutzung**

Copyright (c) 2026, Abdussamed ULUTAŞ (und saQut-Mitwirkende). Alle Rechte vorbehalten.

Diese Lizenz gilt für den saQut-Compiler, seinen Quellcode, die Build-Skripte, alle Backend-Komponenten (den Interpreter, die Bytecode-VM, den Compiler und den Transpiler) sowie die dazugehörige Dokumentation und den architektonischen Entwurf (einschließlich der ADR-Dokumente) — zusammenfassend als „die Software" bezeichnet.

saQut ist ein quelloffenes (einsehbares) Projekt; es ist jedoch **keine freie Software (Free/Libre Software).** Während das Einsehen und Verändern der Software gestattet ist, unterliegt ihre kommerzielle Nutzung den Bedingungen dieser Lizenz und der Genehmigung des Urheberrechtsinhabers.

#### 1. Die Kernphilosophie (Die Toolbox-Regel)
saQut ist als programmierbare, inspizierbare Compiler-Toolbox konzipiert. Ihr Daseinszweck ist es, jede Phase des Kompilierungsprozesses — Tokens, den Abstrakten Syntaxbaum (AST), Symboltabellen und die Zwischendarstellung (IR) — von außen transparent und einsehbar zu halten. Jede Weitergabe oder Veränderung der Software muss diesen Grundsatz der Transparenz wahren.

#### 2. Definitionen
- **Die Software:** Der oben definierte saQut-Compiler und alle seine Komponenten (Quellcode, Interpreter, Bytecode-VM, Compiler- und Transpiler-Backends, Dokumentation und architektonischer Entwurf).
- **Ausgabe:** Alles, was mithilfe der Software erzeugt wird; in der saQut-Sprache geschriebener Quellcode (`.sqt`-Dateien), Token-/AST-/Symboltabellen-Ausgaben, erzeugter Bytecode, transpilierter Quellcode, eigenständige ausführbare Dateien sowie alle Ergebnisse, die aus von der Software verarbeiteten Daten gewonnen werden.
- **Kommerzielle Nutzung:** Das direkte oder indirekte Erzielen von Einnahmen aus der Software selbst; das Anbieten, Bereitstellen oder Betreiben der Software als Produkt, Dienst, Engine, Komponente oder Automatisierungswerkzeug für oder im Auftrag von Dritten.
- **Dritte:** Jeder Kunde, Endnutzer oder sonstige Partei außer der Person oder Organisation, die die Software nutzt.

#### 3. Genehmigung zum Einsehen, Verändern und zur nicht-kommerziellen Nutzung
Diese Lizenz gewährt jeder Person kostenlos die folgenden Rechte:
- Die Software zu persönlichen, schulischen/bildenden und nicht-kommerziellen Zwecken zu nutzen, einzusehen und auszuführen.
- Den Quellcode der Software einzusehen und zu verändern.
- Veränderte oder unveränderte Kopien unter nicht-kommerziellen Bedingungen weiterzugeben, sofern jede weitergegebene Kopie (a) dieser Lizenz unterstellt bleibt, (b) quelloffen (source-available) bleibt und (c) der nachstehenden Namensnennung-Bedingung entspricht.

#### 4. Die Ausnahme für Ausgabe und Ergebnisse (KRITISCH)
Um die Freiheit der Entwickler zu gewährleisten, die die Software nutzen:
- **Ihre Ausgabe gehört Ihnen.** Die gesamte in Abschnitt 2 definierte Ausgabe unterliegt **nicht** den Beschränkungen dieser Lizenz. Entwickler behalten das uneingeschränkte Recht, ihre eigene Ausgabe zu nutzen, zu schließen (proprietär zu machen), zu lizenzieren und **kommerziell zu verkaufen.**
- **Einnahmen werden aus der Ausgabe erzielt.** Es steht Ihnen frei, die Software als privates Werkzeug auf Ihrem eigenen Rechner (privat oder geschäftlich) zu nutzen und die von Ihnen erzeugten Programme, eigenständigen ausführbaren Dateien oder verarbeiteten Daten (etwa einen Excel-Bericht oder ein Datenbankergebnis) zu verkaufen. In diesem Fall erhält der Dritte nur das Ergebnis, nicht die Software.

#### 5. Kommerzielle Beschränkungen
Da die folgenden Nutzungen darauf hinauslaufen, Einnahmen aus der Software selbst zu erzielen, dürfen sie ohne vorherige schriftliche Genehmigung des Urheberrechtsinhabers nicht vorgenommen werden:
- Die Software auf dem Server oder Gerät eines Dritten zu installieren und für ihre Ausführung ein Entgelt zu verlangen (zum Beispiel das Ausführen von `saqut run backup.sql` für einen Kunden gegen Bezahlung).
- Die Software als Online-Dienst anzubieten (eine webbasierte IDE oder einen Online-Kompilier- oder -Ausführungsdienst).
- Die Software zum Backend oder zur Engine eines kommerziellen Dienstes zu machen, der durch die Live-Anfragen Dritter angetrieben wird.
- Die Software als Werkzeug innerhalb einer kommerziellen Automatisierungs-Pipeline zu betreiben oder sie einem System künstlicher Intelligenz als Werkzeug bereitzustellen.
- Die Software als Unterkomponente eines kommerziellen Produkts einzubetten (embedden).
- Die internen Komponenten oder den architektonischen Entwurf der Software (zum Beispiel ihre AST-Visualisierung oder Optimierungsalgorithmen) als kommerzielles Merkmal oder Produkt wiederzuverwenden.

**Server- und Versionshinweis:** Solange die Software zur Ausführung die saQut-Laufzeitumgebung (den Interpreter / die Bytecode-VM) benötigt, bedeutet jede kommerzielle serverseitige Nutzung faktisch das Platzieren der Software auf dem Server und unterliegt daher den obigen Beschränkungen. Sobald ein Backend verfügbar ist, das eigenständige ausführbare Dateien erzeugt, die die saQut-Laufzeitumgebung nicht benötigen, ist das kommerzielle Ausführen einer solchen eigenständigen Ausgabe auf einem Server gemäß Abschnitt 4 gestattet.

#### 6. Genehmigung und kommerzielle Lizenzierung
Alle in Abschnitt 5 aufgeführten Beschränkungen stehen ausschließlich dem Urheberrechtsinhaber zu, der allein befugt ist, Ausnahmen zu gewähren. Jede Person oder Organisation, die eine dieser Nutzungen vornehmen möchte, kann beim Urheberrechtsinhaber (Abdussamed ULUTAŞ) eine vorherige schriftliche Genehmigung beantragen; nach deren Erteilung wird die betreffende Nutzung gestattet.
Kontakt (Anfragen für kommerzielle Lizenzen): saqutsoftware+gitea@gmail.com

#### 7. Namensnennung-Bedingung
Der ursprüngliche Urheberrechtshinweis und dieser Genehmigungshinweis müssen in allen Kopien oder wesentlichen Teilen der Software enthalten sein.

#### 8. Haftungsausschluss
DIE SOFTWARE WIRD „WIE BESEHEN" (AS IS) OHNE JEGLICHE AUSDRÜCKLICHE ODER STILLSCHWEIGENDE GEWÄHRLEISTUNG ZUR VERFÜGUNG GESTELLT, EINSCHLIESSLICH, ABER NICHT BESCHRÄNKT AUF DIE GEWÄHRLEISTUNG DER MARKTGÄNGIGKEIT, DER EIGNUNG FÜR EINEN BESTIMMTEN ZWECK UND DER NICHTVERLETZUNG VON RECHTEN DRITTER. IN KEINEM FALL SIND DIE AUTOREN ODER URHEBERRECHTSINHABER FÜR ANSPRÜCHE, SCHÄDEN ODER ANDERE HAFTUNGEN HAFTBAR, SEI ES IN FOLGE EINES VERTRAGES, EINER UNERLAUBTEN HANDLUNG ODER AUF ANDERE WEISE, DIE SICH AUS ODER IM ZUSAMMENHANG MIT DER SOFTWARE ODER DER NUTZUNG ODER DEM UMGANG MIT DER SOFTWARE ERGEBEN.

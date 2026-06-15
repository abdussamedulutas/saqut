// Faz 0 — DiagnosticEngine birim testleri (çerçevesiz; assert + çıktı).
// Koşmak için: tests/run.sh
#include "diagnostic/diagnostic_engine.hpp"
#include <iostream>
#include <cassert>

int main() {
    DiagnosticEngine diag;
    SourceLocation l1{"test.sqt", 3, 5, 40};
    SourceLocation l2{"test.sqt", 7, 9, 88};
    SourceLocation l3{"test.sqt", 12, 1, 150};

    // Üç tanı — ekleme sırası korunmalı (ADR-013: ilk hatada durma, topla)
    diag.report("E001", l1, "x tanımsız");
    diag.report("E003", l2, "int'e string atanamaz", "açık dönüşüm gerekiyor");
    diag.report("W001", l3, "y kullanılmıyor");

    assert(diag.count() == 3);
    assert(diag.hasErrors());
    assert(diag.errorCount() == 2);
    assert(diag.warningCount() == 1);

    // Sıra korunmuş mu?
    assert(diag.all()[0].code == "E001");
    assert(diag.all()[1].code == "E003");
    assert(diag.all()[2].code == "W001");

    // Seviye kataloğdan çözülmüş mü?
    assert(diag.all()[0].level == DiagLevel::Error);
    assert(diag.all()[2].level == DiagLevel::Warning);

    // Katalog erişimi
    assert(findDiag("E010") != nullptr);
    assert(findDiag("E999") == nullptr);

    std::cout << "--- printAll ---\n";
    diag.printAll(std::cout);
    std::cout << "--- toJson ---\n";
    std::cout << diag.toJson() << "\n";

    std::cout << "test_diagnostic: TUM TESTLER GECTI\n";
    return 0;
}

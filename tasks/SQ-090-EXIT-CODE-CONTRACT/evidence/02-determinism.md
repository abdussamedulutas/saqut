# 02-determinism.md

## Test: run + FX-SYNTAX (3 tekrar)
```
  Run 1: exit=65 stdout=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 stderr=0ce6eac9b3e08cf3328e280cd27957a578e03c413da4cc582590314fe6b8cc44
  Run 2: exit=65 stdout=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 stderr=0ce6eac9b3e08cf3328e280cd27957a578e03c413da4cc582590314fe6b8cc44
  Run 3: exit=65 stdout=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 stderr=0ce6eac9b3e08cf3328e280cd27957a578e03c413da4cc582590314fe6b8cc44
```

## Test: run + FX-RUNTIME (mod_by_zero.sqt, 3 tekrar)
```
  Run 1: exit=70 stdout=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 stderr=0e834bd0ecb7a2d93eb703a037f70056aa764f1b346cd557b96db9a1c8e52685
  Run 2: exit=70 stdout=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 stderr=0e834bd0ecb7a2d93eb703a037f70056aa764f1b346cd557b96db9a1c8e52685
  Run 3: exit=70 stdout=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855 stderr=0e834bd0ecb7a2d93eb703a037f70056aa764f1b346cd557b96db9a1c8e52685
```

## Sonuç
Seçilen `run` + `FX-SYNTAX` ve `run` + `FX-RUNTIME` kombinasyonları, bu ortamda üç tekrar boyunca byte-identical sonuç üretti.

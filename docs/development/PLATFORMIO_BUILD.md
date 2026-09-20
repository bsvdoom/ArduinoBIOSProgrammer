# PlatformIO Arduino Uno build — 7/12

- Ellenőrzés dátuma: 2026-09-20; időzóna: Europe/Budapest.
- Cél: az eredeti Arduino library- és example-struktúra reprodukálható Uno buildje.
- Upload nem történt; a vizsgálat kizárólag fordítás és natív tesztelés volt.

## Projektelrendezés

A gyökérszintű `platformio.ini` az eredeti sketch könyvtárát állítja projektforrásnak:

```ini
[platformio]
src_dir = examples/BIOSprog

[env:uno]
platform = atmelavr@5.3.0
board = uno
framework = arduino
monitor_speed = 115200
lib_extra_dirs = ..
```

Az elrendezés indoka:

- az eredeti `examples/BIOSprog/BIOSprog.ino` marad a sketch, átnevezés vagy `main.cpp` wrapper nélkül;
- a repó gyökere továbbra is Arduino library, ezért a PlatformIO LDF a meglévő `library.properties` és `src/` alapján találja meg;
- a `src/programmer.cpp`, `src/winbondflash.cpp` és `src/xmodem.cpp` közvetlenül az eredeti helyükről, pontosan egyszer fordul;
- nincs másolat, symlink, abszolút út vagy extra buildscript;
- a hibás eredetű `library.properties` metadata nem akadályozta a buildet, ezért nem változott.

Elsőként a gyökér `src_dir = .` és `build_src_filter` megoldást vizsgáltam. A PlatformIO Core 6.2.0 ebben az elrendezésben a beágyazott `.ino` fájlból nem hozott létre linkelt `setup()`/`loop()` implementációt. A sketch könyvtárának `src_dir`-ré tétele a támogatott Arduino `.ino` preprocesszálást használja, míg a könyvtárat a támogatott `lib_extra_dirs` opció találja meg. A végleges konfiguráció nem használja az elavult `src_filter` opciót.

## Parancsok

A repó gyökeréből:

```sh
pio run -e uno
pio run -e uno -t clean
pio run -e uno
```

A karakterizáló csomag:

```sh
bash tests/characterization/run_all.sh
```

A gépen lévő `~/.local/bin/pio` belépési pont használhatatlan volt, mert a hozzá tartozó `platformio` Python-modul hiányzott. A build ellenőrzéséhez ezért repón kívüli `/tmp` venv készült, benne rögzített `platformio==6.2.0` verzióval. Sem a venv, sem a külön PlatformIO Core/package tár nem került a repóba.

## Verziók

| Összetevő | Verzió |
| --- | --- |
| PlatformIO Core | 6.2.0 |
| PlatformIO Atmel AVR platform | 5.3.0, `platform = atmelavr@5.3.0` |
| PlatformIO Arduino AVR framework csomag | 5.4.0 |
| ArduinoCore-avr | 1.8.8 (`platform.txt`) |
| AVR toolchain csomag | 1.70300.191015 |
| `avr-g++` | GCC 7.3.0 |
| Uno MCU/órajel | ATmega328P, 16 MHz |

## Build eredménye és forráslista

Az `pio run -e uno` és a teljes clean rebuild is PASS. A sketch `BIOSprog.ino.cpp.o` objektummá preprocesszálódott. A három produkciós C++ egység:

```text
.pio/build/uno/liba15/ArduinoBIOSProgrammer/programmer.cpp.o
.pio/build/uno/liba15/ArduinoBIOSProgrammer/winbondflash.cpp.o
.pio/build/uno/liba15/ArduinoBIOSProgrammer/xmodem.cpp.o
```

Az objektumlista összesen három találatot adott; egyik produkciós `.cpp` sem fordult kétszer. Az SPI könyvtár és az Arduino framework saját forrásai ezen felül, a szokásos módon fordultak.

## Memóriahasználat

| Memória | Használat | Keret | Arány |
| --- | ---: | ---: | ---: |
| programmemória | 8406 bájt | 32256 bájt | 26,1% |
| dinamikus memória / RAM | 243 bájt | 2048 bájt | 11,9% |

A build létrehozta a `firmware.elf` és `firmware.hex` fájlokat a kizárt `.pio/build/uno/` könyvtárban.

## Warningok

A build hat, már korábban ismert `-Wsign-compare` warningot adott a `src/winbondflash.cpp` táblabejáró ciklusaira:

- `checkPartNo()` két ciklusa: 158. és 181. sor;
- `bytes()`: 238. sor;
- `pages()`: 263. sor;
- `sectors()`: 275. sor;
- `blocks()`: 287. sor.

Mindegyik az `int i` és a `sizeof(...)` unsigned eredményének összehasonlítása. A kérés szerint nem javítottam őket, és nincs `-Werror`. Új produkciós, framework- vagy PlatformIO-konfigurációs warning nem maradt. A PlatformIO által kiírt LDF-, dependency- és memóriaüzenetek információk, nem warningok.

## Clean rebuild és tesztek

| Ellenőrzés | Eredmény |
| --- | --- |
| karakterizáló csomag a PlatformIO-változtatás előtt | 71 PASS, 0 MISSING SAFEGUARD, 0 váratlan hiba |
| első sikeres `pio run -e uno` | PASS |
| karakterizáló csomag a sikeres build után | 71 PASS, 0 MISSING SAFEGUARD, 0 váratlan hiba |
| ASan / UBSan | nincs jelentett hiba |
| ismételt `pio run -e uno` | PASS |
| `pio run -e uno -t clean` | PASS |
| clean utáni `pio run -e uno` | PASS, azonos 8406 B flash / 243 B RAM |

## Buildtermékek és Git

A `.gitignore` egyetlen PlatformIO-specifikus mintával bővült:

```gitignore
.pio/
```

A `git status --short --ignored` szerint a teljes `.pio/` könyvtár ignored. ROM-fájlokra nem került új ignore-szabály.

## Linux, Windows és későbbi CI

A konfiguráció csak PlatformIO-projektopciókat és relatív útvonalat (`..`) használ. Linuxon a repó gyökeréből, Windowson PowerShellből vagy PlatformIO Core CLI terminálból ugyanaz a parancs használható:

```text
pio run -e uno
```

A checkout könyvtárneve nincs beégetve. A szülő könyvtárban lévő más könyvtárakat az LDF átvizsgálhatja, de csak a `programmer.h` include-ot biztosító `ArduinoBiosProgrammer` könyvtár kerül a dependency graphba.

Későbbi CI-integráció minimális ellenőrző parancsa szintén `pio run -e uno`; CI-konfiguráció ebben a lépésben szándékosan nem készült.

## Szándékosan kihagyott műveletek

- Nem történt firmware-upload vagy fizikai Arduino-hozzáférés.
- Nem változott produkciós firmware-kód, Python-szkript, ROM, README vagy `library.properties`.
- Nem készült CI-konfiguráció.
- A 8/12. lépés nem kezdődött el.

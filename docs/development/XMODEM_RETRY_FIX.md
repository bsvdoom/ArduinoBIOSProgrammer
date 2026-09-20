# XMODEM újraküldési javítás — 5A/12.

Dátum: 2026-09-06. Cél: Arduino Uno; kizárólag a küldő NAK utáni mutatókezelése.

## Technikai ok és minimális javítás

A `XModem::block_send()` első küldéskor a `*dst++` olvasással 128 bájttal előreléptette a helyi bemeneti mutatót. NAK után a `goto start` nullázta a CRC-t és újraküldte ugyanazt a fejlécet, de a mutatót nem állította vissza. A második payload így már a forrásbufferen kívülről olvasott; ezt az AddressSanitizer reprodukálta.

Egyetlen produkciós sor változott: `src/xmodem.cpp:114`, `uint8_t octet = *dst++;` helyett `uint8_t octet = dst[i];`. A mutató a blokk elején marad, a meglévő `for` minden próbálkozáskor `i = 0` értékről indul, és csak a 0–127 indexeket olvassa. A fejléc és a CRC-képzés változatlan, így minden próbálkozás ugyanazt a 133 bájtos csomagot küldi, az eredeti bájtsorrendben.

A `block++` és a `NEXT` visszatérés továbbra is kizárólag ACK után történik. A hívó a következő forrásblokkot csak NEXT után küldi; NAK esetén ugyanazon függvényhíváson belül marad. A flashciklus, a fogadóág, az időzítések és a vezérlőkarakterek nem változtak.

## Teszteredmények

Parancs: `python3 -B tests/characterization/run_tests.py`.

- Előtte: **11 PASS, 1 KNOWN FAILURE, 0 váratlan hiba**. A retry tesztben ASan `heap-buffer-overflow`.
- A produkciós javítás után, még az eredeti futtatóval: **12 PASS, 0 KNOWN FAILURE, 0 váratlan hiba**.
- Csak ezt követően a futtató retry-státusza `known_failure` helyett `pass` lett. A teljes csomag újrafuttatva: **12 PASS, 0 KNOWN FAILURE, 0 váratlan hiba**.
- AddressSanitizer és UndefinedBehaviorSanitizer bekapcsolva; a javítás utáni futásokban nincs memóriadiagnosztikai hiba vagy bájtpárcsere.
- A C++ teszt és a teljes első/ismételt csomagot bájtról bájtra összevető `std::equal` ellenőrzés változatlan. A két dokumentált Programmer-eset továbbra is NOT TESTABLE.

## AVR szintaxisellenőrzés

A 4/12. lépésben használt helyi AVR GCC 7.3.0 és Arduino AVR core 1.8.7 mellett mindhárom C++ fordítási egység szigorú ellenőrzése sikeres: kilépési kód 0, diagnosztika nélkül.

```sh
$HOME/.arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-g++ \
  -std=gnu++11 -Os -mmcu=atmega328p -DF_CPU=16000000L \
  -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/cores/arduino \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/variants/standard \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/libraries/SPI/src \
  -Isrc -fsyntax-only src/programmer.cpp src/winbondflash.cpp src/xmodem.cpp
```

Ez szintaxisellenőrzés, teljes linkelés vagy hardveres próba nem történt. Telepítés nem volt.

## Szándékosan érintetlen problémák

- Pontosan chipméretű írásnál az utolsó ACK/EOT kezelése a kapacitáshatár miatt elmaradhat.
- Korai EOF/EOT esetén továbbra is teljes 256 bájtos lap íródhat ki hiányos vagy korábbi buffertartalommal.
- A küldési lezárás változatlanul EOT, ACK-várás, ETB és további várás; a hívó továbbra sem értékeli a `finish_send()` eredményét.
- A fogadóoldali retry mutatóhibája nem része ennek a javításnak. Az 5B/12. lépés nem kezdődött el.

A 4/12. lépés meglévő módosításai megmaradtak. Nem történt járulékos formázás, ROM- vagy hostscript-módosítás, PlatformIO-konfiguráció, staging vagy commit.

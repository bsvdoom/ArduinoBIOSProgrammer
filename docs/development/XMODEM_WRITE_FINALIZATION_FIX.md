# Pontos méretű írás és lezárás — 5B/12.

Dátum: 2026-09-06. Cél: Arduino Uno. A 4/12. buildjavítás és az 5A/12. küldési retry-javítás megőrizve.

## Eredeti hibák és kiváltó feltételek

1. A Programmer az END eredményt is lapírásra jogosító eredményként kezelte, és minden fogadási hívás után csökkentette a hiányzó blokkok számát. Nulla adat utáni EOT így üres/korábbi buffert, 128 bájt utáni EOT hiányos lapot, teljes lap utáni korai EOT pedig az előző lap ismétlését írhatta ki. Mindegyik rövid átvitel sikeresnek látszhatott.
2. Az XMODEM minden EOT-ra ACK-ot és END-et adott, a Programmer által várt összméret ismerete nélkül. A korai EOT ezért a küldőnek is sikeres protokolllezárást jelzett.
3. Az író ciklus a kapacitás betöltésekor NEXT állapotban kilépett. Az adat-ACK csak a következő `block_receive()` elején keletkezik, ezért az utolsó blokk ACK-ja, az EOT fogadása és az EOT ACK-ja elmaradt. Pontos bemenetre is `Out of memory` érkezett.
4. Túlméretes bemenetnél ugyanaz a korai kilépés történt: a következő blokk nem kapott explicit feldolgozást és megszakítást. A hiányzó EOT-t sem különböztette meg a pontos méretű sikeres lezárástól.

## A teszthám elakadása

Az első, `write_empty_silence` teszt akadt el. A tesztenkénti futtatás és az opcionális `WRITE_TRACE=1` napló kimutatta: az XMODEM tíz `C` után timeouttal visszatért, a `Programmer::task()` is befejeződött. A várakozás a teszt konzolnaplójának visszaolvasásában volt, nem a soros protokollban.

A duplex UART helyett használt `tmpfile()` update streamet a hám olvasás és írás között megfelelő pozicionálás nélkül, `ungetc()`-vel kezelte. A tesztjavítás valódi fájlba írt parancsot és explicit `fseek` átmeneteket használ; a naplóolvasás 4096 bájtra korlátozott. A host `vfprintf_P` adapter az AVR és a host `uint32_t` formátumkülönbségét is kezeli. Produkciós konzolkód nem változott.

A fake Serial ellenőrzése szerint a következő keret csak az előző ACK-ja után olvasható; EOT csak az utolsó adat-ACK után válik elérhetővé. Üres bemeneten az `available()` nulla, a `read()` −1. A bemenet vége utáni ismételt ACK nem számít új adat-ACK-nak. A teszt bináris `--list` és tesztnév argumentumot fogad; minden eset külön folyamat, indulás előtt megjelenő név, 2 másodperces timeout és 1 másodperces kényszerleállítási tartalék mellett fut. A fake `delay()` nem vár.

## Baseline és teszteredmények

A teszt a valódi `programmer.cpp` és `xmodem.cpp` forrásokat fordítja, a flash-metódusokat a teszt linkeléskor helyettesíti a valódi fejléc alapján. Nincs lemásolt írási algoritmus vagy produkciós tesztelhetőségi refaktor. A fake kapacitása 256, 512 vagy 768 bájt; a lapcímeket, nyers payloadokat és protokoll-eseménysort ellenőrzi. Automatikus erase/readback hívás teszthibát okoz.

| Futás | Korábbi tesztek | Új írási tesztek | Összesen |
| --- | --- | --- | --- |
| Produkciós javítás előtti baseline, kétszer | 12 PASS | 3 PASS, 11 KNOWN FAILURE | 15 PASS, 11 KNOWN FAILURE |
| Produkciós javítás után, normál szigorú futtató | 12 PASS | 14 PASS | 26 PASS, 0 KNOWN FAILURE |

Mindkét baseline futás befejeződött, a teljes kimenet bájtról bájtra egyezett. Váratlan hiba és folyamat-timeout egyik baseline-ban sem volt. Csak ezután változott a produkciós kód. A végső futásban nincs ASan-/UBSan-hiba, bájtpárcsere vagy váratlan hiba; az 5A retry teszt változatlanul PASS. A meglévő leak-detection kikapcsolva maradt, ezért ez nem szivárgásmentességi igazolás.

Parancsok:

```sh
# Csak a dokumentált, pontos assertion-hibák kaphatnak KNOWN FAILURE státuszt:
bash tests/characterization/run_all.sh --baseline
# Végső regressziós futás: minden hiba sikertelen futást jelent:
bash tests/characterization/run_all.sh
```

A 14 eset teljes előtte/utána táblázata a `CHARACTERIZATION.md` 5. szakaszában szerepel. Lefedik a csendet, négy korai EOT-helyzetet, három kis teljes kapacitást, mindkét végső ACK-ot, túlméretet, a lapírás sorrendjét, hiányzó második blokkot/EOT-t, hibás második CRC-t és az utolsó blokk duplikátumát. Az elvárások nem gyengültek a javítás után.

## Minimális produkciós javítás

- `src/programmer.cpp`: a `received` számláló csak NEXT eredményre nő 128 bájttal; a célmutató és a laphoz hiányzó blokkok száma szintén csak ekkor változik. Lapírás kizárólag két sikeres blokk után történik. Az adatfogadó hívások tiltják az EOT elfogadását. Pontos kapacitás után egy további fogadás kezeli a lezárást; új adatblokkra CAN és `Out of memory` következik, új flashírás nélkül. Az ERROR külön `Invalid file size` üzenetet kap.
- `src/xmodem.h`, `src/xmodem.cpp`: a `block_receive()` új `accept_eot` paramétere alapértelmezésben igaz, így a meglévő egyargumentumos hívások viselkedése megmarad. Hamis értéknél EOT-ra ACK/END helyett CAN/ERROR érkezik. Más protokollág nem változott; az 5A `dst[i]` küldési javítása érintetlen.

A flasholdal mérete, a címciklus, az SPI-parancsok, a write-enable és busy-várakozások változatlanok. A felismerhető chipek kapacitása 256-tal osztható; az utolsó lap a `[size - 256, size)` tartományt fedi. A plusz blokk csak a már meglévő RAM-lapbufferbe kerülhet; a flashcikluson kívül nincs lapírás.

## Protokollállapotok és eredmény

- **Pontos átvitel:** C → első blokk/NEXT → ACK → második blokk/NEXT → teljes lapírás → további blokkok/lapok → utolsó lapírás → utolsó adatblokk ACK → EOT → EOT ACK → END → `Write done`.
- **Korai EOT:** CAN → ERROR → `Invalid file size`. Nincs EOT-ACK és nincs hiányos vagy régi lap kiírása.
- **Hiányzó adat vagy EOT:** timeout, sikerjelzés nélkül. Teljes kapacitás után az utolsó adat-ACK már kimegy, de EOT nélkül nincs END.
- **Új blokk a kapacitás után:** a kapacitásig terjedő lapok megmaradnak; a plusz blokk nem kap ACK-ot és nem kerül flashbe. CAN és `Out of memory` zárja a hibás átvitelt.
- **Utolsó blokk duplikátuma:** az XMODEM meglévő duplikátumkezelése ACK-olja, új lapírás nélkül; a következő EOT szabályosan lezárhatja az átvitelt.

A korábban teljesen fogadott és kiírt lapok rövid/hibás átvitel esetén is megmaradnak. Nincs tranzakciós visszagörgetés. A pontos méret a 128 bájtos XMODEM-payloadok összegére vonatkozik; a kitöltés előtti hostfájlhosszt a protokoll nem közli.

## AVR ellenőrzés

A korábban használt helyi AVR GCC 7.3.0, Arduino AVR core 1.8.7 és Arduino Uno definíciók mellett a három C++ fordítási egység szigorú szintaxisellenőrzése sikeres: kilépési kód 0, diagnosztika nélkül.

```sh
$HOME/.arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-g++ \
  -std=gnu++11 -Os -mmcu=atmega328p -DF_CPU=16000000L \
  -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/cores/arduino \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/variants/standard \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/libraries/SPI/src \
  -Isrc -fsyntax-only src/programmer.cpp src/winbondflash.cpp src/xmodem.cpp
```

Teljes linkelés, firmware-méretmérés, feltöltés vagy hardveres írás nem történt.

## Szándékosan későbbre hagyott funkciók

- Automatikus erase, blank-check és visszaolvasásos verify; az erase külön parancs marad.
- A küldő `finish_send()` EOT/ETB viselkedése és a visszatérésének feldolgozása.
- A fogadó CRC-/részlegesblokk-újrapróbálásának már ismert mutatóhibája; a hibás CRC teszt itt újraküldés nélküli elutasítást vizsgál.
- Általános timeout-, zaj-, megszakítás utáni parancsszinkron- és erőforrás-kezelés, a meglévő XModem-foglalások felszabadítása.
- PlatformIO, Python-módosítások és hostfájlhossz-ellenőrzés.

Nem történt telepítés, ROM-módosítás, staging vagy commit. A 6/12. lépés nem kezdődött el.

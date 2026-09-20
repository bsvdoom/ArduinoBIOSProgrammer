# Arduino BIOS Programmer — hardver nélküli karakterizáló tesztek

- Aktuális lépés: 5B/12.; a 3/12. tesztcsomag és az 5A/12. retry-javítás megőrizve.
- Vizsgálat dátuma: 2026-09-06; időzóna: Europe/Budapest.
- Vizsgált HEAD: `9446c9fe1318a965eb482f32e1d46f20801f5c9c`, branch: `master`.
- Cél: az XMODEM-adatút karakterizálása és a pontos chipméretű írás lezárásának regressziós ellenőrzése.
- Háttéranyagként az `AUDIT.md`, `BASELINE.md` és `PROVENANCE.md` szolgált; egyik sem módosult.

## 1. Tesztkörnyezet

A natív teszt közvetlenül lefordítja és használja a repository jelenlegi `src/xmodem.cpp` fájlját. Nem készült második firmware-oldali XMODEM-implementáció. A tesztoldali minimális `Stream` interfész és a memóriabeli `FakeStream` csak az Arduino soros adatfolyamát helyettesíti (`tests/characterization/stubs/Stream.h`; `tests/characterization/xmodem_characterization.cpp:27-68`). A fake előre megadott kimeneti bájtszámoknál tud ACK, NAK vagy CAN választ elérhetővé tenni.

A tesztoldali keret a küldött 133 bájtos XMODEM-keretekből nyers, egybájtos payloadot emel ki és `uint8_t` értékekként hasonlít (`tests/characterization/xmodem_characterization.cpp:97-130`). A fogadási tesztcsomag előállításához szükséges CRC-kód kizárólag fixture-képző; nem helyettesíti a produkciós protokollkódot (`tests/characterization/xmodem_characterization.cpp:70-95`). A Python futtató sem XMODEM-et valósít meg: a standard library `subprocess` moduljával fordít és tesztenként elindítja a natív binárist.

Megfigyelt helyi eszközök:

- Python: CPython 3.14.4;
- fordító: `/usr/bin/g++`, GCC 15.2.0;
- sanitizerek: AddressSanitizer és UndefinedBehaviorSanitizer sikeresen bekapcsolva;
- buildhely: a Python `tempfile.TemporaryDirectory` által létrehozott ideiglenes könyvtár, amely a futás végén megszűnik (`tests/characterization/run_tests.py:81-88`).

A teljes, 26 tesztes csomag futtatása a repository gyökeréből:

```sh
bash tests/characterization/run_all.sh
```

A futtató először `-fsanitize=address,undefined -fno-omit-frame-pointer` kapcsolókkal fordítja össze a produkciós `src/xmodem.cpp`-t és a tesztet. Csak akkor használ sanitizerek nélküli fallbacket, ha ezt a fordító nem támogatja (`tests/characterization/run_tests.py:30-57,81-98`). Új csomag telepítése nem szükséges.


Az 5B/12. kiegészítés a valódi `src/programmer.cpp` és `src/xmodem.cpp` fájlokat fordítja, változatlan produkciós fejléceikkel. A `write_characterization.cpp` linkeléskor tesztoldali definíciókat ad a használt flash-metódusokhoz; a valódi `winbondflash.cpp` ebben a natív binárisban nincs linkelve. Így a tényleges `Programmer::initialize()` és `Programmer::task()` fut, produkciós dependency-injection refaktor nélkül. A fake `bytes()` 256, 512 vagy 768 bájtos kapacitást jelent, a fake flash minden lapcímet és adatot rögzít, a határt ellenőrzi, és erase/readback hívásra hibázik. Ez a vezérlési útvonal tesztje, nem a SPI-driver vagy a JEDEC-felismerés igazolása.

A `WritePeer` a következő csomagot csak `C`, illetve az előző csomag ACK-ja után teszi olvashatóvá. Az utolsó adat-ACK után válik elérhetővé az EOT. A `Stream::available()` csak a rendelkezésre álló adatot jelzi; `read()` bájtonként fogyaszt, üres bemenetre −1-et ad. A konzolhoz és a GPIO/SPI-deklarációkhoz kizárólag tesztoldali `Arduino.h` és `SPI.h` stub tartozik.

A shell futtató a változatlan Python futtatóval végzi el az eredeti 12 tesztet, majd mind a 14 írási tesztet külön folyamatban, indulás előtt kiírt névvel és **2 másodperces timeouttal, 1 másodperces kényszerleállítási tartalékkal** indítja. A natív írási build kötelezően ASan/UBSan mellett fut, fallback nélkül. A meglévő `detect_leaks=0` beállítás itt is érvényes: ez nem szivárgásvizsgálat. A fake `delay()` nem vár valós időben; a produkciós polling és retry-szám változatlan. A konzolnapló legfeljebb 4096 bájt lehet. Python-fájl nem módosult.

A `--baseline` kapcsoló kizárólag az alább dokumentált 11, pontos hibaüzenettel egyező assertion-hibát osztályozhatja KNOWN FAILURE-ként. Folyamat-timeout, sanitizer-hiba és más hiba nem kap ilyen kivételt. A kapcsoló nélküli normál futás minden hibát elutasít.

## 2. Eredmények

Státuszok jelentése:

- **PASS**: a helyes viselkedésre írt változatlan elvárás teljesült;
- **KNOWN FAILURE**: a helyes elvárás nem teljesült, és a már ismert produkciós hiba reprodukálódott;
- **NOT TESTABLE**: a korábbi dokumentum két Programmer-esetének történeti státusza; az 5B/12. linkeléskor behelyettesített fake-kel mindkettő futtathatóvá vált.

| # | Teszteset | Státusz | Tényleges eredmény |
| ---: | --- | --- | --- |
| 1 | `12 AF 34 56` küldési bájtsorrend | **PASS** | A csomagból nyers bájtonként visszafejtett első négy payload-bájt változatlan (`xmodem_characterization.cpp:133-141`). |
| 2 | `00 01 02 ... FF` küldése | **PASS** | Két egymást követő 128 bájtos blokk összefűzött payloadja pontosan egyezett a 256 bájtos bemenettel (`xmodem_characterization.cpp:143-150`). |
| 3 | Ismétlődő determinisztikus, 384 bájtos minta | **PASS** | Mindhárom blokk nyers payloadja bájtról bájtra egyezett (`xmodem_characterization.cpp:152-159`). |
| 4 | Pontosan 128 bájtos határ | **PASS** | Pontosan egy blokk készült, adateltérés nélkül (`xmodem_characterization.cpp:161-167`). |
| 5 | Több egymást követő blokk | **PASS** | Négy blokk sorrendje és tartalma változatlan maradt (`xmodem_characterization.cpp:169-176`). |
| 6 | Blokkszám 255 → 0 átfordulás | **PASS** | A vezetéken a 254., 255. és 256. csomag sorszáma rendre 254, 255 és 0 volt (`xmodem_characterization.cpp:178-197`). |
| 7 | `C` indítás és ACK-kezelés | **PASS** | A `start_send()` START, az ACK-kal lezárt `block_send()` NEXT eredményt adott (`xmodem_characterization.cpp:199-212`). |
| 8 | EOT-kezelés küldő- és fogadóoldalon | **PASS** | A küldő EOT/ACK után END-et adott és ETB-t küldött; a fogadó EOT-ra ACK-ot küldött és END-et adott (`xmodem_characterization.cpp:214-234`). |
| 9 | CAN-kezelés küldő- és fogadóoldalon | **PASS** | Mindkét ág CANCEL eredményt adott (`xmodem_characterization.cpp:236-249`). |
| 10 | Kezdési timeout | **PASS** | Bemenet nélkül, egy próbára állítva a `start_send()` TIMEOUT eredményt adott (`xmodem_characterization.cpp:251-257`). |
| 11 | Ismételten érkező fogadási blokk | **PASS** | Az ismételt 1. blokk nem írta felül a célbuffert; a következő 2. blokk egyszer került bele, a fogadó ACK-szekvenciája megfelelt az API-nak (`xmodem_characterization.cpp:259-285`). |
| 12 | Első csomag NAK, az ismétlés ACK; a két teljes csomag azonos és memórián belüli | **PASS** | Az 5A/12. javítás után a két teljes, 133 bájtos csomag fejléce, payloadja és CRC-je bájtról bájtra egyezett; az ACK után NEXT érkezett, ASan-/UBSan-hiba nélkül (`xmodem_characterization.cpp:287-305`). |

Az 5A/12. eredménye 12 PASS és két még nem futtatható Programmer-eset volt. Az 5B/12. teljes összesítése: **26 PASS (12 korábbi + 14 új), 0 KNOWN FAILURE, 0 váratlan hiba**, ASan-/UBSan-diagnosztika nélkül. Az új eseteket az 5. szakasz sorolja fel.

## 3. Bájtsorrend és páronkénti csere

A három küldési adatmintán végzett dinamikus vizsgálat nem talált szomszédos bájtcserét a jelenlegi `XModem::block_send()` adatútjában. Különösen a `12 AF 34 56` nem alakult `AF 12 56 34` sorozattá, a `00 ... FF` minta és a 384 bájtos minta is nyers bájtonként egyezett. Ez összhangban áll azzal, hogy a produkciós kód a payloadot egyesével olvassa és írja (`src/xmodem.cpp:113-116`).

Ez az eredmény az izolált `src/xmodem.cpp` rétegre vonatkozik. Nem bizonyítja önmagában a fizikai flash → SPI → firmware → UART → Python fájl teljes útvonalát, mert a fizikai flash/SPI és a Python soros végpont nem része a natív tesztnek. Az 5B/12. írási tesztek ezen felül a Programmer által átadott teljes lapok bájtsorrendjét is ellenőrzik a fake flashen. A korábbi `12 AF` helyett látott `AF 12` megfigyelés oka ezért a teljes rendszerre nézve továbbra sincs végleg lezárva, bár a tesztelt XMODEM küldőág nem támaszt alá páronkénti cserét.

### 3.1. `output.rom` egybájtos szélellenőrzése

Csak az első és utolsó 256 bájt lett megjelenítve `xxd -g1` használatával:

```sh
xxd -g1 -l 256 output.rom
xxd -g1 -s -256 output.rom
```

Az első tartomány nagyrészt `FF` kitöltést és közötte egyedi bájtmezőket, az utolsó tartomány vegyes adatot tartalmazott. Nem látható bennük olyan önmagában értékelhető bizonyíték, amely páronkénti bájtcserét igazolna. Ismert elvárt referencia nélkül ez a két rövid nyers tartomány a bájtcserét kizárni sem képes. A `newbios2.bin` nem lett beolvasva vagy összehasonlítva; eltérő BIOS-tartalmából egyébként sem következne bájtcserére vonatkozó bizonyíték.

## 4. Újraküldési hiba és sanitizer

Az újraküldési teszt egy 128 bájtos, pontosan lefoglalt heap-bufferrel indul, az első teljes csomag után NAK-ot, a tervezett második után ACK-ot ad. A helyes elvárás változatlanul az, hogy mindkét 133 bájtos csomag teljes tartalma azonos legyen, és ne történjen határon túli hozzáférés.

Az 5A/12. javítás előtti futásban a `block_send()` a payloadot `*dst++` művelettel járta be, majd NAK esetén a `start` címkére ugrott a `dst` alapcímének visszaállítása nélkül. A második csomag első payload-olvasása AddressSanitizer `heap-buffer-overflow` hibát adott. A javítás után a `dst[i]` olvasás (`src/xmodem.cpp:114`) és a minden próbálkozáskor nullázott ciklusindex mellett a változatlan teszt PASS: a két teljes csomag bájtról bájtra azonos, ASan-/UBSan-hiba nincs. A futtató retry-besorolása csak ennek igazolása után változott `known_failure` értékről `pass` értékre; az ellenőrzések változatlanok.

## 5. Írási és lezárási tesztek — 5B/12.

### 5.1. Teszthámhiba és determinisztikus baseline

Az eredeti közös folyamat a legelső `write_empty_silence` esetben akadt el. A `WRITE_TRACE=1` nyomvonal igazolta: a firmware tízszer `C`-t küldött, majd a `Programmer::task()` visszatért; az elakadás utána, a teszt konzolnaplójának visszaolvasásakor történt. Nem ACK-ra vagy EOT-ra váró produkciós végtelen ciklus volt.

A hám a duplex UART-konzolt `tmpfile()`-lal és kimenetre alkalmazott `ungetc()`-vel modellezte, az update stream előírt olvasás/írás közti pozicionálása nélkül. A javítás a parancsot `fputc`-vel írja, `fseek`-kel áll rá, a `vfprintf_P` hostadapter pedig `fseek(file, 0, SEEK_CUR)`-ral vált kimenetre. A naplóolvasás külön méretkorlátot kapott. A fake a bemenet elfogyása utáni ismételt ACK-ot megengedi, de nem számolja új adat-ACK-nak. Az adat- és EOT-ACK-khoz teljes beolvasott keretet követel.

A produkciós változtatás előtt két teljes `bash tests/characterization/run_all.sh --baseline` futás kimenete bájtról bájtra egyezett: **15 PASS, 11 KNOWN FAILURE, 0 váratlan hiba, 0 folyamat-timeout**. Ebből a korábbi csomag 12 PASS, az új írási csomag 3 PASS és 11 KNOWN FAILURE. A hám rendbetétele alatt a `src/` az 5A állapotában maradt. Az új elvárások nem változtak a produkciós javítás kedvéért.

### 5.2. Tesztesetenkénti eredmények

Mindegyik név a `tests/characterization/write_characterization.cpp` futó esetét jelöli.

| Teszt | Javítás előtt | Javítás után | Ellenőrzés |
| --- | --- | --- | --- |
| `write_empty_silence` | PASS | PASS | Adat és EOT nélkül timeout, nulla lapírás. |
| `write_eot_before_first_block` | KNOWN FAILURE | PASS | Nulla adat után EOT: CAN, hiba, nulla lapírás, nincs EOT-ACK. |
| `write_eot_after_half_page` | KNOWN FAILURE | PASS | 128 bájt után EOT: a hiányos lap nem íródik ki. |
| `write_eot_after_full_page` | KNOWN FAILURE | PASS | 256/512 bájt után EOT: pontosan egy lap marad kiírva; nincs régi lap ismétlése vagy siker. |
| `write_eot_after_page_and_half` | KNOWN FAILURE | PASS | 384/512 bájt után EOT: az első lap megmarad, a második hiányos lap nem íródik ki. |
| `write_exact_capacity` | KNOWN FAILURE | PASS | 256, 512 és 768 bájtos fake kapacitás teljes, pontos tartalma és sikeres lezárása. |
| `write_last_data_ack` | KNOWN FAILURE | PASS | Az utolsó adatblokk ACK-ja megelőzi az EOT beolvasását. |
| `write_eot_ack` | KNOWN FAILURE | PASS | Egy EOT-ACK és a teljes, pontos vezérlőbájt-sorozat. |
| `write_oversized` | KNOWN FAILURE | PASS | 640/512 bájt: két lap kiírva, plusz blokk feldolgozva, de nincs extra írás/ACK; CAN és hiba. |
| `write_page_requires_two_blocks` | KNOWN FAILURE | PASS | CAN a második fél helyén: nulla írás; sikeres ágon a teljes lapok fogadás–írás–ACK sorrendje pontos. |
| `write_missing_second_block` | PASS | PASS | 128 bájt után csend: timeout, nulla lapírás. |
| `write_missing_eot` | KNOWN FAILURE | PASS | Teljes kapacitás után csend: utolsó adat-ACK, majd timeout, nincs siker. |
| `write_invalid_second_crc` | PASS | PASS | Hibás második CRC, újraküldés nélkül: nincs lapírás vagy hibásblokk-ACK. |
| `write_duplicate_final_block` | KNOWN FAILURE | PASS | Utolsó blokk duplikátuma ACK-ot kap, új lapírás nélkül; utána EOT/ACK és siker. |

A korábbi NOT TESTABLE minősítés túl szűken vette figyelembe a tesztelési lehetőségeket: a linkeléskori flash-helyettesítés a produkciós vezérlési útvonal másolása vagy refaktorálása nélkül megoldotta az izolálást.

### 5.3. Pontos méretű sikeres lezárás

Az oldal két, CRC-ellenőrzött, NEXT eredményű 128 bájtos blokkból áll össze. Csak ezek növelik a `received` bájtszámot, a célmutatót és csökkentik a hiányzó blokkok számát. A teljes oldal kerül flashbe, a meglévő busy-várakozással. A felismert kapacitás elérése után még egy `block_receive(page)` történik: annak elején kimegy az utolsó adatblokk ACK-ja, majd EOT fogadása esetén az EOT ACK-ja és END következik. Csak ekkor jelenik meg a `Write done`.

512 bájtos példában a pontos eseménysor: adat 1 → ACK 1 → adat 2 → lapírás 0 → ACK 2 → adat 3 → ACK 3 → adat 4 → lapírás 256 → ACK 4 → EOT → ACK EOT. A teszt az eseménysort és minden flashbe átadott bájtot összeveti.

### 5.4. Rövid és túl hosszú bemenet

Adatfogadás közben `accept_eot=false`: korai EOT-ra CAN és ERROR érkezik, `Invalid file size` üzenettel. EOT nem számít adatblokknak és nem kap sikeres lezárást jelentő ACK-ot. A hiányos lap nem íródik ki; a korábban már teljesen fogadott és kiírt lapok megmaradnak. Csend vagy hiányzó szükséges blokk timeouttal hibázik.

Kapacitás után egy új adatblokk a meglévő RAM-lapbufferbe érkezhet, de új flashírás nem történik. NEXT eredménye CAN-t és `Out of memory` hibát vált ki; a plusz blokk nem kap ACK-ot. A flashcímek végig a `[0, size)` tartományban maradnak. A felismert chipek kapacitása 256-tal osztható. Az utolsó blokk szabályos duplikátumát az XMODEM továbbra is megkülönbözteti az új, túlméretes blokktól.

A méret a fogadott XMODEM-payloadok összege; a protokoll nem közli a kitöltés előtti hostfájlhosszt. Nincs teljes chipet lefedő RAM-puffer, tranzakció vagy visszagörgetés. Az erase külön parancs, automatikus verify nem készült.

## 6. Hardver nélkül nem ellenőrzött területek és korlátok

- A fake Stream nem méri a valódi AVR UART időzítését, soros driverét, resetviselkedését vagy zajos vonalát.
- A timeout-teszt az állapotgép TIMEOUT ágát ellenőrzi, nem a kommentben állított valós hét másodpercet; a számláló 16 MHz-es feltételezése hoston nem időmérés (`src/xmodem.cpp:38-57`).
- Nem történt SPI-tranzakció, JEDEC-azonosítás, erase, flash read/write, busy-időzítés vagy szintillesztési vizsgálat.
- A teszt nem importálja és nem futtatja a Python segédprogramokat, így a pySerial/host `xmodem` csomag tényleges együttműködését sem állítja.
- A fogadási CRC-fixture csak szabályos bemeneti csomagokat készít. A küldési keret nyers visszafejtése tesztoldali ellenőrzés, nem egy külön firmware- vagy általános XMODEM-implementáció.

## 7. Kapcsolat az 5/12. lépés későbbi javításaival

- A `nak_retry_identical_and_in_bounds` teszt változatlan helyes elvárása az 5A/12. küldési mutatókezelési javítást igazolta: KNOWN FAILURE állapotból PASS-ra váltott.
- A három bájtsorrendteszt, a 128 bájtos határ, a többblokkos átvitel és a 255 → 0 átfordulás regressziós védőháló a retry javítása körül.
- Az ACK/NAK/EOT/CAN, timeout és ismételt blokk tesztek megakadályozzák, hogy a hiba javítása közben a jelenleg rögzített protokollágak és visszatérési értékek észrevétlenül megváltozzanak.
- A két korábbi NOT TESTABLE Programmer-eset az 5B/12. lépésben futó tesztekké vált; a teljes eredmény az 5. szakaszban szerepel.

## 8. Rögzített futási kimenet — 5B/12.

```text
BUILD compiler=/usr/bin/g++
SANITIZERS address,undefined enabled
PASS send_short_byte_order
PASS send_00_ff
PASS send_long_pattern
PASS exact_128_boundary
PASS multiple_blocks
PASS block_number_rollover
PASS start_and_ack_handling
PASS eot_handling
PASS can_handling
PASS timeout
PASS duplicate_receive_block
PASS nak_retry_identical_and_in_bounds
SUMMARY PASS=12 KNOWN_FAILURE=0 UNEXPECTED_FAILURE=0
```


Az új írási csomag normál, KNOWN FAILURE-kivételek nélküli futásának összesítése:

```text
WRITE SUMMARY PASS=14 KNOWN_FAILURE=0 UNEXPECTED_FAILURE=0
```

Együttes eredmény: **26 PASS, 0 KNOWN FAILURE, 0 váratlan hiba**, nincs folyamat-timeout, bájtpárcsere vagy ASan-/UBSan-hiba. A részletes javítás és az AVR szintaxisellenőrzés az `XMODEM_WRITE_FINALIZATION_FIX.md` fájlban szerepel.

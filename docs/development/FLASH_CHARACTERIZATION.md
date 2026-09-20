# SPI flash-meghajtó karakterizálása — 6A/12, frissítve 6B2/12 után

- Eredeti vizsgálat: 2026-09-07; 6B1/6B2-frissítés: 2026-09-20; időzóna: Europe/Budapest.
- Célkörnyezet: Arduino Uno, hardver nélküli natív karakterizálás.
- Produkciós állapot: minden korábbi módosítás megőrizve; a 6B1 BUSY/WEL/hibaterjesztési javításai változatlanok, a 6B2 bájtalapú tartomány- és igazításellenőrzést, read-hibaterjesztést és explicit CS-kimenetet adott hozzá. Python-, XMODEM-, ROM- és PlatformIO-fájl nem változott ebben a lépésben.
- Tesztelt logikai kapacitás: **16 MiB / 128 Mbit**, címterület `0x000000..0xFFFFFF`.
- Fontos azonosítási korlát: az `EF 40 18` tesztfixture és a kódbeli `W25Q128` bejegyzés kapacitáskarakterizálás, **nem a fizikai chip pontos típusának vagy feliratának igazolása**. A hivatkozott W25Q128FV adatlap összehasonlítási referencia; a tényleges chipmodell továbbra sem ismert.

## 1. Módszer és a fake SPI határai

A natív flash-bináris közvetlenül fordítja és futtatja a tényleges `src/winbondflash.cpp`, `src/programmer.cpp` és `src/xmodem.cpp` fájlokat, valamint a produkciós `src/winbondflash.h` konkrét `winbondFlashSPI` osztályát. Nincs tesztoldali flash-algoritmus. A fejlécben deklarált, de a produkciós `.cpp`-ben nem definiált absztrakt `winbondFlashClass::transfer_addr()` miatt a host linker számára egy elérhetetlen, kivételt dobó tesztoldali definíció szükséges; a futó címküldések mind a valódi `winbondFlashSPI::transfer_addr()` inline override-ot használják.

A fake a klasszikus Arduino SPI alapvető szemantikáját követi:

- minden `SPI.transfer(tx)` pontosan egy bájtot küld és ugyanazon hívásban egy fogadott bájtot ad vissza;
- az SPI önmagában nem kezeli a chip-selectet: a produkciós `digitalWrite(nss, LOW/HIGH)` hívások nyitják és zárják a naplózott tranzakciót;
- `/CS=HIGH` mellett transfer teszthiba, egymásba ágyazott `/CS=LOW` teszthiba;
- a fake csak naplóz, valamint előre beállított JEDEC-, státusz-, UID-, determinisztikus olvasási bájtokat és szimulált `millis()` értékeket ad;
- nem érvényesít WEL-t, védelmi biteket, cím- vagy laphatárt, és nem szimulál sikeres programozást/törlést. Így nem javítja ki és nem rejti el a produkciós hiányosságokat.

A produkciós kód a régi globális SPI-beállításokat használja (`begin`, `MSBFIRST`, `SPI_CLOCK_DIV2`, `SPI_MODE0`), nem hív `SPI.beginTransaction()`/`endTransaction()` párt. A fake ezt változtatás nélkül követi; ezért a teszt a bájtsorrendet és a `/CS`-határokat bizonyítja, de más, ugyanazt a buszt használó eszközzel való együttműködést nem.

## 2. A jelenlegi publikus API

### Inicializálás és azonosítás

`winbondFlashSPI::begin(partno, spi, nss)` érték szerint eltárolja az SPI objektumot és a CS lábat, `MISO`-t `INPUT_PULLUP` módra állítja, elindítja az SPI-t, majd `MSBFIRST`, `SPI_CLOCK_DIV2`, `SPI_MODE0` beállítást végez. Ezután `/CS=HIGH`, majd külön tranzakcióban `AB` Release Power-down, 5 µs várakozás és `9F 00 00 00` JEDEC-lekérdezés következik.

A kapott `nss` lábat a kód explicit `OUTPUT` módra állítja, majd `deselect()` hívással HIGH szintre teszi az első SPI-parancs előtt. A fake eseménynapló ezt egyedi D7 és alapértelmezett D10 lábbal is ellenőrzi; nincs hardcode-olt D10 használat.

Automatikus felismeréskor csak az `EF` gyártóbájtot és a kódbeli `4014..4018` eszközazonosítókat fogadja el. Az `EF 40 18` bejegyzéshez 16 777 216 bájt, 65 536 lap, 4 096 szektor és 256 darab 64 KiB-os blokk tartozik. Ismeretlen azonosítónál a `begin()` `false`; a `Programmer` ekkor `INITIALIZE FAILED` üzenetet ír és letiltja a további parancsokat. A hiba nem tartalmazza a kapott JEDEC ID-t vagy az elutasítás okát.

Explicit kódbeli típussal a JEDEC ID-nak egyeznie kell. A `custom` mód bármely `EF` gyártójú ID-t elfogad, de nem állít be érvényes kapacitásbejegyzést; ezért a kapacitáslekérdezések biztonságos használata ebben a módban nincs definiált szerződéssel alátámasztva.

`readManufacturer()` és `readPartID()` külön-külön új `9F` tranzakciót indít. Előbbi az első válaszbájtot, utóbbi a második és harmadik válaszból képzett, vezetéki sorrendű 16 bites értéket adja. `readUniqueID()` a `4B`, négy dummy bájt és nyolc válaszbájt sorrendet használja; a visszatérési érték összeállítása kifejezetten little-endian CPU-ra íródott.

### Kapacitás

`bytes()`, `pages()`, `sectors()` és `blocks()` a sikeresen felismert belső `partno` alapján keres a PROGMEM táblában. Ismeretlen táblabejegyzésre 0-t adnak. A jelenlegi, korábban javított `pages()` `uint32_t`, ezért a 65 536 lap nem csonkolódik.

### Olvasás

`read(addr, buf, n)` eredménye `bool`. A kapacitás és minden cím/hossz bájtban értendő. Előbb ellenőrzi, hogy `addr <= capacity`, majd összeadás nélkül azt, hogy `n <= capacity - addr`; így sem a 24 bites csonkolás, sem a `uint32_t` összeadási túlcsordulás nem fogadhat el hibás tartományt. Érvénytelen tartománynál SPI-forgalom nélkül `false` az eredmény. A nulla hosszúságú olvasás `addr <= capacity` mellett sikeres no-op, szintén SPI-forgalom nélkül. Nem nulla, érvényes tartománynál BUSY esetén `false`; különben `05 FF`, majd `03`, három címbájt MSB→LSB és `n` adatbájt következik. A `Programmer` a `false` eredménynél leáll, `Flash read failed` üzenetet ad, és nem küldi el a lapbuffer korábbi tartalmát.

### Írás és törlés

`writePage(addr, buf)` csak akkor indul el, ha `addr` 256 bájtra igazított és a teljes 256 bájtos lap belefér a bájtban tárolt kapacitásba. Hibás címnél WREN és Page Program nélkül `false` eredményt ad; nincs csendes lefelé igazítás. Érvényes címen a 6B1 sorrend változatlan: WREN, WEL-ellenőrzés, `02`, három címbájt MSB→LSB, pontosan 256 adatbájt, majd határidős BUSY-polling.

`eraseSector()`, `erase32kBlock()` és `erase64kBlock()` rendre 4 KiB, 32 KiB és 64 KiB címigazítást követel, és csak teljesen kapacitáson belüli egységet fogad el. Hibás címnél WREN és erase parancs nélkül `false` az eredmény. Érvényes címen a `20`, `52`, `D8` opkód, 24 bites MSB→LSB cím, WEL-ellenőrzés, BUSY-polling és a 6B1 timeout változatlan. `eraseAll()` továbbra is csak `C7`-et használ. A protection biteket egyik út sem ellenőrzi vagy módosítja. `eraseSuspend()` és `eraseResume()` változatlan.

A `Programmer` a teljes chip törlés és minden lap írásának `bool` eredményét ellenőrzi. Hibánál leállítja az aktuális műveletet, törlésnél `Erase failed`, írásnál `Flash write failed` üzenetet ad, és nem ír `Done`/`Write done` sikerüzenetet. A sikeres útvonal parancssorrendjét a meghajtó kezeli; a korábbi korlátlan Programmer-oldali BUSY-ciklusok megszűntek.

### Write Enable, státusz és BUSY

`setWriteEnable(true)`/`WE(true)` külön tranzakcióban `06`-ot küld, majd külön `05 FF` tranzakcióban visszaolvassa az SR1 WEL bitjét (bit 1), és ennek megfelelő `bool` eredményt ad. `false` esetén `04`-et küld és `true` eredménnyel tér vissza; a WEL=0 visszaellenőrzése ezen az ágon nincs bevezetve. Minden támogatott program/erase metódus a WEL=1 ellenőrzése előtt áll, ezért WEL=0-nál `02`, `20`, `52`, `D8` és `C7` sem kerül a buszra.

`busy()` tranzakciója `05 FF`; csak az SR1 bit 0-t adja vissza logikai értékként. A `waitUntilReady(timeout_ms)` előjel nélküli `uint32_t(millis() - start)` elteltidő-különbséget használ, ezért a `millis()` 32 bites átfordulásánál is helyes. BUSY időbeni törlésénél `true`, a határ elérésekor `false`; nincs végtelen várakozás. A műveleti limitek: Page Program 5 ms, Sector Erase 500 ms, 32 KiB Block Erase 2000 ms, 64 KiB Block Erase 2500 ms, Chip Erase 250000 ms.

`readSR()` két külön tranzakcióban egy-egy bájtot olvas (`05 FF`, majd `35 FF`) és `SR2 << 8 | SR1` értéket ad. A két tranzakció között két egymást követő `deselect()` hívás van, tehát a megfigyelt GPIO-sorrend `LOW,HIGH,HIGH,LOW,HIGH`. Ez egy redundáns második HIGH, nem külön SPI-tranzakció. A WEL-t a célzott SR1-olvasás már értelmezi; a protection biteket a meghajtó és a `Programmer` továbbra sem értelmezi.

### Chip-select

Minden tényleges parancstranzakció előtt `/CS=LOW`, az utolsó bájt után `/CS=HIGH`. Inicializáláskor `SPI.begin()` és a meglévő SPI-beállítások után a kiválasztott `nss` láb `OUTPUT` módot kap, majd egy önálló HIGH hívás állítja az inaktív szintet; csak ezután indul az `AB` tranzakció. A teszt minden naplózott tranzakción ellenőrzi a LOW/HIGH határt, és külön eseménynaplóval bizonyítja a D7/D10 OUTPUT→HIGH→első transfer sorrendet. `SPI.beginTransaction()`/`endTransaction()` továbbra sincs.

## 3. Ténylegesen kiadott SPI-parancsok

Az összevetés alapja a [Winbond W25Q128FV adatlap (közvetlen PDF)](https://www.pjrc.com/teensy/W25Q128FV.pdf), különösen a státuszregiszterek (7.1), a parancsösszefoglaló (8.1) és az alább megadott 8.2.x fejezetek. Ez kompatibilitási referencia, nem fizikai típusazonosítás.

| Kód | Cél | Cím/adat a parancs után | Adatlap szerinti lényeges előfeltétel | Várt befejezés | Jelenlegi kezelés |
| --- | --- | --- | --- | --- | --- |
| `AB` | Release Power-down (8.2.24) | nincs; az ID-változathoz lenne 3 dummy + 1 adat, de a kód nem kéri | táp stabil; BUSY alatt figyelmen kívül maradhat | `/CS=HIGH`, majd `tRES1` | **Igen, részben:** egy bájt és 5 µs delay; hardveres időzítés nem mért. |
| `B9` | Power-down (8.2.23) | nincs | eszköz ne legyen BUSY | `/CS=HIGH`, majd `tDP` | **Igen, részben:** egy bájt és 5 µs delay; BUSY nincs ellenőrizve. |
| `9F` | JEDEC manufacturer/type/capacity (8.2.29) | 3 válaszbájt; a kód három `00`-val órajelezi ki | normál/standby állapot | három bájt beolvasása és `/CS=HIGH` | **Igen:** `EF`, majd `40 18` sorrend; csak a beégetett lista támogatott. |
| `4B` | 64 bites Unique ID (8.2.28) | 4 dummy + 8 válaszbájt | normál állapot | 8 UID-bájt és `/CS=HIGH` | **Igen, platformfüggően:** vezetéki sorrend jó; numerikus összeállítás little-endian feltételű. |
| `05` | Status Register-1, BUSY/WEL/protection (8.2.4) | 1 dummy/1 válaszbájt hívásonként | bármikor, BUSY alatt is engedett | a kiolvasott bájt; BUSY=0 jelzi program/erase végét | **Igen a 6B1 körében:** WEL-t ellenőrzi és BUSY-t műveletspecifikus határidővel pollolja; protection biteket nem értelmez. |
| `35` | Status Register-2 (8.2.4) | 1 dummy/1 válaszbájt | bármikor | a kiolvasott bájt | **Részben:** `readSR()` visszaadja, de nincs értelmezés. |
| `06` | Write Enable (8.2.1) | nincs | program/erase/status write előtt szükséges | `/CS=HIGH` után WEL=1 | **Igen:** minden támogatott program/erase API maga küldi, majd `05`-tel ellenőrzi a WEL bitet; WEL=0-nál leáll. |
| `04` | Write Disable (8.2.3) | nincs | nincs | `/CS=HIGH` után WEL=0 | **Részben:** kiküldi, visszaellenőrzés nélkül. |
| `03` | Read Data (8.2.6) | 24 bites cím MSB→LSB + `n` adatbájt | BUSY=0 | `n` bájt után `/CS=HIGH` | **Igen a 6B2 körében:** túlcsordulásbiztos cím-/hosszhatár és Programmer-hibaterjesztés; érvénytelen vagy nulla hosszú kérésnél nincs SPI-forgalom. |
| `02` | Page Program (8.2.15) | 24 bites cím + pontosan 256 adatbájt | BUSY=0, WEL=1, írható/korábban törölt cél; teljes laphoz A7..A0=0 | `/CS=HIGH` indítja; BUSY 1→0; WEL automatikusan 0 | **Részben:** WEL, timeout, 256 bájtos igazítás és teljes laptartomány kezelt; protection/readback nincs. |
| `20` | 4 KiB Sector Erase (8.2.17) | 24 bites cím | BUSY=0, WEL=1, cél nincs védve; szektorhatár | `/CS=HIGH` indítja; BUSY 1→0 | **Részben:** WEL, timeout, 4 KiB igazítás és teljes tartomány kezelt; protection nincs. |
| `52` | 32 KiB Block Erase (8.2.18) | 24 bites cím | BUSY=0, WEL=1, cél nincs védve; 32 KiB-határ | `/CS=HIGH` indítja; BUSY 1→0 | **Részben:** WEL, timeout, 32 KiB igazítás és teljes tartomány kezelt; protection nincs. |
| `D8` | 64 KiB Block Erase (8.2.19) | 24 bites cím | BUSY=0, WEL=1, cél nincs védve; 64 KiB-határ | `/CS=HIGH` indítja; BUSY 1→0 | **Részben:** WEL, timeout, 64 KiB igazítás és teljes tartomány kezelt; protection nincs. |
| `C7` | teljes Chip Erase (8.2.20) | nincs | BUSY=0, WEL=1, a memória egyetlen része sem védett | `/CS=HIGH` indítja; BUSY 1→0; WEL 0 | **Részben:** WEL és 250000 ms timeout kezelt, hiba eljut a Programmerig; protection és blank-check nincs. |
| `75` | Erase/Program Suspend (8.2.21) | nincs | megfelelő művelet BUSY, SUS=0; chip erase-re nem érvényes | `tSUS` után BUSY=0, SUS=1 | **Nem teljesen:** csak kiküldi, állapotot és határidőt nem ellenőriz. |
| `7A` | Erase/Program Resume (8.2.22) | nincs | korábban felfüggesztett művelet | művelet folytatódik, BUSY ismét 1 lehet | **Nem teljesen:** csak kiküldi, állapotot nem ellenőriz. |

A forrás további opkódmakrókat is definiál (`01`, `0B`, `32`, `60`, `90`, `A3`, `FF`), de a jelenlegi publikus metódusok nem adják ki őket, ezért nem szerepelnek ténylegesen használt parancsként a táblában.

Az adatlap közvetlen fejezethivatkozásai: [státusz, BUSY, WEL és védelem — 7.1, PDF 16–20. oldal](https://www.pjrc.com/teensy/W25Q128FV.pdf#page=17), [Write Enable és státuszolvasás — 8.2.1–8.2.4, PDF 29–31. oldal](https://www.pjrc.com/teensy/W25Q128FV.pdf#page=30), [Read Data — 8.2.6, PDF 34. oldal](https://www.pjrc.com/teensy/W25Q128FV.pdf#page=35), [Page Program — 8.2.15, PDF 49–50. oldal](https://www.pjrc.com/teensy/W25Q128FV.pdf#page=50), [erase parancsok — 8.2.17–8.2.20, PDF 52–55. oldal](https://www.pjrc.com/teensy/W25Q128FV.pdf#page=53), [suspend/resume és power-down — 8.2.21–8.2.24, PDF 56–61. oldal](https://www.pjrc.com/teensy/W25Q128FV.pdf#page=57), [Unique ID és JEDEC ID — 8.2.28–8.2.29, PDF 65–66. oldal](https://www.pjrc.com/teensy/W25Q128FV.pdf#page=66).

## 4. Teszteredmények

Futtatás:

```text
bash tests/characterization/run_all.sh
```

A host `g++` build ASan- és UBSan-instrumentálással készült. A produkciós `winbondflash.cpp` fordítása hat meglévő signed/unsigned összehasonlítási figyelmeztetést adott; fordítási vagy sanitizerhiba nem volt.

### Korábbi karakterizálás

| Csomag | Eredmény |
| --- | --- |
| izolált XMODEM | 12 PASS, 0 KNOWN FAILURE, 0 váratlan hiba |
| Programmer írás/finalizálás | 14 PASS, 0 KNOWN FAILURE, 0 váratlan hiba |
| **Korábbi összesen** | **26 PASS, 0 KNOWN FAILURE, 0 váratlan hiba** |

### Flash-karakterizálás a 6B2 után

| Minősítés | Tesztek / eredmény |
| --- | --- |
| **PASS (45)** | Mind a korábbi 32 PASS megmaradt; a hat korábbi futó MISSING SAFEGUARD PASS-ra váltott, és további hét új határesettel bővült a csomag. Ide tartozik az egyedi CS sorrendje, a read kezdő-/vég-/túlcsordulási/nullahossz-esetei, az első/középső/utolsó lap, mindhárom erase-méret első/utolsó és hibás címei, a hibás bemenet nulla SPI-forgalma és a Programmer read-hibaterjesztése. |
| **MISSING SAFEGUARD (0)** | Nincs futó MISSING SAFEGUARD eset a 6B2 hatókörében. |
| **KNOWN FAILURE (0)** | Nincs külön ebbe a kategóriába sorolt eset; a bizonyított védelmi hiányokat MISSING SAFEGUARD jelöli. |
| **Váratlan hiba (0)** | Nincs. |

A flash-csomag a 6B2 után összesen **45 eset: 45 PASS, 0 MISSING SAFEGUARD, 0 KNOWN FAILURE, 0 váratlan hiba**. A teljes natív futás 71 eset: 71 PASS. A közvetlen 6B2 előtti alapállapot 38 flash-esetből 32 PASS és 6 MISSING SAFEGUARD, teljesen 58 PASS és 6 MISSING SAFEGUARD volt. ASan-/UBSan-hiba nincs; a hat régi signed/unsigned figyelmeztetés változatlan, új figyelmeztetés nem keletkezett.

### AVR szintaxisellenőrzés

A korábban használt AVR GCC 7.3.0, Arduino AVR core 1.8.7, Uno/ATmega328P definíciók és `-Os` mellett a három produkciós C++ fordítási egység szigorú `-fsyntax-only` ellenőrzése **PASS**, kilépési kód 0, diagnosztika nélkül:

```sh
$HOME/.arduino15/packages/arduino/tools/avr-gcc/7.3.0-atmel3.6.1-arduino7/bin/avr-g++ \
  -std=gnu++11 -Os -mmcu=atmega328p -DF_CPU=16000000L \
  -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/cores/arduino \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/variants/standard \
  -I$HOME/.arduino15/packages/arduino/hardware/avr/1.8.7/libraries/SPI/src \
  -Isrc -fsyntax-only src/programmer.cpp src/winbondflash.cpp src/xmodem.cpp
```

Ez továbbra is szintaxisellenőrzés, nem teljes link/build vagy hardveres próba.

### Címzési határeredmények

| Cím/eset | Tényleges busz | Eredmény |
| --- | --- | --- |
| első cím `0x000000` | `03 00 00 00`, helyes adatfolyam | PASS |
| középső cím `0x123456` | `03 12 34 56`, MSB→LSB | PASS |
| utolsó érvényes cím `0xFFFFFF` | `03 FF FF FF`, egy bájt | PASS |
| pontosan a végén záródó read `0xFFFFFC + 4` | érvényes, `03 FF FF FC` + 4 bájt | PASS |
| nulla hossz `0x1000000 + 0` | sikeres no-op, nulla SPI-tranzakció | PASS |
| kezdőcím `>= 0x1000000`, nem nulla hossz | elutasítva, nincs 24 bites visszafordulás és nincs SPI-tranzakció | PASS |
| `0xFFFFFF + 2`, illetve túlcsorduló nagy pár | elutasítva, nulla SPI-tranzakció | PASS |
| első/középső/utolsó lap | `0x000000`, `0x123400`, `0xFFFF00`, változatlan `02` bytesorrend | PASS |
| nem igazított vagy tartományon kívüli lap | elutasítva WREN és `02` nélkül | PASS |
| sector/32 KiB/64 KiB első és utolsó egység | `0x000000`, illetve `0xFFF000` / `0xFF8000` / `0xFF0000` | PASS |
| igazítatlan vagy túl nagy erase-cím | elutasítva WREN és `20`/`52`/`D8` nélkül | PASS |

## 5. Hibakezelési mátrix

| Kérdés | Minősítés | Bizonyíték |
| --- | --- | --- |
| Az `EF 40 18` azonosítóhoz helyes 16 MiB kapacitás tartozik? | **PASS** | A négy kapacitáslekérdezés rendre 16 777 216 / 65 536 / 4 096 / 256. Ez nem chipmodell-azonosítás. |
| Ismeretlen JEDEC ID elutasított? | **PASS** | `EF 40 19` esetén `begin(autoDetect)` false; a `Programmer` általános initialize hibát jelez. |
| Van belső BUSY timeout? | **PASS** | Igen; az öt módosító művelet külön limitet használ, a tartós BUSY belső `false` eredménnyel tér vissza, külső timeout nem szükséges. |
| A néhány poll után törlődő BUSY normálisan befejeződik? | **PASS** | `01, 03, 00` SR1 sorozatnál három `05 FF` tranzakció, két true után false. |
| Ellenőrzi a WEL bitet? | **PASS** | Igen. A `06` után külön `05 FF` olvasás ellenőrzi az SR1 bit 1-et; WEL=0 esetén egyik támogatott program/erase parancs sem indul el. |
| Felismeri a program/erase sikertelenségét? | **PASS / MISSING SAFEGUARD** | A WEL-elutasítás és a BUSY-timeout felismerhető, `false` eredménye a Programmerig terjed. Protection miatti, BUSY-timeout nélküli elutasítást, illetve tartalmi hibát readback/blank-check nélkül továbbra sem tud felismerni. |
| Kezeli a védelmi biteket? | **MISSING SAFEGUARD** | `readSR()` hozzáférést ad SR1/SR2-höz, de a kód nem értelmezi a BP/CMP/TB/SEC/SRP biteket vagy az egyedi lockokat. |
| Ellenőrzi a cím-, hossz- és laphatárokat? | **PASS** | Bájtalapú, kivonásos range check; 256 B page, 4/32/64 KiB erase igazítás; hibánál nulla SPI-forgalom. |
| Konfigurálja a paraméterként kapott CS lábat kimenetként? | **PASS** | Igen; a tényleges `nss` OUTPUT, majd HIGH az első transfer előtt, D7 és D10 teszttel. |
| A publikus API-n át tesztelhető a program/erase hibaterjesztés? | **PASS / NOT TESTABLE** | A WEL=0 és a BUSY-timeout a valós `bool` API-n és Programmer-hívásokon tesztelt. A fizikai program/erase tartalmi sikere hardver és readback/blank-check nélkül továbbra sem igazolható. |
| A publikus API-n át tesztelhető a valós protection-elutasítás? | **NOT TESTABLE** | A parancs kiadása látható, de az eszköz általi elutasítás és annak oka nem jut vissza az API-n. |

## 6. Hardver nélkül nem igazolható

- A fizikai chip gyártója, teljes típusa, revíziója, tokozása és felirata; az `EF 40 18` csak fake bemenet volt.
- A 16 MiB-os fizikai kapacitás, tartalom, erase/program működés, tartósság és adatmegőrzés.
- A tápfeszültség, szintillesztés, bekötés, `/WP`, `/HOLD` vagy multiplexelt IO lábak állapota.
- Az SPI órajel, jelalak, setup/hold idők, CS-idők, power-down/release késleltetések és valós BUSY-idők megfelelősége.
- A védelmi bitek és egyedi block/sector lockok tényleges állapota, illetve a parancsok elfogadása.
- A page program vagy erase sikere, az erased `FF` előfeltétel, valamint programozás utáni byte-/hash-egyezés.
- Más SPI-eszközzel megosztott busz viselkedése a globális beállítások és a hiányzó `beginTransaction()` miatt.

## 7. A 6B1/6B2 eredménye és nyitott kérdések

A BUSY/WEL/hibaterjesztés, a cím-/hossz-/igazításellenőrzés, a read-hibaterjesztés és a CS OUTPUT inicializálása elkészült. A protection, tartalmi ellenőrzés és SPI-tranzakciókezelés nyitva maradt.

| Prioritás | Minimális változtatás | Várható működési kockázat |
| --- | --- | --- |
| Kész | Művelettípushoz illesztett, véges és átfordulásbiztos BUSY-határidő, egyértelmű `bool` eredménnyel. | **Megmaradó kockázat:** a W25Q128FV referenciaértékei nem bizonyítják a fizikai chip pontos típusát; timeout után az eszköz még BUSY lehet. |
| Kész | WREN utáni WEL-visszaolvasás, a write/erase `bool` eredménye és Programmer-szintű leállás/hibaüzenet. | **Megmaradó kockázat:** plusz státusztranzakciók kerültek az útvonalba; protection- és tartalmi hibát nem különböztet meg. |
| Kész | Túlcsordulásbiztos `addr`/`length` ellenőrzés, 256 B page és 4/32/64 KiB erase-igazítás, hibánál nulla SPI-forgalom. | **Szándékos kompatibilitásváltozás:** a korábban csonkolt vagy lefelé kerekített hibás hívások most `false` eredménnyel leállnak. |
| Kész | A `read()` `bool` eredménye és Programmer-oldali ellenőrzése; hibánál nincs régi lapbuffer-küldés. | **Alacsony:** egy valódi olvasási/BUSY hiba most megszakítja a dumpot és látható hibaüzenetet ad. |
| 5 | Olvasd és értelmezd legalább a támogatott chiphez szükséges protection biteket; védett tartomány vagy teljes chip erase tiltása esetén állj meg konkrét hibával. Ne írj státuszregisztert automatikusan. | **Közepes–magas:** a bitek és lock-mechanizmusok pontos chipváltozattól függenek. Hibás értelmezés téves tiltást vagy veszélyes feloldási kísérletet okozhat; ezért előbb chipazonosítás kell. |
| 6 | Programozás után oldal-readback összevetés, chip erase után blank-check; eltérésre sikertelenség. | **Közepes:** jelentősen növeli a futási időt és az UART-visszajelzés igényét, de az olvasás nem növeli a flash írási kopását. A hiba utáni recovery szerződését meg kell határozni. |
| 7 | Ismeretlen chipnél őrizd meg és jelenítsd meg a nyers JEDEC ID-t, tarts explicit „nincs érvényes kapacitás” állapotot, és rendezd a `custom` mód szerződését. | **Alacsony:** diagnosztika és állapotkezelés változik; AVR flash/RAM méretre figyelni kell. Ismeretlen chipet továbbra se engedjen automatikusan írni. |
| Kész | Az eltárolt CS láb explicit `OUTPUT`, majd inaktív HIGH az első tranzakció előtt; D7 és D10 regressziós ellenőrzéssel. | **Alacsony:** az egyedi CS láb most ténylegesen kimenetként működik; hibásan megadott pin hatása láthatóbb. |
| 9 | Megosztott SPI busz támogatásakor keretezd a műveleteket `SPI.beginTransaction()`/`endTransaction()` használatával, a már igazolt mód/órajel megtartása mellett. | **Közepes:** megváltoztatja a buszkonfiguráció életciklusát és régi Arduino core kompatibilitást; csak külön buszteszttel érdemes bevezetni. |

A protection-értelmezés előtt továbbra is fontos a pontos fizikai chip és a támogatni kívánt variánsok megerősítése. A 7/12. lépés ebben a munkamenetben nem kezdődött el.

## 8. Reprodukció és fájlhatókör

A flash-hám a `tests/characterization/flash_characterization.cpp`, az Arduino/SPI hostillesztés a `tests/characterization/stubs/Arduino.h` és `SPI.h`, a teljes futtatás integrációja a `tests/characterization/run_all.sh` fájlban van. A build ideiglenes `/tmp` könyvtárban készül és a futás végén törlődik. A 6B1/6B2 produkciós módosításai a `src/winbondflash.h`, `src/winbondflash.cpp` és `src/programmer.cpp` fájlokat érintik; Python-, XMODEM-, ROM- és PlatformIO-fájl nem változott a 6B2 során.

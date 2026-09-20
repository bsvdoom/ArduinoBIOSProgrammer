# Arduino BIOS Programmer — változtatásmentes műszaki audit

Aktuális lépés: 0/1 (auditfázis)
Használt modell: Astra
Reasoning level: high

## 1. Audit-metaadatok

- Dátum: 2026-09-06; időzóna: Europe/Budapest.
- Vizsgált repository: `ArduinoBIOSProgrammer`; branch: `master`.
- HEAD: `9446c9f` (`9446c9fe1318a965eb482f32e1d46f20801f5c9c`), „Python scripts to dump and write bios roms”, 2023-03-26.
- Megbízás: a felhasználó által végrehajtásra kijelölt `codex-00-audit-prompt.md`. Hatókör: kizárólag audit, egy új `AUDIT.md`; implementáció, telepítés és hardverkapcsolat nélkül.
- Bizonyítékjelölések: **Tény** = közvetlen forrás vagy ellenőrzési eredmény; **Következtetés** = ezekből levezetett viselkedés/kockázat; **NEM ELDÖNTHETŐ** = hiányzó mérés, eredetigazolás vagy tulajdonosi döntés. A kódút levezetése nem hardveres reprodukció.
- A `fájl:kezdősor-végsősor` hivatkozások az audit előtti HEAD fájljaira vonatkoznak. Hiányzó fájlhoz nem rendelhető sorszám: ilyen állítás bizonyítéka a 3. szakasz teljes leltára és a rögzített Git-parancsok. Külső források hozzáférési dátuma: az audit napja.

## 2. Vezetői összefoglaló

1. **Kritikus memóriahiba lehetséges XMODEM retry alatt.** Küldésnél és fogadásnál is továbbhalad a `dst`, az újrapróbálás nem állítja vissza. Következmény: rossz adatok, majd bufferhatár-túllépés. Bizonyíték: `src/xmodem.cpp:100-139,142-254`; részletes kiváltó esetek: 6.1.
2. **Az EOF-kezelés hibás flashírást és teljes méretű ROM-nál hibás lezárást okozhat.** Az EOT önmagában is teljes lap kiírásához vezethet; a kapacitáshatár elérésekor nincs újabb fogadás az utolsó ACK/EOT kezelésére. Bizonyíték: `src/programmer.cpp:81-105`; 6.2.
3. **A „Done” nem igazolt ROM-integritás.** Nincs blank-check, programozás utáni visszaolvasás vagy összehasonlítás; WEL/protection ellenőrzés és busy-timeout sincs. Bizonyíték: `src/programmer.cpp:65-72,90-116`; `src/winbondflash.cpp:199-216,307-350`.
4. **A Python-scriptek Python 3 alatt jelenleg szintaktikailag hibásak**, és javított szintaxis mellett a parancsok `str`/`bytes` különbsége is akadály lenne. A hardver megnyitása importkor történne; a serial callbackek nem érvényesítik a timeoutot. Bizonyíték: mindkét script `1-16,22-36`; 7. szakasz és a 3. szakasz AST-ellenőrzése.
5. **A dump felülírhat korábbi mentést, sikertelen átvitel maradhat észrevétlen.** Rögzített `output.rom`, `wb`, figyelmen kívül hagyott eredmény, explicit lezárás és exit-kód-szerződés nélkül. Bizonyíték: `xmodembiosread.py:38-41`; `xmodembioswrite.py:38-41`.
6. **A forrás jelenlegi Linuxos fordítását fejlécnév-eltérés blokkolja.** A 65 536-os lapszám külön típushiba: szigorú C++11 alatt hiba, a telepített Arduino core kapcsolóival figyelmeztetés és csonkolás. Bizonyíték: `src/programmer.cpp:2-2`; `src/winbondflash.cpp:47-62`; 3. szakasz.
7. **Az eredet részben visszakövethető, a projekt licence NEM ELDÖNTHETŐ.** A helyi előzmény és a nyilvános [ma5ter-projekt](https://github.com/ma5ter/ArduinoBIOSProgrammer) erős upstream-nyom; a Winbond-réteg külön szerzőt nevez meg. Licenc nem szerepel a leltárban. Ez magas prioritású publikálási blokkoló; önkényes licencválasztás nem javasolt. Bizonyíték: `src/winbondflash.cpp:3-8`; 3–4. szakasz.
8. **Board és elektromos alkalmasság NEM ELDÖNTHETŐ.** A D10–D13 bekötés, az AVR stdio és a 16 MHz-es időzítési feltételezés klasszikus AVR Arduino irányába mutat; pontos board, chip-utótag és szintillesztő nincs megadva. Bizonyíték: `README.md:10-30`; `examples/BIOSprog/BIOSprog.ino:21-28`; `src/xmodem.cpp:38-45`.
9. **A repository Arduino library és példasketch, Python hosteszközökkel.** PlatformIO-konfiguráció, teszt, CI és Python-projektkörnyezet nincs a leltárban. A metadata ESP8266- és WebSocket-állításait nem támasztja alá a bemutatott implementáció. Bizonyíték: `library.properties:1-9`; `examples/BIOSprog/BIOSprog.ino:1-34`; `src/programmer.cpp:9-20`; 3. szakasz.
10. **A következő munka alapja a karakterizálás.** Előbb ismert jó dump/hash, eredeti build és protokollnapló; utána egyenként buildjavítás, hibakezelés, környezet és CLI. Minden mérföldkőnek előzetes összehasonlítási alapja és külön visszaállítási pontja legyen; részletek: 11–12. szakasz.

## 3. Kiindulási állapot és fájlleltár

### 3.1. Git és helyi szabályok

Az audit előtti `git status --short` kimenete **üres** volt: nem volt követett módosítás vagy nem követett fájl. `git branch --show-current`: `master`. A `git log -1` az 1. szakaszban szereplő HEAD-et adta.

`git remote -v`:

```text
origin  https://github.com/bsvdoom/ArduinoBIOSProgrammer.git (fetch)
origin  https://github.com/bsvdoom/ArduinoBIOSProgrammer.git (push)
```

A repositoryban és a szülőkönyvtárakban ellenőrzött `AGENTS.md` nem volt jelen. `CONTRIBUTING*`, `LICENSE*`, `COPYING*`, CI-workflow, `platformio.ini`, `pyproject.toml`, `.python-version`, lockfile és teszt nincs a teljes leltárban. A `.agents/` és `.codex/` üres, környezet által biztosított könyvtárak. A `.gitignore:1-9` Visual Studio/Visual Micro jellegű kizárásokat tartalmaz; ez történeti eszköznyom, nem igazolt eredeti buildkörnyezet.

### 3.2. Teljes fájllista és komponensleltár a `.git` tartalma nélkül

Az `rg --files --hidden -g '!.git'`, a `git ls-files` és egy rekurzív, rejtett fájlokra is kiterjedő olvasási bejárás ugyanazt a 13 meglévő fájlt találta. Az üres könyvtárak fent szerepelnek. Az audit során egyetlen új fájl került melléjük: `AUDIT.md`.

| Meglévő fájl | Szerep és bizonyíték |
| --- | --- |
| `.gitignore` | IDE/build-kizárások; `.gitignore:1-9`. |
| `README.md` | Bekötés, terminálparancsok, Python 2 telepítési útmutató, CLI-TODO; `README.md:1-50`. |
| `library.properties` | Arduino library metadata, 0.0.2-es verzió; `library.properties:1-9`. |
| `requirements.txt` | Két hostoldali csomag verziótartománya; `requirements.txt:1-2`. |
| `examples/BIOSprog/BIOSprog.ino` | Arduino `setup`/`loop`, UART–stdio adapter, `Programmer` példány; `examples/BIOSprog/BIOSprog.ino:1-34`. |
| `src/programmer.h` | Állapot, console, flash, 256 bájtos lapbuffer, méret és parancskezelő API; `src/programmer.h:4-25`. |
| `src/programmer.cpp` | Inicializálás, parancsértelmezés, flash–XMODEM összekötés; `src/programmer.cpp:23-171`. |
| `src/winbondflash.h` | Chipenum, flashműveletek, absztrakt transzport és SPI-adapter; `src/winbondflash.h:14-108`. |
| `src/winbondflash.cpp` | JEDEC-tábla, opkódok, státusz-, olvasás-, írás- és törlésműveletek; `src/winbondflash.cpp:15-389`. |
| `src/xmodem.h` | 128 bájtos protokollkonstansok, állapotenum, Stream-alapú API; régi deklarációk; `src/xmodem.h:4-66`. |
| `src/xmodem.cpp` | CRC, bájtkezelés, küldő/fogadó állapotgép; `src/xmodem.cpp:3-254`. |
| `xmodembiosread.py` | Interaktív, rögzített portra és fájlra dolgozó hostoldali dump; `xmodembiosread.py:1-41`. |
| `xmodembioswrite.py` | Hasonló hostoldali írás; `xmodembioswrite.py:1-41`. |

### 3.3. Elérhető eszközök, tényleges ellenőrzések

| Eszköz | Megfigyelt verzió/állapot |
| --- | --- |
| Git | 2.53.0 |
| ripgrep | 15.2.0 |
| `python`, `python3` a PATH-on | CPython 3.14.4 |
| `python2`, `python2.7` | Nem található a PATH-on. Más, fel nem térképezett környezet létezése nem kizárt. |
| pyenv | 2.6.4; megléte nem jelent projektszintű interpreter-rögzítést. |
| Python 3 csomagmetadata | pip 25.1.1; pyserial 3.5; xmodem és platformio nem telepített az ellenőrzött interpreterben. |
| `pio`, `platformio` | Indítófájl létezik, mindkettő `ModuleNotFoundError: No module named 'platformio'` hibával áll le; működő verzió nem állapítható meg. |
| `arduino-cli` | Nem található a PATH-on. |
| rendszer `avr-g++` | GCC 14.3.0 |
| már telepített Arduino AVR toolchain | GCC 7.3.0, csomag: `7.3.0-atmel3.6.1-arduino7`. |
| már telepített Arduino AVR core | 1.8.7 (`platform.txt`, 9. sor); nem a projekt által rögzített függőség. |
| host `g++` | GCC 15.2.0 |
| `clang++`, `cppcheck` | Nem található a PATH-on. |

**Python:** `python3 -B` alatt, kizárólag `ast.parse`-szal mindkét script ellenőrizve, import/végrehajtás nélkül. Mindkettő a 24. soron `SyntaxError: Missing parentheses in call to 'print'`. Nem keletkezett bytecode.

**C++:** a már telepített GCC 7.3.0 és Arduino AVR core 1.8.7 használatával, csak `-fsyntax-only`; a feltételes összehasonlítási cél ATmega328P/UNO, 16 MHz volt. Ez nem boardazonosítás, teljes build vagy hardveres teszt. Közös beállítások: `-std=gnu++11 -mmcu=atmega328p -DF_CPU=16000000L -DARDUINO=10819 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR`, a core, `variants/standard`, `libraries/SPI/src` és helyi `src` include-útvonalakkal; `.ino` esetén `-x c++`.

| Forrás | Szigorú szintaxisellenőrzés | Core-hoz közelített ellenőrzés |
| --- | --- | --- |
| `src/programmer.cpp` | Hiba: `XModem.h` nem található, 2. sor. | Ugyanez a hiba. |
| `src/winbondflash.cpp` | Hiba: 65536 → `uint16_t` narrowing, 62. sorra mutató diagnosztika. | Sikeres szintaxisellenőrzés, narrowing és csonkolás figyelmeztetéssel. |
| `src/xmodem.cpp` | Sikeres szintaxisellenőrzés. | Sikeres szintaxisellenőrzés. |
| `examples/BIOSprog/BIOSprog.ino` | Sikeres szintaxisellenőrzés. | Sikeres szintaxisellenőrzés. |

A második ellenőrzést új bizonyíték indokolta: a helyi core `platform.txt:28-28` tartalmazza a `-Wno-error=narrowing` kapcsolót. A második futás hozzáadta a `-Os -fpermissive -fno-exceptions -fno-threadsafe-statics -Wno-error=narrowing` opciókat. Az első futás optimalizálás hiánya miatti `util/delay.h` figyelmeztetése az audit parancsából következett; nem önálló projekthiba. A core saját dependency-/objektumképzését nem futtattam. Nem jött létre objektum, firmware, teszt vagy fixture.

Az audit elején a 13 fájlról memóriában SHA-256-leltár készült; a befejező összevetés eredményét a 15. szakasz rögzíti.

## 4. Eredet/licenc/publikálhatóság

### 4.1. Helyi commitlánc

A `git log --all --format=... --stat` és `git diff 9446c9f^ 9446c9f --summary` közvetlen eredménye:

| Commit | Dátum, szerzőnév | Változás |
| --- | --- | --- |
| `c627e9c` | 2017-12-20, ma5ter | Kezdeti sketch, Winbond- és XMODEM-források, `.gitignore`. |
| `fb215a1` | 2017-12-21, ma5ter | README létrehozása. |
| `ee71760` | 2017-12-21, ma5ter | README formázása. |
| `3bf39b9` | 2017-12-21, ma5ter | Programmer-réteg kiemelése, sketch/XMODEM változások. |
| `f614f05` | 2020-04-30, ma5ter | README-változás. |
| `9446c9f` | 2023-03-26, adam | Python-scriptek, requirements, library metadata, README-bővítés; mind a hét Arduino-forrás/sketch 100%-os tartalmi egyezésű áthelyezése. |

**Tény:** a jelenlegi C++/sketch tartalma a legutolsó commitban nem változott. A Python-fájlok ekkor jelentek meg. **Következtetés:** a firmware örökölt, a hostscript-réteg későbbi kiegészítés. A Git-szerzőnév a bejegyzés adata, nem önmagában szerzői jogi igazolás.

### 4.2. Upstream-nyomok és határaik

- A nyilvánosan elérhető [ma5ter/ArduinoBIOSProgrammer](https://github.com/ma5ter/ArduinoBIOSProgrammer) oldal öt commitot és a korábbi gyökérbeli firmware-fájlokat mutatja. A helyi első öt commit szerzője és a fájlok szerkezete ezzel egybevág. **Valószínű upstream**; a távoli commit teljes hashének ellenőrzése a sikertelen konkrét commitoldal-lekérés miatt nem történt meg. A jelenlegi `origin` a bsvdoom-forkra mutat; a böngésző onnan nem tudott tartalmat lekérni. Ez nem bizonyítja, hogy a remote megszűnt.
- `src/winbondflash.cpp:3-8` és `src/winbondflash.h:1-6` WarMonkey szerzőt, `bbs.kechuang.org` fórumot és a `code.google.com/p/winbondflash` projektet nevezi meg. Ez külön eredetréteg, amelynek licence az elérhető tartalomból **NEM ELDÖNTHETŐ**. Az archív projektoldal és metadata-lekérés nem sikerült.
- `src/xmodem.cpp:1-13` és `src/programmer.cpp:1-23` nem tartalmaz licenc- vagy szerzői fejlécet. A jelenlegi leltárban nincs licencfájl; a teljes helyi előzmény fájlnevei és licenc/copyright keresése sem szolgáltatott ilyet.
- `library.properties:3-8` a `lukacsa` nevet, helykitöltőnek tűnő e-mailt, WebSocket-leírást és eltérő `bsvdoom/LM` URL-t tartalmaz. **Következtetés:** a metadata nem megbízható eredetigazolás; a másolás forrása **NEM ELDÖNTHETŐ**.
- Mindkét Python-script `11-16,38-41` szerkezete erősen hasonlít az [xmodem csomag használati példájára](https://pypi.org/project/xmodem/). Ez további eredetellenőrzési pont; a hasonlóság nem bizonyítja a pontos másolási forrást vagy időpontot.

### 4.3. Publikálási döntés

**Magas prioritású blokkoló:** a teljes projekt újrapublikálásának licencalapja **NEM ELDÖNTHETŐ**. Előbb komponensenként össze kell kötni az upstream-verziót, licencszöveget, szerzői értesítéseket és a helyi kiegészítések jogosultságát. A nyilvános GitHub-elérhetőség önmagában nem helyettesít licencet; ezt a [GitHub licencelési útmutatója](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository) is elkülöníti. Nem választok licencet, és a Python-függőségek licence nem terjed automatikusan a firmware-re.

Későbbi metadata-rendezés előtt összehasonlítható library-import/build ellenőrzés kell. Megőrzendő az Arduino-csomagolás és a bizonyított attribúció; az `architectures` hiánya alapértelmezésben `*`, az üres `includes` pedig külön ellenőrzendő. A név `Arduino` előtagja az Arduino Library Managerbe történő új felvételnél külön akadály a [library specification](https://docs.arduino.cc/arduino-cli/library-specification/) szerint (`library.properties:1-9`). Ez nem azonos a GitHubon tárolás tilalmával.

## 5. Architektúra és protokollfolyamat

### 5.1. Komponensek és parancsszerződés

A sketch 115200-as `Serial` kapcsolatot indít, `UART READY` sort küld, AVR stdio callbackeket köt a `Serial` objektumra, majd a `loop()` a `Programmer::task()`-ot hívja (`examples/BIOSprog/BIOSprog.ino:8-34`). A szöveges parancsok a `FILE *console` felületen mennek, az XMODEM viszont közvetlenül a globális `Serial`-t kapja (`src/programmer.cpp:44-44,79-79,126-126`). A példában ez ugyanaz a fizikai csatorna; más console injektálása nem irányítaná át a bináris átvitelt.

| Bájt | Jelenlegi viselkedés | Bizonyíték |
| --- | --- | --- |
| ASCII `0` = `0x30` | Inicializálás; nem a bináris `0x00`. | `src/programmer.cpp:49-54` |
| `e`/`E` | Teljes chip törlése, ha inicializált. | `src/programmer.cpp:53-74` |
| `w`/`W` | XMODEM-fogadás és lapprogramozás. | `src/programmer.cpp:75-118` |
| `r`/`R` | Flash teljes méretének XMODEM-küldése. | `src/programmer.cpp:119-163` |
| `C` = `0x43` | Inicializált állapotban közvetlen CRC-módú dump, szöveges fejléc és `start_send()` nélkül. | `src/programmer.cpp:119-140` |
| CR = `0x0D`, CAN = `0x18` | Parancsmódban, inicializálás után figyelmen kívül hagyja. | `src/programmer.cpp:53-60` |
| LF, más bájt | Ismeretlen parancs; inicializálás előtt általános felszólítás. | `src/programmer.cpp:53-55,164-168` |

A Python-oldal szintén 115200-at rögzít, pySerial alapértékekkel; ezek 8N1-et és kikapcsolt flow controlt jelentenek a [pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html) szerint (`xmodembiosread.py:5-8`; `xmodembioswrite.py:5-8`). A firmware baudváltási parancsot nem kínál (`src/programmer.cpp:49-169`).

### 5.2. Jelenlegi szekvencia

Az alábbi ágak alternatív műveletek, nem audit közben végrehajtott parancssor. A Python-scriptek nem adnak ki `e` parancsot; az erase-ág terminálból érhető el. A diagram a fő kódutat mutatja, a hibás lezárásokat külön jelöli.

```mermaid
sequenceDiagram
    participant H as Python vagy terminal
    participant A as Arduino / Programmer
    participant X as XModem / Serial
    participant F as SPI flash
    A-->>H: UART READY (115200)
    H->>A: ASCII 0
    A->>F: SPI begin; AB release; 5 us; 9F JEDEC
    F-->>A: manufacturer + part ID
    alt ismert EF / 4014..4018
        A-->>H: CHIP READY; Type: not implemented; meret
    else ismeretlen ID
        A-->>H: INITIALIZE FAILED
    end
    alt read: r vagy R
        H->>A: r
        A-->>H: Read chip + XMODEM kezdes szoveg
        A->>X: start_send()
        H->>X: C
        loop 128 bajtonkent a teljes meretig
            A->>F: 05 busy; 03 + 24 bites cim + 128 bajt
            F-->>A: adat (vagy read() = 0 busy eseten)
            A->>X: block_send(page)
            X-->>H: SOH + sorszam + komplement + 128 adat + CRC16
            H->>X: ACK / NAK / CAN
        end
        A->>X: finish_send()
        X-->>H: EOT
        H->>X: ACK
        X-->>H: ETB; tovabbi bajtra var
        Note over A,X: finish_send eredmenyet a hivo eldobja
    else erase: e vagy E, terminalbol
        H->>A: e
        A-->>H: Erase all
        A->>F: 06 WREN; C7 chip erase
        loop busy=1, felso idokorlat nelkul
            A->>F: 05 status
            A-->>H: pont 100 ms-os kesleltetessel
        end
        A-->>H: Done (blank-check nelkul)
    else write: w vagy W
        H->>A: w
        A-->>H: Write chip + XMODEM kezdes szoveg
        loop 256 bajtos lapok a kapacitasig
            A->>X: legfeljebb ket block_receive()
            X-->>H: elso hivaskor C; kesobb elozo blokk ACK
            H->>X: SOH + sorszam + komplement + 128 adat + CRC16
            X-->>A: NEXT / END / TIMEOUT / CANCEL
            opt NEXT vagy END
                A->>F: 06 WREN; 02 + lapcim + 256 bajt
                loop busy=1, felso idokorlat nelkul
                    A->>F: 05 status
                end
            end
        end
        Note over H,X: EOT fogadasakor azonnali ACK; a kapacitashatar miatt EOT el is maradhat
        A-->>H: Write done / Out of memory / Timeout / Cancelled
    end
```

Bizonyíték: `examples/BIOSprog/BIOSprog.ino:21-33`; `src/programmer.cpp:23-169`; `src/xmodem.cpp:59-98,142-254`; `src/winbondflash.cpp:123-178,266-279,290-350,367-382`.

### 5.3. XMODEM, címzés és támogatási tartomány

**Tény:** firmware-oldalon csak SOH + 128 adatbájt + két CRC-bájt létezik. CRC-kezdőérték 0, polinom `0x1021`, előbb a magas bájt; nincs 8 bites checksum vagy STX/1K ág. Blokksorszám a vezetéken 8 bit, az állapotban `uint32_t`; 255 után 0-ra fordulás a castokból következik. Konstansok: SOH `01`, ACK `06`, EOT `04`, NAK `15`, ETB `17`, CAN `18`, SUB `1A`; a SUB csak definiált, a firmware nem használja paddingfelismerésre (`src/xmodem.h:6-14,31-33`; `src/xmodem.cpp:20-30,105-139,174-254`).

A Python `XMODEM(getc, putc)` hívás a vizsgált 0.4.6 upstreamben 128 bájtos alapmódot és `0x1A` kitöltést jelent; a fogadó CRC-vel kezd, majd checksumra visszaléphet. Ez nem teljesen azonos a firmware módválasztásával. Forrás: [xmodem 0.4.6 implementáció](https://github.com/tehmaze/xmodem/blob/0.4.6/xmodem/__init__.py); helyi hívások: mindkét script `38-41`. Az eredeti gépen ténylegesen telepített csomagverzió **NEM ELDÖNTHETŐ**.

| Kódbeli név | Elvárt JEDEC bájtok | Táblázott bájtkapacitás | Bináris méret |
| --- | --- | --- | --- |
| W25Q80 | EF 40 14 | 1 048 576 | 1 MiB |
| W25Q16 | EF 40 15 | 2 097 152 | 2 MiB |
| W25Q32 | EF 40 16 | 4 194 304 | 4 MiB |
| W25Q64 | EF 40 17 | 8 388 608 | 8 MiB |
| W25Q128 | EF 40 18 | 16 777 216 | 16 MiB |

Ez **kódbeli felismerési lista**, nem kipróbált chiplista és nem minden W25Q-utótag támogatásának bizonyítéka (`src/winbondflash.cpp:43-61,123-178`). A program 24 bites címet küld; a jelenlegi legnagyobb táblázott cím `0xFFFFFF`, a `uint32_t` külső ciklusok ehhez elegendők. Nincs 4 bájtos mód vagy SFDP-felderítés (`src/winbondflash.h:100-104`; `src/winbondflash.cpp:15-38`; `src/programmer.cpp:82-82,139-139`).

A `writePage()` mindig 256 bájtot küld, az alsó címbájtot nullázza; a hívó nulláról 256-tal lépked. A szektor/32K/64K törlő API jelen van, a CLI csak chip erase-t használ. Busy polling a státuszregiszter 0. bitjét olvassa (`src/winbondflash.cpp:199-208,307-350`; `src/programmer.cpp:65-93`).

### 5.4. Board, bekötés, elektromos feltételek

**Következtetés, nem azonosítás:** UNO R3 vagy klasszikus Nano jellegű AVR board reális jelölt a D10/11/12/13 bekötés, AVR stdio és 16 MHz-es konstans alapján (`README.md:12-24`; `examples/BIOSprog/BIOSprog.ino:26-26`; `src/xmodem.cpp:40-40`). Az [UNO R3 dokumentáció](https://docs.arduino.cc/resources/datasheets/A000066-datasheet.pdf) ATmega328P-t, az [Arduino Nano oldal](https://docs.arduino.cc/hardware/nano) klasszikus ATmega328-as Nano platformot ír le. A tényleges MCU, board/revízió, bootloader és órajel **NEM ELDÖNTHETŐ**. ESP8266 csak metadata-állítás (`library.properties:5-5`), nincs hozzá bizonyított konfiguráció.

Az alábbi összevetés kizárólag az UNO R3 és a gyártói példaként vizsgált **W25Q128JV 8 lábú SOIC** esetére vonatkozik. Nem bekötési engedély ismeretlen chiphez. Forrás: [UNO R3 pinout](https://docs.arduino.cc/resources/pinouts/A000066-full-pinout.pdf), [Winbond W25Q128JV, Rev. E, 3.1–3.3., 4.2. szakasz](https://docs.rs-online.com/cdee/0900766b81622f85.pdf).

| README flashláb → Arduino | Feltételes funkció |
| --- | --- |
| 1 → D10 | /CS, Arduino → flash |
| 2 → D12 | DO/MISO, flash → Arduino |
| 3 → 3.3 V | /WP/IO2 |
| 4 → GND | Föld |
| 5 → D11 | DI/MOSI, Arduino → flash |
| 6 → D13 | CLK, Arduino → flash |
| 7 → 3.3 V | /HOLD vagy /RESET/IO3, konkrét változattól függően |
| 8 → 3.3 V | VCC |

Repository-bizonyíték: `README.md:10-30`. Az UNO R3 IOREF-je 5 V ([UNO dokumentáció, 5.1. szakasz](https://docs.arduino.cc/resources/datasheets/A000066-datasheet.pdf)). A példaként vizsgált W25Q128JV névleges tápja 2.7–3.6 V; a Rev. E abszolút maximális lábfeszültsége VCC+0.4 V. Ez a konkrét alkatrész nem indokol 5 V-os közvetlen meghajtást ([Winbond adatlap, 9.1–9.2. szakasz](https://docs.rs-online.com/cdee/0900766b81622f85.pdf)). Más chip-utótagra az értékeket nem általánosítom.

**Következtetés:** a README minden felsorolt jelre „5 V → 3.3 V” megfogalmazása a MISO adatirányát pontatlanul írja le. A helyes illesztő iránya, bemeneti küszöbei, sebessége, tápellátása és kikapcsolt állapotbeli viselkedése a pontos hardver nélkül **NEM ELDÖNTHETŐ**. A kód MISO-n `INPUT_PULLUP`-ot állít, SPI MODE0/MSBFIRST/DIV2-t választ (`src/winbondflash.cpp:367-377`); az ebből adódó tényleges jelszint és frekvencia mérést igényel. A firmware konfigurációját is érintő módosítás előtt soros/SPI referenciafelvétel szükséges.

Nincs dokumentált szintillesztő-típus, kapcsolási rajz, tápkapacitás, vezetékgeometria, in-circuit/kiemelt chip megkülönböztetés vagy visszatáplálási mérés (`README.md:8-30`; teljes leltár). Ezek hiánya biztos; tényleges hardverkárosodás fennállása **NEM ELDÖNTHETŐ**. A Winbond-adatlap gyártói dokumentum, RS által tárolt másolat; a Winbond saját PDF-URL-jei az audit alatt nem voltak lekérhetők.

## 6. Arduino/C++ megállapítások

Az itt javasolt későbbi javítások mindegyikének előfeltétele a megnevezett karakterizáló eset. Ebben a lépésben egyetlen javítás vagy tesztimplementáció sem készült.

### 6.1. XMODEM újrapróbálás: mutatóeltolódás és bufferhatár

**Tény:** a küldő a payload olvasásakor növeli a `dst` mutatót; NAK után a `goto start` csak CRC-t és fejlécet indít újra, a mutatót nem (`src/xmodem.cpp:100-129`). A fogadó is helyben növeli a célmutatót, majd rövid adat, hiányzó CRC vagy CRC-hiba után ugyanazzal a már eltolódott mutatóval próbálkozik (`src/xmodem.cpp:142-158,184-204,218-249`). A Programmer buffere 256 bájt (`src/programmer.h:15-15`; `src/winbondflash.h:14-14`).

**Következtetés, konkrét kiváltó esetek:**

- Dumpküldés első NAK-ja után ugyanazon blokkszám alatt a `page[128..255]` tartományból megy a payload, pedig a hívó csak 128 bájtot olvasott be. Második NAK után már a `page[256..383]` tartományból olvasna, a tömbön kívül (`src/programmer.cpp:139-142`). Új CRC készül a rossz payloadra, ezért egy későbbi sikeres CRC nem bizonyítja az eredeti flashadatot.
- Írásfogadás második fél lapján egy teljes, hibás CRC-jű 128 bájtos blokk a mutatót a tömb végére viszi. Az újraküldött blokk adatírása rögtön a tömbön kívül indul. Első fél lapon már az első retry is rossz helyre ír; több retry ott is túlcsordulást okozhat (`src/programmer.cpp:82-89`).
- Félbehagyott payload után a retry az addig vett bájtokkal eltolva kezd. A hiba javításához nem elég CRC-ellenőrzést hozzáadni: az már van, a sikertelen kísérlet állapotát kell később rendezni.

**Következő ellenőrzés:** fake `Stream`, eltérő mintázatú fél lapok, NAK-sorozat, hibás CRC és megszakítás minden bájtpozíción; küldött adat és fogadó memória őrzött határainak ellenőrzése. Először a jelenlegi hibás trace legyen rögzítve, majd egyetlen célzott módosítás után ugyanaz az eset ismételve. Súlyosság: kritikus; a mutatóhiba biztos, a hardveres következmény nem mért.

### 6.2. Képméret, részleges lap, üres fájl és EOF

**Tény:** az író ciklus laponként két 128 bájtos fogadást próbál. `END` esetén ugyanúgy kiírja a teljes 256 bájtos `page`-et, mint `NEXT` esetén; nincs érvényes bájtszám, bufferfeltöltés vagy a bemeneti hossz ellenőrzése (`src/programmer.cpp:81-95`). A konstruktor nem inicializálja a `page`-et (`src/programmer.cpp:173-177`; `src/programmer.h:15-18`). A `block_receive()` EOT-ra rögtön ACK-ot küld és visszatér (`src/xmodem.cpp:163-167`).

| Bemeneti helyzet | A kódútból következő viselkedés |
| --- | --- |
| EOT az első adatblokk előtt | A 0. lap teljes buffertartalma programozásra kerülhet, noha nem jött adat. A buffer ekkor inicializálatlan vagy korábbi művelet maradványa. |
| 128 bájt, majd EOT | Első fél lap új adat; második fél lap változatlan/inicializálatlan adat. Mégis teljes lapírás és `Write done`. |
| 256 bájt vagy ennek többszöröse, a chipméretnél kisebb fájl | A következő iteráció EOT-t vesz, majd az előző lap bufferét még egyszer kiírja a következő lapcímre. |
| Nem 128 többszörösű fájl | A host XMODEM paddingje és a megmaradt fél lap egyaránt bekerülhet; a firmware nem ismeri az eredeti fájlhosszt. |
| Pontosan chipméretű fájl | Az utolsó két blokk után a flashkapacitás-ciklus véget ér `NEXT` eredménnyel, és `Out of memory` következik. Az utolsó blokk ACK-jához és az EOT fogadásához szükséges új `block_receive()` nincs meghívva. |
| Chipnél nagyobb fájl | Ugyanaz a határ miatti kilépés; nincs előzetes visszautasítás, az addigi lapok már kiíródtak. |
| CANCEL/TIMEOUT fél lapon | Az aktuális lap nincs kiírva, de a korábban programozott lapok megmaradnak; nincs tranzakció vagy visszagörgetés. |

Bizonyíték: `src/programmer.cpp:81-116`; `src/xmodem.cpp:146-167,245-254`. A padding alapértékének külső bizonyítéka az 5.3. szakaszban hivatkozott 0.4.6 upstream. Ezek statikus levezetések, nem lefuttatott átviteli tesztek.

Az EOT ACK-ja részleges/extra lap esetén **a flashprogramozás előtt** kimegy: a host akár sikert is jelenthet, amikor a későbbi flashművelet még nem ellenőrzött (`src/xmodem.cpp:164-166`; `src/programmer.cpp:86-93`).

**Következő ellenőrzés:** 0, 1, 127, 128, 129, 255, 256, 257, `size-128`, `size`, `size+128` bájtos műveletek fake flashen; teljes programozási cím-/payloadnapló és ACK/EOT-sorrend. Mielőtt bármi változik, el kell dönteni, hogy a későbbi CLI kizárólag teljes chipméretet engedjen-e, illetve van-e valós részleges írási igény. A jelenlegi hibás lapismétlés rögzítendő eltérés, nem megőrzendő kívánt viselkedés.

### 6.3. Típusok, címzés és flashfelismerés

- **Tény:** a W25Q128 táblában `pages=65536`, a mező és a `pages()` visszatérése `uint16_t`; a hiba fordítóval igazolt. A core-hoz közelített kapcsolók ezt figyelmeztetésként elfogadják, csonkolt értékkel. A Programmer `bytes()` alapján dolgozik, így ez nem bizonyít 0 bájtos teljes chipfelismerést (`src/winbondflash.cpp:47-62,218-239`; `src/programmer.cpp:26-33`). **Előzetes ellenőrzés:** mind az öt chip táblája, külön bytes/pages konzisztencia és builddiagnosztika.
- **Tény:** `autoDetect` csak EF gyártót és az öt teljes part ID-t fogadja el; ismeretlen ID-re false, a Programmer pedig törli az `initialized` flaget. Nem igaz, hogy ismeretlen chipet alapértelmezetten ismert kapacitásúnak tekint (`src/winbondflash.cpp:123-178`; `src/programmer.cpp:23-38`). Nincs nyers JEDEC-kiírás normál üzemmódban, a típus szövege `not implemented`. **Előzetes ellenőrzés:** ismeretlen gyártó, ismeretlen part, 00/FF válasz, sikeres majd sikertelen újrainicializálás.
- **Tény:** az explicit chipválasztás és `custom` ág nem állítja be a `partno` mezőt; azt csak az autodetect sikerága írja. A mezőnek nincs explicit inicializáló konstruktora, a `custom` kapacitás sem konfigurálható. A példabeli Programmer kizárólag autodetectet használ, így ez külön könyvtári API-kockázat (`src/winbondflash.h:27-43,72-74`; `src/winbondflash.cpp:149-188`). **Előzetes ellenőrzés:** explicit típusból `begin()` → `bytes()`; custom viselkedés.
- **Tény:** minden SPI-cím három bájt, a magasabb bitek elvesznek; a publikus `read()`/`writePage()` nem ellenőriz kapacitást (`src/winbondflash.h:100-104`; `src/winbondflash.cpp:290-318`). **Következtetés:** a jelenlegi listára a 24 bit elég, de nagyobb chip puszta táblabővítéssel nem támogatható. **Előzetes ellenőrzés:** 0, utolsó lap, `0xFFFFFF`, `0x1000000`, valamint cím+hossz túlfutás a fake SPI-n.
- **Tény:** `writePage()` lapkezdetre kerekít az alsó címbájt nullázásával; a publikus API nem jelzi a rosszul igazított bemenetet. A szektor-/blokkműveletek sem validálnak igazítást vagy méretet; a példaprogram ezeket nem hívja (`src/winbondflash.cpp:307-344`; `src/winbondflash.h:50-60`). **Előzetes ellenőrzés:** lap- és szektorhatárok két oldalának parancsnyoma.
- **Tény:** az író `uint8_t` számláló szándékosan 255 után 0-ra fordul, így 256 bájtot küld; az XMODEM-ciklusok `uint8_t` mellett 128-as határral működnek (`src/winbondflash.cpp:314-318`; `src/xmodem.cpp:103-114,145-145,218-228`). **Következtetés:** a számlálótípus vagy blokk-/lapméret „modernizálása” a ciklusfeltétel nélkül végtelen ciklust vagy protokolltörést hozhat. **Előzetes ellenőrzés:** pontos bájtszám és véghatár minden ilyen átírás előtt.
- A `readUniqueID()` kifejezetten little-endian memóriakiosztást feltételez; a PROGMEM-enumból csak egy bájt olvasódik, és több helyen implicit szűkítés van. Ez a jelenlegi kis enumértékeknél nem bizonyított futási hiba, de platformváltáskor ABI-/ábrázolásfüggő (`src/winbondflash.cpp:91-108,160-164,218-224`). Előbb típusszélesség- és értékkarakterizálás kell.

### 6.4. Write-enable, busy, törlés és visszaellenőrzés

**Tény:** a hívó minden chip erase és lapírás előtt WREN-t küld, de WEL-t/protection állapotot nem ellenőriz. A műveletek `void` visszatérésűek. Törléskor 100 ms késleltetéses, íráskor szoros busy-ciklus fut felső időhatár nélkül; a `DEFAULT_TIMEOUT 200` makró nincs bekötve (`src/programmer.cpp:65-72,90-94`; `src/winbondflash.cpp:45-45,199-216,307-350`).

**Következtetés:** tartós busy-válasz megakaszthatja a programot; állandó 0 státusz vagy figyelmen kívül hagyott parancs viszont hamis sikert eredményezhet. A konkrét W25Q128JV védett régiókat érintő program/erase parancsot figyelmen kívül hagyhat, és programozás előtt törölt területet, valamint WEL-t követel ([Winbond adatlap, 7.1.8. és 8.2.13. szakasz](https://docs.rs-online.com/cdee/0900766b81622f85.pdf)). A használt chip védelmi állapota **NEM ELDÖNTHETŐ**.

**Tény:** nincs automatikus erase az írás előtt, blank-check utána, programozott lap visszaolvasásos ellenőrzése vagy teljes ROM-verify (`src/programmer.cpp:62-118`). A README kézi előfeltételként írja a törlést (`README.md:39-41`). Az XMODEM CRC az átviteli payloadot ellenőrzi, nem a flashben kialakult tartalmat: a CRC-s kód nem olvassa vissza a flash-t (`src/xmodem.cpp:218-243`; `src/programmer.cpp:86-94`).

**Tény:** a flash `read()` busy esetén 0-val tér vissza és nem tölti a buffert; a Programmer ezt nem ellenőrzi, hanem továbbküldi a page-et (`src/winbondflash.cpp:290-304`; `src/programmer.cpp:139-142`). **Következtetés:** érvényes XMODEM CRC-jű, mégis régi/inicializálatlan dumpadat keletkezhet.

**Következő ellenőrzés:** fake flash, amely busy marad, sosem lesz busy, elutasítja a WREN-t, védett lapot modellez, vagy 0 olvasott bájtot ad vissza. Először a jelenlegi sikerszöveg/állapot rögzítése; utána külön mérföldkő a határidő, a műveleti státusz és a verify. Timeoutértéket csak az azonosított chip adatlapja és mérés alapján szabad kiválasztani; a 200-as makróból nem következik megfelelő chip erase-határidő.

### 6.5. XMODEM állapotgép további szélső esetei

| Vizsgálat | Közvetlen bizonyíték és következtetés | Előzetes karakterizálás |
| --- | --- | --- |
| CRC/checksum/1K | A küldő indításkor csak `C`-t fogad, más bájtra CANCEL; a fogadó SOH-t vár, nem STX-et. A két CRC-bájtot összeveti. `src/xmodem.cpp:59-77,174-204,230-243`. | CRC-s partner, checksum-kérés, STX, rossz komplement, ismert payload CRC. |
| Retry-szám | A konstruktor 10-et állít. Egyes timeoutágak csökkentik, a `skip_nak` út nem; folyamatos szemét mellett a drain sem ér véget. `src/xmodem.cpp:3-7,154-193,240-249`. | Végtelen zaj modellezése korlátos tesztidővel; ismételt CRC-/fejléchiba. |
| Duplikált blokk | Az előző sorszámot felismeri, CRC-zi, nem írja újra és újra vár. Ez hasznos meglévő viselkedés; a retry-mutathibát nem oldja meg. A legelső 0-s blokkot is duplikátumnak tekintheti. `src/xmodem.cpp:207-254`. | Elveszett ACK, első 0-s blokk, 255→0→1 átfordulás, idegen sorszám. |
| ACK késleltetés | ACK a következő `block_receive()` elején megy; a lap második blokkja után előbb flashírás/busy várakozás történik. `src/xmodem.cpp:146-150,245-254`; `src/programmer.cpp:85-94`. | Lassú fake page program, ACK-vesztés, teljes kapacitás utolsó blokkja. |
| Küldési timeout | NAK-ra újrapróbál, válasz-timeout után a visszaadott 0-ból ERROR lesz, nem újraküldés. `src/xmodem.cpp:127-135`. | Elveszett ACK, NAK, CAN és csend külön trace-e. |
| Bájtidőzítés | A „7 seconds” egy CPU-/fordítófüggő üres pollinghurok becslése, nem eltelt idő mérése; `F_CPU`-t nem használ. `src/xmodem.cpp:38-56`. | A referencia-buildben tényleges timeout mérése; később fake óra és határidőteszt. |
| CAN | Egyetlen CAN elég bizonyos állapotokban; payload olvasása közben nem külön megszakítójel. Hibadrain közben sincs önálló cancel-ág. `src/xmodem.cpp:117-120,127-135,169-181,218-228`. | CAN fejlécben, adatként és csomagok között; duplázott CAN és terminálmegszakítás. |
| Rövid csomag | Fejléc/adat/CRC timeout kezelése létezik, de adat/CRC után a célmutató hibás maradhat; néhány restart nem üríti a későn érkező maradékot. `src/xmodem.cpp:184-243`. | Csomag elvágása minden pozíción, későn érkező bájtok. |
| Küldés közbeni bejövő bájt | Payload küldés közben minden elérhető bejövő bájtot kiolvas, csak CAN-re reagál; így más vezérlőbájtot eldobhat. `src/xmodem.cpp:113-121`. | Korai/késői ACK és több `C` a hosttól. |
| Küldési befejezés | EOT egyszer, ACK-várás, utána ETB és újabb várás; az utolsó várás eredménye nem számít, a hívó az egész `finish_send()` eredményét eldobja. `src/xmodem.cpp:80-98`; `src/programmer.cpp:146-150`. | EOT ACK-vesztés, EOT-ra NAK-oló partner, ETB-re nem válaszoló partner. |

### 6.6. Erőforrások, chip-select és hordozhatóság

- **Tény:** minden read/write `new XModem(Serial)`-t használ `delete` és foglalási hiba kezelése nélkül. A Programmer egyszeri példánya a sketch teljes életére szól, ez nem azonos a műveletenkénti szivárgással (`src/programmer.cpp:79-79,126-126,173-181`; `examples/BIOSprog/BIOSprog.ino:28-33`). **Következtetés:** sok egymás utáni művelet elfogyaszthatja a heapet. Előbb ismételt műveletek foglalási naplója és memóriahatár-teszt kell; a pontos összeomlási küszöb **NEM ELDÖNTHETŐ**.
- **Tény:** a normál SPI-műveletek párosítják a select/deselect hívásokat; JEDEC-hibánál is előbb deselect történik. A busy miatti read-visszatérés egy már lezárt státusztranzakció után van. Nem találtam bizonyított „CS alacsonyan marad” normál hibautat (`src/winbondflash.cpp:123-144,199-208,290-350`).
- **Tény:** `_nss` pinre nincs saját `pinMode(OUTPUT)`, és az SPI objektum érték szerint másolódik (`src/winbondflash.h:83-108`; `src/winbondflash.cpp:367-377`). A példában `_nss=SS`; az ellenőrzött AVR core `SPI.begin()` ezt OUTPUT-ra állítja. Emiatt a példára nem állítható biztos CS-konfigurációs hiba. Más CS-pin/core esetén külön kockázat. Külső összevetés: [Arduino AVR SPI implementáció](https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/SPI/src/SPI.cpp); helyben ellenőrzött 1.8.7-es fájl `26-63`. Előzetes ellenőrzés: alap-SS és eltérő CS pin-mód-/él-trace.
- **Tény:** nincs SPI transaction-szintű `beginTransaction/endTransaction`, buszbeállítás globális, a `begin()` újrainicializáláskor ismét hívódik, `flash.end()`-et a Programmer nem hív (`src/winbondflash.cpp:367-388`; `src/programmer.cpp:23-38,179-181`). **Következtetés:** több buszhasználó vagy más core mellett a másolás/állapot/élettartam felülvizsgálatot igényel; a jelenlegi egyszereplős példára ez nem bizonyított adatátviteli hiba. Előbb buszbeállítások és ismételt initialize karakterizálása.
- **Tény:** `fdev_setup_stream`, `vfprintf_P`, `__FlashStringHelper`, PROGMEM és `pgm_read_*` kötődés látszik (`examples/BIOSprog/BIOSprog.ino:5-26`; `src/programmer.cpp:9-20`; `src/winbondflash.cpp:56-62,160-164,218-236`). Az első két API-t az [AVR-libc stdio](https://avrdudes.github.io/avr-libc/avr-libc-user-manual/group__avr__stdio.html) dokumentálja. A puszta `framework=arduino` nem bizonyít más MCU-kompatibilitást. Előzetes ellenőrzés: megerősített AVR baseline-build és console byte-trace; csak utána platformadapter.
- A `src/xmodem.h:16-24` régi `xm_*` deklarációihoz nem találtam definíciót a teljes forrásban. A nem pure `winbondFlashClass::transfer_addr` deklarációhoz sincs bázisimplementáció; a SPI-leszármazott felüldefiniálja (`src/winbondflash.h:76-80,100-104`). Jelenlegi hívások nem használják a régi `xm_*` API-t; a bázis vtable/linker-hatása optimalizálásfüggő lehet, **NEM ELDÖNTHETŐ teljes link nélkül**. Előbb exportált szimbólum-/linkellenőrzés kell, nem találomra törlés.

### 6.7. A soros protokoll egyértelműsége

**Tény:** ANSI termináltörlés és emberi szövegek osztoznak a bináris adatcsatornán; nincs protokollverzió, gépi státuszkód, hosszmező vagy parancsazonosító. A `C` egyben dumpindító parancs is; `e`/`w` nem kér firmware-oldali megerősítést (`src/programmer.h:7-7`; `src/programmer.cpp:23-38,49-169`).

**Következtetés:** elcsúszott vagy megszakított átvitel után a bent maradt payloadot a következő `task()` parancsként értelmezheti; az abban előforduló `e`, `w`, `0` nem védett. A konkrét előfordulás időzítés-/pufferfüggő, de a bináris és parancsmód közti védett visszatérés hiánya biztos. A teljes chipméretű írás utolsó blokkjának újraküldése különösen fontos kiváltó jelölt (`src/programmer.cpp:82-118,164-169`; `src/xmodem.cpp:245-254`). Előzetes karakterizálás: félbeszakadt küldés után maradó bájtfolyam, benne parancsbájtokkal, kizárólag fake flashen. A későbbi protokollmódosításnak külön kompatibilitási döntése és régi termináltesztje legyen.

## 7. Python megállapítások

### 7.1. Python 2 → 3: a szintaxison túl

**Tény:** mindkét script `24-28,35-36` sora Python 2 `print` statement; ezt az AST-ellenőrzés igazolta. A `19-19` sor zárójeles printje önmagában Python 3-kompatibilis, az egész script mégsem az. A Python 2 hivatalos támogatása 2020-ban lezárult ([Python.org](https://www.python.org/doc/sunset-python-2/)); a README továbbra is `pip2.7`-et ír (`README.md:46-47`).

A szintaxis későbbi javítása után `ser.write('0')`, `'r'`, `'w'` Python 3 `str` értékeket adna a bájtokat váró API-nak. A helyes migráció külön kezelje a terminál szövegét és a soros bináris adatot; XMODEM-payloadot nem szabad dekódolni. Bizonyíték: mindkét script `11-16,22-36`; [pySerial write/read szerződés](https://pyserial.readthedocs.io/en/latest/pyserial_api.html). A `.decode()` csak a feltételezett szöveges státuszsorokon szerepel; elcsúszott bináris bemenet ott dekódolási hibát is okozhat.

**Előzetes ellenőrzés:** az eredeti bytesorozat rögzítése fake seriallal a hostkód bármely átalakítása előtt; külön eset 00/80/FF adatbájtokra, CRLF-re, ANSI-escape-re és hibás szövegre. A Python 3 szintaxisjavítás nem tekinthető teljes migrációnak.

### 7.2. Függőségek és karbantartottság

| Követelmény | Használat | Tartomány és megfigyelt külső állapot |
| --- | --- | --- |
| `pySerial~=3.5` | `import serial`, `Serial`, `read`, `write`, `readline`; mindkét script `2-16,24-36`. | `>=3.5,<4.0`; a [PyPI](https://pypi.org/project/pyserial/) legutóbbi kiadása 3.5, 2020-11-23. A kiadás kora önmagában nem bizonyít elhagyott projektet vagy hibát. |
| `xmodem~=0.4.6` | `XMODEM`, `recv`/`send`; mindkét script `3-3,38-41`. | `>=0.4.6,<0.5.0`; 0.4.7 is megengedett. A [PyPI kiadástörténet](https://pypi.org/project/xmodem/) szerint 0.4.7: 2023-06-11; 0.5.0: 2026-03-04, utóbbi már kívül esik a tartományon. |

A `~=` értelmezésének elsődleges forrása a [Python Packaging verzióspecifikáció](https://packaging.python.org/en/latest/specifications/version-specifiers/#compatible-release). A két requirement nem lock: nincs pontos feloldás, hash vagy rögzített interpreter (`requirements.txt:1-2`; leltár). Mindkét külső import használt; a `sys` standardkönyvtár, szintén használt (`xmodembiosread.py:1-3,20-33`; `xmodembioswrite.py:1-3,20-33`).

Az xmodem kiadási jegyzeteiben a 0.4.7 fogadási megakadást/sorszámkezelést, a 0.5.0 hibaszámlálást és olvasási összevonást érintő változásokat nevez meg ([csomag changelog](https://pypi.org/project/xmodem/)). **Következtetés:** frissítés befolyásolhatja a jelenlegi Arduino partner időzítését és retry-viselkedését; nem szabad a Python-migrációval automatikusan összekötni. Előbb a tényleges régi verzió, majd régi/új partner fake-serial összehasonlítása szükséges. A teljes karbantartói kapacitás és minden támogatott Python/OS kombináció **NEM ELDÖNTHETŐ** ezekből a metadataadatokból.

### 7.3. Közös és eltérő logika, CLI

**Tény:** a két 41 soros script diffje pontosan három logikai különbséget mutat: `r`/`w`, `output.rom`/`input.rom` és `wb`/`rb`, illetve `recv`/`send` (`xmodembiosread.py:30-41`; `xmodembioswrite.py:30-41`). A teljes bevezetés, callbackek és konzolkezelés duplikált.

Nincs `main` vagy `if __name__ == '__main__'`, argumentumfeldolgozás, súgó, megadható fájl/port/baud, automatikus initialize-eredményértelmezés vagy erase-előfeltétel ellenőrzés. A script elején már létrejönne a serial kapcsolat. `/dev/ttyUSB0`, 115200 és az aktuális munkakönyvtárhoz viszonyított ROM-fájlnév rögzített (mindkét script `1-8,19-41`). Az operációs rendszerenként eltérő portnév szükségességét a [pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html) írja le; a script jelenleg Linuxos alapértéket használ, nem platformfüggetlen CLI-t.

**Következtetés:** a README `xmodembios.py write|read ...` csak jövőbeli cél; közös kódba emelés előtt mindkét mai script külön bemenet/kimenet karakterizálása szükséges. A későbbi CLI parser először hardver nélkül tesztelendő; a felhasználó által megadott baud csak a firmware sebességével egyezve működhet, jelenleg nincs sebességegyeztetés.

### 7.4. Interaktív szinkronizáció és timeoutok

**Tény:** a „Press d” üzenet után csak `ch == '0'` küld inicializálást. Ezután feltétel nélkül öt sort olvas, majd két külön `sys.stdin.read(2)` köré helyezi az `r`/`w` parancsot; a `ch2`, `ch3` értéket nem használja. A read- és write-script promptja azonos, dumpot ígér (mindkettő `19-36`).

**Következtetés:** egy friss `UART READY` és a sikeres initialize négy sora együtt kiadhatja az öt várt sort, de már futó firmware, sikertelen initialize vagy eltérő stdin soremelés esetén ez nem megbízható állapotfelismerés (`examples/BIOSprog/BIOSprog.ino:23-24`; `src/programmer.cpp:25-37`). A felhasználói kézi várakozás a firmware XMODEM-időkorlátjába is belefuthat. Egy ismert működő billentyűsor és soros napló nélkül a pontos kézi eljárás **NEM ELDÖNTHETŐ**.

**Tény:** `Serial(port, baud)` nem ad meg olvasási/írási timeoutot; a `getc` és `putc` argumentumként kapott timeoutját nem alkalmazza (mindkét script `8-16`). A pySerial alapértelmezett olvasás/írás blokkoló, így a felső XMODEM retry-időzítést a callback megállíthatja ([pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html)). Portnyitáskor a driver DTR/RTS-t is állíthat; hogy ettől a tényleges board újraindul-e, **NEM ELDÖNTHETŐ**.

**Előzetes ellenőrzés:** fake serial és stdin, bannerrel/banner nélkül, sikertelen initialize, CRLF/LF, lassú vagy hiányzó sor, részleges read/write, megszűnő port, DTR/RTS-eseménynapló. Az új timeout- és bannerkezelés mind külön viselkedésváltozás.

### 7.5. Fájlok, erőforrások és átviteli eredmény

**Tény:** a ROM-fájlok helyesen bináris módban nyílnak. Nincs `with`, explicit `close`, `try/finally`, lemezhely-, fájlhossz- vagy overwrite-ellenőrzés. A read `wb` azonnal csonkolhatja a meglévő `output.rom`-ot; a write csak a `w` elküldése után próbálja megnyitni az `input.rom`-ot (mindkét script `30-41`). **Következtetés:** hiányzó/olvashatatlan inputnál a firmware már fogadásra vár; hibás dump az előző mentés helyére kerülhet, vagy részleges fájl maradhat vissza.

**Tény:** a `modem.send()`/`recv()` eredménye nincs eltárolva vagy értékelve. A vizsgált 0.4.6 szerződése: send → bool, recv → fogadott bájtszám vagy `None`; az abort alapértelmezésben két CAN-t küld ([upstream forrás](https://github.com/tehmaze/xmodem/blob/0.4.6/xmodem/__init__.py)). A helyi scriptek nem kérnek explicit abortot, retryt vagy eredményhez kötött exit code-ot. **Következtetés:** visszatérési értékkel jelzett átvitelhiba után a script normálisan, akár 0 exit code-dal érhet véget; kivételnél ettől eltérően traceback és nem nulla kód várható, de nincs egységes felhasználói hibaszerződés.

Nincs `KeyboardInterrupt`, `SerialException`, fájl-I/O hiba kezelése vagy strukturált napló; nincs ROM-hash, fogadott méret összevetése vagy visszaolvasásos verify (mindkét script `1-41`). A csomag belső retry/cancel-kezelése létezik, a script viszont nem ellenőrzi a lezárást, és callbackje blokkolhatja azt.

**Előzetes ellenőrzés:** meglévő output, lemezbetelés, jogosultsági hiba, hiányzó input, hamis send-eredmény, `None`/rövid recv, megszakítás minden fázisban. Csak ennek rögzítése után javasolt a soros kapcsolat előtti fájl-/méretvalidálás, felülírás elleni védelem, külön részleges fájl és sikerhez kötött véglegesítés, egyértelmű kilépési kód, SHA-256 és független visszaolvasás. A hash fájlegyezést mutat; önmagában nem bizonyítja, hogy a ROM az adott alaplaphoz helyes.

### 7.6. Tesztelhetőség és publikálandó adatok

**Tény:** az importkor induló I/O és a globális `ser` nehezíti az izolált tesztelést; csak két callback önálló függvény. A konstruktor/serial/stream megnyitás, stdin és XMODEM-függőség később fake-kel helyettesíthető, de a jelenlegi importot nem szabad valódi porttal végrehajtani (mindkét script `1-16,19-41`). Előbb meglévő viselkedést modellező karakterizálás kell, majd külön lépésben függőség-injektálás.

A 13 jelenlegi fájl teljes olvasása és a helyi történet `secret/password/token/api_key/private key` jellegű keresése nem adott nyilvánvaló hitelesítő titkot. Ez nem teljes titokszkennelési tanúsítás. A kódban szerepel gépfüggő port és helyi fájlnév (mindkét script `5-5,40-40`), szerzői e-mail a Winbond-fejlécben (`src/winbondflash.cpp:3-8`) és metadata-kontakt (`library.properties:3-4`); a Git-szerzői metadata szintén személyes adatot tartalmaz. Publikálás előtt attribúciót és elérhetőséget meg kell különböztetni a titkoktól, nem automatikusan törölni.

A `.gitignore:1-9` nem zárja ki a ROM-okat, Python-környezetet, bytecode-ot vagy jövőbeli `.pio` buildkönyvtárat. A mostani leltárban ilyen fájl nincs. Későbbi kizárásszabály-változás előtt `git check-ignore`-jellegű ellenőrzés és csomagleltár szükséges; valódi BIOS-dump publikálhatósága tulajdonosi döntés és tartalomellenőrzés nélkül **NEM ELDÖNTHETŐ**.

## 8. PlatformIO- és Python-környezeti illesztés

### 8.1. A jelenlegi forma és a cél

**Tény:** a gyökérbeli `library.properties`, `src/` implementáció és `examples/BIOSprog/BIOSprog.ino` Arduino library + példasketch szerkezetet ad (`library.properties:1-9`; `examples/BIOSprog/BIOSprog.ino:1-34`; leltár). Ez megfelel az Arduino library szerkezeti mintájának, de a metadata és fordíthatóság hiányosságait nem oldja meg ([Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification/)). Önálló PlatformIO-alkalmazásként most nincs konfigurálva; a `src/` nem tartalmaz `setup()`/`loop()` belépést.

**Javasolt, most létre nem hozott célstruktúra:** az első PlatformIO-mérföldkő őrizze meg a meglévő libraryt és példát, fájlmozgatás nélkül.

```text
platformio.ini                    későbbi, megerősített célhoz rögzített konfiguráció
library.properties                meglévő Arduino metadata megtartva
src/                              meglévő .cpp és publikus .h fájlok maradnak
examples/BIOSprog/BIOSprog.ino     meglévő firmware-belépés marad
include/                          opcionális jövőbeli projektadapter; most nem szükséges
lib/                              opcionális külön helyi könyvtár; a meglévő kódot nem duplázzuk
test/                             későbbi firmware-karakterizálás / hostos fake környezet
tests/                            későbbi Python karakterizálás
xmodembios.py                     későbbi egységes CLI, külön migrációs döntéssel
.python-version                  későbbi konkrét interpreter-rögzítés
pyproject.toml                    későbbi Python-projektmetadata, ha a csomagolási cél igényli
```

A `platformio.ini` a projekt gyökerében definiálhat környezetet ([projektkonfiguráció](https://docs.platformio.org/en/latest/projectconf/index.html)). Lehetséges első illesztés: az alkalmazás `src_dir`-je a meglévő példasketch könyvtára, a repository gyökerét pedig helyi libraryként felvenni. A pontos lokális dependency-megoldást későbbi, telepítést kontrolláló buildpróbával kell igazolni; ne kerüljön ugyanaz a forrás egyszerre alkalmazásként és könyvtárként linkelésre. A [src_dir](https://docs.platformio.org/en/latest/projectconf/sections/platformio/options/directory/src_dir.html) konfigurálható; a [library manager](https://docs.platformio.org/en/latest/librarymanager/creating.html) a `library.properties` manifestet is elfogadja, a [lib_deps](https://docs.platformio.org/en/latest/projectconf/sections/env/options/library/lib_deps.html) pedig függőségeket kezel. Ez terv, nem ellenőrzött kész konfiguráció.

**Feltételes környezet:** a kód AVR-kötődése miatt `framework=arduino`, megerősített klasszikus AVR board esetén `platform=atmelavr` reális. UNO-hoz létezik `board=uno` a [PlatformIO dokumentációban](https://docs.platformio.org/en/latest/boards/atmelavr/uno.html). Ez csak példa, végleges boardot az audit nem választ. Nano esetén a pontos MCU/bootloader és revízió szükséges; ESP8266-hoz előbb az AVR-specifikus részek kompatibilitását kell bizonyítani. Repository-bizonyíték: 5.4. és 6.6. szakasz.

**Minden ilyen illesztés előtt:** eredeti fordító/core/board/flaglista, bináris és memóriahasználat megőrzése. Utána csak build, feltöltés nélkül; az új és régi build diagnosztikájának, méretének és protokolltrace-ének összevetése. A fejlécnév javítása, narrowing rendezése és a buildrendszer-váltás külön ellenőrizhető lépések legyenek.

### 8.2. Arduino-kompatibilitás megőrzése

A `library.properties`, az `examples/` és a `src/` publikus fejlécei maradjanak elérhetők Arduino IDE/CLI számára is. A `name`, `version`, szerző/maintainer, leírás, URL, `architectures` és `includes` későbbi rendezése indokolt, de csak attribúcióellenőrzés és library-import/build baseline után (`library.properties:1-9`). A publikus fejlécek automatikus `include/` alá mozgatása megtörheti az Arduino keresési útját; a specifikáció a library `src/` gyökerét adja hozzá ([Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification/)).

Kompatibilitási töréspontok: case-sensitive include; AVR stdio/PROGMEM; `SPIClass` másolhatóság; CS-pin inicializálás; millis-alapúvá alakított timeout időzítése; `.ino` előfeldolgozás és új `.cpp` belépés; új compiler-optimalizálás; üres/hiányzó metadata mezők. Ezekhez a 6. szakaszban megnevezett külön ellenőrzések szükségesek. A mostani sikeres részleges syntax check nem helyettesíti a teljes linket.

### 8.3. Python-verzió rögzítése és környezetizoláció: két külön döntés

**Jelenlegi állapot:** csak `requirements.txt` van; a leltárban nincs `.python-version`, venv-konfiguráció, lock vagy Python-projektmetadata. A shellben látható 3.14.4 nem a projekt által választott támogatási cél. A Python 2 referenciafuttatás környezete **NEM ELDÖNTHETŐ**.

1. **Interpreter:** a későbbi `.python-version` egy konkrét, tesztelt Python 3 patchverzió kiválasztását rögzítse pyenv számára. A pyenv verziót választ; ezzel önmagában még nincs projektfüggőség-izoláció. A támogatandó OS és csomagkompatibilitás tisztázása előzze meg a verzióválasztást. A pyenv alapmegoldás natív Windowsra nem azonos a UNIX-szerű használattal; ezt az OS-döntésnél külön kell kezelni ([pyenv README](https://github.com/pyenv/pyenv)).
2. **Függőségek elkülönítése:** két nyitott opció. **pyenv + venv:** a kiválasztott interpreterből projektbeli `.venv`, egyszerű standardkönyvtári megoldás, külön aktiválással ([Python venv](https://docs.python.org/3/library/venv.html)). **pyenv + pyenv-virtualenv:** névvel kezelt környezet és opcionális automatikus aktiválás, további plugin-/shellkonfigurációval ([pyenv-virtualenv](https://github.com/pyenv/pyenv-virtualenv)). Utóbbinál a `.python-version` környezetnevet is tartalmazhat; ha ez a választás, az alapinterpreter pontos verziója is legyen külön egyértelműen reprodukálható.

Az audit egyik izolációs opciót sem választja véglegesen. Döntési alap: támogatandó OS-ek, a tulajdonos jelenlegi pyenv-használata, CI és aktiválási preferencia. Bármelyiknél külön szükséges a tesztelt függőségfeloldás rögzítése; a környezetkönyvtár nem publikálandó projektforrás.

Később a `pyproject.toml` `requires-python`, dependencies és CLI entry point mezői rendezhetik a csomagolást ([Python Packaging útmutató](https://packaging.python.org/en/latest/guides/writing-pyproject-toml/)). Ezek nem helyettesítik sem a `.python-version` konkrét interpreterét, sem az izolált környezetet. Minden környezeti/csomagolási változtatás előtt a régi hostprotokoll trace-e, utána hardver nélküli import-, CLI- és dependency-kompatibilitási ellenőrzés kell; telepítés ebben az auditban nem történt.

## 9. README–kód eltérések

| README / metadata állítás | Tényleges állapot és bizonyíték | Következmény / későbbi ellenőrzés |
| --- | --- | --- |
| Bármilyen XMODEM-es terminál; `README.md:6-6`. | A firmware CRC/128-at implementál, checksum/1K nincs; `src/xmodem.cpp:59-77,174-204,230-243`; `src/xmodem.h:14-14`. | Terminálmód és EOF-partner megnevezése előtt interoperabilitási trace. |
| `0/r/e/w`; `README.md:35-41`. | Nagybetűs E/W/R és közvetlen `C` is él, CR/CAN külön kezelt; `src/programmer.cpp:49-60,62-76,119-132`. | Régi terminálhasználat karakterizálása, majd pontos parancstábla. |
| „16 Mb”, kb. egy óra; `README.md:4-4`. | Kódban bytekapacitások és 1–16 MiB vannak; státusz `b/k/M` jelöléssel; `src/winbondflash.cpp:56-61`; `src/programmer.cpp:28-33`. | Bit/bájt jelölés nem egyértelmű; tényleges sebesség és eredeti jelentés mérés nélkül NEM ELDÖNTHETŐ. |
| Minden jelre 5 V → 3.3 V illesztés; `README.md:30-30`. | A feltételes W25Q128JV/UNO példában MISO a chip kimenete; 5.4. szakasz elsődleges dokumentumai. | Kapcsolási rajz és mérés után irányhelyes leírás. |
| Erase írás előtt; `README.md:39-39`. | A writer nem töröl és nem ellenőrzi a törölt állapotot; `xmodembioswrite.py:22-41`; `src/programmer.cpp:75-118`. | A kézi előfeltétel valós, de nincs automatizált biztosítása; fake flash ellenőrzés a változtatás előtt. |
| `pip2.7`; `README.md:46-46`. | Python 2 print, nincs interpreter-pin; mindkét script `24-28,35-36`; leltár. | Dokumentált Python 3 cél csak karakterizált migráció után. |
| Kétszer a read scriptet kell szerkeszteni; `README.md:47-47`. | Külön writer is van; `xmodembioswrite.py:1-41`. | Dokumentációs elírás, később a dokumentált CLI-példával ellenőrizhető. |
| `xmodembios.py write\|read ...` TODO; `README.md:49-50`. | Nincs ilyen fájl, argumentumparser vagy megadható port/baud/ROM; két script `5-8,19-41`; leltár. | Ez még nem használható parancs; későbbi parser-/help-teszt. |
| A script „Press d” promptja dumpot ígér. | Csak `0` küld initialize-t; writer is ugyanezt írja; mindkét script `19-23,31-31`. | Az öt soros várakozás megakadhat; stdin/serial baseline után javítható. |
| Arduino és ESP8266, modern callbackek, WebSocket/WSS; `library.properties:5-6`. | AVR stdio, SPI flash és XMODEM található; nincs WebSocket-komponens; `examples/BIOSprog/BIOSprog.ino:21-33`; `src/programmer.cpp:9-20`; teljes leltár. | A metadata túlzó/idegen tartalmú; előbb eredet- és buildvizsgálat. |
| Projekt URL: `bsvdoom/LM`; `library.properties:8-8`. | Git origin: `bsvdoom/ArduinoBIOSProgrammer`; 3.1. szakasz. | URL-t eredetellenőrzés után rendezni, nem automatikusan upstreamként kezelni. |

A README emellett nem írja le a tényleges buildreceptet, tesztelt board/chip/hostverziót, retry-korlátokat, integritásellenőrzést vagy részleges fájl szabályait (`README.md:1-50`). Minden későbbi útmutató a már ellenőrzött parancsokból és mérési eredményekből készüljön.

## 10. Kockázati jegyzék

A súlyosság a lehetséges következményt, a bizonyosság a megfigyelést minősíti. A „biztos” kódhiba nem jelenti, hogy azt valódi chipen reprodukáltam. Külső hivatkozásokat a kapcsolódó részletes szakaszok tartalmaznak. Az alábbi következő ellenőrzések minden javítást megelőznek.

| ID | Terület | Megfigyelés | Bizonyíték | Következmény | Súlyosság | Bizonyosság | Javasolt következő ellenőrzés |
| --- | --- | --- | --- | --- | --- | --- | --- |
| R01 | XMODEM / RAM | Retry alatt a küldő/fogadó mutató nem áll vissza. | `src/xmodem.cpp:100-129,184-249`; 6.1. | Téves payload, tömbön túli olvasás/írás, vezérlési állapot sérülése. | kritikus | biztos | Fake Stream, NAK/CRC/rövid csomag; őrzött buffer és trace. |
| R02 | Flash / EOF | END mellett is teljes lapírás, érvényes hossz nélkül. | `src/programmer.cpp:81-95`; 6.2. | Üres/részleges ROM-nál nem kért adatok programozása, korábbi lap ismétlése. | kritikus | biztos | 0/128/256/257 bájtos esetek fake flash cím-/payloadnaplóval. |
| R03 | Elektromos illesztés | A pontos board, chipváltozat és szintillesztő nincs dokumentálva; a tényleges alkalmasság ismeretlen. | `README.md:10-30`; 5.4., Arduino/Winbond források. | Nem megfelelő valós bekötés esetén chipkárosodás; meglévő károsodás nem bizonyított. | kritikus | feltételezés | Pontos alkatrész-/rajzazonosítás, táp- és jelalakmérés bármely hardverpróba előtt. |
| R04 | Íráslezárás | Teljes kapacitásnál nincs utolsó ACK/EOT fogadás. | `src/programmer.cpp:82-105`; `src/xmodem.cpp:146-150,251-254`. | A chip már íródott, a host még vár/újraküld; `Out of memory`, sikertelen lezárás. | magas | biztos | Pontosan `size` és `size+128` trace, utolsó blokk/EOT követése. |
| R05 | Flash-integritás | Nincs blank-check/verify, WEL/protection-ellenőrzés; void műveletek. | `src/programmer.cpp:65-116`; `src/winbondflash.cpp:211-216,307-350`. | Elutasított vagy hibás programozás után félrevezető siker. | magas | biztos | WREN/protection/program hiba injektálása; visszaolvasási elvárás rögzítése. |
| R06 | Timeout | Busy-ciklus korlátlan, XMODEM timeout ciklusszámláló, hibadrain korlátlan lehet. | `src/programmer.cpp:68-71,93-93`; `src/xmodem.cpp:38-56,174-193`. | Megakadás, hosttal elvesztett szinkron, nem kezelhető műveletmegszakítás. | magas | biztos | Fake tartós busy/zaj/csend; tényleges régi timeout későbbi mérése. |
| R07 | Dump | `read()` visszatérési értéke eldobva. | `src/winbondflash.cpp:290-304`; `src/programmer.cpp:139-142`. | Busy esetén régi/inicializálatlan adat érvényes transzport-CRC-vel. | magas | biztos | 0 bájtot visszaadó fake read, bufferelőélet követése. |
| R08 | Parancsszinkron | Hibás átvitel után maradó bináris bájtok visszakerülhetnek a parancsértelmezőbe. | `src/programmer.cpp:44-76,99-169`; 6.7. | Véletlen initialize/read/write/erase kiváltása lehetséges. | magas | valószínű | Abort/capacity után maradó payload parancsbájtokkal, kizárólag fake flash. |
| R09 | Python / fájl | Rögzített output `wb`, nincs overwrite-/részlegesfájl-védelem. | `xmodembiosread.py:40-41`. | Korábbi jó mentés elvesztése; hibás részleges dump használata. | magas | biztos | Meglévő output, lemezbetelés, recv-hiba és megszakítás. |
| R10 | Host / státusz | Átviteli eredmény eldobva; file-open az írásparancs után. | Mindkét script `30-41`. | Hamis processzsiker; hiányzó input mellett már fogadásra váró firmware. | magas | biztos | False/None eredmény, hiányzó input, exit code és parancssorrend. |
| R11 | Host / serial | Callback-timeout ignorálva, serial alapértelmezetten blokkoló; nincs hibaág. | Mindkét script `8-16,24-36`; pySerial API, 7.4. | Végtelen várás, retry nem érvényesül, traceback. | magas | biztos | Fake serial csend/rövid I/O/portvesztés és határidők. |
| R12 | Publikálhatóság | Projekt- és örökölt Winbond-licenc nincs igazolva. | `src/winbondflash.cpp:3-8`; `library.properties:3-8`; leltár/commitlánc, 4. szakasz. | Nem igazolható az újrapublikálás licencalapja. | magas | biztos | Upstream-verzió/licenc/attribúció/jogosultság összegyűjtése komponensenként. |
| R13 | Build | `XModem.h` include, tényleges fájl `xmodem.h`. | `src/programmer.cpp:2-2`; `src/xmodem.h:1-4`; syntax check. | Case-sensitive környezetben fordítás megáll. | magas | biztos | Referencia-build és jelenlegi hibadiagnosztika, majd izolált névjavítás ellenőrzése. |
| R14 | Python 3 | Python 2 print és szöveges soros parancsok. | Mindkét script `22-36`; AST és pySerial API. | A script el sem indul Python 3-on; szintaxisjavítás után további API-hiba. | magas | biztos | AST + bytes/str-karakterizálás; csak utána migráció. |
| R15 | Memóriaélettartam | Műveletenként `new XModem`, nincs felszabadítás. | `src/programmer.cpp:79-79,126-126`; teljes `task`: `41-171`. | Ismételt műveleteknél heapelfogyás/instabilitás. | magas | biztos | Ismételt read/write fake műveletek foglaláskövetéssel. |
| R16 | Kapacitás / típus | 65536 nem fér el `uint16_t pages` mezőben. | `src/winbondflash.cpp:47-62,230-239`; syntax check. | Szigorú buildhiba vagy csonkolt lapszám; a bytes-alapú méret ettől még lehet helyes. | közepes | biztos | Öt chip bytes/pages/sectors/blocks konzisztenciája két flagkészlettel. |
| R17 | Könyvtári API | Explicit/custom begin nem állít partno-t, nincs API-cím-/hosszvalidálás. | `src/winbondflash.cpp:149-188,290-318`; `src/winbondflash.h:72-74,100-104`. | Nem autodetect használatnál téves méret; nem megfelelő címen programozás. | közepes | biztos | Explicit/custom esetek és 24 bites/kapacitáshatár; jelenlegi példára hatás külön kezelve. |
| R18 | Protokoll / lezárás | `finish_send()` eredménye eldobva, ETB után további várás; NAK/timeout aszimmetria. | `src/xmodem.cpp:80-98,127-135`; `src/programmer.cpp:146-150`. | Partnerfüggő lezárás, elnyelt hiba és késleltetés. | közepes | biztos | EOT-ACK elvesztése és több terminálpartner naplója. |
| R19 | CLI / szinkron | Téves „d” prompt, fix öt+kétsoros olvasás, nincs parser. | Mindkét script `19-41`; `README.md:49-50`. | Megakadás, felhasználói tévesztés, automatizálhatatlanság. | közepes | biztos | Banner/init-hiba/stdin karakterizálás; később parser/help ellenőrzés. |
| R20 | Hordozhatóság | AVR API-k, SPI-értékmásolás, implicit alap-SS inicializálás. | `examples/BIOSprog/BIOSprog.ino:26-26`; `src/programmer.cpp:16-16`; `src/winbondflash.cpp:367-382`. | Más core/CS esetén build- vagy buszprobléma. | közepes | valószínű | Boardazonosítás; eredeti és célcore build-/SPI-trace. |
| R21 | Reprodukálhatóság | Nincs interpreter/toolchain pin, lock, CI vagy teszt; xmodem tartomány több verziót enged. | `requirements.txt:1-2`; teljes leltár; 7.2–8. szakasz. | Gépenként eltérő környezet, regressziók bizonyítása nehéz. | közepes | biztos | Eredeti verziólista és golden trace; utána környezetenkénti offline tesztterv. |
| R22 | Metadata / publikáció | Idegen leírás/URL, általános architektúra, Library Manager névprobléma. | `library.properties:1-9`; Arduino specification, 4.3. | Félrevezető támogatásígéret, csomagfelvételi akadály. | közepes | biztos | Attribúció és csomagleltár, majd library-import/build és metadata-ellenőrzés. |
| R23 | Repository-higiénia | ROM/környezet/build kizárások hiányoznak. | `.gitignore:1-9`; leltárban jelenleg nincs ilyen adat. | Később nem publikálásra szánt dump/gépfüggő állomány kerülhet Gitbe. | közepes | valószínű | Követendő fájlok tulajdonosi döntése; ignore-/csomagleltár-ellenőrzés. |
| R24 | API / karbantarthatóság | Régi deklarációk, hiányzó bázisfüggvény-definíció, magic count és típusfüggő ciklus. | `src/xmodem.h:16-24`; `src/winbondflash.h:79-79`; `src/programmer.cpp:84-84`; `src/winbondflash.cpp:314-318`. | Későbbi API-hívás/link vagy látszólag stiláris átírás kompatibilitást törhet. | alacsony | valószínű | Szimbólum/link- és pontos bájtszám-ellenőrzés az eltávolítás/átírás előtt. |

## 11. Karakterizálási és tesztstratégia

### 11.1. Megőrzendő golden sample-ek

A tulajdonos a változtatások előtt őrizze meg az alábbi, már létező referenciákat. Ezek jelenleg nem szerepelnek a teljes leltárban; meglétük máshol **NEM ELDÖNTHETŐ**. BIOS-tartalmat nem kell automatikusan a nyilvános repositoryba tenni.

- A tényleges chip teljes felirata/tokozása, nyers JEDEC-azonosítója, kapacitása bájtban; a működő board/revízió, MCU, órajel és szintillesztő pontos típusa, bekötése.
- Egy ismert jó, teljes ROM-dump pontos mérete és SHA-256 hash-e; legalább két egymástól független olvasás összehasonlítása. A „jó” eredetét és ellenőrzési módját is rögzíteni kell.
- A működő firmware forráscommitja és lehetőség szerint eredeti binárisa/hash-e; IDE/core/compiler verzió, boardazonosító, buildkapcsolók és memóriahasználat.
- Egy ismert sikeres terminál-/Python-parancssor és a hozzá tartozó billentyűzési sorrend, OS, interpreter, pySerial/xmodem pontos verzió, port, baud, portnyitási/reset viselkedés.
- Időbélyeges, bájtszintű soros/XMODEM napló: banner, initialize, CRC-kézfogás, ACK/NAK, sorszám-átfordulás és EOT/ETB. Ha már létezik, SPI logic-analyzer felvétel a valós környezet időzítéseiről.
- Már rendelkezésre álló sikeres read/erase/write/**független verify** eredmények. Hiányzó törlés/írás referenciát most nem szabad pótolni: ennek későbbi feltételeit a 11.3. szakasz rögzíti.

Az összehasonlítás különítse el a megőrzendő sikeres viselkedést és a bizonyított hibák szándékos változását. Például a CRC/128 protokoll és a támogatott parancsok baseline-ja megőrzendő lehet; a túlcsordulás vagy extra lapírás megszüntetése dokumentált eltérés legyen, nem elfogadott regresszió.

### 11.2. Hardver nélküli tesztötletek — most csak terv

| Tesztcsalád | Bemenet / modell | Összehasonlítási alap és elvárt bizonyíték |
| --- | --- | --- |
| Firmware build | Eredeti és későbbi megerősített board/core/compiler, case-sensitive fájlrendszer. | Diagnosztika, teljes link, firmware-/RAM-méret; most csak a 3.3. részleges ellenőrzése áll rendelkezésre. |
| XMODEM küldés | Fake Stream: C/ACK/NAK/CAN/csend, két különböző fél lap, többszörös retry. | Pontos vezetéki bájtok; ugyanazon sorszám retryja ugyanazt a payloadot küldje a későbbi javított változatban; bufferhatár. |
| XMODEM fogadás | Jó/rossz CRC, komplement, rövid fejléc/adat/CRC, duplikáció, 255→0, zaj. | ACK/NAK/cancel sorrend, korlátos próbálkozás, őrzött célbuffer, duplikáció ne programozzon kétszer. |
| EOF és kapacitás | 6.2. teljes méretmátrixa, korai EOT, utolsó blokk ACK-vesztés. | Flashműveletek darabszáma/címe/adattartalma; nincs rejtett extra lap; lezárási eredmény egyértelmű. |
| Flash API | Fake JEDEC/SPI, ismert/ismeretlen chip, explicit/custom, rossz cím. | Táblázott kapacitás, tényleges cím-/opkódbájtok, CS-párok; ismeretlen chip után nincs programozás. |
| Flash hibák | WEL hiány, protection, örök busy, read=0, programozás utáni eltérés. | Művelet ne látszódjon sikeresnek ellenőrzés nélkül; határidő és maradék állapot dokumentálva. |
| Command recovery | Megszakítás után maradó bájtfolyam, benne `0/e/w/r/C`. | A későbbi javítás ne értelmezze automatikusan a maradék payloadot új parancsnak; régi terminálkompatibilitás külön mérve. |
| Python I/O | Fake serial/stdin, banner nélkül/hibás bannerrel, részleges read/write, timeout, megszakítás. | Bytes/str helyesség, véges futás, soros parancsok sorrendje és a callback szerződése. |
| Python fájlbiztonság | Meglévő output, hiányzó input, lemezbetelés, hibás méret, send=False, recv=None. | Nincs észrevétlen felülírás/siker; input-validálás soros művelet előtt; erőforrások lezárása. |
| Későbbi CLI | read/write, hiányzó/hibás paraméter, port/baud variáns, help; hardver nélküli indítás. | Dokumentált súgó, determinisztikus exit code, importkor nincs I/O. |
| Függőségváltás | Megerősített régi xmodem és külön jelölt migrációs cél ugyanazzal a fake peerrel. | Packet/retry/EOF trace és visszatérési értékek összevetése; nem pusztán „import sikeres”. |
| Erőforrás / csomag | Ismételt műveletek, library-import, későbbi disztribúciós leltár. | Nincs újabb szivárgás, minden szükséges forrás és értesítés jelen, ROM/környezet nincs csomagolva. |

Új fake réteg, tesztkönyvtár, fixture vagy generált firmware **nem készült**. A tervezett tesztek későbbi megvalósításakor a portnyitást és a flashműveleteket alapértelmezetten fake függőségekhez kell kötni. A gazdagabb viselkedési teszteket nem helyettesíti a függvény törzsét másoló, ugyanazt a hibát reprodukáló külön algoritmus.

### 11.3. Későbbi hardveres smoke-test ellenőrzőlista

Ez a lista nem mostani végrehajtási engedély. Az audit során soros port megnyitása, initialize/read/erase/write nem történt.

**Első szakasz: csak olvasásra irányuló referenciaellenőrzés.**

1. Board, teljes chipazonosító, tokozás és szintillesztő megerősítése; a pontos gyártói adatlap szerinti táp-/jelszint-/bekötésellenőrzés. In-circuit esetben a másik áramkör és a visszatáplálás helyzete előbb tisztázandó. Ezek az 5.4. szakasz hiányzó adatai.
2. A régi, már működő firmware/build és hostkörnyezet azonosítása; új firmware feltöltése nélkül, ha a referencia már rendelkezésre áll. Naplózás és új, korábbit nem felülíró dumpcél előkészítése.
3. Csak a dokumentált initialize és read eljárás későbbi végrehajtása; a jelenlegi initialize `AB` release parancsot is küld, ezért nem pusztán passzív buszfigyelés (`src/winbondflash.cpp:266-277`). Chip-ID és méret egyezés, majd teljes dump.
4. Újabb független dump, méret-/hash-egyezés; ha eltérés, megállás és okfeltárás. A sikeres XMODEM vagy `CHIP READY` önmagában nem elfogadási feltétel.
5. EOT/ETB és visszatérés ellenőrzése, read-időtartam és jelalak rögzítése. Hiba-injektálás valódi értékes chipen nem része az alap olvasási smoke-testnek.

**Második szakasz: törlés/írás kizárólag feláldozható tesztchipen és külön, kifejezett felhasználói jóváhagyással.**

1. Mielőtt ez a szakasz egyáltalán elkezdődik: R01/R02/R04/R08 hardver nélküli tesztje és a művelethez szükséges javítások ellenőrzése; pontos célchip és ismert bemeneti ROM/hash.
2. Jóváhagyott erase, utána teljes visszaolvasásos blank-check; jóváhagyott write, utána teljes független readback és byte-/hash-összevetés.
3. Csak külön jóváhagyott teszttervben megszakított írás/tápvesztés/hibás átvitel és recovery, ugyanezen feláldozható chipen. Az értékes BIOS-chip nem hibateszt-célpont.
4. A teljes build-, host-, chip-, trace- és verify-eredmény megőrzése új golden referenciaként. Egy sikerszöveg alapján nem tekinthető elfogadottnak.

## 12. Javasolt mérföldkő-sorrend — csak magas szintű, kódmódosítás nélkül

Minden sor külön, kicsi és visszavonható változtatás legyen. Bemeneti kapu nélkül a következő lépés ne induljon; a hardverre írt tartalom visszaállítása külön kérdés, azt a Git-revert nem végzi el. A táblázat nem felhatalmazás az audit utáni implementáció automatikus megkezdésére.

| Sorrend | Cél | Előfeltétel / összehasonlítható ellenőrzés | Lezárás és visszaállítási pont |
| --- | --- | --- | --- |
| M0 | Eredet, hardver és referencia rögzítése. | 13. szakasz elsődleges kérdései; meglévő golden minták összegyűjtése, kódváltozás nélkül. | Igazolt/nem igazolt licencek listája, pontos tesztcél, eredeti build-/hostleltár; a referencia változatlan. |
| M1 | Hardver nélküli karakterizáló keret. | A meglévő byte-/lap-/EOF-viselkedés kézi levezetése és rendelkezésre álló trace. | R01/R02/R04/R08 regressziós kiváltók fake környezetben; új tesztek külön commitban visszavonhatók. |
| M2 | Minimális buildakadályok rendezése. | A 3.3. jelenlegi diagnosztikája, eredeti board/core megerősítése. | Fejlécnév, majd lapszámtípus külön változtatás; build/link és bytes/pages teszt, működési átírás nélkül. |
| M3 | Adat- és memóriahibák célzott megszüntetése. | M1 reprodukáló esetei; kért képméret-/EOF-szerződés eldöntése. | Retry-mutatatás, lapvégek, kapacitáslezárás és command recovery egyenként; minden javításhoz trace-diff, külön visszavonható commit. |
| M4 | Hibakezelés és integritás. | WEL/busy/read-failure fake esetek, pontos chip adatlapja, régi sikeres trace. | Határidő, státusz, erőforrás-élettartam, verify külön lépések; régi sikeres parancsok összevetve. Hardveres írás csak 11.3. feltételeivel. |
| M5 | PlatformIO-build a meglévő library mellett. | M2 után igazolt teljes referencia-build; M3–M4 tesztjei. | Rögzített board/platform/framework, eredeti forráshelyek; build/link/méret és protokoll összevetés, külön konfigurációs commit. |
| M6 | Python 3 és elkülönített környezet. | Régi hostverziók és fake serial trace, venv/pyenv-virtualenv döntés. | Interpreter-pin, izoláció, szintaxis/bytes és erőforrás-kezelés külön ellenőrizve; xmodem-frissítés önálló változtatás. |
| M7 | Egységes `xmodembios.py read\|write` CLI és fájlbiztonság. | Mindkét régi script karakterizálása; teljes/részleges ROM- és overwrite-szabály eldöntve. | Parser/help/exit code/input-validálás/partial output/integritás fake tesztje; dokumentált régi belépési pontok sorsa, külön visszaállítás. |
| M8 | Dokumentáció, CI és publikálható csomag. | Ellenőrzött új parancsok/build, tisztázott licencek/attribúció, golden eredmények. | README és metadata valós állapotból; hardver nélküli CI, csomagleltár, titok/ROM-kizárás; publikálás csak rendezett jogosultságokkal. |

A környezetépítési és stiláris munka ne előzze meg a bizonyított adatvesztési kockázatok karakterizálását. Nagy C++-átírás, XMODEM-csere, Python-migráció és boardváltás egyetlen mérföldkőben nem javasolt.

## 13. Nyitott kérdések

A következő, prioritás szerint rendezett kérdések az auditból hátralévő döntések; a jelen audit elkészítéséhez nem igényeltek válaszra várást. Mindegyiknél az érintett hiányzó tény **NEM ELDÖNTHETŐ** a repositoryból.

1. Megerősíthető-e az eredeti Arduino-projekt URL-je és használt commitja, továbbá rendelkezésre áll-e projektlicenc, külön Winbond-library licenc és a helyi Python-kiegészítések attribúciója/jogosultsága? A `ma5ter` projekt és a Google Code-nyom pontos kapcsolatát mi igazolja?
2. Pontosan melyik Arduino boardot, revíziót/klónt, MCU-t, órajelet és bootloadert használta a működő összeállítás?
3. Mely Winbond chipeket próbálták ki sikeresen: teljes típusjel/utótag, tokozás, nyers JEDEC ID és kapacitás bájtban?
4. Mi a szintillesztő pontos típusa és kapcsolási rajza, honnan kap tápot a chip, milyen a bekötés, és kiemelt vagy alaplapra forrasztott chipen történt a használat? Van-e táp-/jelszint-/jelalakmérés?
5. Mi az ismert sikeres Python- vagy terminálindítás, teljes billentyűzési sorrend, interpreter/pySerial/xmodem verzió és read/write lezárási napló? Mekkora fájlt írtak, és történt-e független verify?
6. Megvan-e a működő firmware binárisa és az eredeti IDE/core/compiler/FQBN/buildflag-lista, vagy csak a chipre feltöltött példány létezik?
7. Rendelkezésre áll-e ismert jó ROM, méret és hash, ismételten egyező dump, valamint feláldozható tesztchip a később külön jóváhagyandó írási tesztekhez?
8. Mely host operációs rendszereket és architektúrákat kell támogatni: Linux, macOS, natív Windows, WSL? Mely port-/USB-serial eszközök számítanak támogatási célnak?
9. A pyenvvel választott interpreter mellett projektbeli `venv` vagy `pyenv-virtualenv` legyen a függőségizoláció; fontos-e az automatikus aktiválás?
10. A kívánt CLI kizárólag teljes chipméretű nyers ROM-ot írjon, vagy szükséges részleges írás is? Az erase külön lépés maradjon, és mi legyen a meglévő output, padding, valamint kötelező readback/verify szabálya?
11. Meg kell-e őrizni a régi Arduino library telepítést, terminálparancsokat és a két régi Python-fájl nevét; az ESP8266 ténylegesen elvárt cél vagy csak hibás metadata?
12. Hol és milyen formában tervezett publikálás: GitHub-forrás, Arduino Library Manager, PlatformIO registry, Python-csomag? Milyen szerzői kontakt és tesztadat tehető nyilvánossá?

## 14. Felhasznált külső források

Elsődleges projekt-, API- és gyártói dokumentumok. A távoli „latest” oldalak nem jelentenek a projektre kiválasztott verziót; a dátumok az audit során látott állapotot rögzítik.

| Forrás | Mire használtam |
| --- | --- |
| [ma5ter/ArduinoBIOSProgrammer](https://github.com/ma5ter/ArduinoBIOSProgrammer) | Nyilvános upstream-jelölt, régi fájlszerkezet és ötelemű történet; teljes távoli commitazonosság nem igazolt. |
| [GitHub: repository licencelése](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository) | Nyilvános repository és licenc különbsége. |
| [Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification/) | `src/examples`, metadata, architektúra-alapérték, publikus fejlécek és Library Manager névkorlát. |
| [Arduino UNO R3 dokumentáció](https://docs.arduino.cc/resources/datasheets/A000066-datasheet.pdf) | Feltételes board/MCU- és IOREF-összevetés; nem a tényleges board igazolása. |
| [Arduino UNO R3 pinout](https://docs.arduino.cc/resources/pinouts/A000066-full-pinout.pdf) | D10–D13 SPI-jelek. |
| [Arduino Nano](https://docs.arduino.cc/hardware/nano) | Másik lehetséges klasszikus AVR boardcsalád; nem végleges célválasztás. |
| [Arduino AVR SPI implementáció](https://github.com/arduino/ArduinoCore-avr/blob/master/libraries/SPI/src/SPI.cpp) | Alap-SS OUTPUT és SPI-inicializálás; a helyi 1.8.7-es változatot külön is olvastam. |
| [AVR-libc stdio](https://avrdudes.github.io/avr-libc/avr-libc-user-manual/group__avr__stdio.html) | `fdev_setup_stream` és `vfprintf_P` AVR-kötődése. |
| [Winbond W25Q128JV adatlap, Rev. E, 2017-11-23, RS-másolat](https://docs.rs-online.com/cdee/0900766b81622f85.pdf) | Winbond által készített elsődleges dokumentum: 3.1–3.3./4.2. pinout, 7.1.8. protection, 8.2.13. page program, 9.1–9.2. elektromos határok. Csak a megnevezett változatra. |
| [Winbond W25Q128JVBIMG gyártói termékoldal](https://www.winbond.com/hq/product/code-storage-flash/qspi-nor/w25q-jv/?__locale=en&partNo=W25Q128JVBIMG) | Kiegészítő gyártói ellenőrzés; a konkrét BGA terméket nem azonosítottam a README 8 lábú chipjével. |
| [PlatformIO projektkonfiguráció](https://docs.platformio.org/en/latest/projectconf/index.html) | Gyökérbeli `platformio.ini`, környezetek. |
| [PlatformIO library létrehozása](https://docs.platformio.org/en/latest/librarymanager/creating.html) | Library-szerkezet és `library.properties` elfogadása. |
| [PlatformIO src_dir](https://docs.platformio.org/en/latest/projectconf/sections/platformio/options/directory/src_dir.html) | A meglévő példasketch alkalmazásforrásként történő későbbi illesztése. |
| [PlatformIO lib_deps](https://docs.platformio.org/en/latest/projectconf/sections/env/options/library/lib_deps.html) | Helyi library és függőségkezelés tervezési pontja. |
| [PlatformIO Atmel AVR](https://docs.platformio.org/en/latest/platforms/atmelavr.html), [UNO board](https://docs.platformio.org/en/latest/boards/atmelavr/uno.html) | Feltételes `atmelavr`/Arduino/UNO környezet. |
| [pyenv](https://github.com/pyenv/pyenv) | Interpreter-választás, `.python-version`, OS-határok. |
| [Python venv](https://docs.python.org/3/library/venv.html) | Standard izolált projektkörnyezet. |
| [pyenv-virtualenv](https://github.com/pyenv/pyenv-virtualenv) | Alternatív környezetkezelés és aktiválás. |
| [Python pyproject.toml útmutató](https://packaging.python.org/en/latest/guides/writing-pyproject-toml/) | Későbbi interpreter-kompatibilitási metadata, dependencies és CLI entry point. |
| [Python verzióspecifikáció](https://packaging.python.org/en/latest/specifications/version-specifiers/#compatible-release) | A két `~=` tartomány értelmezése. |
| [pySerial API](https://pyserial.readthedocs.io/en/latest/pyserial_api.html) | Timeout, bytes/str, portnyitás, DTR/RTS és platformfüggő portnév. |
| [pySerial PyPI](https://pypi.org/project/pyserial/) | 3.5-ös kiadás, dátum, kiadási metadata. |
| [xmodem PyPI](https://pypi.org/project/xmodem/) | Használati példa, 0.4.6/0.4.7/0.5.0 kiadástörténet és javítások. |
| [xmodem 0.4.6 forrás](https://github.com/tehmaze/xmodem/blob/0.4.6/xmodem/__init__.py) | Default blokk/padding, CRC-kezdés, visszatérési értékek és abort. Nem importáltam/futtattam. |
| [Python 2 támogatásának lezárása](https://www.python.org/doc/sunset-python-2/) | A migráció támogatási indoka. |

**Sikertelen lekérések / nem igazolt eredetnyomok:** [jelenlegi origin weboldala](https://github.com/bsvdoom/ArduinoBIOSProgrammer), [ma5ter konkrét commitoldal](https://github.com/ma5ter/ArduinoBIOSProgrammer/commit/f614f05), [Winbond-library Google Code archívum](https://code.google.com/archive/p/winbondflash/), annak Google Storage metadata-URL-je, több Winbond-hostolt W25Q128JV PDF-URL. Ezek cache-/lekérési hibája alapján nem állítom az oldalak vagy licencek hiányát. A Winbond-adatlap screenshot-lekérése is sikertelen volt; a szöveges PDF-kivonat elérhető volt. A primer gyártói dokumentum RS-példánya szolgált a tartalmi összevetéshez, nem harmadik fél műszaki összefoglalója.

## 15. Auditkorlátok és kihagyott ellenőrzések

### 15.1. Végrehajtott és kihagyott műveletek

**Végrehajtva:** 13 fájl teljes, sorszámozott olvasása; rejtett fájlokat is tartalmazó leltár; Git status/branch/remote/log/show/diff, korábbi fájlnevek és attribúció-/titokminták keresése; meglévő eszközök verziója és Python csomagmetadata; két Python AST-ellenőrzés végrehajtás nélkül; négy C++ fordítási egység csak szintaxisellenőrzése két indokolt kapcsolókészlettel; elsődleges online dokumentációk olvasása. Az egyetlen létrehozott fájl ez az `AUDIT.md`.

**Kihagyva, indokkal:**

- Python-scriptek indítása/importja: top-level serial portnyitás és további hardverműveletek miatt tiltott. Python 2 nincs a PATH-on; telepítés tiltott.
- XMODEM-fake tesztek, fuzzing, fixture-készítés és dinamikus memóriaellenőrzés: új teszt/fixture létrehozása nem része az auditnak. A hibák statikus kódútból levezetettek.
- Teljes Arduino/PlatformIO build, link és firmware-méret: eredeti board/buildrecept nincs; PlatformIO indító hibás, Arduino CLI nincs a PATH-on. A meglévő AVR eszközökkel biztonságosan elvégezhető syntax check megtörtént; a fejlécnév-hibát nem javítottam meg a további fordulás kedvéért.
- Sem csomag-, toolchain-, interpreter- vagy rendszerfüggőség-telepítés, sem környezetlétrehozás nem történt. Nem készítettem konfigurációt, tesztet, ideiglenes forrásmásolatot, objektumot vagy ROM-fájlt.
- Sem portfelderítő/portnyitó parancs, sem initialize, read, erase, write, upload vagy chipet érintő mérés nem futott. Nincs hardveres teljesítmény-, feszültség-, SPI- vagy ROM-integritási igazolás.
- Nem történt Git fetch/pull/checkout/commit, sem remote-módosítás. A böngészés olvasásra korlátozódott. A távoli fork/commit és a korai Winbond-licenc ellenőrzése a hozzáférési hibák és hiányzó igazolás miatt részleges maradt.
- A secrets-ellenőrzés nem kiterjedt biztonsági termék vizsgálata; a tényleges ROM-ok, nem elérhető branch-ek, más gépek és további környezetek nincsenek auditálva.
- A modell- és reasoning-sorok a megbízásban kért audit-metaadatok; a helyi eszközverzióktól eltérően nem repositoryból mért konfigurációs adatok.

### 15.2. Befejező változásellenőrzés

Az ellenőrzéskor a `git diff --exit-code` és a `git diff --cached --exit-code` egyaránt 0 kilépési kóddal, üres kimenettel zárult. Következésképpen egyetlen követett projektfájl sem tér el a HEAD állapotától, és nincs követett törlés, átnevezés vagy stagingelt változás. A `git status --short` kizárólag az új, nem követett `AUDIT.md` fájlt jelezte.

A 15.2. szakasz módosítása után ismét lefuttatott `git status --short` eredménye:

```text
?? AUDIT.md
```

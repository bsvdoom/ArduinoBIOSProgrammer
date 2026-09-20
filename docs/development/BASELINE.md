# Arduino BIOS Programmer — változtatásmentes baseline-jegyzőkönyv

- Aktuális lépés: 1/12 (baseline rögzítése).
- Rögzítés dátuma: 2026-09-06; időzóna: Europe/Budapest.
- Hatókör: a jelenlegi Git-állapot, a felhasználó által megerősített referencia-összeállítás, a két golden fájl metaadata és a helyi eszközkörnyezet dokumentálása. Nem történt build, firmware-feltöltés, sorosport-nyitás vagy flashművelet.
- Bizonyítékszintek: a Git-, fájl- és eszközadatok helyben mért eredmények; a hardver korábbi működése felhasználói beszámoló; a BIOS-chip típusjelölése másodlagos forrásból származik.

## 1. Git-állapot

- Aktuális branch: `master`.
- HEAD: `9446c9fe1318a965eb482f32e1d46f20801f5c9c`.
- A `git remote -v` kimenete:

```text
origin  https://github.com/bsvdoom/ArduinoBIOSProgrammer.git (fetch)
origin  https://github.com/bsvdoom/ArduinoBIOSProgrammer.git (push)
```

- A kezdeti `git status --short` kimenete:

```text
?? AUDIT.md
?? newbios2.bin
?? output.rom
```

- A kezdeti `git diff --exit-code` 0 exit kóddal, kimenet nélkül zárult: nincs nem stagingelt változás követett fájlban.
- A kezdeti `git diff --cached --exit-code` 0 exit kóddal, kimenet nélkül zárult: nincs stagingelt változás.
- A remote nem módosult; commit és push nem történt.

## 2. Megerősített referencia

| Elem | Rögzített referencia | Bizonyíték állapota |
| --- | --- | --- |
| Board | Arduino Uno | Felhasználói megerősítés. |
| Első támogatási cél | Linux | Felhasználói döntés. |
| Alaplap | ASUS Z170I Pro Gaming | Felhasználói megerősítés. |
| Feltételezett BIOS-chip | W25Q128FVIQ | **Másodlagos forrásból származó típusjelölt, fizikailag még nem ellenőrzött típusjelölés.** A [fórumforrás](https://rog-forum.asus.com/t5/gaming-motherboards/sku-for-the-dip8-bios-chip-on-the-z170-pro-gaming-motherboard/td-p/1068059) másik, hasonló nevű alaplapmodellre, az ASUS **Z170 Pro Gamingre** vonatkozik, nem a felhasználó által kipróbált ASUS **Z170I Pro Gamingre**. A Z170I pontos chipje fizikai felirat, fénykép vagy saját JEDEC-azonosítás nélkül nem igazolt. |
| Szintillesztő | Egyszerű, négy tranzisztoros kínai modul | Felhasználói megerősítés; pontos típusa és kapcsolása ismeretlen. |
| Korábbi működés | BIOS-chip olvasása és írása sikeres volt | A felhasználó beszámolója; ebben a baseline-lépésben nem reprodukált, és nem jelent külön readbackkel igazolt flashintegritást. |

## 3. Golden fájlok

A vizsgálat kizárólag a fájltípusra/olvashatóságra, pontos méretre és SHA-256-ra terjedt ki. Bináris tartalommegjelenítés, tartalomelemzés, BIOS-elemző eszköz, másolás, átnevezés és módosítás nem történt; a fájlok nem kerültek Git stagingbe.

| Fájl | Szerep | Szabályos, olvasható fájl | Méret | SHA-256 |
| --- | --- | --- | ---: | --- |
| `output.rom` | A chipről kiolvasott eredeti BIOS | Igen | 16 777 216 bájt | `09e27dbe44eae4fb0ab1eeabd01a36c19f8ce91ed52645b2dff60f1c59f14cde` |
| `newbios2.bin` | A chipre elküldött új BIOS | Igen | 16 777 216 bájt | `d1a54daf450c32816cc719712e33520f300d5b0b0c5ef9fe9b3248eb312b5cf2` |

- Méretegyezés: **igen**, mindkét fájl pontosan 16 777 216 bájt.
- Hash-egyezés: **nem**, a két SHA-256 különbözik.
- A 16 777 216 bájtos méret megfelel egy 128 Mbit kapacitású flash teljes nyers méretének. A W25Q128FV adatlapja a kapacitást `128M-bit / 16M-byte` értékként adja meg: [W25Q128FV adatlap](https://www.pjrc.com/teensy/W25Q128FV.pdf).
- A két 16 777 216 bájtos dump mérete konzisztens a 128 Mbit kapacitással, de önmagában nem igazolja a flash gyártóját, sorozatát vagy tokozási változatát.
- A különböző hash a két fájl eltérő tartalmát bizonyítja, de nem értékeli vagy elemzi a BIOS-tartalmat.

### 3.1. Byte-order / páronkénti csere gyanú

**Felhasználói emlék:** egy korábbi hexnézetben `12 AF` helyett `AF 12` jelent meg.

**Statikus kódvizsgálat eredménye:** a jelenlegi flash-payload adatútban nem található explicit páronkénti bájtcsere, 16 bites szóként végzett payload-beolvasás vagy -kiírás, `htons`/`ntohs` vagy más endian-konverzió, `struct.pack`/`struct.unpack`, fordított lépésű `bytearray`-szeletelés, kézi low-byte/high-byte payloadcsere, illetve olyan pointer cast, amely a `uint8_t` payloadot `uint16_t` szavakként kezeli.

- Olvasáskor a flashréteg a beérkező bájtokat növekvő index szerint közvetlenül `buf[i]`-be teszi (`src/winbondflash.cpp:290-304`). A lapbuffer `uint8_t` tömb (`src/programmer.h:14-15`); a Programmer ugyanazt a buffert adja át az XMODEM-nek (`src/programmer.cpp:138-142`), amely `uint8_t` pointerrel, sorban küldi a bájtokat (`src/xmodem.cpp:100-125`).
- A Python olvasó a modem fogadásának közvetlenül egy `wb` módban megnyitott fájlstreamet ad át; mentés előtt nincs helyi adattranszformáció (`xmodembiosread.py:38-41`). A `decode()` hívások csak az átvitel előtti szöveges státuszsorokra vonatkoznak (`xmodembiosread.py:24-28,35-36`).
- Íráskor a Python-oldal az `rb` fájlstreamet közvetlenül a modemnek adja (`xmodembioswrite.py:38-41`). A firmware XMODEM-vevője a payloadot bájtonként, `*dst++ = octet` formában írja (`src/xmodem.cpp:218-228`); a Programmer két egymást követő 128 bájtos blokkot helyez a 256 bájtos lapbufferbe, majd változtatás nélkül adja át a flashrétegnek (`src/programmer.cpp:82-93`), amely növekvő index szerint küldi a `buf[i]` bájtokat (`src/winbondflash.cpp:307-319`).
- Az azonosító- és protokoll-metaadatokban vannak sorrendképző műveletek, de ezek nem alakítják át a dump payloadját: státuszregiszter-összeállítás (`src/winbondflash.cpp:64-76`), egyedi azonosító little-endian elrendezése (`src/winbondflash.cpp:91-108`), JEDEC part ID összeállítása (`src/winbondflash.cpp:111-132`), cím küldése magas bájttól lefelé (`src/winbondflash.h:100-104`; `src/winbondflash.cpp:307-313`), valamint az XMODEM CRC magas, majd alacsony bájtja (`src/xmodem.cpp:124-125,230-241`). Az SPI `MSBFIRST` beállítása a bájton belüli bitrendet szabályozza, nem páronkénti payloadcserét végez (`src/winbondflash.cpp:367-376`).

A sima `hexdump` több-bájtos formátumegységeket használhat és azokat a rendszer natív endianitásával jeleníti meg; emiatt ugyanaz a két fájlbájt a kijelzésben felcseréltnek tűnhet. A `hexdump -C` ezzel szemben egyedi, két hexszámjegyes bájtokat mutat, így byte-order-független ellenőrzésre alkalmas. Forrás: [hexdump(1)](https://man7.org/linux/man-pages/man1/hexdump.1.html).

Ez alapján a megjelenítési magyarázat valószínű, de a kérdés dinamikus teszt nélkül **nincs lezárva**. A 3/12. lépésben ismert `00 01 02 ... FF` mintával end-to-end fake flash → firmware → XMODEM → Python fájlteszt szükséges; ez a mostani 1/12. kiegészítésben nem futott le.

## 4. Írási és ellenőrzési szabály

- Az első kiadás kizárólag a felismert chip pontos teljes méretével egyező ROM-ot fogadjon el írásra.
- Az erase külön, kifejezett művelet maradjon.
- A verify ne legyen automatikus vagy kötelező.
- Ajánlott ellenőrzés: külön read művelettel készüljön readback dump, majd annak pontos fájlméretét és SHA-256-át kell összehasonlítani az elküldött ROM-éval.
- Nincs bemutatott külön readback dump, ezért **nem igazolt**, hogy a `newbios2.bin` bájtjai ténylegesen visszaolvasva, méret- és SHA-256-egyezéssel ellenőrizve voltak a flashben.

## 5. Python-cél

- Verziókezelés: `pyenv` által rögzített legfrissebb stabil CPython.
- Jelenlegi jelölt: **Python 3.14.7**. A rögzítés napján a hivatalos oldal a 3.14-es ágat aktív bugfix ágként, a 3.14.7-et pedig annak legújabb felsorolt stabil kiadásaként mutatja: [Python letöltések](https://www.python.org/downloads/).
- Környezet: projektbeli `.venv`.
- Első támogatott operációs rendszer: Linux.
- A Windows-t később, külön környezetben kell validálni; a Linuxos működésből nem tekinthető igazoltnak.
- Ebben a lépésben Python-verzió telepítése, frissítése, `.venv` létrehozása, illetve a projekt Python-scriptjeinek futtatása vagy importálása nem történt.

## 6. Eredet és licencállapot

- Firmware upstream: [ma5ter/ArduinoBIOSProgrammer](https://github.com/ma5ter/ArduinoBIOSProgrammer).
- A `xmodembiosread.py` és `xmodembioswrite.py` a felhasználó által AI segítségével készített helyi kiegészítés.
- Az upstream és az örökölt komponensek licencállapota továbbra is tisztázandó.
- Ez a jegyzőkönyv nem hoz létre és nem választ licencet.

## 7. Helyi eszközök

Az alábbiak csak verziólekérdezések eredményei; telepítés vagy frissítés nem történt.

| Eszköz | Helyi állapot |
| --- | --- |
| Git | 2.53.0 |
| `python3` | CPython 3.14.4 |
| `pyenv` | 2.6.4 |
| `pio` | Nem elérhető működő eszközként: az indítófájl `ModuleNotFoundError: No module named 'platformio'` hibával leáll. |
| `platformio` | Nem elérhető működő eszközként: az indítófájl `ModuleNotFoundError: No module named 'platformio'` hibával leáll. |
| `arduino-cli` | Nem elérhető a PATH-on. |
| AVR C++ compiler (`avr-g++`) | GCC 14.3.0 |

## 8. Végső változásellenőrzés

- A `git diff --exit-code` 0 exit kóddal, kimenet nélkül zárult: követett fájl nem módosult.
- A `git diff --cached --exit-code` 0 exit kóddal, kimenet nélkül zárult: stagingelt változás nincs.
- A végső `git status --short` kimenete:

```text
?? AUDIT.md
?? BASELINE.md
?? newbios2.bin
?? output.rom
```

Ez pontosan a feladatban elfogadott négy nem követett fájl; más eltérés nincs.

## 9. Hiányzó bizonyítékok

- A BIOS-chip fizikai felirata vagy fényképe.
- A szintillesztő fényképe és kapcsolása.
- Külön readback dump az új BIOS visszaolvasásáról, ha valaha készült.
- Korábbi soros/XMODEM napló, ha nincs meg.
- A korábban működő firmware-bináris, ha nincs meg.

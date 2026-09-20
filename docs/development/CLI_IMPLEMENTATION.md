# Egységes XMODEM BIOS CLI (10/12)

## Parancsszintaxis

```text
python xmodembios.py read file.rom [serial_port] [baud_rate]
python xmodembios.py erase [serial_port] [baud_rate]
python xmodembios.py write file.rom [serial_port] [baud_rate]
python xmodembios.py verify file.rom [serial_port] [baud_rate]
```

Az alapértelmezett port `/dev/ttyUSB0`, a baud rate 115200. A baud kizárólag
pozitív egész lehet; argparse használati hibánál a szabványos 2-es exit kódot
adja. A portnév változtatás nélkül kerül a pySerialnak átadásra.

Linux példák:

```sh
python xmodembios.py read backup.rom
python xmodembios.py erase /dev/ttyUSB1 115200
python xmodembios.py write firmware.rom /dev/ttyUSB1 115200
python xmodembios.py verify firmware.rom /dev/ttyUSB1 115200
```

Windows példa:

```powershell
python xmodembios.py verify firmware.rom COM3 115200
```

A `xmodembiosread.py` és `xmodembioswrite.py` megmaradt kompatibilitási
belépési pontként, de már ugyanazokat a közös műveleteket használja, mint az
új CLI. Nincs három külön read/write implementáció.

## Exit kódok

| Kód | Jelentés |
| ---: | --- |
| 0 | Siker |
| 1 | Műveleti, fájl-, protokoll- vagy soros hiba |
| 2 | Argparse parancssori használati hiba |
| 3 | A verify eltérést talált |
| 130 | Felhasználói megszakítás (`KeyboardInterrupt`) |

A normál státusz stdout-ra, a hiba stderr-re kerül. Várt felhasználói hibánál
nincs traceback, és bináris payload nem kerül a terminálra.

## Pontos méretkövetelmény

A produkciós `ROM_SIZE_BYTES` értéke pontosan **16 777 216 bájt**. A
következő szabályok érvényesek:

- read csak pontosan ekkora fogadott fájlt véglegesít;
- write a port megnyitása előtt ellenőrzi a létező, normál, olvasható fájlt,
  annak méretét és SHA-256 hashét;
- verify a port megnyitása előtt ugyanígy validálja a referenciafájlt;
- eltérő write/verify inputméretnél soros kapcsolat sem nyílik.

A tesztek dependency injectionnel kis fájlméreteket használnak; ez nem
változtatja meg a produkciós 16 MiB-os alapértéket.

## Közös modulstruktúra

- `xmodembios.py`: argparse, felhasználói kimenet és exit kódok;
- `xmodem_operations.py`: soros session, firmware-státuszkezelés, read,
  erase, write, verify, hash és blokkos összehasonlítás;
- `xmodem_transport.py`: timeout-aware XMODEM `getc`/`putc` callbackek;
- `xmodembiosread.py`, `xmodembioswrite.py`: vékony legacy wrapperek.

Mindegyik modul importálható portnyitás, fájlművelet vagy átvitel nélkül.

## Soros inicializálási sorrend

Minden művelet ugyanazt a sorrendet használja:

1. port megnyitása a megadott névvel és baud értékkel;
2. explicit 8 adatbit, paritás nélkül, 1 stopbit;
3. `xonxoff=False`, `rtscts=False`, `dsrdtr=False`;
4. `STARTUP_RESET_DELAY_SECONDS = 2.0` másodperc várakozás az Arduino
   portnyitás utáni resetére/startupjára;
5. egyszeri `reset_input_buffer()` a reset előtti/startup során keletkezett
   régi bemenet eltávolítására;
6. `b"0"` initialize parancs;
7. a firmware tényleges válaszainak véges várakozása.

Az initialize elküldése után már nincs input-buffer ürítés, ezért érvényes
válasz nem vész el. Felhasználói Enterre vagy más stdin-szinkronizációra az új
CLI és a közös üzleti logika nem vár. A port context managerrel minden
siker-, hiba- és megszakítási útvonalon bezárul.

## Firmware-parancsok és tényleges válaszok

| Művelet | Parancs | A forrásban szereplő sikerjelzés | A forrásban szereplő hibák példái |
| --- | --- | --- | --- |
| Initialize | `b"0"` | `Initializing Chip...`, `CHIP READY`, `Type: ...`, `Flash size: ...` | `INITIALIZE FAILED` |
| Read | `b"r"` | `Read chip`, `Sending file using xmodem, please start receive`; nincs külön read-done sor | `Failed to start xmodem`, `Flash read failed`, `Transfer cancelled`, `Transfer error` |
| Erase | `b"e"` | `Erase all`, majd `Done` | `Erase failed` |
| Write | `b"w"` | `Write chip`, `Ready to receive file using xmodem, please start send`, majd `Write done` | `Flash write failed`, `Out of memory`, `Invalid file size`, `Timeout`, `Cancelled by other party`, `Unknown result` |

A kód nem feltételez nem létező promptot vagy válaszhatárolót. A firmware
`\r\n` sorvégét használja; a host véges `readline()` hívásokkal olvas, szigorú
UTF-8 dekódolással. Hiányos sor, dekódolási hiba, timeout vagy ismert firmware
hibasor műveleti hiba. A dönthető állapotoknál a konkrét szöveges válasz, nem
pusztán eltelt idő határozza meg a sikert.

## Timeoutok

| Cél | Host timeout |
| --- | ---: |
| Startup/reset várakozás | 2 s |
| Parancsbájt írása | 2 s |
| Initialize válasz | 10 s |
| Read/write XMODEM prompt | 5 s |
| Write utáni firmware-eredmény | 10 s |
| Chip erase firmware-eredmény | 300 s |

A 300 másodperces erase timeout nagyobb a firmware flash-driverének 250
másodperces chip-erase timeoutjánál. Minden státuszvárás monoton, közös
határidőt használ; nincs végtelen readline ciklus. Az XMODEM csomag a saját
timeoutját adja át a korábban karakterizált `SerialTransport` callbackeknek.
Teljes műveletre automatikus retry nincs.

## Read fájlkezelés

A read a célfájllal azonos könyvtárban `<cél>.part` fájlba fogad. Sikeres
XMODEM után flush és `fsync` történik, majd ellenőrzésre kerül a pontos
16 777 216 bájtos méret. Csak ezután következik az atomikus `os.replace()`.

XMODEM-hiba, rövid vagy hosszú eredmény, protokollhiba és megszakítás esetén a
`.part` törlődik, a korábbi végleges célfájl változatlan marad. Siker után a
CLI kiírja a kész fájl méretét és SHA-256 hashét. Payload-transzformáció vagy
bájtpárcsere nincs.

## Erase és write szétválasztása

Az erase initialize után kizárólag `b"e"` parancsot küld, majd `Done` vagy
hibaválasz érkezéséig vár. Nem indít XMODEM-et és nem indít write műveletet.

A write a bemeneti fájlt portnyitás előtt validálja, majd initialize után
kizárólag `b"w"` parancsot és a fájl bytes tartalmát küldi. Nem küld `b"e"`
vagy `b"r"` parancsot. Csak igaz `send()` és a firmware `Write done` válasza
együtt jelent sikert. A CLI külön jelzi, hogy erase és verify nem történt
automatikusan, és kiírja az input méretét és SHA-256 hashét.

## Opcionális verify

A verify kizárólag külön parancsra fut. A referenciafájlt portnyitás előtt
validálja, majd initialize után read (`b"r"`) művelettel ideiglenes fájlba
olvassa a chipet. Nem küld erase vagy write parancsot.

Az összehasonlítás 64 KiB-os blokkokban történik; a teljes 16 MiB nincs
memóriában. Mindkét oldal SHA-256 hashét és tényleges méretét kiszámítja.
Egyezésnél exit 0, eltérésnél exit 3 és az első eltérő offset, valamint a
referencia- és readback-bájt jelenik meg. Rövid vagy hosszú readbacknél a
hiányzó oldal `<EOF>` jelölést kap. Az ideiglenes fájl siker, eltérés,
XMODEM-hiba és megszakítás után is törlődik.

## Ellenőrzési eredmények

```text
py_compile: PASS
Python unittest: 56 PASS, 0 hiba
fő help és read/erase/write/verify help: PASS
pip check: PASS (No broken requirements found.)
firmware karakterizálás: 71 PASS, 0 MISSING_SAFEGUARD, 0 váratlan hiba
ASan/UBSan: PASS
PlatformIO Arduino Uno build: PASS
Flash: 8406 / 32256 bájt (26,1%)
RAM: 243 / 2048 bájt (11,9%)
```

A `12 AF 34 56` és a determinisztikus `00 01 ... FF` minta read és write
irányban változatlan maradt. Páronkénti bájtcsere nincs. A firmware natív
tesztfordításában a korábban dokumentált hat signed/unsigned warning maradt;
új regresszió vagy warning nem keletkezett.

Valódi soros port, hardveres read/write/erase/verify és repóbeli ROM-fájl
módosítása nem történt.

## A 11/12 dokumentációs lépésre hagyott feladatok

- a README régi Python 2 útmutatójának és TODO-jának cseréje a tényleges
  Python 3 CLI használatára;
- a pyenv/venv, PlatformIO és tesztparancsok egységes felhasználói leírása;
- a hardveres biztonsági és külön erase/write/verify munkafolyamat kiemelése;
- a jelenlegi implementációs jegyzőkönyvek végleges dokumentációs
  kereszthivatkozásai.

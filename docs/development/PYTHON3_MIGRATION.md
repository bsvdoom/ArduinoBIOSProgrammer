# Python 3 migráció (9/12)

## Kiinduló állapot

A két 41 soros legacy szkript Python 3.14.7 alatt nem volt lefordítható a
Python 2-es `print` utasítások miatt. A következő további problémák voltak
jelen:

- a `/dev/ttyUSB0` port már importáláskor megnyílt;
- a Python 3 alatt szükséges `bytes` helyett `str` parancsok szerepeltek;
- a `getc` és `putc` figyelmen kívül hagyta a kapott timeoutot;
- a port és a fájlok lezárása nem volt garantált;
- a `recv()`/`send()` eredménye nem volt ellenőrizve;
- a reader közvetlenül `wb` módban nyitotta és így előre csonkolta az
  `output.rom` fájlt;
- nem volt meghatározott sikeres vagy hibás exit kód és normál hibánál
  traceback jelenhetett meg;
- a writer csak a firmware `w` parancsa után próbálta megnyitni az inputot.

## Megőrzött legacy viselkedés

A különálló `xmodembiosread.py` és `xmodembioswrite.py` belépési pontok
megmaradtak. A kompatibilitási alapértékek:

| Tulajdonság | Reader | Writer |
| --- | --- | --- |
| Soros port | `/dev/ttyUSB0` | `/dev/ttyUSB0` |
| Baud rate | 115200 | 115200 |
| ROM-fájl | `output.rom` | `input.rom` |
| XMODEM művelet | `recv()` | `send()` |

A firmware előtti interaktív sorrend is megmaradt:

1. egy karakter olvasása stdinről;
2. ha ez pontosan `0`, akkor `b"0"` inicializáló parancs;
3. öt `readline()` státuszsor;
4. két karakter olvasása stdinről, az érték eldobásával;
5. `b"r"` a readerben, illetve `b"w"` a writerben;
6. újabb két karakter olvasása stdinről, az érték eldobásával;
7. két `readline()` státuszsor;
8. XMODEM `recv()` vagy `send()`.

Az eredeti kód nem végzett explicit soros input-buffer ürítést, ezért a
migráció sem vezetett be ilyet. Az eredeti, félrevezető „Press d” prompt és a
valójában `0` értéket vizsgáló feltétel kompatibilitási okból ebben a lépésben
szintén megmaradt. A parancsok most Python 3-kompatibilis bytes értékek.

## Modul- és függvénystruktúra

- `xmodem_transport.py`: közös, importálható `SerialTransport`, benne a
  timeout-aware `getc` és `putc` callbackek;
- `xmodembiosread.py`: reader konstansok, session-segédfüggvények és `main()`;
- `xmodembioswrite.py`: writer konstansok, session-segédfüggvények és `main()`;
- mindkét scriptet `if __name__ == "__main__"` őr indítja;
- a serial- és XMODEM factory, valamint az I/O streamek tesztből injektálhatók.

Importálás önmagában nem nyit soros portot, nem nyit ROM-fájlt és nem indít
átvitelt. Nem készült közös argparse CLI vagy `xmodembios.py`.

## Callback timeout-szemantika

### `getc(size, timeout)`

- kizárólag bytes-szerű pySerial read eredményt fogad;
- egy `time.monotonic()` alapú közös határidőt használ;
- minden rész-read előtt a még hátralévő időt állítja `Serial.timeout` értéknek;
- rövid read után a fennmaradó méretet tovább gyűjti, amíg van idő;
- teljes méretnél bytes eredményt ad;
- határidőnél a már megérkezett részleges bytes eredményt nem dobja el;
- teljesen üres timeoutnál `None` értéket ad az xmodem szerződése szerint;
- legfeljebb a kért méretet kéri és adja vissza;
- `finally` ágban visszaállítja az eredeti `Serial.timeout` értéket;
- a soros olvasási kivételt továbbengedi, nem alakítja sikerré.

A pySerial rövid read tehát nem automatikus hiba: a callback a közös
határidőig megpróbálja kiegészíteni, majd szükség esetén részleges adatot ad
vissza. Az xmodem csomag dönti el, hogy az adott protokollhelyen ez elegendő-e.

### `putc(data, timeout)`

- csak `bytes`, `bytearray` vagy `memoryview` adatot fogad;
- egy `time.monotonic()` alapú közös határidőt használ;
- minden rész-write előtt a fennmaradó időt állítja `Serial.write_timeout`
  értéknek;
- részleges írás után csak a hátralévő bájtokat küldi újra;
- teljes siker esetén a teljes elküldött bájtszámot adja vissza;
- nulla előrehaladás, lejárt határidő vagy `SerialTimeoutException` esetén
  `None` értéket ad;
- más soros kivételt nem nyel el;
- `finally` ágban visszaállítja az eredeti `Serial.write_timeout` értéket.

Egy callback sem állítja sikeresnek a részlegesen teljesült, de timeouttal
lezárult írást.

## Átviteli eredmény és exit kód

A reader a `recv()` visszatérését az xmodem 0.5.0 szerződése szerint kezeli:
`None` hiba, bármely egész eredmény — a nulla is — siker. A writer kizárólag
igaz `send()` bool eredményt fogad el. Hibás transfer után mindkét script rövid
stderr üzenetet ír, traceback nélkül.

| Eredmény | Exit kód |
| --- | ---: |
| Igazolt `recv`/`send` siker | 0 |
| XMODEM-, fájl- vagy soros hiba | 1 |
| `KeyboardInterrupt` | 130 |

Kezelt felhasználói hibák: `OSError`, `SerialException`,
`SerialTimeoutException` és `KeyboardInterrupt`. A soros port minden megnyitás
után `finally` ágban záródik, a ROM-streamek context managerben élnek.

## Fájlbiztonság

A reader ugyanabban a könyvtárban a `<célfájl>.part` fájlba fogad. Sikeres,
nem `None` XMODEM-eredmény után flush és `fsync` történik, a stream bezáródik,
majd az `os.replace()` atomikusan a végleges névre cseréli. Hiba vagy
megszakítás esetén a `.part` törlődik, a korábbi végleges fájl változatlan
marad.

A writer a soros port megnyitása és a firmware `w` parancsa előtt ellenőrzi,
hogy az input létező normál fájl-e. Bináris `rb` streamet ad közvetlenül az
XMODEM-nek. A pontos 16 MiB-os méretellenőrzés szándékosan nincs még
hardcode-olva.

Automatikus erase, verify és readback nem került be.

## Teszteredmények

A `tests/python/` standard-library `unittest` csomag fake seriallal és fake
XMODEM factoryval fut, valódi port nélkül:

```text
Ran 31 tests
OK
```

Lefedett esetek többek között:

- mindkét modul mellékhatásmentes importja;
- teljes, részleges és timeoutos `getc`;
- teljes, többrészes és timeoutos `putc`;
- `SerialTimeoutException` és timeoutérték-visszaállítás;
- reader siker, XMODEM-hiba, megszakítás, atomikus csere és cleanup;
- writer siker, XMODEM-hiba, hiányzó input és portnyitási hiba;
- parancsbájtok, sorrend, erőforrás-zárás és exit kódok.

Bájtsorrend: a `12 AF 34 56` és a determinisztikus `00 01 ... FF` minta read
és write irányban is byte-pontosan változatlan maradt. A külön páronkénti
csereellenőrzés igazolta, hogy nem keletkezett `AF 12 56 34` sorrend.

További ellenőrzések:

```text
py_compile: PASS
pip check: PASS (No broken requirements found.)
firmware karakterizálás: 71 PASS, 0 MISSING_SAFEGUARD, 0 váratlan hiba
ASan/UBSan: PASS
PlatformIO Arduino Uno build: PASS
Flash: 8406 / 32256 bájt (26,1%)
RAM: 243 / 2048 bájt (11,9%)
```

A firmware-tesztekben továbbra is a korábban dokumentált hat signed/unsigned
warning jelent meg; új warning vagy regresszió nem keletkezett.

## A 10/12. lépésre hagyott feladatok

- egységes `xmodembios.py` argparse CLI és súgó;
- read/write művelet, fájl, port és baud parancssori megadása;
- a félrevezető legacy prompt és kézi stdin-szinkronizáció kiváltása;
- pontos, chipkapacitáshoz kötött 16 MiB-os write méretellenőrzés;
- overwrite-szabály és felhasználói megerősítési szerződés;
- a két legacy belépési pont hosszabb távú kompatibilitási szerepe.

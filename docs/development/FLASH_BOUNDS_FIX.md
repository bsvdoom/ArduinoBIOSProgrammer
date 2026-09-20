# Flash cím-, hossz- és igazításellenőrzés — 6B2/12

- Dátum: 2026-09-20; időzóna: Europe/Budapest.
- Célkörnyezet: Arduino Uno; hardver nélküli natív tesztelés.
- Tesztelt logikai kapacitás: 16 MiB / 128 Mbit, `0x000000..0xFFFFFF`.
- Az `EF 40 18` fixture 16 MiB-os kapacitáskezelést tesztel; a fizikai chip pontos modellje és felirata továbbra sincs igazolva.

## Kapacitás és elfogadott tartományok

A meghajtó `partDescription.bytes` mezője és a `bytes()` API bájtban tartja/adja vissza a kapacitást. A 6B2 minden cím-, hossz- és igazításellenőrzése ugyanezt a bájtalapú mértékegységet használja.

Az általános fél-nyílt tartomány `[address, address + length)` akkor érvényes, ha:

1. `address <= capacity`;
2. `length <= capacity - address`.

Ennek következményei:

- nem nulla hossz esetén a kezdőcímnek a kapacitás alatt kell lennie;
- a pontosan `capacity` értéken végződő tartomány érvényes;
- `address == capacity && length == 0` érvényes no-op;
- `address > capacity` nulla hosszal is érvénytelen;
- érvénytelen tartománynál sem BUSY-lekérdezés, sem más SPI-tranzakció nem indul.

## Túlcsordulásbiztos ellenőrzés

A kód nem számolja ki az esetleg túlcsorduló `address + length` összeget. Először elutasítja az `address > capacity` esetet, és csak ezután hasonlítja a hosszt a biztonságosan képezhető `capacity - address` értékhez:

```cpp
if (address > capacity)
    return false;
return length <= capacity - address;
```

Natív teszt fedi a kapacitással egyenlő és annál nagyobb kezdőcímet, a kapacitás végén átnyúló hosszt, valamint a `UINT32_MAX - 10` és `UINT16_MAX` párt. Mindegyik hibás, nem nulla hosszú eset SPI-forgalom nélkül tér vissza.

## Page Program

A `writePage()` elfogadási feltételei:

- `address % 256 == 0`;
- a teljes 256 bájtos lap érvényes tartomány a közös ellenőrzés szerint.

Az első (`0x000000`), középső (`0x123400`) és utolsó (`0xFFFF00`) lap elfogadott. Igazítatlan, `0x01000000` vagy extrém nagy cím `false` eredményt ad, WREN (`06`) és Page Program (`02`) nélkül. Érvényes címen a parancs, MSB→LSB cím, WEL-ellenőrzés, BUSY-polling és 5 ms timeout változatlan.

## Erase-igazítások

| API | Egység és igazítás | Utolsó érvényes kezdőcím |
| --- | ---: | ---: |
| `eraseSector()` | 4096 bájt / 4 KiB | `0xFFF000` |
| `erase32kBlock()` | 32768 bájt / 32 KiB | `0xFF8000` |
| `erase64kBlock()` | 65536 bájt / 64 KiB | `0xFF0000` |

Mindháromnál az egység teljes tartományának kapacitáson belül kell maradnia. Az első és utolsó egység elfogadott; az igazítatlan, kapacitással egyenlő és extrém nagy cím elutasított. Hibás címnél WREN és `20`/`52`/`D8` parancs sem kerül a buszra. A 6B1 timeoutértékei nem változtak.

## Read API és Programmer-hibaterjesztés

A `read(uint32_t, uint8_t *, uint16_t)` visszatérési típusa `uint16_t` helyett `bool`. `true` jelenti a teljes kért olvasás sikerét vagy az érvényes nulla hosszúságú no-opot; `false` jelenti az érvénytelen tartományt vagy a BUSY miatti elutasítást.

A produkciós hívási helyek keresése egyetlen hívót talált: a `Programmer` read/XMODEM ága. Ez minden blokk előtt ellenőrzi az eredményt. `false` esetén:

- az aktuális művelet leáll;
- `Flash read failed` üzenet jelenik meg;
- a korábbi vagy érvénytelen lapbuffer nem kerül az XMODEM-kimenetre;
- az útvonal nem fut tovább sikeres befejezésre.

A teszt a tényleges `winbondflash.cpp`, `programmer.cpp` és `xmodem.cpp` forrást fordítja, BUSY olvasási hibát injektál, és igazolja, hogy nem indul `03` Read Data tranzakció és nem kerül adat a soros kimenetre.

## Chip-select inicializálási sorrend

A `winbondFlashSPI::begin()` sorrendje:

1. eltárolja a kapott SPI objektumot és a tényleges `nss` lábat;
2. a MISO lábat `INPUT_PULLUP` módra állítja;
3. elindítja és változatlanul beállítja az SPI-t (`MSBFIRST`, `SPI_CLOCK_DIV2`, `SPI_MODE0`);
4. `pinMode(nss, OUTPUT)`;
5. `digitalWrite(nss, HIGH)` az inaktív CS-állapothoz;
6. csak ezután indul az első `AB` SPI-parancs.

A fake Arduino közös eseménynaplója D7 egyedi CS-nél és az alapértelmezett D10-nél is ellenőrzi, hogy az OUTPUT és HIGH megelőzi az első transfert. A kód nem hardcode-olja a 10-es lábat.

## Teszteredmények

### Változtatás előtt

| Csomag | Eredmény |
| --- | --- |
| korábbi kommunikációs tesztek | 26 PASS |
| flash-karakterizálás | 32 PASS, 6 MISSING SAFEGUARD |
| teljes natív futás | 58 PASS, 6 MISSING SAFEGUARD, 0 váratlan hiba |

### Változtatás után

| Csomag | Eredmény |
| --- | --- |
| izolált XMODEM | 12 PASS |
| Programmer írás/finalizálás | 14 PASS |
| flash-karakterizálás | 45 PASS, 0 MISSING SAFEGUARD |
| teljes natív futás | 71 PASS, 0 váratlan hiba |
| ASan / UBSan | nincs jelentett hiba |

Mind a korábbi 58 PASS megmaradt, a hat futó MISSING SAFEGUARD PASS-ra váltott, és hét új határesettel bővült a csomag.

A host fordítás továbbra is ugyanazt a hat, már meglévő signed/unsigned összehasonlítási figyelmeztetést adja a `partDescription` táblát bejáró ciklusokra. A 6B2 ezeket a kérés szerint nem módosította; az új kód nem okozott új figyelmeztetést.

## AVR ellenőrzés

A korábban használt AVR GCC 7.3.0, Arduino AVR core 1.8.7, Uno/ATmega328P definíciók és `-Os` melletti `-fsyntax-only` ellenőrzés a `src/programmer.cpp`, `src/winbondflash.cpp` és `src/xmodem.cpp` egységekre PASS. Ez nem teljes link/build és nem hardveres próba.

## Nyitva hagyott kérdések

- A protection bitek és egyedi lockok értelmezése/módosítása nincs implementálva.
- Nincs programozás utáni automatikus readback verify.
- Nincs törlés utáni blank-check.
- A BUSY=0 önmagában nem bizonyítja a programozott/törölt tartalom helyességét.
- Nincs automatikus retry.
- Nincs `SPI.beginTransaction()`/`endTransaction()` keretezés.
- A fizikai chip pontos típusa, valós kapacitása, időzítése és elektromos működése hardver nélkül nem igazolható.

A 7/12. lépés ebben a munkamenetben nem kezdődött el.

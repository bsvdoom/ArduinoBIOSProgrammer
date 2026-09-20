# Flash BUSY-timeout, WEL-ellenőrzés és hibaterjesztés — 6B1/12

- Dátum: 2026-09-20; időzóna: Europe/Budapest.
- Célkörnyezet: Arduino Uno; hardver nélküli natív tesztelés.
- Tesztelt logikai kapacitás: 16 MiB / 128 Mbit.
- Az `EF 40 18` fixture és a W25Q128FV adatlap használata kompatibilitási referencia. A fizikai chip pontos modellje és felirata továbbra sincs igazolva.

## Eredeti problémák

A módosító flash API-k `void` eredményűek voltak. A `Programmer` külön adta ki a Write Enable parancsot, de nem olvasta vissza a WEL bitet, majd korlátlan ciklusban várta a BUSY törlését. Emiatt egy beragadt BUSY végtelenül blokkolhatta az alkalmazást, a WEL=0 ellenére elindult a program/erase parancs, és a meghajtó hibáját a `Programmer` nem tudta megkülönböztetni a sikertől.

A 6A futás ezt 27 flash-esetből 18 PASS és 9 MISSING SAFEGUARD eredménnyel rögzítette. A három közvetlenül érintett hiány a Page Program WEL-előfeltétele, a Chip Erase WEL-előfeltétele és a végtelen BUSY-várakozás volt.

## Timeoutértékek és adatlap-alap

Az értékek a [Winbond W25Q128FV adatlap](https://www.pjrc.com/teensy/W25Q128FV.pdf) AC Characteristics táblájának program/erase maximális idejéből indulnak ki. A [közvetlen időzítési táblázat](https://www.pjrc.com/teensy/W25Q128FV.pdf#page=74) a Page Program, Sector Erase, 32/64 KiB Block Erase és Chip Erase maximumait adja meg. Ez referencia egy kompatibilis parancskészlethez, nem a beépített fizikai típus bizonyítása.

| Művelet | Adatlapi maximum | Alkalmazott timeout | Biztonsági ráhagyás |
| --- | ---: | ---: | ---: |
| Page Program (`02`) | 3 ms | 5 ms | +2 ms, kb. 67% |
| Sector Erase (`20`) | 400 ms | 500 ms | +100 ms, 25% |
| 32 KiB Block Erase (`52`) | 1600 ms | 2000 ms | +400 ms, 25% |
| 64 KiB Block Erase (`D8`) | 2000 ms | 2500 ms | +500 ms, 25% |
| Chip Erase (`C7`) | 200 s | 250000 ms | +50 s, 25% |

A jelenlegi publikus meghajtó más olyan parancsot nem ad ki, amely új program/erase BUSY ciklust indítana. A `75` suspend és `7A` resume egy már futó művelet állapotát kezeli; ezekhez ebben a lépésben nem került új várakozás. A forrásban definiált, de nem használt Status Register Write (`01`) szintén nem kapott timeoutot.

A `waitUntilReady(timeout_ms)` a következő, 32 bites átfordulásbiztos feltételt használja:

```cpp
(uint32_t)(millis() - start) >= timeout_ms
```

Az előjel nélküli kivonás miatt a mérés akkor is helyes, ha a `millis()` a várakozás közben `UINT32_MAX` értékről nullára fordul. A függvény BUSY időbeni törlésénél `true`, a határ elérésekor `false`; a timeoutot nem tekinti sikernek.

## WEL-ellenőrzés

`setWriteEnable(true)` külön `/CS` tranzakcióban elküldi a `06` Write Enable parancsot, majd külön `05 FF` tranzakcióval visszaolvassa az SR1 regisztert. Csak a WEL bit (SR1 bit 1) beállása ad `true` eredményt.

Minden támogatott módosító API saját maga hívja ezt az ellenőrzést. WEL=0 esetén az API azonnal `false` eredményt ad, nincs automatikus védelemfeloldás vagy újrapróbálkozás, és az alábbi parancsok nem kerülnek kiküldésre:

- Page Program `02`;
- Sector Erase `20`;
- 32 KiB Block Erase `52`;
- 64 KiB Block Erase `D8`;
- Chip Erase `C7`.

`setWriteEnable(false)` továbbra is `04` Write Disable parancsot küld. Ezen az ágon nincs WEL=0 visszaellenőrzés; a visszatérési érték a parancs kiküldését jelzi.

## API-k és hívási helyek

Az alábbi visszatérési típusok `void` helyett `bool` értékűek:

- `setWriteEnable(bool)` és az inline `WE(bool)`;
- `writePage(uint32_t, uint8_t *)`;
- `eraseSector(uint32_t)`;
- `erase32kBlock(uint32_t)`;
- `erase64kBlock(uint32_t)`;
- `eraseAll()`.

Új publikus segéd a `bool waitUntilReady(uint32_t timeout_ms)`. A program/erase metódusok `false` értéket adnak WEL=0 vagy BUSY-timeout esetén; `true` azt jelenti, hogy WEL beállt és a BUSY a határidőn belül törlődött. Ez nem tartalmi readback-verify és nem blank-check.

A produkciós hívási helyek teljes keresése két érintett `Programmer`-használatot talált:

- az `e`/`E` ág ellenőrzi az `eraseAll()` eredményét;
- a `w`/`W` ág minden teljes 256 bájtos lapnál ellenőrzi a `writePage()` eredményét, hibánál kilép a ciklusból és nem folytatja az XMODEM sikeres lezárását.

A `Programmer` már nem ad ki külön, ellenőrizetlen WREN parancsot, és nem tart fenn saját korlátlan BUSY-ciklust. A Sector/Block Erase API-knak jelenleg nincs más produkciós hívója; a natív tesztek minden visszatérési értéküket ellenőrzik.

## Felhasználói hibaüzenetek

- Sikertelen teljes törlés: `Erase failed`; a `Done` üzenet elmarad.
- Sikertelen lapírás: `Flash write failed`; a `Write done` üzenet elmarad, és további lap nem íródik.

Az üzenetek szándékosan rövidek. Nem állítják, hogy WEL, timeout, protection vagy fizikai tartalmi hiba volt a pontos ok, mert a jelenlegi `bool` szerződés ezeket nem különbözteti meg.

## Teszteredmények

### Változtatás előtt

| Csomag | Eredmény |
| --- | --- |
| korábbi kommunikációs tesztek | 26 PASS, 0 KNOWN FAILURE, 0 váratlan hiba |
| flash-karakterizálás | 18 PASS, 9 MISSING SAFEGUARD, 0 váratlan hiba |
| AVR szintaxisellenőrzés | PASS |

### Változtatás után

| Csomag | Eredmény |
| --- | --- |
| izolált XMODEM | 12 PASS |
| Programmer írás/finalizálás | 14 PASS |
| korábbi kommunikációs összesen | 26 PASS, 0 KNOWN FAILURE, 0 váratlan hiba |
| flash-karakterizálás | 32 PASS, 6 MISSING SAFEGUARD, 0 váratlan hiba |
| teljes natív futás | 58 PASS, 6 várt MISSING SAFEGUARD, 0 váratlan hiba |
| ASan / UBSan | nincs jelentett hiba |

A determinisztikus fake idő teszteli az azonnal törlődő, néhány poll után törlődő, közvetlenül határidő előtt törlődő és soha nem törlődő BUSY esetet, továbbá a `millis()` átfordulását. Külön eset igazolja mind az öt timeoutértéket, a WEL=0 miatti parancselmaradást, a művelet utáni timeouthibát, valamint azt, hogy a tényleges `Programmer` sikertelen write/erase után nem ír sikerüzenetet. Egyetlen teszt sem vár valós másodperceket vagy perceket.

## AVR ellenőrzés

A korábban használt AVR GCC 7.3.0, Arduino AVR core 1.8.7, Uno/ATmega328P definíciók és `-Os` melletti `-fsyntax-only` ellenőrzés a `src/programmer.cpp`, `src/winbondflash.cpp` és `src/xmodem.cpp` egységekre PASS. Ez szintaxisellenőrzés, nem teljes AVR link/build és nem hardveres próba.

## 6B2-re és későbbre hagyott problémák

- A 24 bites címtartomány, olvasási hossz, 256 bájtos lapigazítás, valamint a sector/32 KiB/64 KiB erase igazítás és tartomány nincs ellenőrizve. Ezek a kért 6B2 hatókörben maradnak.
- A protection biteket a kód nem értelmezi és nem módosítja. Protection okozta, rövid időn belül BUSY=0 állapotba jutó elutasítást a `bool` eredmény önmagában nem különböztet meg a sikertől.
- Nincs programozás utáni automatikus readback verify és törlés utáni blank-check.
- Nincs automatikus retry.
- Az egyedi chip-select láb explicit OUTPUT konfigurációja továbbra is hiányzik.
- Az olvasási rövid/0 eredmény Programmer-oldali kezelése továbbra is hiányzik.
- Nincs `SPI.beginTransaction()`/`endTransaction()` keretezés.
- Az ismeretlen JEDEC ID elutasított, de a felhasználói üzenet nem tartalmazza a nyers azonosítót.

Ezek közül a 6B2 cím- és laphatár-javítása ebben a lépésben szándékosan nem kezdődött el.

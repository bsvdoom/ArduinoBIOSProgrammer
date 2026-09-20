# Arduino BIOS Programmer — forrás- és licencaudit

- Aktuális lépés: 2/12.
- Vizsgálat dátuma: 2026-09-06; időzóna: Europe/Budapest.
- Vizsgált HEAD: `9446c9fe1318a965eb482f32e1d46f20801f5c9c`, branch: `master`.
- Cél: a firmware, az Arduino-library csomagolás és a Python hostszkriptek eredetének, valamint licencbizonyítékainak technikai feltérképezése nyilvános publikálás előtt.
- Ez technikai licencvizsgálat, **nem jogi tanács**. A szerzői jogi jogosultság végleges megítéléséhez szükség esetén jogi szakember bevonása indokolt.
- Ebben a lépésben programkód, `AUDIT.md`, `BASELINE.md`, README, konfiguráció és Git metadata nem módosult; LICENSE nem készült. A két ROM-fájl nem volt a vizsgálat tárgya, nem lett beolvasva vagy stagingelve.

## 1. Módszer és besorolások

Helyben lefutott a kért `git remote -v`, a teljes dekorált egysoros log, fájlonkénti `git log --follow`, valamint a releváns commitok `git show`/tree/diff összevetése. A távoli branchek és tagek ellenőrzése olvasási `git ls-remote --heads --tags` lekérdezéssel történt. Az upstream webes állításokhoz közvetlen elsődleges vagy az eredeti projekt által hivatkozott forráslink tartozik.

Az eredetoszlop kizárólag az alábbi értékeket használja:

- **verified origin**: a jelenlegi fájl közvetlen, tartalmi Git-útja igazolt;
- **probable origin**: erős attribúció vagy tartalmi egyezés van, de a teljes átadási lánc nem igazolt;
- **locally created/modified**: a helyi commitláncban létrehozott vagy érdemben módosított fájl;
- **unknown origin**: a mélyebb forrás/szerzőség a rendelkezésre álló bizonyítékokból nem azonosítható.

A licencoszlop külön kezeli az **explicit license**, **inherited/verified license**, **no license found** és **unknown** állapotokat. A GitHub nyilvános elérhetősége nem terjesztési licenc: a [GitHub licencelési dokumentációja](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository) szerint licenc hiányában az alapértelmezett szerzői jogi szabályok maradnak irányadók.

## 2. Helyi Git-előzmény

### 2.1. Remote és commitlánc

A `git remote -v` eredménye:

```text
origin  https://github.com/bsvdoom/ArduinoBIOSProgrammer.git (fetch)
origin  https://github.com/bsvdoom/ArduinoBIOSProgrammer.git (push)
```

A `git log --all --decorate --oneline` eredménye:

```text
9446c9f (HEAD -> master, origin/master, origin/HEAD) Python scripts to dump and write bios roms
f614f05 Update README.md
3bf39b9 major refinements
ee71760 md formatted
fb215a1 Create README.md
c627e9c Initial commit
```

Az első öt helyi commit pontos láncot alkot a megadott [ma5ter/ArduinoBIOSProgrammer upstream](https://github.com/ma5ter/ArduinoBIOSProgrammer) jelenlegi `master` csúcsáig (`f614f059e669c88b6f84f6c31b2e02b11c77842a`). A távoli `ls-remote` ugyanezt a teljes hash-t adta a `refs/heads/master` refre.

| Commit | Helyi bizonyíték és jelentőség |
| --- | --- |
| [`c627e9c`](https://github.com/ma5ter/ArduinoBIOSProgrammer/commit/c627e9ca2976a437b051a614b3f2194c40215a7d) | 2017-12-20, `ma5ter`, „Initial commit”: sketch, Winbond- és XMODEM-fájlok létrehozása. |
| [`fb215a1`](https://github.com/ma5ter/ArduinoBIOSProgrammer/commit/fb215a1ceb0fb2a654b6313cf10a0c82fe83b6a3) | 2017-12-21, README létrehozása. |
| [`ee71760`](https://github.com/ma5ter/ArduinoBIOSProgrammer/commit/ee71760fcfa310471886646dab7fb77ade5a01a0) | 2017-12-21, README formázása. |
| [`3bf39b9`](https://github.com/ma5ter/ArduinoBIOSProgrammer/commit/3bf39b94c8c6be91b6b48c2812c7f72435fe33f7) | 2017-12-21: a programmer logika külön `.cpp/.h` fájlokba szervezése, sketch-átalakítás, öt sor hozzáadása az XMODEM küldési cancel-kezeléséhez. |
| [`f614f05`](https://github.com/ma5ter/ArduinoBIOSProgrammer/commit/f614f059e669c88b6f84f6c31b2e02b11c77842a) | 2020-04-30, upstream README-módosítás; ez az ellenőrzött upstream `master` csúcsa. |
| `9446c9f` | 2023-03-26, helyi `adam` commit: Python-scriptek, requirements és `library.properties`; README-bővítés; a hét firmware/sketch fájl könyvtárba rendezése. |

A `9446c9f` commit rename-felismeréssel mind a hét firmware/sketch fájlt **R100**, azaz 100%-os tartalmi átnevezésként mutatja. Ez a commit nem változtatta meg a firmware tartalmát. A Git author mező commitmetadata; önmagában nem igazolja a teljes szerzői jogi jogosultságot.

### 2.2. Fájlonkénti `--follow` összefoglaló

- A `winbondflash.cpp/.h` fájlok a `c627e9c` commitban jelentek meg, és tartalmuk a jelenlegi `src/` változatig változatlan maradt.
- Az `xmodem.cpp/.h` a `c627e9c` commitban jelent meg; csak az `xmodem.cpp` kapott öt új cancel-kezelő sort a `3bf39b9` commitban.
- A Programmer osztály fájljai a `3bf39b9` commitban jöttek létre a korábbi monolitikus sketch logikájának átszervezésekor.
- A sketch a `c627e9c` óta követhető; a `3bf39b9` után vékony belépési pont lett.
- A README az upstream `fb215a1` commitban jött létre, majd upstream módosítások és a helyi `9446c9f` kiegészítései követték.
- A két Python-script és a `library.properties` előzménye egyetlen commit: `9446c9f`; korábbi helyi változatuk nincs.

## 3. Upstream repository és licenc

Az [upstream repository gyökérlistája](https://github.com/ma5ter/ArduinoBIOSProgrammer) kilenc fájlt és öt commitot mutat a `master` ágon. A helyben azonosított upstream tree teljes fájllistájában nincs `LICENSE`, `LICENCE`, `COPYING` vagy hasonló nevű fájl. A teljes upstream commitláncon végzett licenc-/copyright-keresés sem talált licencszöveget vagy copyright-nyilatkozatot.

Az olvasási `git ls-remote --heads --tags` eredménye egyetlen ref volt:

```text
f614f059e669c88b6f84f6c31b2e02b11c77842a  refs/heads/master
```

Más branch és tag nem jelent meg. A repository webes [Branches](https://github.com/ma5ter/ArduinoBIOSProgrammer/branches/all) és [Tags](https://github.com/ma5ter/ArduinoBIOSProgrammer/tags) oldalára mutató linkek itt közvetlen ellenőrzési pontok; az audit tényleges eredménye a fenti Git-ref lekérdezésből származik.

Fájlfejlécek:

- Az upstream/current `BIOSprog.ino`, `programmer.cpp/.h` és `xmodem.cpp/.h` nem tartalmaz szerzői vagy licencfejlécet.
- A [Winbond implementáció](https://github.com/ma5ter/ArduinoBIOSProgrammer/blob/master/winbondflash.cpp) WarMonkey nevet, e-mailt, `bbs.kechuang.org` hostot és a korábbi Google Code projektet jelöli meg (`src/winbondflash.cpp:3-8`; ugyanaz a fejléc a `.h` fájlban). Ez attribúció, **nem licencengedély**.
- Az upstream README nem nevez meg licencet. Egyetlen commitüzenet sem közöl licencet vagy olyan eredetengedélyt, amely a forrás terjesztését tisztázná.

Következtetés: az upstream egészére **no license found**. Nyilvános olvashatóságból vagy forkolhatóságból nem következik szabad módosítási/terjesztési jog; ezt a [GitHub „No License” összefoglalója](https://choosealicense.com/no-permission/) is külön rögzíti.

## 4. Fájlonkénti eredet- és licenctábla

| Jelenlegi fájl | Eredetbesorolás | Eredeti út / helyi változás | Licencállapot | Rövid indok |
| --- | --- | --- | --- | --- |
| `examples/BIOSprog/BIOSprog.ino` | **verified origin** | Upstream `BIOSprog.ino`; 2023-ban R100 áthelyezés. | **no license found** | Folytonos `--follow` történet `c627e9c` óta; upstream licenc nincs. |
| `src/programmer.cpp` | **verified origin** | Upstream `programmer.cpp`; `3bf39b9` hozta létre, majd R100. | **no license found** | Az upstream sketch átszervezésének része, licencfejléc nélkül. |
| `src/programmer.h` | **verified origin** | Upstream `programmer.h`; `3bf39b9`, majd R100. | **no license found** | Közvetlen Git-út igazolt, licencfejléc nincs. |
| `src/winbondflash.cpp` | **probable origin** | WarMonkey / `code.google.com/p/winbondflash`; változatlanul került az upstream kezdeti commitjába, majd R100. | **unknown** | Az attribúció erős, de az eredeti licenc nem volt visszanyerhető. |
| `src/winbondflash.h` | **probable origin** | Ugyanaz a WarMonkey/Google Code eredetnyom; változatlan Git-tartalom, majd R100. | **unknown** | A fejléc szerzőt és projektet nevez, licencet nem. |
| `src/xmodem.cpp` | **unknown origin** | Azonnali forrásként upstream `c627e9c` igazolt; `3bf39b9` öt helyi upstream sort adott hozzá, majd R100. | **no license found** | Nincs szerző/licencfejléc, a mélyebb forrás nem azonosítható. |
| `src/xmodem.h` | **unknown origin** | Azonnali forrásként upstream `c627e9c` igazolt; tartalmilag változatlan, majd R100. | **no license found** | Nincs szerző/licencfejléc vagy igazolt korábbi projekt. |
| `xmodembiosread.py` | **locally created/modified** | Helyben jött létre `9446c9f` alatt; felhasználói adat szerint AI-segítséggel. Az `xmodem` példájából felismerhető vázat tartalmaz. | **no license found** | Saját fejléc/licenc nincs; a valószínűleg átvett példarészek MIT-forrása külön kezelendő. |
| `xmodembioswrite.py` | **locally created/modified** | Ugyanaz a helyi commit és AI-segítség; a read script közeli párja és `xmodem` példaváz. | **no license found** | Saját fejléc/licenc nincs; az MIT-példa nem licenceli automatikusan a teljes helyi fájlt. |
| `library.properties` | **locally created/modified** | Helyben jött létre `9446c9f` alatt; leíró bekezdése valószínűleg ArduinoWebsockets metadata. | **unknown** | Nincs helyi licenc; a más projektből származó metadata jogi jelentősége és az átvétel terjedelme tisztázandó. |
| `README.md` | **locally created/modified** | Upstream eredet igazolt; 2023-ban helyi szintillesztő- és Python-rész került hozzá. | **no license found** | Az upstream és a helyi kiegészítések sincsenek licencnyilatkozattal lefedve. |

Megjegyzés: a **verified origin** nem azonos a **verified license** állapottal. Az upstreamhez való pontos tartalmi visszavezetés csak azt igazolja, honnan került ide a fájl, nem azt, hogy nyilvánosan továbbterjeszthető.

## 5. Winbond-réteg eredete

A két fájl fejlécének közvetlen állításai (`src/winbondflash.cpp:3-8`, `src/winbondflash.h:1-6`): WarMonkey, `luoshumymail@gmail.com`, `bbs.kechuang.org`, valamint `code.google.com/p/winbondflash`. A helyi Git szerint a fájlok a ma5ter kezdeti commitjába már készen kerültek, és azóta tartalmilag nem változtak.

Az [Arduino Forum 2013-as eredeti bejegyzése](https://forum.arduino.cc/t/winbond-spi-flash-library-for-avr-arduino-written-in-c/174269) egy Winbond SPI flash library fejlesztéséről beszél, ugyanazokat a W25Q családokat sorolja fel, ugyanarra a `bbs.kechuang.org`/Google Code projektre mutat, és `winbondflash.tar.gz` mellékletet kínál. Ez erős, időben korábbi eredetbizonyíték, de a fórumoldal nem közöl licencet.

A [Google Code Archive](https://code.google.com/archive/p/winbondflash/) projektoldala és a hivatkozott `bbs.kechuang.org` gyökér az ellenőrzéskor nem adott kiértékelhető projekt-/licenctartalmat. Kereséssel más későbbi másolatok előkerültek, de ezek nem megbízható bizonyítékai az eredeti kiadás licencének. Ezért az eredet **probable origin**, a licenc pedig **unknown**; más repók utólagos licence nem ruházható át erre a kódra.

## 6. Firmware XMODEM eredete

Az XMODEM fájlok a ma5ter `c627e9c` kezdeti commitjában jelentek meg. A `3bf39b9` csak öt sort adott az `xmodem.cpp` küldési cancel-kezeléséhez; a header változatlan. Nincs szerző-, copyright- vagy licencfejléc (`src/xmodem.cpp:1-13`; `src/xmodem.h:1-24`).

Distinktív függvénynevekre és kódrészletekre végzett célzott keresés nem azonosított megbízható, a ma5ter commitot megelőző implementációt. A keresési eredmény hiánya nem szerzőségi bizonyíték. Emiatt a közvetlen repository-forrás igazolt, de a mélyebb kóderedet **unknown origin**, a licenc **no license found**.

Ez a firmware C++ XMODEM implementáció nem azonos a hostoldalon használt `tehmaze/xmodem` Python-csomaggal; az utóbbi MIT licence nem terjed át rá.

## 7. Python-scriptek és az `xmodem` csomag

A helyi Git szerint mindkét script teljes egészében a `9446c9f` commitban jött létre. Korábbi helyi fájl vagy upstream ma5ter-változat nincs. A felhasználói eredetközlés szerint AI segítségével készített helyi kiegészítések.

Az [`xmodem` 0.4.6 README példája](https://github.com/tehmaze/xmodem/blob/0.4.6/README.rst) ugyanazt az import/serial callback/XMODEM/`send`/`recv` mintát tartalmazza, beleértve a timeout figyelmen kívül hagyásáról szóló megjegyzést is. A helyi scriptek releváns részei: `xmodembiosread.py:1-16,38-41` és `xmodembioswrite.py:1-16,38-41`. Az interaktív promptok, firmware-parancsok és státuszsor-kezelés helyi kiegészítések.

Az [`xmodem` 0.4.6 LICENSE](https://github.com/tehmaze/xmodem/blob/0.4.6/LICENSE) MIT licencet és copyright-értesítéseket tartalmaz; a 0.4.6 kiadási jegyzet külön jelzi, hogy a LICENSE bekerült a forrásdisztribúcióba. Technikai következtetés:

- maga az `xmodem` csomag és a kiadásban szereplő példakód MIT-feltételek alatt érhető el;
- ha a helyi scriptek a példakód lényeges részét átvették, publikáláskor az MIT copyright- és licencértesítés megtartását rendezni kell;
- a csomag MIT licence nem licenceli automatikusan a helyi saját sorokat, a C++ firmware-t vagy az egész repositoryt;
- a scriptekben jelenleg nincs saját licenc- vagy attribúciós fejléc, ezért a teljes fájlokra **no license found** a dokumentált állapot.

## 8. Arduino library metadata eredete

A `library.properties` helyben jött létre `9446c9f` alatt. Szerző-/maintainermezője és URL-je nem egyezik sem a ma5ter upstreammel, sem a repository tényleges tartalmával. A WebSocket-leíró bekezdés gyakorlatilag egyezik a [gilmaimon/ArduinoWebsockets `library.properties`](https://github.com/gilmaimon/ArduinoWebsockets/blob/master/library.properties) bekezdésével, miközben a jelen projekt nem WebSocket-library.

Az ArduinoWebsockets repositoryban [GPL-3.0 licencfájl](https://github.com/gilmaimon/ArduinoWebsockets/blob/master/LICENSE) található. Ez nem bizonyítja, hogy a jelen teljes firmware GPL-3.0 alatt áll, és egy metadata-mondat szerzői jogi terjedelméről ez a technikai audit nem hoz jogi döntést. Biztonságos következő lépésként a téves bekezdést később tényszerű, önállóan megírt metadata váltsa fel; addig a fájl licencállapota **unknown**.

## 9. Publikálási döntési tábla

| Terület | Igazolt licenccel publikálható? | Blokkoló / szükséges lépés |
| --- | --- | --- |
| Teljes jelenlegi repository | **Nem igazolt.** | A firmware és több dokumentációs/metadata elem nincs teljes, összeegyeztetett licenclánccal lefedve. Ne legyen nyilvános kiadás pusztán egy új gyökérlicenc hozzáadásával. |
| ma5ter firmware: sketch + Programmer | **Nem jelen állapotban.** | Upstream licenc vagy írásos engedély/pontosítás szükséges, beleértve annak megerősítését, hogy az upstream jogosult volt a komponensek továbbengedélyezésére. |
| WarMonkey Winbond-réteg | **Bizonytalan.** | Eredeti Google Code licenc vagy szerzői engedély szükséges; ha nem szerezhető meg, a komponens cserélendő vagy tisztán újraimplementálandó. |
| Firmware XMODEM-réteg | **Bizonytalan.** | A mélyebb eredet és licenc tisztázása, ma5ter engedélye, vagy clean-room csere szükséges. |
| Python `xmodem` csomag mint külső függőség | **Igen, MIT-feltételekkel.** | Verzió és MIT notice dokumentálása; ez nem ad projektlicencet a hívó kódra. |
| Két helyi Python-script | **Részben tisztázható, de még nem kész.** | A felhasználó saját/AI-segített részeinek jogosultságát rögzíteni, az átvett MIT-példarészek értesítéseit rendezni, majd csak a jogosult részre projektlicencet választani. |
| `library.properties` | **Nem ajánlott jelen formában.** | Téves, más projektből származó metadata és ismeretlen licencscope; később önálló, tényszerű újraírás szükséges. |
| README helyi kiegészítései | **Csak a jogosult helyi rész tisztázása után.** | Az upstream README licenc nélküli; a helyi hozzáadások különválasztása vagy önálló dokumentáció készítése szükséges. |

### 9.1. Ha az eredeti firmware licence nem tisztázható

Technikai lehetőségek:

1. Kérni a ma5ter maintainerétől és a WarMonkey-kód jogosultjától írásos engedélyt vagy utólagos, egyértelmű licencközlést.
2. Az ismeretlen licencű firmware-részeket nem publikálni; helyettük dokumentált követelményekből, chipadatlapból és nyilvános protokollspecifikációból clean-room módon új implementációt készíteni, a régi kód másolása nélkül.
3. A Winbond- és XMODEM-réteget ismert, kompatibilis licencű, karbantartott komponensekre cserélni, majd azok notice- és forráskövetelményeit komponensenként teljesíteni.
4. A helyi Python eszközt önálló követelményből újraírni, vagy az MIT-es `xmodem` API-t függőségként használni a szükséges értesítések megtartásával.
5. Az új implementáció működését kizárólag viselkedési tesztekkel és ismert golden mintákkal összevetni; az új forrás szerzői/hozzájárulói nyilvántartását az első committól vezetni.

## 10. Következő licencdöntés ajánlása

- Ebben a lépésben ne kerüljön kiválasztásra gyökérprojekt-licenc.
- Előbb legyen írásos válasz a ma5ter upstream licencéről és a beágyazott Winbond/XMODEM kód továbbengedélyezési jogáról.
- A Python-scriptek MIT-példaátvételének notice-követelményét és a felhasználó saját/AI-segített részeinek jogosultságát külön kell dokumentálni.
- A hibás ArduinoWebsockets metadata későbbi lecserélése után újra kell futtatni a fájlonkénti provenance-ellenőrzést.
- Csak ezután választható licenc azokra az új vagy helyi részekre, amelyek felett a projektgazda ténylegesen rendelkezik; egy új LICENSE nem írhatja felül az örökölt komponensek feltételeit vagy a hiányzó engedélyt.

## 11. Végső változásellenőrzés

- A `git diff --check` 0 exit kóddal, kimenet nélkül zárult.
- A `git diff --cached --check` 0 exit kóddal, kimenet nélkül zárult.
- A `git status --short` kimenete:

```text
?? AUDIT.md
?? BASELINE.md
?? PROVENANCE.md
?? newbios2.bin
?? output.rom
```

A követett projektfájlok változatlanok; a `PROVENANCE.md` az egyetlen új fájl a korábban ismert négy nem követett fájl mellett. Stagingelt változás nincs.

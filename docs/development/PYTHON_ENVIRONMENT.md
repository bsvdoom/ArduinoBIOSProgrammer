# Python-környezet (8/12)

## Rögzített verzió

A projekt Python-verziója **3.14.7**, ezt a gyökérszintű
`.python-version` rögzíti. A verzió a feladat előírt célverziója, és a
[Python 3.14.7 hivatalos kiadása](https://www.python.org/downloads/release/python-3147/)
2026. augusztus 5-én jelent meg.

Az ellenőrzött interpreter:

```text
python --version: Python 3.14.7
pyenv által választott útvonal: <pyenv-root>/versions/3.14.7/bin/python
.venv belépési pont: <repository>/.venv/bin/python
.venv feloldott interpreter: <pyenv-root>/versions/3.14.7/bin/python3.14
fordító: GCC 15.2.0
```

Ez igazolja, hogy a `.venv` nem a rendszer korábbi 3.14.4 interpreterét
használja.

## pyenv állapota és a 3.14.7 telepítése

A pyenv már telepítve volt. A kiinduló verzió `pyenv 2.6.4` volt, és csak a
`system` interpreter állt rendelkezésre. Az első `pyenv install 3.14.7`
kísérlet még a fordítás előtt leállt, mert az elavult python-build definíció
nem ismerte ezt a kiadást; a listában legfeljebb `3.14.0b4` szerepelt.

A tiszta, Git-alapú meglévő pyenv telepítés saját `pyenv update` parancsával
frissült `pyenv 2.8.6` verzióra. Ezután a kért parancs sikeresen lefutott:

```sh
pyenv install 3.14.7
```

A telepítés célja a pyenv saját `<pyenv-root>/versions/3.14.7` könyvtára. Nem történt `sudo`,
rendszercsomag-kezelős vagy globális Python-csomagtelepítés, és shell profile
sem változott.

## Virtuális környezet

A projektbeli környezet létrehozása:

```sh
python -m venv .venv
```

Linuxon opcionális aktiválás:

```sh
source .venv/bin/activate
```

Az ellenőrzések nem függnek aktiválástól; az ajánlott forma:

```sh
.venv/bin/python --version
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python -m pip check
```

Windows checkoutban a virtuális környezetet helyben kell újralétrehozni a
3.14.7-es interpreterrel; Unixról nem szabad átmásolni. PowerShell aktiválás:

```powershell
.\.venv\Scripts\Activate.ps1
```

Aktiválás nélkül a `\.venv\Scripts\python.exe` és
`\.venv\Scripts\python.exe -m pip` formák használhatók.

A `.venv/`, `__pycache__/` és `*.py[cod]` Gitből ki vannak zárva. A
`.python-version` szándékosan nincs kizárva.

## Futásidejű függőségek

A két legacy szkript importjai alapján csak a következő külső runtime
csomagokra van szükség:

```text
xmodem==0.5.0
pyserial==3.5
```

A `requirements.txt` nem tartalmaz PlatformIO-t, lintert vagy teszteszközt.
A telepítés kizárólag a projekt venvjébe történt:

```sh
.venv/bin/python -m pip install -r requirements.txt
```

Ellenőrzött eredmény:

```text
.venv Python: 3.14.7
pip: 26.2.1 (Python 3.14)
pip check: No broken requirements found.
import serial, xmodem: PASS
serial.VERSION: 3.5
importlib.metadata pyserial: 3.5
importlib.metadata xmodem: 0.5.0
```

A pip a nem írható felhasználói cache miatt figyelmeztetett, majd cache nélkül
helyesen lefutott. Ez nem érinti a venvbe telepített csomagokat.

## Legacy szkriptek diagnosztikája

A szkripteket nem indítottuk el; csak a Python fordítási ellenőrzés futott:

```sh
.venv/bin/python -m py_compile xmodembiosread.py
.venv/bin/python -m py_compile xmodembioswrite.py
```

Mindkettő eredménye **EXPECTED MIGRATION FAILURE**. A 24. sor Python 2-es
`print ser.readline().decode()` szintaxisa Python 3.14.7 alatt
`SyntaxError: Missing parentheses in call to 'print'` hibát ad. A fájlok ebben
a lépésben szándékosan nem változtak; migrációjuk a 9/12. lépés feladata.

## Firmware-regresszió ellenőrzése

Az új Python-környezet létrehozása után:

- natív karakterizáló tesztek: **71 PASS**, 0 futó `MISSING SAFEGUARD`,
  0 váratlan hiba;
- ASan/UBSan: **PASS**, sanitizer-hiba nélkül;
- Arduino Uno PlatformIO build: **PASS**;
- programmemória: 8406 / 32256 bájt (26,1%);
- dinamikus memória: 243 / 2048 bájt (11,9%).

A natív buildben továbbra is megjelent a korábban dokumentált hat
signed/unsigned összehasonlítási warning a `winbondflash.cpp` fájlban. A
Python-környezet nem okozott új firmware-warningot vagy regressziót.

## Reprodukció új fejlesztő számára

Linuxon, telepített pyenv használatával:

```sh
pyenv install 3.14.7
python --version
python -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python -m pip check
.venv/bin/python -c "import serial, xmodem"
bash tests/characterization/run_all.sh
pio run -e uno
```

Ha a helyi pyenv definíció még nem ismeri a hivatalos 3.14.7 kiadást, előbb a
pyenv támogatott saját frissítési módját kell használni. Windows alatt pontosan
3.14.7-es Pythonból kell létrehozni a helyi `.venv` könyvtárat, majd a fenti
pip parancsokat `\.venv\Scripts\python.exe -m pip` formában futtatni.

Soros portos vagy más hardveres művelet nem történt.

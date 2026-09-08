## A project célja egy saját építésű és saját kódot futtató eszköz elkészítése volt.

Az ESP8266 OLED panelre (HW-364A) esett a választás, mert így azért látható a működése, és könnyebb a hibakeresés.

<img width="748" height="397" alt="Vágólap_09-01-2026_03" src="https://github.com/user-attachments/assets/c7632f38-97b1-46bd-bd8f-61d52e9944e6" />


A panelen kívül csak 1 db 10Kohm-os ellenálásra van szükség, és egy RJ12 6P6C dugóra, lapos kábelre, és egy krimpelő fogóra.

## 🔌 Hardver Felépítés és Bekötés

### RJ12 (P1 csatlakozó) Bekötése

A P1 port egy standard 6P6C (RJ12) csatlakozót használ:

<img width="643" height="159" alt="P1 pinout" src="https://github.com/user-attachments/assets/c57e9598-1fa0-42ce-871a-f8490dd7cc21" />

A Mérőóra szabvány P1 portja teljesen galvanikus leválasztású, az alábbi képen látható a belső felépítése:
<img width="1191" height="880" alt="P1 port schematics" src="https://github.com/user-attachments/assets/bd77d414-e692-4f6f-8d98-78c32359d14d" />


Bekötés:

| RJ12 Láb | Név | Leírás | ESP8266 Bekötés |
| :--- | :--- | :--- | :--- |
| **1** | **+5V** | Tápellátás (max 250mA, villanyórától függő) | ESP8266 5V / VIN  |
| **2** | **Data Request (RTS)** | Adatkérés engedélyezése (High szinten tartandó) | ESP8266 **3.3V** láb |
| **3** | **Data GND** | Adat Föld | ESP8266 **GND** |
| **4** | **NC** | Nincs bekötve | - |
| **5** | **TxD** | Invertált soros adat kimenet (P1-ből) | ESP8266 **RX (GPIO3)** |
| **6** | **Power GND** | Táp Föld | ESP8266 **GND** |

> ⚠️ **FONTOS**: A P1 interfész TxD jele **Open Collector** kimenetű és **INVERTÁLT D3 (Logic 0 = 5V, Logic 1 = 0V)** ezért egy 10K ellenálással felvan húzva 3V-ra.

Az alábbi ábrán látható a bekötés:


<img width="682" height="657" alt="Vágólap_09-01-2026_01" src="https://github.com/user-attachments/assets/d1b8724b-5b83-4735-9dcb-2a9cc9daf56f" />



Működés:


<img width="849" height="392" alt="Vágólap_09-01-2026_04" src="https://github.com/user-attachments/assets/99bcfe61-8922-4de2-9d67-a67d798bfd83" />

A működéshez szükség van WIFI hálózatra, ezért fontos hogy a villanyóránál megfelelő erősségű WIFI csatlakozás legyen.
Valamint az MQTT üzenetek fogadásához kell egy MQTT broker, ez lehet egy saját szerver, de akár felhős szolgáltató is lehet.

Én egy HP510 vékonykliensből épített LINUX szerverrel kötöttem össze.
Ez a gép kb 1A-t fogyaszt 13.8V DC tápról.
Valójában 18V a gyári tápja , de egy UPS funkcióval ellátott rácsos tápról vígan működik egy 12V 7Ah akkuval kombinálva.
Így megoldott az áramszünetek idején az újraindulás kiküszöbölése.
Egy SSD-re van telepítve egy UBUNTU server 16.04, és ezen fut a mosquitto broker. (meg egy komplett ház automatizálás Node-RED-ben)

Folytatás nemsokára ......


Chip is ESP8266EX
Features: Wi-Fi, 160MHz
Crystal is 26MHz
MAC: 8cce4ede3652
Manufacturer: c4
Device: 6016
Status value: 0x600000
Detected flash size: 4MB

https://www.espboards.dev/tools/program/

vagy 

https://esptool.spacehuhn.com/


0x00000



## 📄 Licensz

Ez a projekt nyílt forráskódú, szabadon felhasználható és módosítható a MIT licenc feltételei szerint.
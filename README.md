<div align="left">
  <h1>📡 ESP-HACK FW   <a href="#en">English</a> | <a href="#ru">Русский</a></h1>

  ![ESP-HACK_LOGO](Pictures/ESP-HACK.png)
</div>

<div id="en">

## 🚀 About ESP-HACK FW
ESP-HACK is a powerful universal firmware for the ESP32, built for RF research and pentesting of radio frequencies, Bluetooth, infrared devices and GPIO integrations.  
The project targets enthusiasts and pentesters who want to explore protocols and devices in Sub-GHz ranges and other wireless technologies.

> *The firmware is stable within its declared functionality, but some features are marked as "in development". Use the device according to the laws in your region.*

---

### ⚠️ Disclaimer
This firmware is developed strictly for research and hardware testing purposes.  
You must comply with the laws of your country. The author is not responsible for improper / illegal use. Jammers are **ILLEGAL** in most countries.

---

### ⚡ Features

**WiFi**
- ~~Deauther~~ *(80 pckts/sec)*  
- Beacon Spam  
- EvilPortal  
- Wardriving

**Bluetooth**
- iOS, Android, WIN Spam  
- BadBLE

**SubGHz**
- SubRead  
- ~~SubSend~~ *(in development)*  
- Analyzer  
- Jammer

**Infrared**
- IR-Send  
- IR-Read  
- TV, PJ, AC OFF

**GPIO**
- ~~iButton (without module)~~ *(in development)*  
- NRF24  
- ~~Serial~~ *(in development)*

---

### 📡 Supported SubGHz modulations
- Princeton  
- RcSwitch  
- Came  
- Holtec  
- Nice  
- StarLine  
- KeeLoq

---

## 🛠️ Building

### 🔧 Required components
| Component | Link |
|-----------|--------|
| ESP32-WROOM | [TAP](https://sl.aliexpress.ru/p?key=A7e3VOZ) |
| CC1101 | [TAP](https://sl.aliexpress.ru/p?key=Ale3VnU) |
| Display | [TAP](https://sl.aliexpress.ru/p?key=9O83V87) |
| SD-Card module | [TAP](https://sl.aliexpress.ru/p?key=Px83VhI) |
| IR-TX and IR-RX | [TAP](https://sl.aliexpress.ru/p?key=nW83Vd3) |

---

### 🔌 Connection scheme (pinout)

| Module | Pin 1 | Pin 2 | Pin 3 | Pin 4 | Pin 5 | Pin 6 | Pin 7 |
|--------|-------|-------|-------|-------|-------|-------|-------|
| **📺 Display** | VCC → 3V3 | GND → GND | SCL → G22 | SDA → G21 | - | - | - |
| **🔘 Buttons** | UP (K1) → G27 | DOWN (K2) → G26 | OK (K3) → G33 | BACK (K4) → G32 | - | - | - |
| **📡 CC1101** | 1 → GND | 2 → 3V3 | 3 → G34 | 4 → G5 | 5 → G18 | 6 → G23 | 7 → G19 |
| **📡 IR** | IR-T → G4 | IR-R → G14 | - | - | - | - | - |
| **🔌 GPIO (extra)** | G35 | G25 | G17 | G16 | G13 | G12 | G4 |
| **💾 SD-Card** | MOSI → G15 | CLK → G2 | MISO → G0 | CS → GND | - | - | - |

---

## Errors (ERROR:)
During operation ESP-HACK may show the following errors:

| Error code | ❌ Problem | 🛠️ Possible fix |
|------------|-----------|------------------|
| **0x000**  | SD card initialization failed | 🛠️ Format the SD card as **FAT32** or replace it. |
| **0x001**  | CC1101 initialization failed | 🛠️ Check wiring and module functionality (power, SPI, contacts). |
| **0x002**  | NRF24 initialization failed | 🛠️ Verify chosen pins/connections and reboot the device. |

---

## 📸 Final result
[![Watch the video](assets/preview.jpg)](assets/demo.mp4)

> Click the preview

---

## ✉️ Contacts / Author
Author: Teapot174  
Telegram: `@ESPH4CK`

</div>

<div id="ru" hidden>

## 🚀 О проекте ESP-HACK FW
ESP-HACK — мощная универсальная прошивка для ESP32, собранная для исследований и пентестинга радиочастот, Bluetooth, инфракрасных устройств и GPIO-интеграций.  
Проект ориентирован на энтузиастов и пентестеров, желающих исследовать протоколы и устройства в суб-гигагерцовых диапазонах и в беспроводных технологиях.

> *Прошивка стабильна в рамках заявленного функционала, но некоторые фичи отмечены как “в разработке”. Используйте устройство согласно законам вашего региона.*

---

### ⚠️ Дисклеймер
Данная прошивка разработана исключительно для исследовательских целей и тестирования оборудования.  
Вы обязаны соблюдать законодательство вашей страны. Автор не несёт ответственности за неправомерное использование. Глушилки — **НЕЛЕГАЛЬНЫ** в большинстве стран.

---

### ⚡ Возможности

**WiFi**
- ~~Deauther~~ *(80 pckts/sec)*  
- Beacon Spam  
- EvilPortal  
- Wardriving

**Bluetooth**
- iOS, Android, WIN Spam  
- BadBLE

**SubGHz**
- SubRead  
- ~~SubSend~~ *(в разработке)*  
- Analyzer  
- Jammer

**Infrared**
- IR-Send  
- IR-Read  
- TV, PJ, AC OFF

**GPIO**
- ~~iButton (без модуля)~~ *(в разработке)*  
- NRF24  
- ~~Serial~~ *(в разработке)*

---

### 📡 Поддерживаемые модуляции SubGHz
- Princeton  
- RcSwitch  
- Came  
- Holtec  
- Nice  
- StarLine  
- KeeLoq

---

## 🛠️ Сборка

### 🔧 Необходимые компоненты
| Компонент | Ссылка |
|-----------|--------|
| ESP32-WROOM | [ТЫК](https://sl.aliexpress.ru/p?key=A7e3VOZ) |
| CC1101 | [ТЫК](https://sl.aliexpress.ru/p?key=Ale3VnU) |
| Дисплей | [ТЫК](https://sl.aliexpress.ru/p?key=9O83V87) |
| SD-Card модуль | [ТЫК](https://sl.aliexpress.ru/p?key=Px83VhI) |
| IR-TX и IR-RX | [ТЫК](https://sl.aliexpress.ru/p?key=nW83Vd3) |

---

### 🔌 Схема подключения (распиновка)

| Module | Pin 1 | Pin 2 | Pin 3 | Pin 4 | Pin 5 | Pin 6 | Pin 7 |
|--------|-------|-------|-------|-------|-------|-------|-------|
| **📺 Display** | VCC → 3V3 | GND → GND | SCL → G22 | SDA → G21 | - | - | - |
| **🔘 Buttons** | UP (K1) → G27 | DOWN (K2) → G26 | OK (K3) → G33 | BACK (K4) → G32 | - | - | - |
| **📡 CC1101** | 1 → GND | 2 → 3V3 | 3 → G34 | 4 → G5 | 5 → G18 | 6 → G23 | 7 → G19 |
| **📡 IR** | IR-T → G4 | IR-R → G14 | - | - | - | - | - |
| **🔌 GPIO (доп.)** | G35 | G25 | G17 | G16 | G13 | G12 | G4 |
| **💾 SD-Card** | MOSI → G15 | CLK → G2 | MISO → G0 | CS → GND | - | - | - |

---

## Ошибки (ERROR:)
В процессе работы ESP-HACK могут возникать следующие ошибки:

| Код ошибки | ❌ Описание ошибки                        | 🛠️ Возможное решение                                                                 |
|------------|-------------------------------------------|--------------------------------------------------------------------------------------|
| **0x000**  | Ошибка инициализации SD-карты             | 🛠️ Отформатируйте SD-карту в **FAT32** либо замените её.                            |
| **0x001**  | Ошибка инициализации **CC1101**           | 🛠️ Проверьте подключение и работоспособность модуля (питание, SPI, контакты).        |
| **0x002**  | Ошибка инициализации **NRF24**            | 🛠️ Проверьте правильность выбора пинов, соединений и перезагрузите устройство.       |

---

## 📸 Финальный результат
[![Watch the video](assets/preview.jpg)](assets/demo.mp4)

> Нажмите на превью

---

## ✉️ Контакты / Автор
Автор: Teapot174  
Telegram: `@ESPH4CK`

</div>

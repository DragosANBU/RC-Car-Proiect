# RC Car Project

Proiect realizat în cadrul facultății ce constă într-o mașinuță RC controlată prin intermediul unui ESP32 și al unei interfețe web accesibile de pe telefon.

## Funcționalități

- Control prin Wi-Fi folosind ESP32 în modul Access Point.
- Interfață web pentru controlul direcției și vitezei.
- Control cu joystick sau butoane.
- Servo pentru direcție.
- Motoare DC controlate prin driver.
- Senzori ultrasonici față și spate pentru detectarea obstacolelor.
- Buzzere pentru avertizare în funcție de distanță.
- Senzor de lumină BH1750.
- Aprinderea automată sau manuală a farurilor.
- Oprire automată în cazul pierderii conexiunii.

## Componente utilizate

- ESP32
- Driver motoare
- 2 × Motoare DC
- Servomotor
- 2 × Senzor ultrasonic HC-SR04
- 2 × Buzzere
- Senzor de lumină BH1750
- LED pentru far
- Alimentare cu baterii

## Schema proiectului

Schema de conectare este prezentată mai jos.

![Schema RC Car](schema.png)

> Înlocuiește `schema.png` cu numele imaginii din repository.

## Structura proiectului

- `RC-Car-Proiect.ino` – codul principal pentru ESP32.
- `schema.png` – schema electrică a proiectului.

## Cum se utilizează

1. Se încarcă programul pe ESP32.
2. ESP32 creează rețeaua Wi-Fi **ESP32_RC_CAR**.
3. Telefonul se conectează la această rețea.
4. Se accesează adresa **192.168.4.1** din browser.
5. Mașinuța poate fi controlată din interfața web.

## Autori
- Dragos Florin Draga
- Raicu Rares

# Password-Based-Door-Lock-Using-STM32-
🔐 Password Based Door Lock System Using STM32

📌 Project Overview

The Password Based Door Lock System using STM32 is an electronic security system designed to replace traditional mechanical locks with a password-controlled locking mechanism.

The system uses an STM32 microcontroller as the main controller. A 4×4 matrix keypad is used to enter the password, while an I2C LCD display provides user instructions and system status. When the correct password is entered, the STM32 controls a 28BYJ-48 stepper motor through a ULN2003 driver module to unlock the door. After a specified time, the motor returns the lock to its locked position.

The project is programmed using the Arduino IDE.

---

🎯 Objectives

- Develop a low-cost electronic door security system.
- Replace traditional keys with password-based authentication.
- Control a stepper motor using an STM32 microcontroller.
- Display system status using an I2C LCD.
- Provide protection against repeated incorrect password attempts.
- Demonstrate the practical application of embedded systems and microcontrollers.

---

🛠️ Components Required

Component| Purpose
STM32F103C8T6 Blue Pill| Main microcontroller
4×4 Matrix Keypad| Password input
16×2 I2C LCD| Display and user interface
ULN2003 Driver Module| Stepper motor driver
28BYJ-48 Stepper Motor| Locking/unlocking mechanism
USB-to-TTL Converter| Programming/serial communication
Jumper Wires| Connections
5V Power Supply| Power supply for motor and modules

---

🔧 Software Requirements

- Arduino IDE
- STM32 Arduino Core / STM32duino
- Keypad Library
- LiquidCrystal_I2C Library
- EEPROM Library

---

⚙️ Working Principle

The working of the system is as follows:

1. The STM32 initializes the keypad, LCD and stepper motor.
2. The LCD displays "Enter Password".
3. The user enters the password using the 4×4 keypad.
4. The entered password is compared with the stored password.
5. If the password is correct:
   - LCD displays "Access Granted".
   - The stepper motor rotates through the ULN2003 driver.
   - The door mechanism moves to the unlocked position.
6. After the predefined unlocking time, the stepper motor rotates in the opposite direction.
7. The door returns to the locked position.
8. If the password is incorrect:
   - LCD displays "Wrong Password".
   - A buzzer can provide an error indication.
9. After multiple incorrect attempts, the system enters a temporary lockout mode.

---

🧩 Block Diagram
![Block Diagram](block%20diagram.jpeg)

---

🔌 Pin Connections

4×4 Matrix Keypad

Keypad| STM32
<br>R1| PA0
<br>R2| PA1
<br>R3| PA2
<br>R4| PA3
<br>C1| PA4
<br>C2| PA5
<br>C3| PA6
<br>C4| PA7

I2C LCD

<br>LCD| STM32
<br>SDA| PB7
<br>SCL| PB6
<br>VCC| 5V
<br>GND| GND

ULN2003 + Stepper Motor

<br>ULN2003| STM32
<br>IN1| PC0
<br>IN2| PC1
<br>IN3| PC2
<br>IN4| PC3
<br>GND| GND

Connect the 28BYJ-48 stepper motor to the motor connector on the ULN2003 module.

USB-to-TTL Converter

<br>USB-TTL| STM32
<br>TX| PA10 (RX)
<br>RX| PA9 (TX)
<br>GND| GND

Important: Make sure the USB-to-TTL converter uses the appropriate 3.3 V logic level for STM32 UART signals.

---

🔑 Default Password

The initial password in the example program is:

1234

For an actual deployment, change the default password before using the system.

---

🔐 Security Features

- Password-based authentication
- Password masking on LCD
- Incorrect password detection
- Multiple-attempt lockout
- Automatic locking after a predefined time
- Non-volatile password storage
- Optional buzzer indication
- Optional access-event logging

---

💻 Arduino IDE Setup

Step 1 — Install Arduino IDE

Install the Arduino IDE on your computer.

Step 2 — Install STM32 Board Support

Add STM32 board support to Arduino IDE and select the appropriate STM32F1 board.

Step 3 — Install Libraries

Install:

Keypad
LiquidCrystal_I2C
EEPROM

Step 4 — Select Board

For a typical Blue Pill:

Board: Generic STM32F1 Series
MCU: STM32F103C8

Select the appropriate upload method for your board.

Step 5 — Connect USB-to-TTL

Connect the USB-to-TTL converter to the STM32 UART pins and upload the program.

---



📸 Project Images

## Project Prototype

![Project Prototype](Images/project_setup.jpg)

Circuit diagram:

## Circuit Diagram

![Circuit Diagram](Circuit/circuit_diagram.png)

---

🚦 System Operation

Correct Password

Enter Password:
<br>****
    <br>   ↓
<br>Access Granted
  <br>     ↓
<br>Motor Rotates
    <br>   ↓
<br>Door Unlocked
    <br>   ↓
<br>Wait
<br>       ↓
<br>Motor Rotates Back
 <br>      ↓
<br>Door Locked

Incorrect Password

Enter Password
      <br>↓
<br>Wrong Password
    <br>  ↓
<br>Attempt Counter++
     <br> ↓
<br>3 Wrong Attempts
    <br>  ↓
<br>System Lockout

---

🌟 Features

- 🔐 Password authentication
- ⌨️ 4×4 keypad input
- 📟 I2C LCD display
- ⚙️ Stepper motor-based locking mechanism
- 🔌 ULN2003 motor driver
- 💾 Password storage
- 🚫 Failed-attempt protection
- 🔄 Automatic locking
- 💻 Arduino IDE programming
- 🔗 USB-to-TTL serial communication

---

🚀 Future Improvements

The system can be further improved by adding:

- RFID authentication
- Fingerprint authentication
- Bluetooth-based access
- Wi-Fi/IoT monitoring
- Mobile application
- RTC-based access logging
- Multiple user passwords
- OTP authentication
- Face recognition
- Tamper detection
- Battery backup

---

📚 Applications

This system can be used for:

- Homes
- Offices
- Laboratories
- College laboratories
- Server rooms
- Storage rooms
- Small businesses
- Restricted-access areas

---

⚠️ Important Note

This project is intended primarily for educational and prototype purposes. The 28BYJ-48 stepper motor and mechanical locking mechanism should be properly mounted and tested before use on an actual door.

The motor should be powered from a suitable external supply through the ULN2003 driver. Do not attempt to power the motor directly from an STM32 GPIO pin.

---

👨‍💻 Technologies Used

Microcontroller : STM32F103C8T6
Programming     : Embedded C / Arduino C++
IDE             : Arduino IDE
Display         : 16×2 I2C LCD
Input           : 4×4 Matrix Keypad
Motor Driver    : ULN2003
Motor           : 28BYJ-48 Stepper Motor
Communication   : UART
Programming     : USB-to-TTL

---

📄 License

This project is created for educational and academic purposes. You are free to use and modify the project for learning and non-commercial purposes.

---

⭐ Acknowledgement

This project demonstrates the integration of STM32 microcontroller programming, keypad interfacing, I2C communication, LCD interfacing, motor control, and embedded security in a practical application.

If you find this project useful, consider giving the repository a ⭐ on GitHub.

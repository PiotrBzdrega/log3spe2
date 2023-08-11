| LOG3SPE2 |
| -------- |

# Description

Idea behind was to create simple, private use, alternative for hardware authentication keys like Yubikey. This was to fulfill only static passwords automatically assigning credentials on windows interface.
The project consists of two applications: 
PC (logpc):
- C/ C++ (Visual Studio) 

µC (log3spe2):
- C (Espressif-IDE, Visual Studio Code)

Communication is established via a classic Bluetooth Serial Port Profile that is supported by Virtual Serial Port on Windows. All credentials are stored on non-volatile storage that can be added, deleted or modified on run-time.
How does it work?
PC application reads URL from the currently opened tab on the web browser (currently only Chrome).
It sends afterward a request with the alias of URL and found login or/and password field.

Application LogPC must contain hardcoded AutomationID of login field for interested us domain to be able successfully find credential fields. Password fields contain special property (IsPasswordProperty) that make it possible to recognize it without additional information.


## RAW TELEGRAMS:

|          LOGPC         |        LOG3SPE2        |          DESCRIPTION                                                                         | 
| :---------------------:| :---------------------:|  -------------------------------------------------------------------------------------------:| 
|                        |     `0(UI_DOMAIN)`     | LOG3SPE2 wakes-up LOGPC to search for known url and login fields                             |
|  `1(UI_LOGIN) ,“boo”`  |                        | LOGPC found login field on website “boo” (alias is delivered)                                |
|                        | `6(UI_MISSED) ,“boo”`  | LOG3SPE2 response with information that doesn’t have credentials for this website in storage |
| ---------------------- | ---------------------- | ------------------------------------------------------------------------------------------   |


## To be implemented
* secure exchange credentials,
* installable Windows application,
* Qt+ interface with tray minimize
* design of esp32 lego prototype,
* esp32 on deep sleep,
* other web browsers compatibility (edge, mozilla),
* linux compatibility,
* better alternative for SPP and virtual Serial Port  (Serial Port needs to be poll if device available)
* replace the old C-syntax for >=C++11 in LogPC
* log library in logpc

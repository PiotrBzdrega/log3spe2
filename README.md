| LOG3SPE2 |
| -------- |

# Description

Idea behind was to create simple, private use, alternative for hardware authentication keys like Yubikey. This was to fulfill only static passwords automatically assigning credentials on windows interface.
The project consists of two applications:   
PC (LOGPC):
- C/ C++ (Visual Studio) 

µC (LOG3SPE2):
- C (Espressif-IDE, Visual Studio Code)

Communication is established via a classic Bluetooth Serial Port Profile that is supported by Virtual Serial Port on Windows. All credentials are stored on non-volatile storage that can be added, deleted or modified on run-time.
How does it work?
PC application reads URL from the currently opened tab on the web browser (currently only Chrome).
It sends afterward a request with the alias of URL and found login or/and password field.

Application LogPC must contain hardcoded AutomationID of login field for interested us domain to be able successfully find credential fields. Password fields contain special property (IsPasswordProperty) that make it possible to recognize it without additional information.  


## RAW TELEGRAMS:

|          LOGPC             |        LOG3SPE2                       |          DESCRIPTION                                                                                          | 
| :---------------------:    | :-----------------------------------: | :-----------------------------------------------------------------------------------------------------------: | 
|        :arrow_left:        |             `0(UI_DOMAIN)`            | LOG3SPE2 wakes-up LOGPC to search for known url and login fields                                              |
|  `1(UI_LOGIN) ,“boo”`      |             :arrow_right:             | LOGPC found login field on website “boo” (alias is delivered)                                                 |
|        :arrow_left:        |       `6(UI_MISSED) ,“boo”`           | LOG3SPE2 response with information that doesn’t have credentials for this website in storage                  |
|:computer:|:iphone:|:scroll:|
|        :arrow_left:        |     `0(UI_DOMAIN)`                    | LOG3SPE2 wakes-up LOGPC to search for known url and login fields                                              |
| `1(UI_LOGIN) ,“yandex”`    |             :arrow_right:             | LOGPC found login field on website “yandex” (alias is delivered)                                              |
|        :arrow_left:        |    `1(UI_LOGIN) ,“yandex”,"Adam"`     | LOG3SPE2 respond with found login related to "yandex" from NVS                                                |
| `4(UI_DONE) ,“yandex”`     |             :arrow_right:             | LOGPC acknowledges that credential has been inserted                                                          |
|:computer:|:iphone:|:scroll:|
|        :arrow_left:        |     `0(UI_DOMAIN)`                    | LOG3SPE2 wakes-up LOGPC to search for known url and login fields                                              |
|`2(UI_PASSWORD) ,“google”`  |             :arrow_right:             | LOGPC found login field on website “google” (alias is delivered)                                              |
|        :arrow_left:        |    `2(UI_LOGIN) ,“google”,"1$%2as"`   | LOG3SPE2 respond with found login related to "google" from NVS                                                |
| `4(UI_DONE) ,“google”`     |             :arrow_right:             | LOGPC acknowledges that credential has been inserted                                                          |
|:computer:|:iphone:|:scroll:|
|        :arrow_left:        |     `0(UI_DOMAIN)`                    | LOG3SPE2 wakes-up LOGPC to search for known url and login fields                                              |
| `3(UI_LOGPASS) ,“github”`  |             :arrow_right:             | LOGPC found login field on website “github” (alias is delivered)                                              |
|        :arrow_left:        |  `3(UI_LOGPASS) ,“github”,"JOhn","qwerty123"`   | LOG3SPE2 respond with found login related to "github" from NVS                                      |
| `4(UI_DONE) ,“github”`     |             :arrow_right:             | LOGPC acknowledges that credential has been inserted                                                          |
|:computer:|:iphone:|:scroll:|
| `5(UI_NEW_CREDENTIAL) ,“bp”,"Andrew1","1234"` |    :arrow_right:   | LOGPC (wakes up LOG3SPE2) notify about new credentials that should be stored in LOG3SPE2 NVS                  |
|        :arrow_left:        | `4(UI_DONE) ,“bp”` | LOG3SPE2 acknowledges that credentials are stored                                                                                |
|:computer:|:iphone:|:scroll:|
| `6(UI_ERASE) ,“book”`      |             :arrow_right:             | LOGPC  (wakes up LOG3SPE2) informs LOG3SPE2 that credentials related to website “facebook” should be removed  |
|        :arrow_left:        |         `4(UI_DONE) ,“book”`          | LOG3SPE2 acknowledges that credentials are stored                                                             |

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

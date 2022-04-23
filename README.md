# ESP_IDF WIFI
This is very simple component for working with WIFI base on ESP_IDF V4.3.
 
## Installation
in your project directory create `components` folder and move into it.

 `git clone https://github.com/Farzinkh/wifi.git`

 now import it in your main.c by `#include "wifi.h"`.

 and choice beetwen STA , AP , SMART modes.
## OTA config
move anything inside `config & server` folder to root of your project then create `server_certs` folder and run

`openssl req -x509 -newkey rsa:2048 -keyout server_certs/ca_key.pem -out server_certs/ca_cert.pem -days 365 -nodes`

enter server ip as common name then install server requirements once by

`pip install -r requirments.txt`

finally run server 

`python server.py`

and you are done whenever you change app version in menuconfig and recompile your code by `idf.py build`
and restart esp32 it will update constantly.

## API
`wifi_init_sta()` station mode.

`wifi_init_softap()` AP mode.

`wifi_init()` smart mode.

`get_rssi()` get rssi value.

`initialise_mdns()` init mdns.

`check_time()` update time if internet is available.

`get_ip()` get ip address and wait for connection if disconnected.

`wifi_scan()` scan for APs.

`start_ota()` update code by ota.

`ota_verify()` to verify your ota app.

`ota_discredit()` to flag your ota app as invalid.

`save_key_value()` save data in nvs.

`load_key_value()` load data from nvs.

 *Enjoy :)*

 You can find me on [![Twitter][1.2]][1] or on [![LinkedIn][3.2]][2]

<!-- Icons -->

[1.2]: http://i.imgur.com/wWzX9uB.png (twitter icon without padding)
[3.2]: https://raw.githubusercontent.com/MartinHeinz/MartinHeinz/master/linkedin-3-16.png (LinkedIn icon without padding)

<!-- Links to your social media accounts -->

[1]: https://twitter.com/FarzinKhodavei1
[2]: https://www.linkedin.com/in/farzin-khodaveisi-84288a18a/

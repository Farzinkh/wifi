# ESP_IDF WIFI
This is very simple component for working with WIFI base on ESP_IDF V4.3.
 
## Installation
in your project directory create components folder and move into it.

 `git clone https://github.com/Farzinkh/wifi.git`

 now import it in your main.c by `#include "wifi.h"`.

 and choice beetwen STA , AP , SMART modes.
## Config
this application is secured by DES in default config but you can simply change it to AES or turn off encrypting,whatever you decide to do remember to do it
in same way at the other side and also if you choose to encrypt you have to share your encrypt file(.bin) manually and use `-key <address_of_key>` in client side.

 *Enjoy :)*

 You can find me on [![Twitter][1.2]][1] or on [![LinkedIn][3.2]][2]

<!-- Icons -->

[1.2]: http://i.imgur.com/wWzX9uB.png (twitter icon without padding)
[3.2]: https://raw.githubusercontent.com/MartinHeinz/MartinHeinz/master/linkedin-3-16.png (LinkedIn icon without padding)

<!-- Links to your social media accounts -->

[1]: https://twitter.com/FarzinKhodavei1
[2]: https://www.linkedin.com/in/farzin-khodaveisi-84288a18a/

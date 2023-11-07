![alt text](https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade/blob/main/Software/CANBRIDGE-2port/Banner.jpg)

# Nissan-LEAF-Battery-Upgrade
So you are looking to upgrade the battery in your LEAF? You've come to the right place for information and software/hardware required. Take some time to familiarize with the content.

## Read this first 
Start by reading all the terminology and info about what a battery upgrade, direct upgrade, paring, CAN-bridges etc. is by following this link to the wiki: https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade/wiki

## Getting the hardware
The BatteryUpgrade software runs on both the Muxsan 3-port CAN-bridge, and on the 2-port Budget CAN-bridge. This guide focuses on the 2-port since it is the easiest to source at the moment. Here are some purchase links for both

|  Bridge version |  Purchase link |
| :--------: | :---------: |
| 3-port Muxsan | [Muxsan's Tindie store](https://www.tindie.com/products/muxsan/can-mitm-bridge-3-port-rev-25/)   |
| 2-port budget | [AliExpress "MB CAN Filter 18 in 1](https://www.aliexpress.com/item/1005003112723581.html?)   |

Note, for 2-port it has to be the BLUE PCB, the other varients are not supported.

## Software needed to flash
|  Bridge version |  Software |  Download link |
| :--------: | :--------: | :---------: |
| 3-port Muxsan | Atmel Studio v7.0.1931 |  [Download here](https://www.microchip.com/en-us/tools-resources/archives/avr-sam-mcus)   |
| 2-port budget | STM32 ST-LINK Utility |  [Download here](https://www.st.com/en/development-tools/stsw-link004.html)   |

## Hardware needed to flash
|  Bridge version |  Flasher |  Product Link |
| :--------: | :--------: | :---------: |
| 3-port Muxsan | Atmel AVRisp mkII |  [Ebay](https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2334524.m570.l1313&_nkw=Atmel+AVRisp+mkII&_sacat=0&LH_TitleDesc=0&_odkw=ST+link+v2&_osacat=0)   |
| 2-port budget | ST Link V2 |  [Ebay](https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2334524.m570.l1313&_nkw=ST+link+v2&_sacat=0&LH_TitleDesc=0&_odkw=ST+link+v23&_osacat=0)   |

## Flashing instructions
### 3-port
See this video: https://youtu.be/eLcNSo2Vn6U?t=167
### 2-port
Flash with BridgeFlasher.exe located in software folder. The compiled .srec files are ![located in the releases section](https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade/releases)  . For ST LINK CLI, point the exe towards the "ST-LINK_CLI.exe" located in the "STM32 ST-LINK Utility" folder that appears after installing it.

This is what the BridgeFlasher.exe should look like. Press Start to flash.
![alt text](https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade/blob/main/software/CANBRIDGE-2port/FlashingTool.jpg)

Connect the ST Link V2 to the J1 ports on the PCB. It can be hard to connect dupont wires here, so I recommend getting needle pins. Pay close attention to the pin labels on the ST Link flasher, they can vary location depending on type.

7 3.3V  	- J1

Nothing 	- J2

3 GND   	- J3

2 SWDIO   - J4

6 SWCLK   - J5

![alt text](https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade/blob/main/Software/CANBRIDGE-2port/FlashingInstr.jpg)


## Wiring in the hardware
When installing the 2-port into the vehicle, here are the wiring instructions:

- Red wire #1 -> +12V constant (fuse with 3A)
- Red wire #2 -> +12V constant (fuse with 3A, join together with other red wire)
- Purple -> EV-CAN Low, Green wire (battery side)
- Green -> EV-CAN High, Blue wire (battery side)
- Yellow -> EV-CAN Low, Green wire (vehicle side)
- Blue -> EV-CAN High, Blue wire (vehicle side)
- Black -> Ground
- Black -> Ground

Here is an example on a 2012 LEAF:
![alt text](https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade/blob/main/Software/CANBRIDGE-2port/Install2012.jpg)

## Software needed to make changes to code
Incase you want to make changes and actually recompile the code, you will need the following IDE
- Keil uVision5

## e-nv200 notes
The 2-port CAN-bridge can also be used on the env200. It just needs to be flashed with a different software. See this repository for the .srec file: https://github.com/dalathegreat/Nissan-env200-Battery-Upgrade/blob/main/leaf-can-bridge-3-port-env200/Debug/canbridge.srec

## Dependencies 📖
This code was made possible with the help of Muxsan and their excellent 3-port hardware. Also massive thanks to my Patreon Glen for introducing me to the 2-port alternative.

## Like this project? 💖
Leave a ⭐ If you think this project is useful. Consider hopping onto my Patreon to encourage more open-source projects!

<a href="https://www.patreon.com/dala">
	<img src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png" width="160">
</a>

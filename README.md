# Nissan-LEAF-Battery-Upgrade
So you are looking to upgrade the battery in your LEAF? You've come to the right place for information and software/hardware required. Take some time to familiarize with the content.

## Read this first 
Start by reading all the terminology and info about what a battery upgrade, direct upgrade, paring, CAN-bridges etc. is by following this link to the wiki: https://github.com/dalathegreat/Nissan-LEAF-Battery-Upgrade/wiki

## Getting the hardware
The BatteryUpgrade software runs on both the Muxsan 3-port CAN-bridge, and on the 2-port Budget CAN-bridge. Here are some purchase links

- 3-port https://www.tindie.com/products/muxsan/can-mitm-bridge-3-port-rev-25/
- 2-port https://www.aliexpress.com/w/wholesale-mb-can-filter-18-in-1.html

## Setting up the software
Depending on what hardware you have, the software flashing will differ.
### 

## Wiring in the hardware
2-port wiring instructions:
· Red wire #1 -> +12V constant (fuse with 3A)
· Red wire #2 -> +12V constant (fuse with 3A, join together with other red wire)
· Purple -> EV-CAN Low, Green wire (battery side)
· Green -> EV-CAN High, Blue wire (battery side)
· Yellow -> EV-CAN Low, Green wire (vehicle side)
· Blue -> EV-CAN High, Blue wire (vehicle side)
· Black -> Ground
· Black -> Ground

## Like this project? 💖
Leave a ⭐ If you think this project is useful. Consider hopping onto my Patreon to encourage more open-source projects!

<a href="https://www.patreon.com/dala">
	<img src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png" width="160">
</a>

# EtherLCD

<img width="621" height="675" alt="EtherLCD" src="https://github.com/user-attachments/assets/a4cf50ed-6602-4fc0-a244-de8400203c2b" />
<br>
EtherLCD is an optional hardware add-on that provides an Ethernet interface (10BASE-T and 100BASE-TX) to the SmartEVSE v3.<br>
It is compatible with all SmartEVSE v3 controllers (S/N 5000 and higher).

# Converting your SmartEVSE controller

## Disassembly
1. *Important!* First update your controller to firmware version v3.10.3 or newer.  
2. Power down, and disconnect all cables. Unplug the green 12 pin plug.  
3. Remove the bottom plate of the controller by pushing the tabs outward.  
<img width="3441" height="2445" alt="EtherLCD_1" src="https://github.com/user-attachments/assets/d140086b-0ec8-4559-b8ad-5226376fc1dd" />
  
4. Take out the mainboard, be carefull the flatcable is still connected to the LCD board.  
<img width="3511" height="2659" alt="EtherLCD_2" src="https://github.com/user-attachments/assets/25d435e2-b060-4b02-8d04-0b6ddb4369cd" />
  
5. Pull out the flatcable from the mainboard.

## Reassembly
1. Take the flatcable from the EtherLCD enclosure and plug it into your mainboard.  
<img width="3320" height="2601" alt="EtherLCD_3" src="https://github.com/user-attachments/assets/953727a9-59a4-4aa4-b944-f0ff387fdcf6" />
  
2. Place the mainboard back into the enclosure.  
3. Refit the bottom plate of the controller. Make sure that the grey clip is at the low voltage side of the controller.  
<img width="4000" height="2475" alt="EtherLCD_4" src="https://github.com/user-attachments/assets/d0e011af-cad7-4c9f-a80e-dc013a9503d8" />
4. Reconnect all cables.

## Using the controller

A few remarks:
- Connecting an Ethernet cable will disable the Wi-Fi connection.
- The Ethernet interface obtains an IP address from a DHCP server, just like the Wi-Fi connection.
- Firmware versions older than V3.10.3 are **not** supported. If you flash an older firmware version, the display will show no output and the buttons will not work.

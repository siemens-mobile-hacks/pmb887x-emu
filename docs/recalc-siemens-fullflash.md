# Why is this needed?
Siemens Mobile is paranoid and all fullflashes have hardware binding to the NOR flash serial number.
You need to recalculate some keys in the fullflash before running it on the emulator.

# How to do this?
This can be done with a few simple steps:

1. Download [x65PapuaUtils V1.1.1b](https://web.archive.org/web/20120711125854/http://forum.allsiemens.com/download.php?id=67331) (works fine in Wine)

2. Go to the "Codes" tab and enter these values:
    - **IMEI:** 490154203237518
    - **ESN:** 12345678
    - **SKEY:** 12345678
    
    ![recalc-flash-0.png](recalc-flash-0.png)
    
3. Press the button "Calc HASH + BootKEY from ESN and SKEY".

4. Go to the "Convert" tab and press the button "Recalc Fullflash". In the opened window, select your input fullflash and output file.
    ![recalc-flash-1.png](recalc-flash-1.png)

5. Done! Now you can use the newly saved fullflash in the emulator.

# Running with original ESN and IMEI
If you know your original IMEI and ESN, you can run the emulator without key recalculation.

Example:
```
pmb887x-emu --siemens-esn=12345678 --siemens-imei=490154203237518
```

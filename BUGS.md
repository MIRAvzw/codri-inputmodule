Major
=====

connect-debounce
----------------

On the EfikaMX, the input modules doesn't work when connected during boot.

Log:
> [   74.739291] hub 2-1:1.0: connect-debounce failed, port 3 disabled  

Reconnecting the thing triggers proper initialization:
> [  108.159552] usb 2-1.3: new low speed USB device using fsl-ehci and address 4  
> [  108.305187] usb 2-1.3: configuration #1 chosen from 1 choice  
> [  108.528750] input: MIRA.be Ad-Astra 3 input module as /devices/platform/fsl-ehci.1/usb2/2-1/2-1.3/2-1.3:1.0/input/input1  
> [  108.540179] generic-usb 0003:16C0:05DC.0001: input: USB HID v1.01 Keyboard [MIRA.be Ad-Astra 3 input module] on usb-fsl-ehci.1-1.3/input0  
> [  108.552659] usbcore: registered new interface driver usbhid  
> [  108.558242] usbhid: v2.6:USB HID core driver  

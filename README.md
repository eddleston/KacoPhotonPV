# KacoPhotonPV
Particle Photon code for reading solar PV values via serial RS232 from a Kaco inverter and writing outputs to PVOutput.org

## Set up instructions  
  
1. Buy a Particle Photon :smile: (https://amzn.to/2Lg97d0)  

[![Photon](https://ws-eu.amazon-adsystem.com/widgets/q?_encoding=UTF8&MarketPlace=GB&ASIN=B012D6UYTA&ServiceVersion=20070822&ID=AsinImage&WS=1&Format=_SL250_&tag=lateralmindsl-21)](https://amzn.to/2Lg97d0)  
  
2. Buy a serial adapter for the Photon (e.g. https://amzn.to/2QMKJFo)  
  
[![Serial Adapter](https://ws-eu.amazon-adsystem.com/widgets/q?_encoding=UTF8&MarketPlace=GB&ASIN=B07DK3874B&ServiceVersion=20070822&ID=AsinImage&WS=1&Format=_SL250_&tag=lateralmindsl-21)](https://amzn.to/2QMKJFo)  
  
3. Use a breadboard to connect the Photon to the serial adapter  
  
![Serial connections](https://community.particle.io/uploads/default/original/2X/2/24ebf4e244c7084e83e1b0b7b43b2281b04fdbf1.jpg)  
  
4. Upload kacoreader.ino to your Photon, via https://build.particle.io  
5. Set up a webhook to post your 5-minute updates to https://pvoutput.org  
  
![PVStatus Webhook](/pvstatus-webhook.png)  
  
## Testing  
To test, un-comment the LOCAL_TESTING=“Test”. This will prevent your webhooks from firing, but let you see what is being posted. You could also set up some different webhooks for these events ("TestSolar_Update", "TestPVStatus").  
  

Note: These are WIP instructions
1. Purchase the M5Stack Core V2.7 Dev Kit (similar M5Stack models may work, but are untested)  
2. Install the Arduino IDE from [here](https://www.arduino.cc/en/software/)
3. Open the Arduino IDE
4. Install the following (with dependancies):
     Board Manager:
     `esp32` by `Espressif Systems`
     Library Manager:
     `M5Unified` by `M5Stack`
     `ESPAsyncWebServer` by `ESP32Async`
     `TJpg_Decoder` by `Bodmer`
5. Plug in your M5Stack Dev Kit (if Arduino IDE doesn't detect your board type, go to Tools > Board > M5Core) 
6. Load `.INO` into IDE  
7. Set WiFi and IP info in `.INO`  
8. Upload onto M5Stack Dev Kit  
9. Navigate to M5Stack IP in a browser  
10. Check "No Sleep" box in web interface  

   <img src="https://github.com/user-attachments/assets/f34641c3-7736-4afc-afc5-8d2557484f17" alt="No Sleep checkbox" width="100px" />

11. Upload `sound1.wav` `sound2.wav` and `config.txt` as well as any .jpg cover art (needs to be 180×242) into web interface  
   Note that you can use the `imageconvert.py` script to help you in this conversion process  

   <img src="https://github.com/user-attachments/assets/4045c072-2c27-4b15-a733-af693d3fdf87" alt="Upload interface" width="200px" />

11. Fill out the rest of the config in the web gui  

   <img src="https://github.com/user-attachments/assets/bee80089-f084-4042-b55e-7bc569ac1bdc" alt="Config GUI" width="700px" />

11. Uncheck the "No Sleep" box  
12. Start up `bridge.py` on the machine that you are running dolphin on  
13. Use the Left and Right buttons on the device to switch between titles, and use the middle button to load them  
    Note: the device is set to go to sleep after 30 seconds, you can wake it up by pressing the power button  

Finished Product:  
<img src="https://github.com/user-attachments/assets/d3ff5e99-c6c9-433e-a7ec-23bbc23a16b5" alt="Finished Product" width="400px" />

Current Amazon Listing (as of June 2025): https://a.co/d/jbjptAN  
<img src="https://github.com/user-attachments/assets/9854b377-71f5-4549-938f-8c1be427becd" alt="Amazon Link" width="500px" />

#include "M5Cardputer.h"
#include "Arduino.h"
#include <SPI.h>
#include <SD.h>

#define SD_SPI_SCK_PIN  40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN   12

#define FRAME_WAIT 33
#define FRAME_SIZE 4050
#define FRAME_HEIGHT 135
#define FRAME_WIDTH 240

bool setup_sd() 
{
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    return SD.begin(SD_SPI_CS_PIN, SPI, 25000000);
}

void memory_info()
{
    size_t freeSRAM = ESP.getFreeHeap();
    size_t freePSRAM = ESP.getFreePsram();

    // Clear the display
    M5Cardputer.Lcd.clear();
    
    // Set text size and color
    M5Cardputer.Lcd.setTextSize(2);
    M5Cardputer.Lcd.setTextColor(TFT_WHITE);

    // Display memory information on the screen
    M5Cardputer.Lcd.setCursor(10, 20);
    M5Cardputer.Lcd.print("Free SRAM: ");
    M5Cardputer.Lcd.print(freeSRAM);
    M5Cardputer.Lcd.println(" bytes");

    M5Cardputer.Lcd.setCursor(10, 60);
    M5Cardputer.Lcd.print("Free PSRAM: ");
    M5Cardputer.Lcd.print(freePSRAM);
    M5Cardputer.Lcd.println(" bytes");
}

void convert_bpp_to_565(uint8_t* bitBuffer, uint16_t* colorBuffer)
{
    for (int y = 0; y < FRAME_HEIGHT; y++) {
        for (int x = 0; x < FRAME_WIDTH; x++) {
            int byteIndex = (y * FRAME_WIDTH + x) / 8;
            int bitIndex = 7 - ((y * FRAME_WIDTH + x) % 8);

            bool isPixelSet = (bitBuffer[byteIndex] >> bitIndex) & 1;
            colorBuffer[y * FRAME_WIDTH + x] = isPixelSet ? 0xFFFF : 0x0000;
        }
    }
}

void drawFramesTask(void *parameter) {
    File frameFile = SD.open("/badapple/frames.bin", FILE_READ);
    uint8_t buffer[FRAME_SIZE];
    uint16_t colorBuffer[FRAME_WIDTH * FRAME_HEIGHT];

    ulong nextFrame = millis() + FRAME_WAIT;
    while (frameFile.available()) 
    {
        int bytes_read = frameFile.read(buffer, FRAME_SIZE);
        convert_bpp_to_565(buffer, colorBuffer);
        //M5Cardputer.Lcd.drawBitmap(0,0, FRAME_WIDTH, FRAME_HEIGHT, colorBuffer);
        M5Cardputer.Lcd.pushImage(0, 0, FRAME_WIDTH, FRAME_HEIGHT, colorBuffer);
        //M5Cardputer.Lcd.draw_bitmap(0, 0, buffer, FRAME_WIDTH, FRAME_HEIGHT, 0x0000, 0xFFFF);
        while (millis() < nextFrame) {}
        nextFrame = millis() + FRAME_WAIT;
        delay(1);
    }

    frameFile.close();
    vTaskDelete(NULL);
}

void setup()
{
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);

    setup_sd();

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.setTextDatum(middle_center);
    M5Cardputer.Display.setTextFont(&fonts::DejaVu9);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.drawString("PRESS ANY KEY", M5Cardputer.Display.width() / 2, M5Cardputer.Display.height() / 2);
    M5Cardputer.Speaker.setVolume(128);

    while (!M5Cardputer.Keyboard.isPressed()) {
        M5Cardputer.update();
        delay(100);
    }

    File audioFile = SD.open("/badapple/audio.pcm");

    // Buffer for reading audio data
    const int16_t halfBufferSize = 256;
    const int16_t bufferSize = halfBufferSize * 2;
    int bytesRead = 0;
    
    uint8_t buffer0[bufferSize * 2];
    uint8_t buffer1[bufferSize * 2];
    uint8_t buffer2[bufferSize * 2];
    
    int16_t* buffer16 = (int16_t*)buffer0;
    bytesRead = audioFile.read(buffer0, bufferSize * 2);
    bytesRead = audioFile.read(buffer1, bufferSize * 2);

    //memory_info();

    xTaskCreate(
        drawFramesTask,
        "DrawFramesTask",
        131071,
        NULL,
        1,
        NULL
    );

    while (audioFile.available()) {
        M5Cardputer.Speaker.playRaw(buffer16, bufferSize, 8000, false, 1, -1, true);
        if (buffer16 == (int16_t*)buffer0) { bytesRead = audioFile.read(buffer2, bufferSize * 2);}
        if (buffer16 == (int16_t*)buffer1) { bytesRead = audioFile.read(buffer0, bufferSize * 2);}
        if (buffer16 == (int16_t*)buffer2) { bytesRead = audioFile.read(buffer1, bufferSize * 2);}
        //
        if (buffer16 == (int16_t*)buffer0) { buffer16 = (int16_t*)buffer1;}
        else if (buffer16 == (int16_t*)buffer1) { buffer16 = (int16_t*)buffer2;}
        else {buffer16 = (int16_t*)buffer0;}
        //
        while (M5Cardputer.Speaker.isPlaying()) {}
    }

    audioFile.close();
}

void loop() {

}
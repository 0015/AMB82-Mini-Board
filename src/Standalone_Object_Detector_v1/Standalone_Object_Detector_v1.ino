/////////////////////////////////////////////////////////////////
/*
  "No WiFi" "Tiny Yolo v7" runs on in a microcontroller! #standalone #vision #MCU #ThatProject
  For More Information: https://youtu.be/TGqOUVhQQY8
  Created by Eric N. (ThatProject)
*/
/////////////////////////////////////////////////////////////////

/*
[1] Board
    > Realtek Ameba Boards(32-bit Arm v8M) 
    > version: 4.0.5

[2] Library
    > **Arduino TJpg_Decoder library** https://github.com/Bodmer/TJpg_Decoder
    > version: 1.1.0
    To draw the JPEG image buffer to the display, we need to decode it.
    Unfortunately, it does not yet support the "Ameba Pro 2" architecture,
    so you need to modify the User_Config.h file for a successful build.
    > open "/src/User_Config.h"
    > remove "#define TJPGD_LOAD_SD_LIBRARY"
*/

#include "SPI.h"
#include "AmebaILI9341.h"

#include <TJpg_Decoder.h>
#include "StreamIO.h"
#include "VideoStream.h"
#include "NNObjectDetection.h"
#include "VideoStreamOverlay.h"
#include "ObjectClassList.h"

#define CHANNELTFT 1
#define CHANNELNN 3

// Lower resolution for NN processing
#define NNWIDTH 576
#define NNHEIGHT 320

// For all supportted boards (AMB21/AMB22, AMB23, BW16/BW16-TypeC, AW-CU488_ThingPlus),
// Select 2 GPIO pins connect to TFT_RESET and TFT_DC. And default SPI_SS/SPI1_SS connect to TFT_CS.
#define TFT_RESET 5
#define TFT_DC 4
#define TFT_CS SPI_SS

AmebaILI9341 tft = AmebaILI9341(TFT_CS, TFT_DC, TFT_RESET);
#define ILI9341_SPI_FREQUENCY 20000000
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
#define TFT_FPS 10

VideoSetting configNN(NNWIDTH, NNHEIGHT, 10, VIDEO_RGB, 0);
VideoSetting configTFT(TFT_WIDTH, TFT_HEIGHT, TFT_FPS, VIDEO_JPEG, 1);
NNObjectDetection ObjDet;
StreamIO videoStreamerNN(1, 1);

uint32_t img_addr = 0;
uint32_t img_len = 0;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  tft.drawBitmap(x, y, w, h, bitmap);
  return 1;
}

void setup() {
  Serial.begin(115200);

  SPI.setDefaultFrequency(ILI9341_SPI_FREQUENCY);

  tft.begin();
  tft.setRotation(1);
  intoText();

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  Camera.configVideoChannel(CHANNELNN, configNN);
  Camera.configVideoChannel(CHANNELTFT, configTFT);
  Camera.videoInit();

  // Configure object detection with corresponding video format information
  // Select Neural Network(NN) task and models
  ObjDet.configVideo(configNN);
  ObjDet.modelSelect(OBJECT_DETECTION, DEFAULT_YOLOV7TINY, NA_MODEL, NA_MODEL);
  ObjDet.begin();

  Camera.channelBegin(CHANNELTFT);

  // Configure StreamIO object to stream data from RGB video channel to object detection
  videoStreamerNN.registerInput(Camera.getStream(CHANNELNN));
  videoStreamerNN.setStackSize();
  videoStreamerNN.setTaskPriority();
  videoStreamerNN.registerOutput(ObjDet);
  if (videoStreamerNN.begin() != 0) {
    Serial.println("StreamIO link start failed");
  }

  Camera.channelBegin(CHANNELNN);
}

void loop() {
  // Note: Handling is required when the JPEG buffer is full
  Camera.getImage(CHANNELTFT, &img_addr, &img_len);
  TJpgDec.drawJpg(0, 0, (const uint8_t*)img_addr, img_len);

  std::vector<ObjectDetectionResult> results = ObjDet.getResult();
  printf("Total number of objects detected = %d\r\n", ObjDet.getResultCount());
  if (ObjDet.getResultCount() > 0) {
    for (uint32_t i = 0; i < ObjDet.getResultCount(); i++) {
      int obj_type = results[i].type();
      if (itemList[obj_type].filter) {  // check if item should be ignored

        ObjectDetectionResult item = results[i];
        // Result coordinates are floats ranging from 0.00 to 1.00
        // Multiply with RTSP resolution to get coordinates in pixels
        int xmin = (int)(item.xMin() * tft.getWidth());
        int xmax = (int)(item.xMax() * tft.getWidth());
        int ymin = (int)(item.yMin() * tft.getHeight());
        int ymax = (int)(item.yMax() * tft.getHeight());

        // Draw boundary box
        tft.drawRectangle(xmin, ymin, xmax, ymax, ILI9341_RED);

        // Print identification text
        char text_str[20];
        snprintf(text_str, sizeof(text_str), "%s %d", itemList[obj_type].objectName, item.score());

        int textLength = strlen(itemList[obj_type].objectName) + String(item.score()).length() + 1;
        if (textLength > 20) {
          textLength = 20;
        }

        for (int i = 0; i < textLength; i++) {
          tft.drawChar(xmin + (i * 12), ymin, text_str[i], ILI9341_GREEN, ILI9341_RED, 2);
        }
      }
    }
  }
}

unsigned long intoText() {
  tft.clr();
  tft.setCursor(0, 30);
  tft.setForeground(ILI9341_GREEN);
  tft.setFontSize(3);
  tft.println(">ThatProject<");
  tft.println();
  tft.setForeground(ILI9341_WHITE);
  tft.setFontSize(4);
  tft.println("Tiny Yolo v7");
  tft.setForeground(ILI9341_YELLOW);
  tft.setFontSize(2);
  tft.println("Pre-trained items: 80");
  tft.println();
  tft.println();
  tft.setForeground(ILI9341_LIGHTGREY);
  tft.setFontSize(1);
  tft.println("Tiny YOLO version 7 does not need to have a large amount of memory and hardware performance but lose some of the detection accuracy.");
  return 0;
}

/*
 * Ham Radio License Study - Question Pool Downloader
 * Automatically downloads JSON files from GitHub when missing
 */

#ifndef TRAINING_LICENSE_DOWNLOADER_H
#define TRAINING_LICENSE_DOWNLOADER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include "../core/config.h"
#include "../storage/sd_card.h"
#include "../audio/i2s_audio.h"  // For beep() function

// GitHub raw URLs for question pool JSON files.
// Pinned to a specific commit: the upstream repo has renamed its default
// branch (master -> main) and restructured directories/filenames before,
// which 404'd every download. A commit SHA can never move underneath us.
#define QUESTION_POOL_BASE_URL "https://raw.githubusercontent.com/russolsen/ham_radio_question_pool/a34fe8456ac76602caa58c1ff02b24476999aa71"
#define TECHNICIAN_URL QUESTION_POOL_BASE_URL "/technician-2026-2030/technician-2026-2030.json"
#define GENERAL_URL QUESTION_POOL_BASE_URL "/general-2023-2027/general-2023-2027.json"
#define EXTRA_URL QUESTION_POOL_BASE_URL "/extra-2024-2028/extra-2024-2028.json"

// Download status
enum DownloadStatus {
  DOWNLOAD_SUCCESS,
  DOWNLOAD_FAILED_NO_WIFI,
  DOWNLOAD_FAILED_HTTP,
  DOWNLOAD_FAILED_SD_CARD,
  DOWNLOAD_FAILED_WRITE
};

/**
 * Check if a question pool file exists on SD card
 */
bool questionFileExists(const char* filename) {
  if (!sdCardAvailable) return false;
  return SD.exists(filename);
}

/**
 * Check if all question pool files exist
 */
bool allQuestionFilesExist() {
  return questionFileExists("/license/technician.json") &&
         questionFileExists("/license/general.json") &&
         questionFileExists("/license/extra.json");
}

/**
 * Download a file from URL to SD card
 */
DownloadStatus downloadFile(const char* url, const char* filepath) {
  HTTPClient http;

  Serial.print("Downloading: ");
  Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int len = http.getSize();

    Serial.print("File size: ");
    Serial.print(len);
    Serial.println(" bytes");

    // Open file for writing
    File file = SD.open(filepath, FILE_WRITE);
    if (!file) {
      Serial.println("ERROR: Failed to open file for writing");
      http.end();
      return DOWNLOAD_FAILED_WRITE;
    }

    // Download in chunks
    WiFiClient* stream = http.getStreamPtr();
    // Heap chunk buffer instead of 1 KB on the loop task stack
    const size_t bufferSize = 1024;
    uint8_t* buffer = (uint8_t*)malloc(bufferSize);
    if (!buffer) {
      file.close();
      http.end();
      return DOWNLOAD_FAILED_WRITE;
    }
    int bytesRead = 0;
    int totalRead = 0;

    while (http.connected() && (len > 0 || len == -1)) {
      size_t size = stream->available();

      if (size) {
        int c = stream->readBytes(buffer, ((size > bufferSize) ? bufferSize : size));
        file.write(buffer, c);
        totalRead += c;

        if (len > 0) {
          len -= c;
        }

        // Print progress every 10KB
        if (totalRead - bytesRead >= 10240) {
          Serial.print("Downloaded: ");
          Serial.print(totalRead / 1024);
          Serial.println(" KB");
          bytesRead = totalRead;
        }
      }
      delay(1);
    }

    free(buffer);
    file.close();
    http.end();

    // A dropped connection leaves a truncated file that would pass the
    // exists() check forever - delete it and report failure instead.
    if (len > 0 || totalRead == 0) {
      Serial.print("ERROR: Incomplete download (");
      Serial.print(totalRead);
      Serial.println(" bytes), removing partial file");
      SD.remove(filepath);
      return DOWNLOAD_FAILED_HTTP;
    }

    Serial.print("Download complete: ");
    Serial.print(totalRead);
    Serial.println(" bytes");

    return DOWNLOAD_SUCCESS;
  } else {
    Serial.print("HTTP error code: ");
    Serial.println(httpCode);
    http.end();
    return DOWNLOAD_FAILED_HTTP;
  }
}

#endif // TRAINING_LICENSE_DOWNLOADER_H

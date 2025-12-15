#pragma once
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include <WString.h>

namespace NFC {
  void init(Adafruit_PN532* nfc);
  String checkTag();
  void resetGuard();

  void tick(unsigned long now,
            bool& isActive,
            unsigned long& lastTagTime,
            bool& tagPresentOut);
}

// Optional: Hooks (werden schwach in nfc.cpp definiert; eigene Implementierung möglich)
void NFC_OnPreempt(const String& uid); // z.B. Display sofort aktualisieren
void NFC_OnActive();                   // z.B. Idle abbrechen

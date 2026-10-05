#include <Preferences.h>
#include "settings.h"
#include "fingerprint.h"

static Preferences gPrefs;
static String      gName;      // "" means never named
static bool        gLoaded = false;

void settingsInit() {
  gPrefs.begin("tealight", false);
  gName   = gPrefs.getString("name", "");
  gLoaded = true;
}

bool settingsHasName() {
  return gLoaded && gName.length() > 0;
}

String settingsName() {
  return settingsHasName() ? gName : getDefaultName();
}

void settingsSetName(const String& name) {
  gName = name;
  gPrefs.putString("name", gName);
}

void settingsClearName() {
  gName = "";
  gPrefs.remove("name");
}

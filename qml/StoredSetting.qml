import QtCore

// One saved choice in ~/.config/legato/settings.conf: give it a category and one alias.
//
// One alias per object, never more: when Qt's Settings writes a key missing from the file as it
// loads, it records every alias's value at that moment, and the aliases after it are not loaded yet,
// so their defaults overwrite the saved choices (Qt 6.12). A new key in an update reset the choices
// listed after it.
Settings {
    location: StandardPaths.writableLocation(StandardPaths.AppConfigLocation) + "/settings.conf"
}

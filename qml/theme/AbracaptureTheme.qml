pragma Singleton

import QtQuick

QtObject {
    id: theme

    // These properties are intentionally writable so the shell can change
    // appearance and accessibility settings without restarting the app.
    property bool darkMode: true
    property bool highContrast: false
    property real fontScale: 1.0
    property alias fontScaleFactor: theme.fontScale
    property alias highContrastMode: theme.highContrast

    readonly property color primary: highContrast
                                          ? (darkMode ? "#7dd3fc" : "#075985")
                                          : (darkMode ? "#38bdf8" : "#0369a1")
    readonly property color secondary: highContrast
                                            ? (darkMode ? "#f0abfc" : "#86198f")
                                            : (darkMode ? "#c084fc" : "#7e22ce")
    readonly property color background: highContrast
                                            ? (darkMode ? "#000000" : "#ffffff")
                                            : (darkMode ? "#0b1120" : "#f8fafc")
    readonly property color surface: highContrast
                                        ? (darkMode ? "#111111" : "#ffffff")
                                        : (darkMode ? "#111827" : "#ffffff")
    readonly property color surfaceElevated: highContrast
                                                   ? (darkMode ? "#1a1a1a" : "#ffffff")
                                                   : (darkMode ? "#161920" : "#ffffff")
    readonly property color controlBackground: highContrast
                                                     ? (darkMode ? "#202020" : "#ffffff")
                                                     : (darkMode ? "#0f172a" : "#ffffff")
    readonly property color controlBackgroundHover: highContrast
                                                          ? (darkMode ? "#303030" : "#f1f5f9")
                                                          : (darkMode ? "#1e293b" : "#f1f5f9")
    readonly property color border: highContrast
                                        ? (darkMode ? "#ffffff" : "#0f172a")
                                        : (darkMode ? "#334155" : "#cbd5e1")
    readonly property color divider: highContrast
                                         ? (darkMode ? "#ffffff" : "#94a3b8")
                                         : (darkMode ? "#242933" : "#e2e8f0")
    readonly property color text: highContrast
                                      ? (darkMode ? "#ffffff" : "#000000")
                                      : (darkMode ? "#f8fafc" : "#0f172a")
    readonly property color textSecondary: highContrast
                                               ? (darkMode ? "#ffffff" : "#111111")
                                               : (darkMode ? "#cbd5e1" : "#475569")
    readonly property color error: highContrast ? "#ff6b6b" : "#dc2626"
    readonly property color success: highContrast ? "#00ff85" : (darkMode ? "#34d399" : "#166534")
    readonly property color warning: highContrast ? "#ffff00" : (darkMode ? "#fbbf24" : "#a16207")
    readonly property color disabledText: darkMode ? "#64748b" : "#94a3b8"
    readonly property color controlForeground: text

    // capture-active / capture-idle indicators use dedicated backgrounds.
    // Both foreground/background pairs exceed the WCAG AA 4.5:1 ratio.
    readonly property color captureActive: highContrast
                                               ? "#00ff85"
                                               : (darkMode ? "#34d399" : "#166534")
    readonly property color captureActiveBackground: highContrast
                                                         ? "#003d20"
                                                         : (darkMode ? "#052e16" : "#dcfce7")
    readonly property color captureIdle: highContrast
                                             ? "#ffffff"
                                             : (darkMode ? "#e2e8f0" : "#334155")
    readonly property color captureIdleBackground: highContrast
                                                       ? "#202020"
                                                       : (darkMode ? "#1e293b" : "#f1f5f9")

    readonly property real fontSizeCaption: 11 * fontScale
    readonly property real fontSizeBody: 14 * fontScale
    readonly property real fontSizeSubtitle: 18 * fontScale
    readonly property real fontSizeTitle: 26 * fontScale

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12
    readonly property int spacingLg: 20
    readonly property int spacingXl: 28
}

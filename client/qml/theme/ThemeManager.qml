pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    // Current theme
    readonly property var current: isDark ? dark : light

    // Dark theme colors
    readonly property var dark: ({
            "bg": "#1e1e2e",
            "sidebarBg": "#181825",
            "sidebarBorder": "#313244",
            "sidebarText": "#cdd6f4",
            "sidebarTextActive": "#3874F7",
            "sidebarHover": "#313244",
            "sidebarSelected": "#313244",
            "pageBg": "#1e1e2e",
            "cardBg": "#313244",
            "cardBorder": "#45475a",
            "surface": "#1e1e2e",
            "surfaceAlt": "#2a2a3c",
            "border": "#45475a",
            "text": "#cdd6f4",
            "textMuted": "#a6adc8",
            "textFaint": "#585b70",
            "accent": "#3874F7",
            "accentText": "#1e1e2e",
            "accentSoft": "#313244",
            "chipBg": "#45475a",
            "chipText": "#cdd6f4",
            "statusBarBg": "#11111b",
            "statusBarBorder": "#313244",
            "statusBarText": "#a6adc8",
            "danger": "#f38ba8",
            "warning": "#f9e2af",
            "success": "#4CAF50",
            "info": "#3874F7",
            "tooltipBg": "#45475a",
            "tooltipText": "#cdd6f4"
        })
    property bool isDark: true

    // Light theme colors
    readonly property var light: ({
            "bg": "#f5f5f5",
            "sidebarBg": "#ffffff",
            "sidebarBorder": "#e0e0e0",
            "sidebarText": "#1e1e2e",
            "sidebarTextActive": "#3b82f6",
            "sidebarHover": "#f0f0f0",
            "sidebarSelected": "#e8f0fe",
            "pageBg": "#f5f5f5",
            "cardBg": "#ffffff",
            "cardBorder": "#e0e0e0",
            "surface": "#ffffff",
            "surfaceAlt": "#f8f9fa",
            "border": "#e0e0e0",
            "text": "#1e1e2e",
            "textMuted": "#666666",
            "textFaint": "#999999",
            "accent": "#3b82f6",
            "accentText": "#ffffff",
            "accentSoft": "#e8f0fe",
            "chipBg": "#e0e0e0",
            "chipText": "#1e1e2e",
            "statusBarBg": "#e0e0e0",
            "statusBarBorder": "#d0d0d0",
            "statusBarText": "#666666",
            "danger": "#dc2626",
            "warning": "#f59e0b",
            "success": "#16a34a",
            "info": "#2563eb",
            "tooltipBg": "#1e1e2e",
            "tooltipText": "#ffffff"
        })

    function setDark(dark) {
        isDark = dark;
    }
    function toggle() {
        isDark = !isDark;
    }
}

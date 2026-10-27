import QtQuick
import Quickshell
import Quickshell.Io
import qs.Commons
import qs.Ui

// Read-only EVO-X2 power mode indicator. Polls pmode-status every 2s;
// never writes anything. Left click: notification with details.
BarWidget {
  id: root
  moduleName: "aarshiv.pmode"

  property string mode: ""
  property var info: ({})
  property bool primed: false

  readonly property var modes: ({
    "performance": { icon: "󰓅", label: "Perf",  limit: "~120 W sustained, ~140 W boost" },
    "balanced":    { icon: "󰾅", label: "Bal",   limit: "~85 W sustained, ~120 W boost" },
    "quiet":       { icon: "󰾆", label: "Quiet", limit: "~54 W sustained" }
  })
  readonly property var current: modes[mode] || { icon: "󰾆", label: "?", limit: "unknown" }

  readonly property string tooltip: mode === "" ? "P-mode: driver not loaded"
    : "P-mode: " + mode + " (" + current.limit + ")\n"
      + "CPU fans: " + info.fan1 + " / " + info.fan2 + " rpm\n"
      + "System fan: " + info.fan3 + " rpm\n"
      + "EC CPU temp: " + info.temp + " °C\n"
      + "Change with the physical P-mode button"

  implicitWidth: button.implicitWidth
  implicitHeight: button.implicitHeight

  Process {
    id: statusProc
    command: ["pmode-status", "--json"]
    stdout: StdioCollector {
      waitForEnd: true
      onStreamFinished: {
        var d
        try {
          d = JSON.parse(text)
        } catch (e) {
          root.mode = ""
          return
        }
        var changed = root.primed && d.mode !== root.mode
        root.info = d
        root.mode = (d.mode === "unavailable") ? "" : d.mode
        root.primed = true
        // OSD is best-effort; a failure here must never blank the indicator
        if (changed && root.bar && root.mode !== "") {
          try {
            root.bar.run("omarchy-osd -i " + Util.shellQuote(root.current.icon)
                         + " -m " + Util.shellQuote("P-mode: " + root.mode))
          } catch (e) {
            console.warn("aarshiv.pmode: OSD failed: " + e)
          }
        }
      }
    }
  }

  Timer {
    interval: 2000
    running: true
    repeat: true
    triggeredOnStart: true
    onTriggered: if (!statusProc.running) statusProc.running = true
  }

  // WidgetButton sizes to its text (BarIconButton is a fixed icon-sized slot)
  WidgetButton {
    id: button
    anchors.fill: parent
    bar: root.bar
    text: root.mode === "" ? "󰾆 –" : root.current.icon + " " + root.current.label
    tooltipText: root.tooltip

    onPressed: function(b) {
      if (root.bar) root.bar.run("omarchy-notification-send \"$(pmode-status)\"")
    }
  }
}

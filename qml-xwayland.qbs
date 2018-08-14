import qbs 1.0

Project {
    name: "XWayland"

    readonly property string version: "0.0.0"

    readonly property string minimumQtVersion: "5.9.0"

    property bool useStaticAnalyzer: false
    property bool autotestEnabled: false
    property stringList autotestArguments: []
    property stringList autotestWrapper: []

    condition: qbs.targetOS.contains("linux") && !qbs.targetOS.contains("android")

    minimumQbsVersion: "1.8.0"

    references: [
        "src/xwayland/xwayland.qbs",
    ]
}

// MapController: the Navigation Controller of this POC.
//
// Decides what the map shows. It keeps the view (centre and zoom), works out
// which tiles are visible and where they go on screen, keeps the robot's
// position and its trail (in memory only), and follows the robot.
//
// Coordinates handed to QML are small numbers relative to an "origin": the
// top-left corner of the top-left visible tile. QML puts everything (tiles,
// trail, robot) inside one layer item at (layerX, layerY), so panning only
// moves that layer; tiles are rebuilt only when a new row or column of tiles
// comes into view. Keeping the numbers small avoids float precision loss in
// the scene graph at high zoom (the world is 67 million pixels wide at z18).
#pragma once

#include <vector>

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVariantList>

class MapController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int zoom READ zoom NOTIFY viewChanged)
    Q_PROPERTY(int minZoom READ minZoom CONSTANT)
    Q_PROPERTY(int maxZoom READ maxZoom CONSTANT)
    Q_PROPERTY(double layerX READ layerX NOTIFY viewChanged)
    Q_PROPERTY(double layerY READ layerY NOTIFY viewChanged)
    Q_PROPERTY(double centerLat READ centerLat NOTIFY viewChanged)
    Q_PROPERTY(double centerLon READ centerLon NOTIFY viewChanged)
    Q_PROPERTY(double scaleBarPixels READ scaleBarPixels NOTIFY viewChanged)
    Q_PROPERTY(QString scaleBarLabel READ scaleBarLabel NOTIFY viewChanged)
    Q_PROPERTY(QVariantList tiles READ tiles NOTIFY tilesChanged)
    Q_PROPERTY(bool hasRobot READ hasRobot NOTIFY robotChanged)
    Q_PROPERTY(bool robotHasFix READ robotHasFix NOTIFY robotChanged)
    Q_PROPERTY(double robotX READ robotX NOTIFY robotChanged)
    Q_PROPERTY(double robotY READ robotY NOTIFY robotChanged)
    Q_PROPERTY(double robotHeading READ robotHeading NOTIFY robotChanged)
    Q_PROPERTY(bool headingValid READ headingValid NOTIFY robotChanged)
    Q_PROPERTY(QVariantList trail READ trail NOTIFY trailChanged)
    Q_PROPERTY(int trailPoints READ trailPoints NOTIFY trailChanged)
    Q_PROPERTY(double trailLengthM READ trailLengthM NOTIFY trailChanged)
    Q_PROPERTY(bool follow READ follow WRITE setFollow NOTIFY followChanged)

public:
    MapController(int minZoom, int maxZoom, int startZoom,
                  double startLat, double startLon, QObject *parent = nullptr);

    int zoom() const { return m_zoom; }
    int minZoom() const { return m_minZoom; }
    int maxZoom() const { return m_maxZoom; }
    double layerX() const { return m_layerX; }
    double layerY() const { return m_layerY; }
    double centerLat() const;
    double centerLon() const;
    double scaleBarPixels() const { return m_scalePx; }
    QString scaleBarLabel() const { return m_scaleLabel; }
    QVariantList tiles() const { return m_tiles; }
    bool hasRobot() const { return m_hasRobot; }
    bool robotHasFix() const { return m_robotHasFix; }
    double robotX() const { return m_robotX; }
    double robotY() const { return m_robotY; }
    double robotHeading() const { return m_heading; }
    bool headingValid() const { return m_headingValid; }
    QVariantList trail() const { return m_trailScreen; }
    int trailPoints() const { return static_cast<int>(m_trail.size()); }
    double trailLengthM() const { return m_trailLengthM; }
    bool follow() const { return m_follow; }
    void setFollow(bool follow);

    // Called by QML.
    Q_INVOKABLE void setViewportSize(double width, double height);
    Q_INVOKABLE void pan(double dx, double dy);                 // drag by dx, dy screen pixels
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomAt(int steps, double screenX, double screenY); // keep the point under the cursor fixed
    Q_INVOKABLE void recenter();                                 // centre on the robot and follow it again
    Q_INVOKABLE void clearTrail();

public Q_SLOTS:
    // New position from the TelemetryController.
    void onPosition(double lat, double lon, double headingDeg, bool headingValid, bool hasFix);

Q_SIGNALS:
    void viewChanged();
    void tilesChanged();
    void robotChanged();
    void trailChanged();
    void followChanged();

private:
    struct GeoPoint { double lat; double lon; };

    void setZoom(int zoom, double anchorX, double anchorY);
    void updateView(bool forceTiles = false);
    void updateRobotScreen();
    void updateTrailScreen();
    void updateScaleBar();

    int m_minZoom;
    int m_maxZoom;
    int m_zoom;

    double m_viewW = 0.0;
    double m_viewH = 0.0;
    double m_centerX = 0.0;   // view centre in world pixels at m_zoom
    double m_centerY = 0.0;

    // Visible tile range and the origin of the QML layer (world pixels).
    int m_tileZoom = -1;
    int m_tx0 = 0, m_ty0 = 0, m_tx1 = -1, m_ty1 = -1;
    double m_originX = 0.0;
    double m_originY = 0.0;
    double m_layerX = 0.0;
    double m_layerY = 0.0;
    QVariantList m_tiles;

    double m_scalePx = 0.0;
    QString m_scaleLabel;

    // Robot (memory only)
    bool m_hasRobot = false;
    bool m_robotHasFix = false;
    GeoPoint m_robot{0.0, 0.0};
    double m_heading = 0.0;
    bool m_headingValid = false;
    double m_robotX = 0.0;
    double m_robotY = 0.0;
    bool m_follow = true;

    // Trail since the app started (memory only, not stored - HLD 5.5)
    std::vector<GeoPoint> m_trail;
    double m_trailLengthM = 0.0;
    QVariantList m_trailScreen;
};

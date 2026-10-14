// TelemetryController: the latest GPS telemetry, as properties QML can show.
//
// Receives TelemetrySample objects on the UI thread (posted by RosBridge),
// keeps the latest values, measures the message rate and the age of the last
// message, works out the heading from the direction of travel, and tells the
// MapController about every new position. Nothing here is stored: telemetry is
// shown live only (HLD Section 5.3).
#pragma once

#include <deque>

#include <QObject>
#include <QString>
#include <QTimer>

#include "TelemetrySample.h"

class TelemetryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasData READ hasData NOTIFY sampleChanged)
    Q_PROPERTY(QString frameId READ frameId NOTIFY sampleChanged)
    Q_PROPERTY(double latitude READ latitude NOTIFY sampleChanged)
    Q_PROPERTY(double longitude READ longitude NOTIFY sampleChanged)
    Q_PROPERTY(double altitude READ altitude NOTIFY sampleChanged)
    Q_PROPERTY(bool hasFix READ hasFix NOTIFY sampleChanged)
    Q_PROPERTY(double accuracyM READ accuracyM NOTIFY sampleChanged)
    Q_PROPERTY(double heading READ heading NOTIFY sampleChanged)
    Q_PROPERTY(bool headingValid READ headingValid NOTIFY sampleChanged)
    Q_PROPERTY(QString lastUpdateText READ lastUpdateText NOTIFY sampleChanged)
    Q_PROPERTY(double messageCount READ messageCount NOTIFY sampleChanged)
    Q_PROPERTY(double ageSeconds READ ageSeconds NOTIFY statsChanged)
    Q_PROPERTY(double rateHz READ rateHz NOTIFY statsChanged)
    Q_PROPERTY(bool live READ live NOTIFY statsChanged)

public:
    explicit TelemetryController(QObject *parent = nullptr);

    bool hasData() const { return m_hasData; }
    QString frameId() const { return m_last.frameId; }
    double latitude() const { return m_last.latitude; }
    double longitude() const { return m_last.longitude; }
    double altitude() const { return m_last.altitude; }
    bool hasFix() const { return m_hasFix; }
    double accuracyM() const { return m_last.horizontalAccuracyM; }
    double heading() const { return m_headingValid ? m_heading : 0.0; }
    bool headingValid() const { return m_headingValid; }
    QString lastUpdateText() const;
    double messageCount() const { return double(m_count); }
    double ageSeconds() const;
    double rateHz() const { return m_rateHz; }
    bool live() const;

    // Called on the UI thread for every message.
    void onSample(const TelemetrySample &sample);

Q_SIGNALS:
    void sampleChanged();
    void statsChanged();
    // A new position for the map. hasFix is false when the GPS has no fix.
    void positionUpdated(double lat, double lon, double headingDeg, bool headingValid, bool hasFix);

private:
    void updateStats();

    TelemetrySample m_last;
    bool m_hasData = false;
    bool m_hasFix = false;

    // Heading from the direction of travel
    bool m_hasHeadingRef = false;
    double m_refLat = 0.0, m_refLon = 0.0;
    double m_heading = 0.0;
    bool m_headingValid = false;

    quint64 m_count = 0;
    std::deque<qint64> m_arrivals;  // arrival times (ms) in the last second
    double m_rateHz = 0.0;
    QTimer m_statsTimer;            // refreshes age and rate 5 times a second
};

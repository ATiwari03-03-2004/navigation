#include "TelemetryController.h"

#include <cmath>

#include <QDateTime>

namespace {
constexpr qint64 kLiveAgeMs = 1000;   // older than this = stale (HLD: link lost after 1 s)
constexpr int kNavSatStatusFix = 0;   // sensor_msgs/NavSatStatus::STATUS_FIX
}

TelemetryController::TelemetryController(QObject *parent)
    : QObject(parent)
{
    m_statsTimer.setInterval(200);
    connect(&m_statsTimer, &QTimer::timeout, this, &TelemetryController::updateStats);
    m_statsTimer.start();
}

void TelemetryController::onSample(const TelemetrySample &s)
{
    // Sequence gaps = messages lost on the link. A jump backwards means the
    // publisher restarted: start counting again.
    if (m_hasData && s.seq > m_last.seq + 1)
        m_lost += s.seq - m_last.seq - 1;

    m_last = s;
    m_hasData = true;
    ++m_count;
    m_arrivals.push_back(s.receivedMs);

    // A position is usable only with a fix and real numbers. 0,0 is treated as
    // "no fix" too: some GPS drivers send zeros before the first fix.
    const bool numbersOk = std::isfinite(s.latitude) && std::isfinite(s.longitude)
                           && !(s.latitude == 0.0 && s.longitude == 0.0)
                           && std::abs(s.latitude) <= 90.0 && std::abs(s.longitude) <= 180.0;
    m_hasFix = numbersOk && s.fixStatus >= kNavSatStatusFix;
    m_headingValid = std::isfinite(s.headingDeg);

    Q_EMIT sampleChanged();
    Q_EMIT positionUpdated(s.latitude, s.longitude, s.headingDeg, m_headingValid, m_hasFix);
    updateStats();
}

QString TelemetryController::lastUpdateText() const
{
    if (!m_hasData)
        return QStringLiteral("—");
    return QDateTime::fromMSecsSinceEpoch(m_last.receivedMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

double TelemetryController::ageSeconds() const
{
    if (!m_hasData)
        return -1.0;
    return (QDateTime::currentMSecsSinceEpoch() - m_last.receivedMs) / 1000.0;
}

bool TelemetryController::live() const
{
    return m_hasData && (QDateTime::currentMSecsSinceEpoch() - m_last.receivedMs) <= kLiveAgeMs;
}

void TelemetryController::updateStats()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    while (!m_arrivals.empty() && now - m_arrivals.front() > 1000)
        m_arrivals.pop_front();
    m_rateHz = static_cast<double>(m_arrivals.size());
    Q_EMIT statsChanged();
}

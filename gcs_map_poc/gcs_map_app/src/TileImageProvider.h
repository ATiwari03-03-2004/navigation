// TileImageProvider: lets a QML Image load a map tile from the TileStore.
//
// QML asks for "image://maptiles/<z>/<x>/<y>". Each request runs on a worker
// thread from the provider's own pool, so reading SQLite and decoding PNG/JPEG
// never blocks the user interface. The view never touches the database itself:
// it only knows the URL (HLD rules R1 and R4).
#pragma once

#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QQuickImageResponse>
#include <QRunnable>
#include <QThreadPool>

class TileStore;

class TileImageResponse : public QQuickImageResponse, public QRunnable
{
public:
    TileImageResponse(const TileStore *store, int z, int x, int y);

    QQuickTextureFactory *textureFactory() const override;
    void run() override;   // worker thread

private:
    const TileStore *m_store;
    int m_z, m_x, m_y;
    QImage m_image;
};

class TileImageProvider : public QQuickAsyncImageProvider
{
public:
    explicit TileImageProvider(const TileStore *store);
    ~TileImageProvider() override;

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    const TileStore *m_store;
    QThreadPool m_pool;
};

#include "TileImageProvider.h"

#include <QStringList>

#include "TileStore.h"

TileImageResponse::TileImageResponse(const TileStore *store, int z, int x, int y)
    : m_store(store), m_z(z), m_x(x), m_y(y)
{
    // The QML engine deletes the response after finished(); the pool must not.
    setAutoDelete(false);
}

QQuickTextureFactory *TileImageResponse::textureFactory() const
{
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void TileImageResponse::run()
{
    m_image = m_store->loadTile(m_z, m_x, m_y);
    Q_EMIT finished();
}

TileImageProvider::TileImageProvider(const TileStore *store)
    : m_store(store)
{
    // A few threads are enough: SQLite reads of one indexed row are fast,
    // and most of the time goes into decoding the image.
    m_pool.setMaxThreadCount(4);
}

TileImageProvider::~TileImageProvider()
{
    m_pool.waitForDone();
}

QQuickImageResponse *TileImageProvider::requestImageResponse(const QString &id, const QSize &)
{
    // id is "z/x/y"
    const QStringList parts = id.split(QLatin1Char('/'));
    int z = -1, x = -1, y = -1;
    if (parts.size() == 3) {
        z = parts[0].toInt();
        x = parts[1].toInt();
        y = parts[2].toInt();
    }
    auto *response = new TileImageResponse(m_store, z, x, y);
    m_pool.start(response);
    return response;
}

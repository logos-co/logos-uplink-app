#include "devnet/DevnetNodeService.h"

#include <QNetworkReply>
#include <QNetworkRequest>

#include "devnet/DevnetNodeJson.h"

namespace {

constexpr int kPollMs = 2000;
constexpr int kTimeoutMs = 3000;

} // namespace

DevnetNodeService::DevnetNodeService(const QUrl& api, const QString& nodeId, QObject* parent)
    : QObject(parent)
    , m_api(api)
    , m_nodeId(nodeId.trimmed().toLower())
    , m_status(devnet::unreachable(QStringLiteral("Waiting for the devnet node at %1").arg(api.toString())))
{
    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, &DevnetNodeService::poll);
    m_timer.start();
    QTimer::singleShot(0, this, &DevnetNodeService::poll);
}

// One round: info, then chain id and blend info, as blockchain_module's status reads them.
void DevnetNodeService::poll()
{
    if (m_polling)
        return;
    m_polling = true;
    get(QStringLiteral("cryptarchia/info"), [this](const QByteArray& info) {
        m_info = info;
        get(QStringLiteral("chain/id"), [this](const QByteArray& chain) {
            m_chain = chain;
            get(QStringLiteral("blend/info"), [this](const QByteArray& blend) {
                m_status = devnet::status(m_info, m_chain, blend, m_nodeId);
                m_polling = false;
            });
        });
    });
}

void DevnetNodeService::get(const QString& path, std::function<void(const QByteArray&)> then)
{
    QUrl url = m_api;
    url.setPath(url.path().endsWith(QLatin1Char('/')) ? url.path() + path : url.path() + QLatin1Char('/') + path);
    QNetworkRequest request(url);
    request.setTransferTimeout(kTimeoutMs);
    QNetworkReply* reply = m_net.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, then = std::move(then)]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(QStringLiteral("No answer from the devnet node at %1: %2").arg(url.toString(), reply->errorString()));
            return;
        }
        then(reply->readAll());
    });
}

void DevnetNodeService::fail(const QString& error)
{
    m_status = devnet::unreachable(error);
    m_polling = false;
}

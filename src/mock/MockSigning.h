#pragma once

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

// What the mock node app answers for the referral intents, and what the mock programme
// accepts: a mock "signature" (a hash of the node, the intent and its values) and a mock
// receipt secret, per node. Both sides compute them here, so an answer for another node,
// another request or other values is refused, as the real module refuses them.
namespace mock_signing {

inline const QString kEnrolIntent = QStringLiteral("node.sign_referral_enrolment");
inline const QString kPayoutIntent = QStringLiteral("node.sign_referral_payout");
inline const QString kReceiptSecretIntent = QStringLiteral("node.referral_receipt_secret");

inline QString signature(const QString& node, const QString& intent, const QVariantMap& params)
{
    const QByteArray message = node.toUtf8() + '\0' + intent.toUtf8() + '\0'
        + QJsonDocument(QJsonObject::fromVariantMap(params)).toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(message, QCryptographicHash::Sha512).toHex());
}

inline QString receiptSecret(const QString& node)
{
    return QString::fromLatin1(QCryptographicHash::hash("mock-receipt-secret:" + node.toUtf8(),
                                                        QCryptographicHash::Sha256).toHex());
}

} // namespace mock_signing

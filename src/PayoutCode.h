#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "interfaces/ReferralService.h"

// The code a user pastes into the payout form: "uplink-payout-v2:" + base64url(JSON of the
// receipt's opening — program account, node, blinding factor — and the node's signature
// over that opening). The form's side recomputes the receipt address from the opening,
// reads the points, and checks the signature against `node`: a code copied by someone
// else is useless without the node that earned it. "uplink-payout:" was the unsigned v1.
namespace payout_code {

inline const QString kPrefix = QStringLiteral("uplink-payout-v2:");

// What the node signs: the opening, as compact JSON with sorted keys.
inline QByteArray signedPayload(const referral::Opening& opening)
{
    const QJsonObject o{
        {QStringLiteral("program_account"), opening.programAccount},
        {QStringLiteral("node"), opening.node},
        {QStringLiteral("blinding_factor"), opening.blindingFactor},
    };
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

inline QString encode(const referral::Opening& opening, const QString& signatureHex)
{
    QJsonObject o = QJsonDocument::fromJson(signedPayload(opening)).object();
    o.insert(QStringLiteral("signature"), signatureHex);
    return kPrefix + QString::fromLatin1(QJsonDocument(o).toJson(QJsonDocument::Compact)
                                             .toBase64(QByteArray::Base64UrlEncoding
                                                       | QByteArray::OmitTrailingEquals));
}

inline QJsonObject fields(const QString& code)
{
    const QString trimmed = code.trimmed();
    if (!trimmed.startsWith(kPrefix))
        return {};
    const auto bytes = QByteArray::fromBase64Encoding(trimmed.mid(kPrefix.size()).toLatin1(),
                                                      QByteArray::Base64UrlEncoding
                                                          | QByteArray::AbortOnBase64DecodingErrors);
    return bytes ? QJsonDocument::fromJson(*bytes).object() : QJsonObject();
}

inline QString signature(const QString& code)
{
    return fields(code).value(QStringLiteral("signature")).toString();
}

// The opening in a code; empty fields if `code` isn't one.
inline referral::Opening decode(const QString& code)
{
    referral::Opening opening;
    const QString trimmed = code.trimmed();
    if (!trimmed.startsWith(kPrefix))
        return opening;
    const auto bytes = QByteArray::fromBase64Encoding(trimmed.mid(kPrefix.size()).toLatin1(),
                                                      QByteArray::Base64UrlEncoding
                                                          | QByteArray::AbortOnBase64DecodingErrors);
    if (!bytes)
        return opening;
    const QJsonObject o = QJsonDocument::fromJson(*bytes).object();
    opening.programAccount = o.value(QStringLiteral("program_account")).toString();
    opening.node = o.value(QStringLiteral("node")).toString();
    opening.blindingFactor = o.value(QStringLiteral("blinding_factor")).toString();
    return opening;
}

} // namespace payout_code

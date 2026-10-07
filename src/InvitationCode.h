#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QString>

// The shareable form of an invitation: "uplink-invite:" + base64url(JSON blob).
// The blob is what lez_core hands out and takes back ({parent_node, npk, vpk},
// logos-execution-zone#896's Invitation); the wrapping keeps it one token that
// survives chats and a wrong paste fails cleanly. A format change gets a new prefix.
namespace invitation_code {

inline const QString kPrefix = QStringLiteral("uplink-invite:");

inline QString encode(const QString& blob)
{
    return kPrefix + QString::fromLatin1(blob.toUtf8().toBase64(
                         QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

// The blob inside a code, or "" if `code` isn't one.
inline QString decode(const QString& code)
{
    const QString trimmed = code.trimmed();
    if (!trimmed.startsWith(kPrefix))
        return {};
    const auto bytes = QByteArray::fromBase64Encoding(trimmed.mid(kPrefix.size()).toLatin1(),
                                                      QByteArray::Base64UrlEncoding
                                                          | QByteArray::AbortOnBase64DecodingErrors);
    return bytes ? QString::fromUtf8(*bytes) : QString();
}

// The inviter's node from a blob, or "" if the blob isn't a well-formed invitation.
inline QString inviterNode(const QString& blob)
{
    const QJsonObject o = QJsonDocument::fromJson(blob.toUtf8()).object();
    static const QRegularExpression hex32(QStringLiteral("^[0-9a-f]{64}$"));
    const QString node = o.value(QStringLiteral("parent_node")).toString();
    if (!hex32.match(node).hasMatch()
            || o.value(QStringLiteral("npk")).toString().isEmpty()
            || o.value(QStringLiteral("vpk")).toString().isEmpty())
        return {};
    return node;
}

} // namespace invitation_code

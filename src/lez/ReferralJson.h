#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "interfaces/ReferralService.h"

// uplink_module's replies, read into ReferralService's types. Every reply is
// {"ok": true, …} or {"ok": false, "code", "error"}. Kept apart from LezReferralService
// so tests can check them without the SDK.
namespace referral_json {

inline Result<QJsonObject> reply(const QString& json)
{
    if (json.isEmpty())
        return Result<QJsonObject>::failure(QStringLiteral("uplink_module did not answer"));
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    if (o.value(QStringLiteral("ok")).toBool())
        return Result<QJsonObject>::success(o);
    const QString error = o.value(QStringLiteral("error")).toString();
    return Result<QJsonObject>::failure(error.isEmpty() ? QStringLiteral("uplink_module: unexpected answer") : error);
}

// The check_program verdict; other failures (lez_core down, …) stay errors.
inline Result<referral::ProgramCheck> programCheck(const QString& json)
{
    const auto r = reply(json);
    if (r.ok())
        return Result<referral::ProgramCheck>::success({referral::ProgramCheck::Ready, {}});
    const QString code = QJsonDocument::fromJson(json.toUtf8()).object().value(QStringLiteral("code")).toString();
    static const QMap<QString, referral::ProgramCheck::State> states = {
        {QStringLiteral("not_configured"), referral::ProgramCheck::NotConfigured},
        {QStringLiteral("program_not_deployed"), referral::ProgramCheck::NotDeployed},
        {QStringLiteral("program_mismatch"), referral::ProgramCheck::Mismatch},
    };
    if (!states.contains(code))
        return Result<referral::ProgramCheck>::failure(r.error);
    return Result<referral::ProgramCheck>::success({states.value(code), r.error});
}

// One field of a successful reply.
inline Result<QString> field(const QString& json, const char* name)
{
    const auto r = reply(json);
    return r.ok() ? Result<QString>::success(r.value.value(QLatin1String(name)).toString())
                  : Result<QString>::failure(r.error);
}

inline Result<bool> flag(const QString& json, const char* name)
{
    const auto r = reply(json);
    return r.ok() ? Result<bool>::success(r.value.value(QLatin1String(name)).toBool())
                  : Result<bool>::failure(r.error);
}

// An operation: submitting / pending → Pending, settled → Settled, failed → its error.
inline Result<referral::Status> operationStatus(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<referral::Status>::failure(r.error);
    const QString status = r.value.value(QStringLiteral("status")).toString();
    if (status == QLatin1String("settled"))
        return Result<referral::Status>::success(referral::Status::Settled);
    if (status == QLatin1String("failed")) {
        const QString error = r.value.value(QStringLiteral("error")).toString();
        return error.isEmpty() ? Result<referral::Status>::success(referral::Status::Rejected)
                               : Result<referral::Status>::failure(error);
    }
    return Result<referral::Status>::success(referral::Status::Pending);
}

// A status check: as operationStatus, except that a check that couldn't be made (uplink_module
// or lez_core didn't answer in time) leaves the operation pending, to be checked again. Only
// the module not knowing the reference ends it.
inline Result<referral::Status> operationCheck(const QString& json)
{
    const auto r = reply(json);
    if (r.ok())
        return operationStatus(json);
    const QString code = QJsonDocument::fromJson(json.toUtf8()).object().value(QStringLiteral("code")).toString();
    if (code == QLatin1String("unknown_reference"))
        return Result<referral::Status>::failure(r.error);
    return Result<referral::Status>::success(referral::Status::Pending);
}

// The points claimable now, and the published epochs that aren't counted yet.
inline Result<referral::Claimable> claimable(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<referral::Claimable>::failure(r.error);
    referral::Claimable c;
    c.points = r.value.value(QStringLiteral("points")).toString(QStringLiteral("0"));
    const QJsonObject history = r.value.value(QStringLiteral("history")).toObject();
    c.history.code = history.value(QStringLiteral("code")).toString();
    c.history.missingEpochs = history.value(QStringLiteral("missingEpochs")).toArray().size();
    c.history.pointsWaiting = history.value(QStringLiteral("pointsWaiting")).toString(QStringLiteral("0"));
    return Result<referral::Claimable>::success(c);
}

inline QStringList strings(const QJsonValue& v)
{
    QStringList out;
    for (const QJsonValue& item : v.toArray())
        out << item.toString();
    return out;
}

// Children come as settled epoch ranges; Uplink shows the last epoch settled.
inline Result<referral::Participant> participant(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<referral::Participant>::failure(r.error);
    if (!r.value.value(QStringLiteral("registered")).toBool())
        return Result<referral::Participant>::failure(QStringLiteral("this participant is not registered"));
    referral::Participant p;
    p.node = r.value.value(QStringLiteral("node")).toString();
    p.referrer = r.value.value(QStringLiteral("referrer")).toString();
    p.rewardBalance = r.value.value(QStringLiteral("rewardBalance")).toString();
    const QJsonObject children = r.value.value(QStringLiteral("children")).toObject();
    for (auto it = children.begin(); it != children.end(); ++it) {
        quint32 last = 0;
        for (const QJsonValue& range : it.value().toArray())
            last = qMax(last, quint32(range.toArray().at(1).toInteger()));
        p.children.insert(it.key(), last);
    }
    const QJsonObject since = r.value.value(QStringLiteral("childrenSince")).toObject();
    for (auto it = since.begin(); it != since.end(); ++it)
        p.since.insert(it.key(), quint32(it.value().toInteger()));
    return Result<referral::Participant>::success(p);
}

inline Result<referral::InvitationCheck> invitationCheck(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<referral::InvitationCheck>::failure(r.error);
    return Result<referral::InvitationCheck>::success({r.value.value(QStringLiteral("referrer")).toString(),
                                                       r.value.value(QStringLiteral("registered")).toBool()});
}

inline Result<referral::Registry> registry(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<referral::Registry>::failure(r.error);
    referral::Registry registry;
    registry.epoch = quint32(r.value.value(QStringLiteral("epoch")).toInteger());
    registry.nodes = strings(r.value.value(QStringLiteral("nodes")));
    registry.active = strings(r.value.value(QStringLiteral("active")));
    return Result<referral::Registry>::success(registry);
}

inline Result<referral::Activity> activity(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<referral::Activity>::failure(r.error);
    referral::Activity activity;
    for (const QJsonValue& epoch : r.value.value(QStringLiteral("epochs")).toArray())
        activity.epochs << quint32(epoch.toInteger());
    const QJsonObject perNode = r.value.value(QStringLiteral("nodes")).toObject();
    for (auto it = perNode.begin(); it != perNode.end(); ++it) {
        QList<int> marks;
        for (const QJsonValue& mark : it.value().toArray())
            marks << mark.toInt();
        activity.nodes.insert(it.key(), marks);
    }
    return Result<referral::Activity>::success(activity);
}

inline Result<QList<referral::Note>> notes(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<QList<referral::Note>>::failure(r.error);
    QList<referral::Note> notes;
    for (const QJsonValue& v : r.value.value(QStringLiteral("notes")).toArray()) {
        const QJsonObject o = v.toObject();
        referral::Note note;
        note.account = o.value(QStringLiteral("account")).toString();
        note.kind = o.value(QStringLiteral("kind")).toString() == QLatin1String("credit") ? referral::Note::Credit
                                                                                         : referral::Note::Child;
        note.node = o.value(QStringLiteral("node")).toString();
        note.since = quint32(o.value(QStringLiteral("since")).toInteger());
        note.amount = o.value(QStringLiteral("amount")).toString(QStringLiteral("0"));
        notes << note;
    }
    return Result<QList<referral::Note>>::success(notes);
}

inline Result<referral::Terms> terms(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<referral::Terms>::failure(r.error);
    referral::Terms terms;
    terms.version = r.value.value(QStringLiteral("version")).toInt();
    terms.sha256 = r.value.value(QStringLiteral("sha256")).toString();
    terms.lastUpdated = r.value.value(QStringLiteral("lastUpdated")).toString();
    terms.sections = r.value.value(QStringLiteral("sections")).toArray().toVariantList();
    return Result<referral::Terms>::success(terms);
}

// payout_message's reply: the text to sign, and the opening's values read from it, so the
// node app is asked for exactly what the module will check.
inline Result<referral::PayoutMessage> payoutMessage(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<referral::PayoutMessage>::failure(r.error);
    referral::PayoutMessage m;
    m.message = r.value.value(QStringLiteral("message")).toString();
    m.receipt = r.value.value(QStringLiteral("receipt")).toString();
    m.points = r.value.value(QStringLiteral("points")).toString(QStringLiteral("0"));
    const QJsonObject opening = QJsonDocument::fromJson(m.message.toUtf8()).object();
    m.programAccount = opening.value(QStringLiteral("program_account")).toString();
    m.blindingFactor = opening.value(QStringLiteral("blinding_factor")).toString();
    if (m.programAccount.isEmpty() || m.blindingFactor.isEmpty())
        return Result<referral::PayoutMessage>::failure(QStringLiteral("uplink_module: not a payout opening"));
    return Result<referral::PayoutMessage>::success(m);
}

inline Result<QList<referral::Receipt>> receipts(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<QList<referral::Receipt>>::failure(r.error);
    QList<referral::Receipt> receipts;
    for (const QJsonValue& v : r.value.value(QStringLiteral("receipts")).toArray()) {
        const QJsonObject o = v.toObject();
        referral::Receipt receipt;
        receipt.index = quint64(o.value(QStringLiteral("index")).toInteger());
        receipt.account = o.value(QStringLiteral("account")).toString();
        receipt.points = o.value(QStringLiteral("points")).toString();
        receipts << receipt;
    }
    return Result<QList<referral::Receipt>>::success(receipts);
}

// auto_claim answers {"claimed": {operation}} or {"skipped": reason}: the reference, or "".
inline Result<QString> autoClaimReference(const QString& json)
{
    const auto r = reply(json);
    if (!r.ok())
        return Result<QString>::failure(r.error);
    return Result<QString>::success(
        r.value.value(QStringLiteral("claimed")).toObject().value(QStringLiteral("reference")).toString());
}

} // namespace referral_json

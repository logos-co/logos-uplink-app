#pragma once

#include <QMap>
#include <QSet>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariantList>

// Which nodes were in the oracle's active set, epoch by epoch, as Uplink saw it published.
// The program keeps only the latest epoch, so this is recorded on each poll; an epoch
// published while Uplink wasn't running stays unknown. Only the shown window is kept.
// Epochs before a referral was found are unknown too: the wallet finds referrals by
// scanning blocks, so one can turn up long after it joined.
class ActivityLog {
public:
    enum Mark { Active = 1, Inactive = 0, Unknown = -1 };

    static constexpr int kWindow = 16;

    // Records `epoch` for every node in `nodes` not yet recorded for it, so a referral
    // found partway through an epoch still gets that epoch. True if anything was new.
    bool record(quint32 epoch, const QStringList& nodes, const QStringList& active)
    {
        if (m_started && epoch < m_lastEpoch)
            return false;
        const QSet<QString> activeSet(active.cbegin(), active.cend());
        bool changed = false;
        for (const QString& node : nodes) {
            if (m_marks.value(node).contains(epoch))
                continue;
            m_marks[node].insert(epoch, activeSet.contains(node));
            changed = true;
        }
        if (!changed)
            return false;
        m_started = true;
        m_lastEpoch = epoch;
        prune();
        return true;
    }

    quint32 lastEpoch() const { return m_lastEpoch; }

    // The last kWindow published epochs for `node`, oldest first.
    QVariantList window(const QString& node) const
    {
        QVariantList out;
        const qint64 newest = m_lastEpoch;
        for (qint64 e = newest - kWindow + 1; e <= newest; ++e) {
            const QMap<quint32, bool> marks = m_marks.value(node);
            if (e < 0 || !m_started) {
                out << Unknown;
            } else if (marks.contains(quint32(e))) {
                out << (marks.value(quint32(e)) ? Active : Inactive);
            } else {
                out << Unknown;
            }
        }
        return out;
    }

    // As plain INI keys in the current group of `ini`, readable by hand:
    //   last=13   marks/<node>=12:1,13:0
    void writeTo(QSettings& ini) const
    {
        ini.remove(QString());
        ini.setValue(QStringLiteral("last"), m_lastEpoch);
        for (auto n = m_marks.cbegin(); n != m_marks.cend(); ++n) {
            QStringList epochs;
            for (auto e = n->cbegin(); e != n->cend(); ++e)
                epochs << QStringLiteral("%1:%2").arg(e.key()).arg(e.value() ? 1 : 0);
            ini.setValue(QStringLiteral("marks/") + n.key(), epochs.join(QLatin1Char(',')));
        }
    }

    static ActivityLog readFrom(QSettings& ini)
    {
        ActivityLog log;
        if (!ini.contains(QStringLiteral("last")))
            return log;
        log.m_started = true;
        log.m_lastEpoch = ini.value(QStringLiteral("last")).toUInt();
        ini.beginGroup(QStringLiteral("marks"));
        for (const QString& node : ini.childKeys())
            for (const QString& mark : ini.value(node).toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
                log.m_marks[node].insert(mark.section(QLatin1Char(':'), 0, 0).toUInt(),
                                         mark.section(QLatin1Char(':'), 1, 1) == QLatin1String("1"));
        ini.endGroup();
        return log;
    }

private:
    void prune()
    {
        const qint64 oldest = qint64(m_lastEpoch) - kWindow + 1;
        for (auto n = m_marks.begin(); n != m_marks.end(); ++n)
            for (auto e = n->begin(); e != n->end();)
                e = qint64(e.key()) < oldest ? n->erase(e) : std::next(e);
    }

    bool m_started = false;
    quint32 m_lastEpoch = 0;
    QMap<QString, QMap<quint32, bool>> m_marks;
};

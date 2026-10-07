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
class ActivityLog {
public:
    enum Mark { Active = 1, Inactive = 0, Unknown = -1, NotYet = -2 };   // NotYet: before it was a referral

    static constexpr int kWindow = 16;

    // Records `epoch` for every node in `nodes`; true if it was new.
    bool record(quint32 epoch, const QStringList& nodes, const QStringList& active)
    {
        if (epoch <= m_lastEpoch && m_started)
            return false;
        if (!m_started) {
            m_started = true;
            m_firstEpoch = epoch;
        }
        const QSet<QString> activeSet(active.cbegin(), active.cend());
        for (const QString& node : nodes) {
            if (!m_firstSeen.contains(node))
                m_firstSeen.insert(node, epoch);
            m_marks[node].insert(epoch, activeSet.contains(node));
        }
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
            } else if (m_firstSeen.contains(node) && quint32(e) < m_firstSeen.value(node)
                       && m_firstSeen.value(node) > m_firstEpoch) {
                out << NotYet;   // appeared after recording began, so it wasn't a referral yet
            } else {
                out << Unknown;
            }
        }
        return out;
    }

    // As plain INI keys in the current group of `ini`, readable by hand:
    //   last=13   first=10   firstSeen/<node>=10   marks/<node>=12:1,13:0
    void writeTo(QSettings& ini) const
    {
        ini.remove(QString());
        ini.setValue(QStringLiteral("first"), m_firstEpoch);
        ini.setValue(QStringLiteral("last"), m_lastEpoch);
        for (auto n = m_marks.cbegin(); n != m_marks.cend(); ++n) {
            QStringList epochs;
            for (auto e = n->cbegin(); e != n->cend(); ++e)
                epochs << QStringLiteral("%1:%2").arg(e.key()).arg(e.value() ? 1 : 0);
            ini.setValue(QStringLiteral("marks/") + n.key(), epochs.join(QLatin1Char(',')));
        }
        for (auto f = m_firstSeen.cbegin(); f != m_firstSeen.cend(); ++f)
            ini.setValue(QStringLiteral("firstSeen/") + f.key(), f.value());
    }

    static ActivityLog readFrom(QSettings& ini)
    {
        ActivityLog log;
        if (!ini.contains(QStringLiteral("last")))
            return log;
        log.m_started = true;
        log.m_firstEpoch = ini.value(QStringLiteral("first")).toUInt();
        log.m_lastEpoch = ini.value(QStringLiteral("last")).toUInt();
        ini.beginGroup(QStringLiteral("marks"));
        for (const QString& node : ini.childKeys())
            for (const QString& mark : ini.value(node).toString().split(QLatin1Char(','), Qt::SkipEmptyParts))
                log.m_marks[node].insert(mark.section(QLatin1Char(':'), 0, 0).toUInt(),
                                         mark.section(QLatin1Char(':'), 1, 1) == QLatin1String("1"));
        ini.endGroup();
        ini.beginGroup(QStringLiteral("firstSeen"));
        for (const QString& node : ini.childKeys())
            log.m_firstSeen.insert(node, ini.value(node).toUInt());
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
    quint32 m_firstEpoch = 0;
    quint32 m_lastEpoch = 0;
    QMap<QString, QMap<quint32, bool>> m_marks;
    QMap<QString, quint32> m_firstSeen;
};

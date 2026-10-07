#pragma once

#include <QMap>
#include <QObject>
#include <QTimer>
#include <memory>

#include "ActivityLog.h"
#include "logos_ui_plugin_context.h"
#include "rep_uplink_ui_source.h"
#include "interfaces/NodeService.h"
#include "interfaces/ReferralService.h"

// Source side of uplink_ui.rep.
class UplinkBackend : public UplinkUiSimpleSource, public LogosUiPluginContext {
    Q_OBJECT

public:
    explicit UplinkBackend(QObject* parent = nullptr);
    ~UplinkBackend() override;

    void onContextReady() override;

public slots:
    void refresh() override;
    void createIdentity() override;
    void checkInvitation(QString code) override;
    void joinUnder(QString code) override;
    void prepareEnroll() override;
    void completeEnroll(QString signatureHex, QString publicKeyHex) override;
    void reportSignFailed(QString error) override;
    void cashOutAll() override;
    void finishCashOut() override;
    void completePayoutSignature(QString signatureHex, QString publicKeyHex) override;
    void reportPayoutSignFailed(QString error) override;
    void setReferralLabel(QString node, QString label) override;

private:
    struct Operation {
        QString kind;
        int status = OperationPending;
    };

    void refreshNode();
    void refreshWallet();
    void showIdentity();
    void syncWallet();
    static QString dataFile();
    void recordActivity(const referral::Registry& registry, const QString& myNode, const QStringList& referralNodes);
    void submitRegistration(const QByteArray& signature);
    void startCashOut();
    void preparePayoutCode();
    void failCashOut(const QString& error);
    void syncNextChunk();
    void refreshReferral();
    void reconcileOperations();
    void submit(const QString& kind, const Result<referral::Status>& submitted, const QString& reference);
    void publishOperations();
    void fail(const QString& error);

    std::unique_ptr<referral::ReferralService> m_referral;
    std::unique_ptr<node::NodeService> m_node;

    QMap<QString, Operation> m_operations;           // reference -> operation
    QString m_registerReference;
    QString m_collectReference;                      // the Collect behind a cash out
    QString m_cashOutReference;
    referral::Opening m_payoutOpening;              // what the payout code is being signed for
    QMap<QString, QString> m_labels;                 // node -> label
    ActivityLog m_activity;
    QString m_activityOwner;                         // participant whose saved data is loaded
    QTimer m_poll;
    bool m_syncing = false;
    qint64 m_syncTarget = 0;
};

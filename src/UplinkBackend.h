#pragma once

#include <QMap>
#include <QObject>
#include <QSet>
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
    // For tests: services handed in rather than picked by UPLINK_BACKEND, and Uplink's
    // own file kept in `dataDir` (macOS ignores XDG_DATA_HOME).
    UplinkBackend(std::unique_ptr<referral::ReferralService> referral,
                  std::unique_ptr<node::NodeService> node, const QString& dataDir,
                  QObject* parent = nullptr);
    ~UplinkBackend() override;

    void onContextReady() override;

public slots:
    void refresh() override;
    void createIdentity() override;
    void checkInvitation(QString code) override;
    void joinUnder(QString code) override;
    void prepareEnroll() override;
    void completeEnroll(QString payloadHex, QString signatureHex, QString publicKeyHex) override;
    void reportSignFailed(QString payloadHex, QString error) override;
    void cashOutAll() override;
    void payoutCodeFor(int receiptIndex) override;
    void finishCashOut() override;
    void completePayoutSignature(QString payloadHex, QString signatureHex, QString publicKeyHex) override;
    void reportPayoutSignFailed(QString payloadHex, QString error) override;
    void setReferralLabel(QString node, QString label) override;

private:
    void init();

    struct Operation {
        QString kind;
        int status = OperationPending;
    };

    void refreshNode();
    void refreshWallet();
    void showIdentity();
    void forgetIdentity();
    bool isCurrentSignRequest(const QString& payloadHex) const;
    QString checkSignature(const QString& signatureHex, const QString& publicKeyHex) const;
    void syncWallet();
    QString dataFile() const;
    void recordActivity(const referral::Registry& registry, const QString& myNode, const QStringList& referralNodes);
    void submitRegistration(const QByteArray& signature);
    void startCashOut();
    void preparePayoutCode(quint64 receiptIndex);
    void prepareNewReceiptCode();
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
    QSet<quint64> m_receiptsBefore;                  // receipts that existed before m_cashOutReference
    referral::Opening m_payoutOpening;              // what the payout code is being signed for
    QMap<QString, QString> m_labels;                 // node -> label
    ActivityLog m_activity;
    QString m_activityOwner;                         // participant whose saved data is loaded
    QString m_dataDir;                               // "" = the standard location
    bool m_identityUnlabelled = false;               // created this session, but the wallet didn't keep its label
    QTimer m_poll;
    bool m_syncing = false;
    qint64 m_syncTarget = 0;
};

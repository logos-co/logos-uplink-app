#pragma once

#include <QMap>
#include <QObject>
#include <QTimer>
#include <memory>

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
    void joinUnder(QString invitationBlob) override;
    void prepareEnroll() override;
    void completeEnroll(QString signatureHex, QString publicKeyHex) override;
    void reportSignFailed(QString error) override;
    void claimPoints() override;
    void cashOut() override;
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
    QMap<QString, QString> m_labels;                 // node -> label
    QTimer m_poll;
    bool m_syncing = false;
    qint64 m_syncTarget = 0;
};

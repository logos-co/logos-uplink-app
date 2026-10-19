#pragma once

#include <QMap>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <functional>
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

    // For tests: services handed in rather than picked by UPLINK_BACKEND, and Uplink's
    // own file kept in `dataDir` (macOS ignores XDG_DATA_HOME).
    // `nodeApp` answers the node app's requests in place of the real one (mock mode).
    using NodeApp = std::function<QVariantMap(const QVariantMap& request)>;
    UplinkBackend(std::unique_ptr<referral::ReferralService> referral,
                  std::unique_ptr<node::NodeService> node, const QString& dataDir,
                  NodeApp nodeApp = {}, QObject* parent = nullptr);
    ~UplinkBackend() override;

    // The deployment the release names in metadata.json (read at startup); tests set it here.
    void setReleaseDeployment(const QString& chainId, const QString& programAccount);

    void onContextReady() override;

public slots:
    void refresh() override;
    void completeSignRequest(QString requestRef, QVariantMap answer) override;
    void reportSignFailed(QString requestRef, QString error) override;
    void acceptTerms() override;
    void changeAutoClaim(bool on) override;
    void createIdentity() override;
    void checkInvitation(QString code) override;
    void joinUnder(QString code) override;
    void enrol() override;
    void cashOutAll() override;
    void payoutCodeFor(int receiptIndex) override;
    void finishCashOut() override;
    void setReferralLabel(QString node, QString label) override;

private:
    void init();

    struct Operation {
        QString kind;
        int status = OperationPending;
    };

    void refreshNode();
    void applyDeployment();
    void checkProgram();
    void refreshWallet();
    void refreshTerms();
    void showIdentity();
    void forgetIdentity();
    void syncWallet();
    QString dataFile() const;
    void loadSettings();
    void loadLabels();
    void startCollecting();
    void startCashOut();
    void preparePayoutCode(quint64 receiptIndex);
    void prepareNewReceiptCode();
    void failCashOut(const QString& error);
    void askNodeApp(const QString& intent, const QVariantMap& params);
    void finishEnrol(const QString& node, const QString& signature);
    void syncNextChunk();
    void refreshReferral();
    void autoClaimIfDue();
    void reconcileOperations();
    bool hasPendingOperation() const;
    bool track(const QString& kind, const Result<referral::Status>& submitted, const QString& reference);
    void publishOperations();
    void fail(const QString& error);

    std::unique_ptr<referral::ReferralService> m_referral;
    std::unique_ptr<node::NodeService> m_node;

    QMap<QString, Operation> m_operations;           // reference -> operation
    QString m_registerReference;
    QString m_collectReference;                      // a claim behind a cash out
    QString m_cashOutReference;
    QSet<quint64> m_receiptsBefore;                  // receipts that existed before m_cashOutReference
    QMap<QString, QString> m_labels;                 // node -> label
    QString m_labelsOwner;                           // participant whose labels are loaded
    QString m_dataDir;                               // "" = the standard location
    NodeApp m_nodeApp;                               // mock mode's; empty: the real one, through QML
    QVariantMap m_request;                           // {ref, intent, params} the node app is asked
    int m_enrolBefore = 0;                           // the enrol state to go back to if it says no
    quint64 m_payoutIndex = 0;                       // the receipt whose code is being signed
    QString m_releaseChain;                          // "" = any chain
    QString m_releaseProgram;                        // "" = the module's own choice
    bool m_deploymentApplied = false;
    bool m_sending = false;                          // a transaction is being proved and sent
    QTimer m_poll;
    bool m_syncing = false;
    qint64 m_syncTarget = 0;
};

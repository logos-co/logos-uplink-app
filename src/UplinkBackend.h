#pragma once

#include <QObject>

#include "logos_ui_plugin_context.h"
#include "rep_uplink_ui_source.h"

// Source side of uplink_ui.rep.
class UplinkBackend : public UplinkUiSimpleSource, public LogosUiPluginContext {
    Q_OBJECT

public:
    explicit UplinkBackend(QObject* parent = nullptr);

    void onContextReady() override;
};
